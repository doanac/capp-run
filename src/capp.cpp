#include <algorithm>
#include <boost/filesystem.hpp>
#include <boost/process.hpp>
#include <sstream>
#include <sys/mount.h>
#include <unistd.h>

#include "capp.h"
#include "context.h"
#include "oci-hooks.h"
#include "project.h"
#include "utils.h"

#ifndef DOCKER_ARCH
#error Missing DOCKER_ARCH
#endif

static bool is_mounted(const std::string &path) {
  auto file = open_read("/proc/mounts");
  std::string line;
  while (getline(file, line)) {
    auto start = line.find(' ') + 1;
    auto end = line.find(' ', start + 1);
    auto mnt = line.substr(start, end - start);
    if (mnt == path) {
      return true;
    }
  }
  return false;
}

static boost::filesystem::path
overlay_mount(const Context &ctx, const boost::filesystem::path &imgdir,
              const boost::filesystem::path &base) {
  auto rootfs = base / "rootfs";
  boost::filesystem::create_directories(rootfs);
  auto upper = base / ".upper";
  boost::filesystem::create_directories(upper);
  auto work = base / ".work";
  boost::filesystem::create_directories(work);

  if (is_mounted(rootfs.string())) {
    ctx.out() << "Overlay is mounted, skipping re-mount\n";
    return rootfs;
  }

  std::string cmd = "mount -t overlay overlay -o lowerdir=";
  cmd += imgdir.string() + ",upperdir=" + upper.string() +
         ",workdir=" + work.string() + " " + rootfs.string();

  ctx.out() << "Mounting overlay\n";
  if (boost::process::system(cmd) != 0) {
    throw std::runtime_error("Unable to mount overlayfs");
  }
  return rootfs;
}

static void up(const Context &ctx, const Service &svc,
               const std::vector<Volume> &volumes) {
  ctx.out() << "Starting " << svc.name << "\n";
  auto spec =
      boost::filesystem::current_path() / ".specs" / svc.name / DOCKER_ARCH;
  if (!boost::filesystem::is_regular_file(spec)) {
    spec = boost::filesystem::current_path() / ".specs" / svc.name / "default";
    if (!boost::filesystem::is_regular_file(spec)) {
      throw std::runtime_error("Could not find oci spec file for service");
    }
  }

  auto hosts = ctx.var_run / "etc_hosts";
  std::ofstream outfile(hosts.string(), std::ios_base::app);
  outfile.close();

  auto imgdir = ctx.var_lib / "images" / svc.name;
  if (!boost::filesystem::is_directory(imgdir)) {
    throw std::runtime_error("Could not find image for service");
  }

  auto rootfs = overlay_mount(ctx, imgdir, ctx.var_lib / "mounts" / svc.name);

  auto dst = ctx.var_run / svc.name / "config.json";
  boost::filesystem::create_directories(dst.parent_path());
  auto sha1 = sha1sum(spec);
  ocispec_create(ctx.app, ctx.volumes(), svc, volumes, spec, dst, rootfs,
                 hosts);

  ctx.out() << "Execing: crun run -f " << dst << " " << ctx.app << "-"
            << svc.name << "\n";
  const char *crun =
      strdup(boost::process::search_path("crun").string().c_str());
  const char *name = strdup((ctx.app + "-" + svc.name).c_str());
  const char *config = strdup(dst.string().c_str());

  // Why fork/exec just to dump out the content as-is?
  // SystemD's journal uses a socket for the stdout/stderr file descriptor.
  // Docker uses a pipe and many containers, such as nginx, require their
  // /dev/std[out|err] to be a pipe and can fail in very hard to diagnose
  // ways since they'll not actually provide any output when they crash.
  std::string failure = "Unable to execute crun";
  // TODO if tty, this should be changed
  pid_t pid;
  int pipefd[2];
  if (pipe(pipefd) == -1) {
    goto cleanup;
  }
  pid = fork();
  if (pid == -1) {
    goto cleanup;
  } else if (pid == 0) {
    setenv("OCISPEC_SHA1", sha1.c_str(), 1);
    close(pipefd[0]);
    dup2(pipefd[1], STDERR_FILENO);
    dup2(pipefd[1], STDOUT_FILENO);
    if (execl(crun, crun, "run", "-f", config, name, NULL) == -1) {
      throw std::system_error(errno, std::generic_category(),
                              "Unable to execute crun");
    }
  }

  char buffer[BUFSIZ];
  ssize_t bytes_read;
  close(pipefd[1]);
  while (1) {
    bytes_read = read(pipefd[0], buffer, sizeof(buffer));
    if (bytes_read < 0 && errno == EINTR) {
      continue;
    } else if (bytes_read < 0) {
      failure = "Unable to read container output";
      goto cleanup;
    } else if (bytes_read == 0) {
      int status;
      if (waitpid(pid, &status, 0) == -1) {
        failure = "Unable to get exit code from crun";
        goto cleanup;
      }
      if (WIFEXITED(status)) {
        exit(WEXITSTATUS(status));
      }
      throw std::runtime_error("Unknown waitpid rc: " + std::to_string(status));
    }
    fwrite(buffer, bytes_read, 1, stderr);
  }

cleanup:
  umount(rootfs.c_str());
  throw std::system_error(errno, std::generic_category(), failure);
}

void capp_up(const std::string &app_name, const std::string &svc) {
  auto ctx = Context::Load(app_name);
  auto proj = ProjectDefinition::Load("docker-compose.json");

  for (const auto &v : proj.volumes) {
    auto p = ctx.volumes() / v.name;
    if (!boost::filesystem::exists(p)) {
      ctx.out() << "Creating volume: " << v.name << "\n";
      boost::filesystem::create_directories(p);
    }
  }

  up(ctx, proj.get_service(svc), proj.volumes);
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
  auto proj = ProjectDefinition::Load("docker-compose.json");

  if (svc.size() != 0) {
    pull(ctx, proj.get_service(svc));
  } else {
    for (const auto &svc : proj.services) {
      pull(ctx, svc);
    }
  }
}

static std::vector<std::string> _unit_deps(const std::string &unit) {
  boost::process::ipstream out;
  int rc = boost::process::system("systemctl list-dependencies --plain " + unit,
                                  boost::process::std_out > out);
  if (rc != 0) {
    throw std::runtime_error("Unable to find current units");
  }

  std::vector<std::string> units;
  std::string line;
  bool first_line = true;
  while (std::getline(out, line)) {
    if (!first_line) {
      boost::trim(line);
      if (line.find(unit) == 0) {
        units.push_back(line);
      }
    }
    first_line = false;
  }
  return units;
}

static void _remove_svc(const boost::filesystem::path &units_dir,
                        const std::string &unit) {
  boost::process::system("systemctl stop " + unit);
  boost::process::system("systemctl disable " + unit);
  if (unlink((units_dir / unit).c_str()) != 0) {
    throw std::system_error(errno, std::generic_category(),
                            "Unable to delete " + unit);
  }
}

static std::string _read_file(const boost::filesystem::path &f) {
  // this ignores a file not exists, this is okay based on the
  // copy_if_changed_logic
  std::ifstream t(f.string());
  std::stringstream buffer;
  buffer << t.rdbuf();
  return buffer.str();
}

static bool _copy_if_changed(const std::string &app_name,
                             const boost::filesystem::path &src,
                             const boost::filesystem::path &dst) {
  auto src_content = _read_file(src);
  auto dst_content = _read_file(dst);

  auto exe = boost::filesystem::read_symlink("/proc/self/exe");
  boost::replace_all(src_content, "{{app}}", app_name);
  boost::replace_all(src_content, "{{binary}}", exe.string());
  boost::replace_all(src_content, "{{appdir}}",
                     boost::filesystem::current_path().string());

  if (src_content != dst_content) {
    open_write(dst) << src_content;
    return true;
  }
  return false;
}

void capp_sync_systemd(const boost::filesystem::path &units_dir,
                       const std::string &app_name) {
  auto ctx = Context::Load(app_name);
  auto proj = ProjectDefinition::Load("docker-compose.json");

  bool changed = false;

  auto deps = _unit_deps("capp_" + app_name);
  std::string enables = "capp_" + app_name;

  for (const auto &svc : proj.services) {
    ctx.out() << "Syncing unit " << svc.name << "\n";
    std::string svc_name = "{{app}}_" + svc.name + ".service";
    auto unit = boost::filesystem::current_path() / ".systemd" / svc_name;
    if (!boost::filesystem::exists(unit)) {
      throw std::runtime_error(unit.string() + " does not exist");
    }
    std::string dst = "capp_" + app_name + "_" + svc.name + ".service";
    if (_copy_if_changed(app_name, unit, units_dir / dst)) {
      ctx.out() << " unit changed\n";
      changed = true;
    }
    deps.erase(std::remove(deps.begin(), deps.end(), dst), deps.end());
    enables += " " + dst;
  }
  ctx.out() << "Syncing unit\n";
  auto unit = boost::filesystem::current_path() / ".systemd/{{app}}.service";
  auto dst = units_dir / ("capp_" + app_name + ".service");
  if (_copy_if_changed(app_name, unit, dst)) {
    ctx.out() << " unit changed\n";
    changed = true;
  }

  for (const auto &unit : deps) {
    ctx.out() << "Service has been removed: " << unit << "\n";
    _remove_svc(units_dir, unit);
    changed = true;
  }

  if (changed) {
    ctx.out() << "Reloading systemd\n";
    auto rc = boost::process::system("systemctl daemon-reload");
    if (rc != 0) {
      throw std::runtime_error("Unable to re-configure systemd");
    }

    ctx.out() << "Enabling " << enables << "\n";
    rc = boost::process::system("systemctl enable " + enables);
    if (rc != 0) {
      throw std::runtime_error("Unable to re-configure systemd");
    }
  }
}