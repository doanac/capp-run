#include "oci-hooks.h"

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
