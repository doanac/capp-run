#include <boost/process.hpp>
#include <iostream>
#include <string>
#include <vector>

namespace bp = ::boost::process;

#include <boost/filesystem.hpp>
#include <iostream>
#include <poll.h>
#include <unistd.h>

#include "CLI11.hpp"

#include "capp.h"
#include "context.h"
#include "oci-hooks.h"

static void runall(const std::string &app_name);

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
  create.add_option("service", svc, "Compose service")->required();
  auto &teardown = *app.add_subcommand("poststop", "OCI poststop hook");
  teardown.add_option("service", svc, "Compose service")->required();
  auto &upall = *app.add_subcommand("upall", "Start all services");

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
      oci_createRuntime(app_name, svc);
    } else if (teardown) {
      oci_poststop(app_name, svc);
    } else if (upall) {
      runall(app_name);
    }
  } catch (const std::exception &ex) {
    std::cerr << ex.what() << "\n";
    return EXIT_FAILURE;
  }

  return 0;
}

static void run(const std::string &capp_exe, const std::string &app_name,
                const std::string &svc_name, size_t prefix_width) {
  std::string prefix = svc_name;
  auto pad = prefix_width - svc_name.size();
  while (pad--) {
    prefix += " ";
  }
  prefix += " | ";

  std::string cmd = capp_exe + " -n " + app_name + " up " + svc_name;
  FILE *fp = popen(cmd.c_str(), "r");
  if (fp == NULL) {
    perror("Unable to start service");
    exit(1);
  }
  char buf[1024];
  while (fgets(buf, sizeof(buf), fp) != NULL) {
    std::cout << prefix << buf;
  }
  int status = pclose(fp);
  if (status == -1) {
    perror("Unable to close service");
  } else {
    std::cout << prefix;
    if (WIFEXITED(status)) {
      std::cout << "exited with rc=" << WEXITSTATUS(status) << "\n";
    } else if (WIFSIGNALED(status)) {
      std::cout << "killed with sig=" << WTERMSIG(status) << "\n";
    } else if (WIFSTOPPED(status)) {
      std::cout << "stopped with sig=" << WSTOPSIG(status) << "\n";
    } else {
      std::cout << "unexpectedly ended with " << status << "\n";
    }
  }
}

static void runall(const std::string &app_name) {
  auto ctx = Context::Load(app_name);
  auto proj = ProjectDefinition::Load("docker-compose.yml");
  auto exe = boost::filesystem::read_symlink("/proc/self/exe");

  size_t width = 0;
  for (const auto svc : proj.services) {
    if (svc.name.size() > width) {
      width = svc.name.size();
    }
  }

  std::vector<std::thread> threads;
  for (const auto svc : proj.services) {
    std::string cmd = exe.string() + " -n " + app_name + " up " + svc.name;
    std::thread t(run, exe.string(), app_name, svc.name, width);
    threads.push_back(std::move(t));
  }

  for (auto &t : threads) {
    t.join();
  }
}