#include "context.h"

#include <boost/process.hpp>
#include <fcntl.h>
#include <sys/file.h>
#include <sys/stat.h>

#include "json.h"

class Lock {
public:
  Lock(int fd) : fd_(fd) {}
  ~Lock() {
    if (fd_ != -1) {
      close(fd_);
    }
  }

private:
  int fd_;
};

static std::unique_ptr<Lock> create_lock(boost::filesystem::path lockfile) {
  int fd = open(lockfile.c_str(), O_RDWR | O_CREAT | O_APPEND, 0666);
  if (fd < 0) {
    throw std::runtime_error("Unable to open network lock file");
  }
  if (flock(fd, LOCK_EX) < 0) {
    throw std::runtime_error("Unable to acquire network lock file");
    close(fd);
  }
  return std::make_unique<Lock>(fd);
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
  auto lock = create_lock(path / ".lock");

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

  int exit_code = boost::process::system(path / "mk-network");
  if (exit_code != 0) {
    throw std::runtime_error("Unable to setup network");
  }
}
