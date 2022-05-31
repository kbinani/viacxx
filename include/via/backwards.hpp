#pragma once

#include <functional>
#include <string>

namespace viacxx {

enum class Version : int8_t {
  Version1_18 = 12,
  Version1_17 = 11,
  Version1_16_2 = 10,
  Version1_16 = 9,
  Version1_15 = 8,
  Version1_14 = 7,
  Version1_13_2 = 6,
  Version1_13 = 5,
  Version1_12 = 4,
  Version1_11 = 3,
  Version1_10 = 2,
  Version1_9_4 = 1,
};

class Backwards {
  Backwards() = delete;

public:
  using Converter = std::function<std::string(std::string const &)>;

  static Converter ComposeConverter(Version from, Version to);
};

} // namespace viacxx
