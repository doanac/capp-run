#include "project.h"

#include "yaml-cpp/yaml.h"
#include <boost/process.hpp>
#include <iostream>
#include <sys/stat.h>

// TODO - this is quite all of what docker does
static Port parse_port(const std::string raw) {
  Port p{.host_ip = "0.0.0.0", .protocol = "tcp"};
  std::vector<std::string> parts;
  boost::split(parts, raw, boost::is_any_of(":"));

  std::string port_and_proto;
  if (parts.size() == 3) {
    p.host_ip = parts[0];
    p.host_port = std::stoi(parts[1]);
    port_and_proto = parts[2];
  } else if (parts.size() == 2) {
    p.host_port = std::stoi(parts[0]);
    port_and_proto = parts[1];
  } else if (parts.size() == 1) {
    p.target_port = p.host_port = std::stoi(parts[0]);
  } else {
    std::string err = "Unsupported port mapping: " + raw;
    throw std::runtime_error(err);
  }

  if (port_and_proto.size() > 0) {
    std::vector<std::string> parts;
    boost::split(parts, port_and_proto, boost::is_any_of("/"));
    if (parts.size() == 1) {
      p.target_port = std::stoi(parts[0]);
    } else if (parts.size() == 2) {
      p.target_port = std::stoi(parts[0]);
      p.protocol = parts[1];
    } else {
      std::string err = "Unsupported port mapping: " + raw;
      throw std::runtime_error(err);
    }
  }
  return p;
}

static void parse_ports(const std::vector<std::string> &ports_raw,
                        std::vector<Port> &ports) {
  for (const auto &raw : ports_raw) {
    ports.emplace_back(parse_port(raw));
  }
}

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

    auto mode = it->second["network_mode"];
    if (mode.IsDefined()) {
      svc.network_mode = mode.as<std::string>();
    }

    auto node = it->second["networks"];
    if (node.IsDefined()) {
      svc.networks = node.as<std::vector<std::string>>();
    }
    if (svc.networks.size() == 0 && svc.network_mode != "host") {
      svc.networks.emplace_back("default");
    }

    auto user = it->second["user"];
    if (user.IsDefined()) {
      svc.user = user.as<std::string>();
    }
    svc.image = it->second["image"].as<std::string>();

    auto ports = it->second["ports"];
    if (ports.IsDefined()) {
      parse_ports(ports.as<std::vector<std::string>>(), svc.ports);
    }
    def.services.push_back(svc);
  }
  return def;
}

Service ProjectDefinition::get_service(const std::string &name) {
  for (const auto &s : services) {
    if (s.name == name) {
      return s;
    }
  }
  std::string msg = "No such service: ";
  throw std::runtime_error(msg + name);
}
