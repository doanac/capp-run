#include "oci-hooks.h"

#include "json.h"

#include "context.h"
#include "project.h"

void oci_createRuntime(const std::string &app_name) {
  Context::Load(app_name);
  ProjectDefinition::Load("docker-compose.yml");
}

void oci_poststop(const std::string &app_name) {
  Context::Load(app_name);
  ProjectDefinition::Load("docker-compose.yml");
}

void ocispec_create(const boost::filesystem::path &spec,
                    const boost::filesystem::path &out,
                    const boost::filesystem::path &rootfs) {
  boost::filesystem::ifstream config(spec);
  nlohmann::json data;
  config >> data;
  data["root"]["path"] = rootfs.string();
  boost::filesystem::ofstream os(out);
  os << data;
}
