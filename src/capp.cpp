#include <boost/filesystem.hpp>

#include "capp.h"
#include "context.h"
#include "project.h"

#ifndef DOCKER_ARCH
#error Missing DOCKER_ARCH
#endif

static void up(const Context &ctx, const Service &svc) {
  ctx.out() << "Starting " << svc.name << "\n";
  auto spec = boost::filesystem::current_path() / ".specs" / svc.name / DOCKER_ARCH;
  if (!boost::filesystem::is_regular_file(spec)) {
    throw std::runtime_error("Could not find oci spec file for service");
  }

  auto dst = ctx.var_run / svc.name / "config.json";
  boost::filesystem::create_directories(dst.parent_path());
  boost::filesystem::copy_file(spec, dst);
}

void capp_up(const std::string &app_name, const std::string &svc) {
  auto ctx = Context::Load(app_name);
  auto proj = ProjectDefinition::Load("docker-compose.yml");

  for (const auto &s : proj.services) {
    if (s.name == svc) {
      up(ctx, s);
      return;
    }
  }
  std::string msg = "No such service: ";
  throw std::runtime_error(msg + svc);
}
