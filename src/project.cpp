#include "project.h"

#include <boost/process.hpp>
#include <iostream>
#include <sys/stat.h>

#include "json.h"
#include "utils.h"

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

  nlohmann::json data;
  open_read(path) >> data;

  for (const auto &network : data["networks"]) {
    def.networks.emplace_back(network.get<std::string>());
  }

  for (const auto &volume : data["volumes"].items()) {
    def.volumes.emplace_back(volume.key());
  }

  for (const auto &item : data["services"].items()) {
    Service svc(item.key());

    auto mode = item.value()["network_mode"];
    if (mode.is_string()) {
      svc.network_mode = mode.get<std::string>();
    }

    for (const auto &net : item.value()["networks"]) {
      svc.networks.emplace_back(net);
    }
    if (svc.networks.size() == 0 && svc.network_mode != "host") {
      svc.networks.emplace_back("default");
    }

    auto user = item.value()["user"];
    if (!user.is_null()) {
      svc.user = user.get<std::string>();
    }
    svc.image = item.value()["image"].get<std::string>();

    auto ports = item.value()["ports"];
    if (ports.is_array()) {
      parse_ports(ports.get<std::vector<std::string>>(), svc.ports);
    }

    auto sec_opts = item.value()["security_opt"];
    if (sec_opts.is_array()) {
      svc.security_opts = sec_opts.get<std::vector<std::string>>();
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
