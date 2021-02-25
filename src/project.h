#pragma once

#include <string>
#include <vector>

struct Network {
  Network(std::string name) : name(name) {}
  std::string name;
};

struct Volume {
  Volume(std::string name) : name(name) {}
  std::string name;
};

struct Port {
  std::string host_ip;
  uint32_t host_port;
  uint32_t target_port;
  std::string protocol;
};

struct Service {
  Service(std::string name) : name(name) {}
  std::string name;
  std::string image;
  std::string user;
  std::string network_mode;
  std::vector<std::string> networks;
  std::vector<Port> ports;
};

struct ProjectDefinition {
  Service get_service(const std::string &name);

  std::vector<Network> networks;
  std::vector<Volume> volumes;
  std::vector<Service> services;

  static ProjectDefinition Load(const std::string &path);
};
