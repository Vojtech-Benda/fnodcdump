#include <unordered_map>
#include <vector>

#include "fmt/format.h"

#include "dcmtk/dcmdata/dctagkey.h"
#include "dcmtk/ofstd/ofcond.h"

enum class E_Dump_Level { STUDY, SERIES };

struct Tag {
  DcmTagKey key{};
  std::string name{};
  std::string value{};
};

namespace fmt {
template <> class fmt::formatter<Tag> {

public:
  constexpr auto parse(format_parse_context &ctx) {
    auto i = ctx.begin(), end = ctx.end();

    if (i != end && *i != '}') {
      throw format_error("invalid format");
    }
    return i;
  }

  template <typename FmtContext>
  constexpr auto format(const Tag &_tag, FmtContext &ctx) const {
    return fmt::format_to(ctx.out(), "{}", _tag.name);
  }
};
} // namespace fmt

// key: Series/StudyInstanceUID, values: vector of (TagName, TagValue)
using TagsMap = std::unordered_map<std::string, std::vector<Tag>>;

OFCondition gatherTags(const std::string &input_directory,
                       std::vector<Tag> &input_tags,
                       const std::string &output_filepath, E_Dump_Level level);

OFCondition writeTags(const TagsMap &tags_map, const std::string &filepath);