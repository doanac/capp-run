#pragma once

#include <string>

#include "context.h"
#include "project.h"

void network_render(const Context &ctx, const std::string &name);
void network_join(const Context &ctx, const Service &svc, int pid);
bool network_destroy(const Context &ctx, const Service &svc);
