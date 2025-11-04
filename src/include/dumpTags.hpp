#include <filesystem>
#include <set>
#include <variant>
#include <vector>

#include "dcmtk/ofstd/ofcond.h"

struct Tag {
  const std::string name{};
  const std::string value{};
};

OFCondition gatherTags();

OFCondition iteratePaths(/*const std::vector<std::filesystem::path> &paths*/);