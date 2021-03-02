#pragma once

#include <boost/filesystem.hpp>
#include <string>

#include "project.h"

void oci_createRuntime(const std::string &app_name, const std::string &svc);
void oci_poststop(const std::string &app_name, const std::string &svc);

void ocispec_create(const std::string &app_name,
                    const boost::filesystem::path &volumes_path,
                    const Service &svc, const std::vector<Volume> &volumes,
                    const boost::filesystem::path &spec,
                    const boost::filesystem::path &out,
                    const boost::filesystem::path &rootfs,
                    const boost::filesystem::path &etc_hosts);
