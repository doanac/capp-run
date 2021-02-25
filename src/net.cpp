#include "net.h"

#include <fcntl.h>
#include <sys/file.h>
#include <sys/stat.h>

#include "json.h"

static int shell(const std::string &command, std::string *output) {
  std::array<char, 128> buffer{};
  std::string full_command(command);
  full_command += " 2>&1";

  FILE *pipe = popen(full_command.c_str(), "r");
  if (pipe == nullptr) {
    *output = "popen() failed!";
    return -1;
  }
  while (feof(pipe) == 0) {
    if (fgets(buffer.data(), buffer.size(), pipe) != nullptr) {
      *output += buffer.data();
    }
  }
  int exitcode = pclose(pipe);
  return WEXITSTATUS(exitcode);
}

class LockedFile {
public:
  LockedFile(const boost::filesystem::path &path);
  ~LockedFile();

  std::string read() const;
  void write(const std::string &buf);

private:
  FILE *fd_;
};

LockedFile::LockedFile(const boost::filesystem::path &path) {
  fd_ = fopen(path.string().c_str(), "a+");
  if (fd_ == NULL) {
    throw std::runtime_error("Unable to file");
  }
  if (flock(fileno(fd_), LOCK_EX) != 0) {
    throw std::runtime_error("Unable to lock file");
  }
}

LockedFile::~LockedFile() { fclose(fd_); }

std::string LockedFile::read() const {
  fseek(fd_, 0, SEEK_END);
  size_t size = ftell(fd_);
  fseek(fd_, 0, SEEK_SET);

  char *buf = (char *)calloc(size + 1, 1);
  if (fread(buf, 1, size, fd_) != size) {
    free(buf);
    throw std::runtime_error("Unable to read contents of file");
  }
  std::string rv(buf);
  free(buf);
  return rv;
}

void LockedFile::write(const std::string &buf) {
  fseek(fd_, 0, SEEK_SET);
  ftruncate(fileno(fd_), 0);
  fwrite(buf.c_str(), 1, buf.size(), fd_);
}

static std::string
find_bridge(const std::map<std::string, std::string> &interfaces) {
  std::string base = "bcomp-";
  for (int i = 1; i < 30; i++) {
    std::string name = base + std::to_string(i);
    if (interfaces.find(name) == interfaces.end()) {
      return name;
    }
  }
  throw std::runtime_error("Unable to find an available bridge");
}

static bool contains_val(const std::map<std::string, std::string> &interfaces,
                         const std::string &val) {
  for (const auto &it : interfaces) {
    if (it.second == val) {
      return true;
    }
  }
  return false;
}

static std::string
find_netbase(const std::map<std::string, std::string> &interfaces) {
  std::string base = "172.42.";
  for (int i = 1; i < 30; i++) {
    std::string name = base + std::to_string(i);
    if (!contains_val(interfaces, name + ".1")) {
      return name;
    }
  }
  throw std::runtime_error("Unable to find an available subnet");
}

void network_render(const Context &ctx, const std::string &name) {
  auto path = ctx.var_run / "networks" / name;
  boost::filesystem::create_directories(path);

  // Make sure 2 different containers don't do this at the same time if they
  // share the same network
  LockedFile lock(path / ".lock");

  auto gwinfo = path / "info";
  if (boost::filesystem::exists(gwinfo)) {
    ctx.out() << "Network(" << name << ") already in place\n";
    return;
  }

  auto intf = ctx.network_interfaces();
  auto bridge = find_bridge(intf);
  ctx.out() << "Creating bridge: " << bridge << "\n";
  auto base = find_netbase(intf);
  std::string network = base + ".0";
  std::string gateway = base + ".1";

  ctx.out() << "Creating network(" << network << ") gateway-ip(" << gateway
            << ")\n";

  std::ofstream gw(gwinfo.string());
  if (!gw.is_open()) {
    throw std::runtime_error("Unable to create gateway info");
  }
  nlohmann::json data;
  data["gateway"] = gateway;
  data["bridge"] = bridge;
  data["hosts"] = {};
  gw << data;

  std::ofstream mk((path / "mk-network").string());
  if (!mk.is_open()) {
    throw std::runtime_error("Unable to create network script");
  }
  // TODO - the bridges are allowing traffic between them
  mk << "#!/bin/sh -ex\n"
     << "ip link add " << bridge << " type bridge\n"
     << "ip link set " << bridge << " up\n"
     << "ip addr add " << gateway << "/24 brd + dev " << bridge << "\n"
     << "iptables -A FORWARD -o " << bridge << " -j ACCEPT\n"
     << "iptables -A FORWARD -i " << bridge << " -j ACCEPT\n"
     << "iptables -t nat -A POSTROUTING -s " << network
     << "/24 -j MASQUERADE\n";
  mk.close();
  chmod((path / "mk-network").string().c_str(), S_IRWXU);

  std::ofstream rm((path / "rm-network").string());
  if (!rm.is_open()) {
    throw std::runtime_error("Unable to create network script");
  }
  rm << "#!/bin/sh -x\n"
     << "iptables -t nat -D POSTROUTING -s " << network << "\n"
     << "iptables -D FORWARD -i " << bridge << " -j ACCEPT\n"
     << "iptables -D FORWARD -o " << bridge << " -j ACCEPT\n"
     << "ip link del name " << bridge << " type bridge\n"
     << "rm -rf " << path << "\n";
  rm.close();
  chmod((path / "rm-network").string().c_str(), S_IRWXU);

  std::string out;
  int exit_code = shell((path / "mk-network").string(), &out);
  ctx.out() << out << "\n";
  if (exit_code != 0) {
    throw std::runtime_error("Unable to setup network");
  }
}

struct ipinfo {
  std::string ip;
  std::string gateway;
  std::string bridge;
};

static ipinfo acquire_ip(const boost::filesystem::path &info,
                         const std::string &host) {
  ipinfo inf{};
  LockedFile lock(info);

  std::ifstream in(info.string());
  if (!in.is_open()) {
    throw std::runtime_error("Unable to read gateway info");
  }
  nlohmann::json data;
  in >> data;
  inf.gateway = data["gateway"].get<std::string>();
  inf.bridge = data["bridge"].get<std::string>();

  auto base = inf.gateway.substr(0, inf.gateway.size() - 1);
  for (int i = 2; i < 30; i++) {
    bool found = false;
    std::string ip = base + std::to_string(i);
    for (const auto &it : data["hosts"].items()) {
      if (it.value() == ip) {
        found = true;
      } else if (it.key() == host) {
        found = false;
        ip = it.value();
        break;
      }
    }
    if (!found) {
      data["hosts"][host] = ip;
      inf.ip = ip;
      std::ofstream gw(info.string());
      if (!gw.is_open()) {
        throw std::runtime_error("Unable to update network info");
      }
      gw << data;
      return inf;
    }
  }
  throw std::runtime_error("Unable to find an available IP in network");
}

static std::string
find_intf(const std::map<std::string, std::string> &interfaces) {
  std::string base = "vcomp-";
  for (int i = 1; i < 30; i++) {
    std::string name = base + std::to_string(i);
    // the link we'll see is "br-" side - the "name" will be in a netns
    if (interfaces.find("br-" + name) == interfaces.end()) {
      return name;
    }
  }
  throw std::runtime_error("Unable to find an available interface");
}

static void _set_hosts(const boost::filesystem::path &path,
                       const std::string &host, const std::string &ip) {
  LockedFile hosts(path);
  auto content = hosts.read();
  if (content.size() == 0) {
    content = "127.0.0.1\tlocalhost\n";
  }
  std::string line = ip + "\t" + host + "\n";
  if (content.find(line) == std::string::npos) {
    content += line;
  }
  hosts.write(content);
}

void network_join(const Context &ctx, const Service &svc, int pid) {
  std::string ns = ctx.app + "-" + svc.name;

  auto path = ctx.var_run / svc.name;
  boost::filesystem::create_directories(path);

  std::ofstream rm((path / "rm-network").string());
  if (!rm.is_open()) {
    throw std::runtime_error("Unable to create network script");
  }
  rm << "#!/bin/sh -x\n";

  std::ofstream mk((path / "mk-network").string());
  if (!mk.is_open()) {
    throw std::runtime_error("Unable to create network script");
  }

  auto interfaces = ctx.network_interfaces();

  mk << "#!/bin/sh -ex\n"
     << "[ -d /var/run/netns ] || mkdir /var/run/netns\n"
     << "ln -sf /proc/" << pid << "/ns/net /var/run/netns/" << ns << "\n";

  std::string default_ip;
  bool default_set = false;
  for (const auto net : svc.networks) {
    ctx.out() << "Joining " << net << "\n";
    auto inf = acquire_ip(ctx.var_run / "networks" / net / "info", svc.name);
    ctx.out() << " bridge: " << inf.bridge << "\n";
    ctx.out() << " gateway: " << inf.gateway << "\n";
    ctx.out() << " ip: " << inf.ip << "\n";

    auto intf = find_intf(interfaces);
    ctx.out() << " interface: " << intf << "\n";
    interfaces["br-" + intf] =
        inf.ip; // mark it so the next loop doesn't use it

    mk << "\n# net " << net << "\n"
       << "ip link add " << intf << " type veth peer name br-" << intf << "\n"
       << "ip link set " << intf << " netns " << ns << "\n"
       << "ip netns exec " << ns << " ip addr add " << inf.ip << "/24 dev "
       << intf << "\n"
       << "ip link set br-" << intf << " up\n"
       << "ip netns exec " << ns << " ip link set lo up\n"
       << "ip netns exec " << ns << " ip link set " << intf << " up\n"
       << "ip link set br-" << intf << " master " << inf.bridge << "\n";
    if (!default_set) {
      mk << "ip netns exec " << ns << " ip route add default via "
         << inf.gateway << " || echo andy????"
         << "\n";
      default_ip = inf.ip;
      default_set = true;
    }

    _set_hosts(ctx.var_run / "etc_hosts", svc.name, inf.ip);
  }

  for (const auto &p : svc.ports) {
    mk << "iptables -t nat -A OUTPUT -p " << p.protocol << " --match "
       << p.protocol << " --dport " << p.host_port << " --jump DNAT --to "
       << default_ip << ":" << p.target_port << "\n";
    rm << "iptables -t nat -D OUTPUT -p " << p.protocol << " --match "
       << p.protocol << " --dport " << p.host_port << " --jump DNAT --to "
       << default_ip << ":" << p.target_port << "\n";
  }
  mk.close();
  chmod((path / "mk-network").string().c_str(), S_IRWXU);

  rm << "ip netns del " << ns << "\n";
  rm.close();
  chmod((path / "rm-network").string().c_str(), S_IRWXU);

  std::string out;
  int exit_code = shell((path / "mk-network").string(), &out);
  ctx.out() << out << "\n";
  if (exit_code != 0) {
    throw std::runtime_error("Unable to setup network");
  }
}

bool network_destroy(const Context &ctx, const Service &svc) {
  auto path = ctx.var_run / svc.name / "rm-network";
  std::string out;
  int exit_code = shell(path.string(), &out);
  ctx.out() << out << "\n";
  return exit_code == 0;
}
