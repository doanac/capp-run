#pragma once

#include <string>

void oci_createRuntime(const std::string &app_name);
void oci_poststop(const std::string &app_name);
