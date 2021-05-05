#pragma once

#include <boost/filesystem.hpp>
#include <fstream>

std::ofstream open_write(const boost::filesystem::path &p);