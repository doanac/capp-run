#pragma once

#include <boost/filesystem.hpp>
#include <fstream>

std::ifstream open_read(const boost::filesystem::path &p);
std::ofstream open_write(const boost::filesystem::path &p);