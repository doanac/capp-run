#include <iostream>

#include "CLI11.hpp"

int main(int argc, char **argv) {
  CLI::App app{"capp-run"};

  CLI11_PARSE(app, argc, argv);

  std::cout << "hello\n";
  return 0;
}
