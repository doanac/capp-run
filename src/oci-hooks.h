#pragma once

#include <boost/filesystem.hpp>
#include <string>

void oci_createRuntime(const std::string &app_name);
void oci_poststop(const std::string &app_name, const std::string &svc);

void ocispec_create(const std::string &app_name, const std::string &svc,
                    const boost::filesystem::path &spec,
                    const boost::filesystem::path &out,
                    const boost::filesystem::path &rootfs);
