#include "utils.h"

#include <boost/uuid/detail/sha1.hpp>

std::ofstream open_write(const boost::filesystem::path &p) {
  std::ofstream f(p.string());
  if (!f.is_open()) {
    throw std::system_error(errno, std::generic_category(),
                            "Unable to open " + p.string() + " for writing");
  }
  return f;
}

std::ifstream open_read(const boost::filesystem::path &p) {
  std::ifstream f(p.string());
  if (!f.is_open()) {
    throw std::system_error(errno, std::generic_category(),
                            "Unable to open " + p.string() + " for reading");
  }
  return f;
}

std::string sha1sum(const boost::filesystem::path &p) {
  auto f = open_read(p);
  f.seekg(0, std::ios::end);
  auto size = f.tellg();
  std::string buf(size, '\0');
  f.seekg(0);
  if (!f.read(&buf[0], size)) {
    throw std::system_error(errno, std::generic_category(),
                            "Unable to read content of " + p.string());
  }
  boost::uuids::detail::sha1 sha1;
  sha1.process_bytes(buf.data(), buf.size());
  unsigned hash[5] = {0};
  sha1.get_digest(hash);

  char dgst[41] = {0};
  for (int i = 0; i < 5; i++) {
    sprintf(dgst + (i << 3), "%08x", hash[i]);
  }
  return std::string(dgst);
}