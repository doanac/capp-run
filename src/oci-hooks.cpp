#include "oci-hooks.h"

#include "project.h"

void oci_createRuntime() {
  ProjectDefinition::Load("docker-compose.yml");
}

void oci_poststop() {
  ProjectDefinition::Load("docker-compose.yml");
}
