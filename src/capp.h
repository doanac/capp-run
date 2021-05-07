#pragma once

#include <string>

void capp_pull(const std::string &app_name, const std::string &svc);
void capp_up(const std::string &app_name, const std::string &svc);
void capp_sync_systemd(const boost::filesystem::path &units_dir,
                       const std::string &app_name);
void capp_status(const std::string &app_name);