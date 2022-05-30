#pragma once

namespace ViaBackwards {

enum class Version : uint8_t {
  Version1_18 = 1,
  Version1_17 = 2,
  Version1_16_2 = 3,
  Version1_16 = 4,
  Version1_15 = 5,
  Version1_14 = 6,
  Version1_13_2 = 7,
  Version1_13 = 8,
  Version1_12 = 9,
  Version1_11 = 10,
  Version1_10 = 11,
  Version1_9_4 = 12,
};

class Backwards {
  Backwards() = delete;

public:
  using Converter = std::function<std::shared_ptr<mcfile::je::Block const>(std::shared_ptr<mcfile::je::Block const> const &)>;

  static Converter ComposeConverter(Version from, Version to);
};

} // namespace ViaBackwards
