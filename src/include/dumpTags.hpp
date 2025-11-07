#include <filesystem>
#include <set>
#include <unordered_map>
#include <variant>
#include <vector>

#include "dcmtk/dcmdata/dctag.h"
#include "dcmtk/ofstd/ofcond.h"

enum class E_Dump_Level { STUDY, SERIES };

struct Tag {
  DcmTag dcm_tag{};
  std::string value{};
};

// key: Series/StudyInstanceUID, values: vector of (TagName, TagValue)
using TagsMap = std::unordered_map<std::string, std::vector<Tag>>;

OFCondition gatherTags(const std::string &input_directory,
                       std::vector<Tag> &input_tags,
                       const std::string &output_filepath, E_Dump_Level level);

OFCondition writeTags(const TagsMap &tags_map, const std::string &filepath);