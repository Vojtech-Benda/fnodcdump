#include <chrono>
#include <filesystem>

#include "fmt/chrono.h"
#include "fmt/format.h"
#include "fmt/os.h"
#include "fmt/ranges.h"

#include "dcmtk/dcmdata/dcdatset.h"
#include "dcmtk/dcmdata/dcdeftag.h"
#include "dcmtk/dcmdata/dcfilefo.h"

#include "dumpTags.hpp"
#include "logger.hpp"

OFCondition gatherTags(const std::string &input_directory,
                       std::vector<Tag> &input_tags,
                       const std::string &output_filepath,
                       E_Dump_Level dump_level) {
  DcmFileFormat dcmff{};
  TagsMap tagsMap{};

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
    switch (dump_level) {
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

    auto [it, elem_inserted] = tagsMap.try_emplace(instanceUID);
    if (!elem_inserted) {
      OFLOG_INFO(logger, "uid exists: " << it->first);
      continue;
    }

    for (Tag &tag : input_tags) {
      cond = dataset->findAndGetOFString(tag.key, tag.value);

      if (cond.bad()) {
        OFLOG_WARN(logger, "failure getting tag `" << tag.name << "`");
        OFLOG_WARN(logger, "reason: " << cond.text());
        tag.value = "e/r";
      }

      if (tag.value.empty())
        tag.value = "n/a";
    }

    OFLOG_INFO(logger, "adding tags for " << instanceUID);
    it->second = input_tags;
  }

  cond = writeTags(tagsMap, output_filepath);

  if (cond.bad()) {
    OFLOG_ERROR(logger, "error writing tags");
    OFLOG_ERROR(logger, "reason: " << cond.text());
  }

  return cond;
};

OFCondition writeTags(const TagsMap &tags_map, const std::string &filepath) {
  OFCondition cond{EC_Normal};

  const auto tt =
      std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
  const std::tm tm = *std::localtime(&tt);

  const std::string dumpFilepath =
      fmt::format("{}-{:%Y-%m-%d-%H-%M-%S}.csv", filepath, tm);
  auto filestream = fmt::output_file(dumpFilepath);

  const auto temp_tags = tags_map.begin()->second;

  /*
  median character count of non-retired DICOM tags = 22 -> round up to 30,
  maybe small performance improvement??
  */
  std::string header_row{};
  header_row.reserve(temp_tags.size() * 30);

  // construct header row as: TagName1;TagName2;TagName3;...
  auto iter = temp_tags.begin();
  header_row.append(iter->name);
  ++iter;

  for (; iter != temp_tags.end(); ++iter) {
    header_row.push_back(';');
    header_row.append(iter->name);
  }

  filestream.print("{}\n", header_row);

  for (const auto &[key, tags_vec] : tags_map) {
    std::string row{};
    row.reserve(200); // guessed size, can be 0

    auto iter = tags_vec.begin();
    row.append(iter->value);
    ++iter;

    for (; iter != tags_vec.end(); ++iter) {
      row.push_back(';');
      row.append(iter->value);
    }

    filestream.print("{}\n", row);
    OFLOG_INFO(logger, "written row for `" << key << "`");
  }

  filestream.close();
  return cond;
};