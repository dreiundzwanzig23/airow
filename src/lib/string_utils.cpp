#include "project/string_utils.hpp"
#include <cctype>

namespace project {

/**
 * @design D-900 — to_upper_ascii
 * @title ASCII uppercase converter
 * @satisfies [A-900]
 * @notes Converts each character to its ASCII uppercase equivalent.
 */
std::string to_upper_ascii(std::string s) {
  for (auto &ch : s) {
    ch = static_cast<char>(std::toupper(static_cast<unsigned char>(ch)));
  }
  return s;
}

} // namespace project
