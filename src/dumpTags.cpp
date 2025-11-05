#include <chrono>
#include <filesystem>

#include "fmt/chrono.h"
#include "fmt/format.h"
#include "fmt/os.h"
#include "fmt/ranges.h"

#include "dcmtk/dcmdata/dcdatset.h"
#include "dcmtk/dcmdata/dcdeftag.h"
#include "dcmtk/dcmdata/dcfilefo.h"
#include "dcmtk/ofstd/ofcond.h"

#include "dumpTags.hpp"
#include "logger.hpp"

OFCondition gatherTags(const std::string &input_directory,
                       std::vector<Tag> &input_tags,
                       const std::string &output_filepath, E_Dump_Level level) {
  DcmFileFormat dcmff{};
  TagsMap tagsMap{};
  std::set<std::string> dumpedInstances{};

  OFCondition cond{EC_Normal};
  for (const auto &entry :
       std::filesystem::recursive_directory_iterator(input_directory)) {
    if (entry.is_directory() || entry.path().stem() == "DICOMDIR")
      continue;

    cond = dcmff.loadFile(entry.path().string());
    if (cond.bad()) {
      OFLOG_ERROR(logger,
                  "error loading file `" << entry.path().string() << "`");
      OFLOG_ERROR(logger, "reason: " << cond.text());
      continue;
    }

    DcmDataset *dataset = dcmff.getDataset();

    std::string instanceUID{}; // can be series or study instance uid value
    switch (level) {
    case E_Dump_Level::STUDY:
      cond = dataset->findAndGetOFString(DCM_StudyInstanceUID, instanceUID);
      break;

    case E_Dump_Level::SERIES:
      cond = dataset->findAndGetOFString(DCM_SeriesInstanceUID, instanceUID);
      break;
    }

    if (cond.bad()) {
      OFLOG_ERROR(logger, "error retrieve study/series instance uid from file");
      OFLOG_ERROR(logger, "file: `" << entry.path().string() << "`");
      OFLOG_ERROR(logger, "reason: " << cond.text());
    }

    if (instanceUID.empty()) {
      OFLOG_WARN(logger, "empty instance uid");
      OFLOG_WARN(logger, "file: `" << entry.path().string() << "`");
      continue;
    }

    // if (dumpedInstances.contains(instanceUID))
    //   continue;

    // skip if study/series instance uid exists in set
    const auto res = dumpedInstances.insert(instanceUID);
    if (!res.second)
      continue;

    // auto [it, key_exists] = tagsMap.try_emplace(instanceUID);
    // if (key_exists)
    //   continue;

    for (auto &[dcm_tag, value] : input_tags) {
      cond = dataset->findAndGetOFString(dcm_tag.getTagKey(), value);

      if (cond.bad()) {
        OFLOG_WARN(logger,
                   "failure getting tag `" << dcm_tag.getTagName() << "`");
        OFLOG_WARN(logger, "reason: " << cond.text());
        value = "n/a";
      }
    }
    // it->second = input_tags;
  }

  cond = writeTags(input_tags, output_filepath);

  return cond;
};

OFCondition writeTags(const std::vector<Tag> &retrieved_tags,
                      const std::string &filepath) {
  OFCondition cond{EC_Normal};

  const auto tt =
      std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
  const std::tm tm = *std::localtime(&tt);

  // std::filesystem::path outputPath{opt_outDirectory};
  const std::string dumpFilepath =
      fmt::format("{}-{:%Y-%m-%d-%H-%M-%S}.csv", filepath, tm);
  // outputPath /= dumpFilename;
  auto filestream = fmt::output_file(dumpFilepath);

  // random guess of initial size
  std::string header_row{};
  // FIXME: specify .reserve(size)
  header_row.reserve(retrieved_tags.size() * 50);

  for (const Tag &tag : retrieved_tags) {
    // copy due to .getTagName not marked as const
    DcmTag dcm_tag = tag.dcm_tag;
    header_row += std::string(dcm_tag.getTagName()) + ";";
    header_row.append("ff"); // TODO: use this instead of += or +
  }

  filestream.print("{}\n", header_row);

  return cond;
};