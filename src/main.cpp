#include <boost/filesystem.hpp>
#include <iostream>
#include <unistd.h>

#include "CLI11.hpp"

#include "capp.h"
#include "oci-hooks.h"

int main(int argc, char **argv) {
  CLI::App app{"capp-run"};

  std::string app_name(boost::filesystem::current_path().filename().string());
  app.add_option("-n,--name", app_name, "Compose application name", true);

  std::string app_dir("./");
  app.add_option("-d,--app-dir", app_dir, "Compose application directory",
                 true);

  std::string svc;
  auto &up = *app.add_subcommand("up", "Start a compose service");
  up.add_option("service", svc, "Compose service")->required();
  auto &pull = *app.add_subcommand("pull", "Pull container image for service");
  pull.add_option("service", svc, "Compose service")->required();
  auto &create = *app.add_subcommand("createRuntime", "OCI createRuntime hook");
  auto &teardown = *app.add_subcommand("poststop", "OCI poststop hook");
  teardown.add_option("service", svc, "Compose service")->required();

  app.require_subcommand(1);
  CLI11_PARSE(app, argc, argv);

  if (chdir(app_dir.c_str()) != 0) {
    perror("Unable to change into app directory");
    return EXIT_FAILURE;
  }

  try {
    if (up) {
      capp_up(app_name, svc);
    } else if (pull) {
      capp_pull(app_name, svc);
    } else if (create) {
      oci_createRuntime(app_name);
    } else if (teardown) {
      oci_poststop(app_name, svc);
    }
  } catch (const std::exception &ex) {
    std::cerr << ex.what() << "\n";
    return EXIT_FAILURE;
  }

  return 0;
}
