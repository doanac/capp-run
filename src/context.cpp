#include "context.h"

#include <arpa/inet.h>
#include <ifaddrs.h>
#include <sstream>

#include "utils.h"

std::map<std::string, std::string> Context::network_interfaces() const {
  std::map<std::string, std::string> found;

  struct ifaddrs *ifaddr;
  if (getifaddrs(&ifaddr) == -1) {
    throw std::system_error(errno, std::generic_category(),
                            "Unable to list IP addresses");
  }

  for (struct ifaddrs *ifa = ifaddr; ifa != NULL; ifa = ifa->ifa_next) {
    if (ifa->ifa_addr != NULL) {
      int fam = ifa->ifa_addr->sa_family;
      if (fam == AF_INET || fam == AF_INET6) {
        struct sockaddr_in *sa = (struct sockaddr_in *)ifa->ifa_addr;
        char buf[INET6_ADDRSTRLEN];
        if (inet_ntop(fam, &sa->sin_addr, buf, sizeof(buf)) == NULL) {
          std::string msg = "Unable to parse ";
          throw std::runtime_error(msg + ifa->ifa_name);
        }
        found.emplace(ifa->ifa_name, buf);
      }
    }
  }
  freeifaddrs(ifaddr);
  return found;
}

resolv_conf Context::host_dns() const {
  std::string line;
  std::string word;
  resolv_conf conf;
  auto inf = open_read("/etc/resolv.conf");
  while (std::getline(inf, line)) {
    std::stringstream linestream(line);
    linestream >> word;
    if (word == "search") {
      while (linestream >> word) {
        conf.search.emplace_back(word);
      }
    } else if (word == "nameserver") {
      linestream >> word;
      conf.nameservers.emplace_back(word);
    }
  }
  return conf;
}
