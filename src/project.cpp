#include "project.h"

#include "yaml-cpp/yaml.h"
#include <boost/process.hpp>
#include <iostream>
#include <sys/stat.h>

ProjectDefinition ProjectDefinition::Load(const std::string &path) {
  ProjectDefinition def{};

  YAML::Node node = YAML::LoadFile(path);
  auto networks = node["networks"];
  for (YAML::const_iterator it = networks.begin(); it != networks.end(); ++it) {
    def.networks.emplace_back(it->first.as<std::string>());
  }
  auto volumes = node["volumes"];
  for (YAML::const_iterator it = volumes.begin(); it != volumes.end(); ++it) {
    def.volumes.emplace_back(it->first.as<std::string>());
  }
  auto services = node["services"];
  for (YAML::const_iterator it = services.begin(); it != services.end(); ++it) {
    Service svc(it->first.as<std::string>());
    auto node = it->second["networks"];
    if (node.IsDefined()) {
      svc.networks = node.as<std::vector<std::string>>();
    }
    def.services.push_back(svc);
  }
  return def;
}
