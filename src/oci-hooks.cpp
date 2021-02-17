#include "oci-hooks.h"

#include <boost/algorithm/string.hpp>
#include <sys/mount.h>

#include "json.h"

#include "context.h"
#include "project.h"

void oci_createRuntime(const std::string &app_name) {
  Context::Load(app_name);
  ProjectDefinition::Load("docker-compose.yml");
}

void oci_poststop(const std::string &app_name, const std::string &svc) {
  auto ctx = Context::Load(app_name);
  auto rootfs = ctx.var_lib / "mounts" / svc / "rootfs";
  if (umount(rootfs.c_str()) != 0) {
    throw std::runtime_error("Unable to unmount container rootfs");
  }
}

struct user {
  int uid;
  int gid;
};

static void find_user(const boost::filesystem::path &passwd,
                      const std::string &user, struct user &entry) {
  std::ifstream infile(passwd.string());
  std::string line;
  while (std::getline(infile, line)) {
    std::vector<std::string> parts;
    boost::split(parts, line, boost::is_any_of(":"));
    if (parts[0] == user) {
      entry.uid = std::stoi(parts[2]);
      entry.gid = std::stoi(parts[3]);
      return;
    }
  }
  throw std::runtime_error("Unable to find user");
}

static int find_group(const boost::filesystem::path &grp,
                      const std::string &group) {
  std::ifstream infile(grp.string());
  std::string line;
  while (std::getline(infile, line)) {
    std::vector<std::string> parts;
    boost::split(parts, line, boost::is_any_of(":"));
    if (parts[0] == group) {
      return std::stoi(parts[2]);
    }
  }
  throw std::runtime_error("Unable to find group");
}

static void fix_user(const std::string &user,
                     const boost::filesystem::path &rootfs,
                     nlohmann::json &spec) {
  if (!user.empty()) {
    int uid = 0;
    int gid = 0;
    bool gid_set = false;

    std::vector<std::string> parts;
    boost::split(parts, user, boost::is_any_of(":"));
    if (parts.size() == 2) {
      try {
        gid = std::stoi(parts[1]);
        gid_set = true;
      } catch (const std::exception &ex) {
        gid = find_group(rootfs / "etc/group", parts[1]);
        gid_set = true;
      }
    }
    try {
      uid = std::stoi(parts[0]);
    } catch (const std::exception &ex) {
      struct user entry;
      find_user(rootfs / "etc/passwd", parts[0], entry);
      uid = entry.uid;
      if (!gid_set) {
        gid = entry.gid;
      }
    }
    spec["process"]["user"]["uid"] = uid;
    spec["process"]["user"]["gid"] = gid;
  }
}

void ocispec_create(const std::string &app_name, const Service &svc,
                    const boost::filesystem::path &spec,
                    const boost::filesystem::path &out,
                    const boost::filesystem::path &rootfs) {

  auto exe = boost::filesystem::read_symlink("/proc/self/exe");
  boost::filesystem::ifstream config(spec);
  nlohmann::json data;
  config >> data;
  data["root"]["path"] = rootfs.string();
  nlohmann::json hooks = {};
  nlohmann::json entry = {
      {"path", exe.string()},
      {"args", {"capp-run", "-n", app_name, "poststop", svc.name}},
  };
  hooks.emplace_back(entry);
  data["hooks"]["poststop"] = hooks;

  fix_user(svc.user, rootfs, data);

  boost::filesystem::ofstream os(out);
  os << data;
}
