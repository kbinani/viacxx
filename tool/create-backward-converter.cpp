#include <stdio.h>

#include <filesystem>
#include <fstream>
#include <iostream>
#include <set>
#include <sstream>
#include <vector>

#include <nlohmann/json.hpp>

namespace {

static std::vector<std::string> Split(std::string const &sentence, char delimiter) {
  std::istringstream input(sentence);
  std::vector<std::string> tokens;
  for (std::string line; std::getline(input, line, delimiter);) {
    tokens.push_back(line);
  }
  return tokens;
}

inline std::string LTrim(std::string const &s, std::string const &left) {
  if (left.empty()) {
    return s;
  }
  std::string ret = s;
  while (ret.starts_with(left)) {
    ret = ret.substr(left.size());
  }
  return ret;
}

inline std::string RTrim(std::string const &s, std::string const &right) {
  if (right.empty()) {
    return s;
  }
  std::string ret = s;
  while (ret.ends_with(right)) {
    ret = ret.substr(0, ret.size() - right.size());
  }
  return ret;
}

inline std::string Trim(std::string const &left, std::string const &s, std::string const &right) { return RTrim(LTrim(s, left), right); }

inline std::string Replace(std::string const &target, std::string const &search, std::string const &replace) {
  using namespace std;
  if (search.empty()) {
    return target;
  }
  if (search == replace) {
    return target;
  }
  size_t offset = 0;
  string ret = target;
  while (true) {
    auto found = ret.find(search, offset);
    if (found == string::npos) {
      break;
    }
    ret = ret.substr(0, found) + replace + ret.substr(found + search.size());
    offset = found + replace.size();
  }
  return ret;
}

static std::string Indent(std::string const &input, std::string const &indentString) {
  auto lines = Split(input, '\n');
  std::ostringstream ss;
  for (auto const &line : lines) {
    ss << indentString << line << std::endl;
  }
  return RTrim(RTrim(ss.str(), " "), "\n");
}

static std::string Trim(std::string const &v) {
  return Trim("\n", Trim(" ", v, " "), "\n");
}

std::string headerFormat = R"(
#pragma once

#include <functional>
#include <string>

namespace viacxx {

enum class Version : int8_t {
@{enum_definition}
};

class Backwards {
  Backwards() = delete;

public:
  using Converter = std::function<std::string(std::string const &)>;

  static Converter ComposeConverter(Version from, Version to);
};

} // namespace viacxx
)";

std::string classFormat = R"(
#include <via/backwards.hpp>

namespace viacxx {
namespace {

struct CompositeConverter {
  std::vector<Backwards::Converter> fConverters;

  std::string operator()(std::string const &input) const {
    auto output = input;
    for (auto const &converter : fConverters) {
      output = converter(output);
    }
    return output;
  }
};

@{convert_function_definitions}

Backwards::Converter SelectConverter(Version from) {
  static std::unordered_map<Version, Backwards::Converter> const sConverters = {
@{converters_table}
  };
  if (auto found = sConverters.find(from); found != sConverters.end()) {
    return found->second;
  } else {
    return nullptr;
  }
}

std::string Identity(std::string const &input) {
  return input;
}

} // namespace

Backwards::Converter Backwards::ComposeConverter(Version from, Version to) {
  int8_t vFrom = static_cast<int8_t>(from);
  int8_t vTo = static_cast<int8_t>(to);

  if (vFrom < vTo) {
    return nullptr;
  }
  if (vFrom == vTo) {
    return Identity;
  }
  if (vFrom == vTo + 1) {
    return SelectConverter(from);
  }

  CompositeConverter composite;
  for (int8_t v = static_cast<int8_t>(from); v > static_cast<int8_t>(to); v--) {
    auto converter = SelectConverter(static_cast<Version>(v));
    if (!converter) {
      return nullptr;
    }
    composite.fConverters.push_back(converter);
  }
  return composite;
}

} // namespace viacxx
)";

std::string emptyTableFunctionFormat = R"(
std::string Convert@{version_pair}(std::string const &input) {
  return input;
}
)";
} // namespace

class Version {
public:
  explicit Version(std::string const &v) {
    auto tokens = Split(v, '.');
    if (tokens.size() < 2) {
      throw std::runtime_error("invalid version format: " + v);
    }
    fMajor = std::stoi(tokens[0]);
    fMinor = std::stoi(tokens[1]);
    if (tokens.size() > 2) {
      fBugfix = std::stoi(tokens[2]);
    }
    if (tokens.size() > 3) {
      throw std::runtime_error("invalid version format: " + v);
    }
  }

  std::string toString(std::string separator = ".") const {
    std::string ret = std::to_string(fMajor) + separator + std::to_string(fMinor);
    if (fBugfix) {
      ret += separator + std::to_string(*fBugfix);
    }
    return ret;
  }

  bool equals(Version const &other) const {
    if (fMajor != other.fMajor) {
      return false;
    }
    if (fMinor != other.fMinor) {
      return false;
    }
    if (!fBugfix && !other.fBugfix) {
      return true;
    }
    if (fBugfix && other.fBugfix) {
      return *fBugfix == *other.fBugfix;
    } else {
      return false;
    }
  }

  bool less(Version const &b) const {
    if (fMajor == b.fMajor) {
      if (fMinor == b.fMinor) {
        if (fBugfix && b.fBugfix) {
          return *fBugfix > *b.fBugfix;
        } else if (fBugfix) {
          return true;
        } else {
          return false;
        }
      } else {
        return fMinor > b.fMinor;
      }
    } else {
      return fMajor > b.fMajor;
    }
  }

  int fMajor;
  int fMinor;
  std::optional<int> fBugfix;
};

int main(int argc, char *argv[]) {
  using namespace std;
  using namespace nlohmann;
  namespace fs = std::filesystem;

  if (argc < 2) {
    return -1;
  }

  fs::path directory(argv[2]);
  if (!fs::is_directory(directory)) {
    return -1;
  }

  vector<pair<Version, Version>> versionPairs;
  ostringstream convertFunctionDefinitions;
  ostringstream convertersTable;

  for (auto item : fs::directory_iterator(directory)) {
    if (!item.is_regular_file()) {
      continue;
    }
    auto p = item.path();
    auto filename = p.filename();
    if (filename.extension().string() != ".json" || !filename.string().starts_with("mapping-")) {
      continue;
    }
    auto pair = filename.replace_extension().string().substr(8);
    auto indexTo = pair.find_first_of("to");
    if (indexTo == string::npos) {
      throw runtime_error("invalid version pair format: " + pair);
    }
    auto toVersionString = pair.substr(0, indexTo);
    auto fromVersionString = pair.substr(indexTo + 2);
    Version to(toVersionString);
    Version from(fromVersionString);
    versionPairs.push_back(make_pair(from, to)); // filename: 1.18to1.19, make_pair(1.19, 1.18)

    string versionPair = from.toString("_") + "To" + to.toString("_");

    convertersTable << "{Version::Version" << from.toString("_") << ", Convert" << versionPair << "}," << endl;

    ifstream ifs(p.string());
    auto obj = json::parse(ifs);
    auto found = obj.find("blockstates");
    if (found == obj.end()) {
      convertFunctionDefinitions << Replace(emptyTableFunctionFormat, "@{version_pair}", versionPair);
    } else {
      json &blockstates = found.value();
      ostringstream dedicated;
      ostringstream rename;
      for (json::iterator it = blockstates.begin(); it != blockstates.end(); it++) {
        string key = it.key();
        if (key.ends_with("[")) {
          cout << "warning: eratta in key?: key=" << key << endl;
          continue;
        }
        string value = it.value().get<string>();
        if (value.ends_with("[")) {
          rename << "s[\"" << key << "\"] = \"" << value.substr(0, value.size() - 1) << "\";" << endl;
        } else {
          dedicated << "s[\"" << key << "\"] = \"" << value << "\";" << endl;
        }
      }
      std::string dedicatedConverterMapFormat = R"(
std::unordered_map<std::string, std::string> const *CreateDedicatedConvertMap@{version_pair}() {
  using namespace std;
  auto t = new unordered_map<string, string>();
  auto &s = *t;
@{dedicated_maps}
  return t;
}
)";

      string renameConverterMapFormat = R"(
std::unordered_map<std::string, std::string> const *CreateRenameConvertMap@{version_pair}() {
  using namespace std;
  auto t = new unordered_map<string, string>();
  auto &s = *t;
@{rename_maps}
  return t;
}
)";

      ostringstream def;
      auto dedicatedSrc = dedicated.str();
      if (!dedicatedSrc.empty()) {
        def << Replace(dedicatedConverterMapFormat, "@{dedicated_maps}", Indent(dedicatedSrc, "  "));
      }
      auto renameSrc = rename.str();
      if (!renameSrc.empty()) {
        def << Replace(renameConverterMapFormat, "@{rename_maps}", Indent(renameSrc, "  "));
      }

      def << R"(
std::string Convert@{version_pair}(std::string const &input) {
  using namespace std;
)";
      if (!dedicatedSrc.empty()) {
        def << R"(
  static unique_ptr<unordered_map<string, string> const> const sDedicatedConvertMap(CreateDedicatedConvertMap@{version_pair}());
  auto const &dedicated = *sDedicatedConvertMap;

  if (auto found = dedicated.find(input); found != dedicated.end()) {
    return found->second;
  }
)";
      }
      if (!renameSrc.empty()) {
        def << R"(
  static unique_ptr<unordered_map<string, string> const> const sRenameConvertMap(CreateRenameConvertMap@{version_pair}());
  auto const &rename = *sRenameConvertMap;

  string name = input;
  string props;
  if (auto found = input.find('['); found != string::npos) {
    if (input.ends_with(']')) {
      name = input.substr(0, found);
      props = input.substr(found + 1);
    } else {
      // invalid data string format
      return input;
    }
  }

  if (auto found = rename.find(name); found != rename.end()) {
    if (props.empty()) {
      return found->second;
    } else {
      return found->second + "[" + props + "]";
    }
  }
)";
      }

      def << endl;
      def << "  return input;" << endl;
      def << "}" << endl;

      string src = Replace(def.str(), "@{version_pair}", versionPair);
      convertFunctionDefinitions << src;
    }
  }

  sort(versionPairs.begin(), versionPairs.end(), [](auto const &pairA, auto const &pairB) {
    return pairA.first.less(pairB.first);
  });

  for (int i = 0; i < versionPairs.size() - 1; i++) {
    auto from = versionPairs[i].second;
    auto to = versionPairs[i + 1].first;
    if (!from.equals(to)) {
      throw runtime_error("version numbers are not consecutive");
    }
  }

  int enumValue = versionPairs.size() + 1;
  ostringstream enumDefinition;
  for (auto const &v : versionPairs) {
    enumDefinition << "Version" << v.first.toString("_") << " = " << enumValue << "," << endl;
    enumValue--;
  }
  enumDefinition << "Version" << versionPairs.back().second.toString("_") << " = " << enumValue << "," << endl;

  ofstream out((fs::path(argv[1]) / "src" / "backwards.cxx").string().c_str());
  string src = Replace(classFormat, "@{convert_function_definitions}", Indent(Trim(convertFunctionDefinitions.str()), ""));
  src = Replace(src, "@{converters_table}", Indent(Trim(convertersTable.str()), "      "));
  out << Trim(src) << endl;

  ofstream header((fs::path(argv[1]) / "include" / "via" / "backwards.hpp").string().c_str());
  string headerSrc = Replace(headerFormat, "@{enum_definition}", Indent(Trim(enumDefinition.str()), "  "));
  header << Trim(headerSrc) << endl;

  return 0;
}