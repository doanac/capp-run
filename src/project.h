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

struct Service {
  Service(std::string name) : name(name) {}
  std::string name;
  std::string image;
  std::string user;
  std::vector<std::string> networks;
};

struct ProjectDefinition {
  std::vector<Network> networks;
  std::vector<Volume> volumes;
  std::vector<Service> services;

  static ProjectDefinition Load(const std::string &path);
};
