#include <boost/filesystem.hpp>
#include <boost/process.hpp>

#include "capp.h"
#include "context.h"
#include "project.h"

#ifndef DOCKER_ARCH
#error Missing DOCKER_ARCH
#endif

static void up(const Context &ctx, const Service &svc) {
  ctx.out() << "Starting " << svc.name << "\n";
  auto spec =
      boost::filesystem::current_path() / ".specs" / svc.name / DOCKER_ARCH;
  if (!boost::filesystem::is_regular_file(spec)) {
    throw std::runtime_error("Could not find oci spec file for service");
  }

  auto dst = ctx.var_run / svc.name / "config.json";
  boost::filesystem::create_directories(dst.parent_path());
  boost::filesystem::copy_file(spec, dst);

  auto path = ctx.var_lib / "mounts" / svc.name;
  auto rootfs = path / "rootfs";
  boost::filesystem::create_directories(rootfs);
  auto upper = path / ".upper";
  boost::filesystem::create_directories(upper);
  auto work = path / ".work";
  boost::filesystem::create_directories(work);

  auto imgdir = ctx.var_lib / "images" / svc.name;
  if (!boost::filesystem::is_directory(imgdir)) {
    throw std::runtime_error("Could not find image for service");
  }

  std::string cmd = "mount -t overlay overlay -o lowerdir=";
  cmd += imgdir.string() + ",upperdir=" + upper.string() +
         ",workdir=" + work.string() + " " + rootfs.string();
  ctx.out() << "Mounting overlay\n";
  boost::process::system(cmd);

  // TODO add ourselve to hooks in config.json
  // TODO add path to root.path in config.json
  // exec crun ...
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

static void pull(const Context &ctx, const Service &svc) {
  ctx.out() << "Pulling " << svc.name << ": " << svc.image << "\n";
  std::string cmd = "docker pull ";
  boost::process::system(cmd + svc.image);
  ctx.out() << "Extracting\n";
  cmd = "docker create " + svc.image;
  boost::process::ipstream out;
  boost::process::system(cmd, boost::process::std_out > out);
  std::string id;
  out >> id;

  auto imgdir = ctx.var_lib / "images" / svc.name;
  boost::filesystem::create_directories(imgdir);

  boost::process::pipe intermediate;
  boost::process::child docker_export(boost::process::search_path("docker"),
                                      "export", id,
                                      boost::process::std_out > intermediate);
  boost::process::child extract(boost::process::search_path("tar"), "-C",
                                imgdir, "-xf", "-",
                                boost::process::std_in < intermediate);

  docker_export.wait();
  extract.wait();
}

void capp_pull(const std::string &app_name, const std::string &svc) {
  auto ctx = Context::Load(app_name);
  auto proj = ProjectDefinition::Load("docker-compose.yml");

  for (const auto &s : proj.services) {
    if (s.name == svc) {
      pull(ctx, s);
      return;
    }
  }
  std::string msg = "No such service: ";
  throw std::runtime_error(msg + svc);
}
