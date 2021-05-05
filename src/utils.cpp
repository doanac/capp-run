#include "utils.h"

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
