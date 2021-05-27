#pragma once

#include <boost/filesystem.hpp>
#include <iostream>
#include <map>
#include <string>

#include "utils.h"

struct resolv_conf {
  std::vector<std::string> nameservers;
  std::vector<std::string> search;
};

struct Context {
  std::string app;
  boost::filesystem::path var_run;
  boost::filesystem::path var_lib;
  std::unique_ptr<OSTreeRepo> ostree;

  std::ostream *out_;

  resolv_conf host_dns() const;
  std::map<std::string, std::string> network_interfaces() const;
  boost::filesystem::path volumes() const { return var_lib / "volumes"; }

  std::ostream &out() const { return *out_; }

  static Context Load(const std::string app) {
    boost::filesystem::path run("/var/run/capprun");
    boost::filesystem::path lib("/var/lib/capprun");
    const char *ptr = getenv("CAPPRUN_RUN");
    if (ptr != nullptr) {
      run = ptr;
    }
    run = run / app;
    ptr = getenv("CAPPRUN_LIB");
    if (ptr != nullptr) {
      lib = ptr;
    }

    std::string server = "http://localhost:8443";
    ptr = getenv("CAPP_OSTREE_SERVER");
    if (ptr != nullptr) {
      server = ptr;
    }
    return {.app = app,
            .var_run = run,
            .var_lib = lib / app,
            .ostree = std::make_unique<OSTreeRepo>(server, lib / "ostree"),
            .out_ = &std::cout};
  }
};
