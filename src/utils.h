#pragma once

#include <boost/filesystem.hpp>
#include <fstream>
#include <ostree.h>

std::ifstream open_read(const boost::filesystem::path &p);
std::ofstream open_write(const boost::filesystem::path &p);
std::string sha1sum(const boost::filesystem::path &p);

class OSTreeRepo {
public:
  OSTreeRepo(const std::string &url, boost::filesystem::path repo_dir);
  ~OSTreeRepo();

public:
  void checkout(const std::string &commit_hash,
                const boost::filesystem::path &dst_dir) const;
  void pull(const std::string &commit_hash) const;

private:
  const boost::filesystem::path path_;
  OstreeRepo *repo_;
};