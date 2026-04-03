#include <cstddef>
#include <iostream>
#include <string_view>
#include <vector>

#include "project/orchestrator/cli.hpp"

int main(int argc, char **argv) { // trace: trivial
  std::vector<std::string_view> args;
  args.reserve(argc > 0 ? static_cast<std::size_t>(argc - 1) : 0U);
  for (int index = 1; index < argc; ++index) {
    args.emplace_back(argv[index]);
  }

  return project::run_headless_cli(args, std::cout, std::cerr);
}
