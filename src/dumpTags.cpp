#include "dumpTags.hpp"
#include <dcmtk/oflog/oflog.h>
#include <dcmtk/ofstd/ofcond.h>
#include <filesystem>

#include "logger.hpp"

OFCondition gatherTags() { return {EC_Normal}; };

OFCondition iteratePaths(/*const std::vector<std::filesystem::path> &paths*/) {
  OFCondition cond{EC_Normal};
  OFLOG_WARN(logger, "some warn message");
  OFLOG_ERROR(logger, "some error message");
  //   for (const auto &path : paths) {
  //     if (std::filesystem::is_regular_file(path)) {
  //     } else {
  //       OFLOG_WARN(logger, msg)
  //       continue;
  //     }
  //   }
  return cond;
};