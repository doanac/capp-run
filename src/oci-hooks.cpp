#include "oci-hooks.h"

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

void ocispec_create(const std::string &app_name, const std::string &svc,
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
      {"args", {"capp-run", "-n", app_name, "poststop", svc}},
  };
  hooks.emplace_back(entry);
  data["hooks"]["poststop"] = hooks;
  boost::filesystem::ofstream os(out);
  os << data;
}
