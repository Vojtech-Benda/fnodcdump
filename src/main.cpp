#include <filesystem>
#include <set>
#include <string>
#include <vector>

#include "fmt/format.h"

#include "dcmtk/dcmdata/cmdlnarg.h"
#include "dcmtk/dcmdata/dcdeftag.h"
#include "dcmtk/dcmdata/dcdicent.h"
#include "dcmtk/dcmdata/dcdict.h"
#include "dcmtk/dcmdata/dcuid.h"
#include "dcmtk/ofstd/ofconapp.h"

#include "include/dumpTags.hpp"
#include "include/logger.hpp"

static DcmTag parseTagKey(OFConsoleApplication &app,
                          const std::string &input_tag);

constexpr auto FNO_CONSOLE_APPLICATION{"fnodcdump"};

OFLogger logger = OFLog::getLogger(FNO_CONSOLE_APPLICATION);

int main(int argc, char *argv[]) {
  constexpr std::string APP_VERSION{"0.1.0"};
  constexpr std::string RELEASE_DATE{"2025-06-10"};
  const std::string rcsid = fmt::format(
      "${}: ver. {} rel. {}\n$dcmtk: ver. {} rel. {}", FNO_CONSOLE_APPLICATION,
      APP_VERSION, RELEASE_DATE, OFFIS_DCMTK_VERSION, OFFIS_DCMTK_RELEASEDATE);

  OFConsoleApplication app(FNO_CONSOLE_APPLICATION,
                           "DICOM tag file dumping tool", rcsid.c_str());
  OFCommandLine cmd{};

  std::string opt_inDirectory{};
  std::string opt_outDirectory{"./"};
  std::string opt_dumpFilepath{"./dumped_tags.csv"};
  std::vector<Tag> opt_inputTags{{DcmTag{DCM_PatientID}}};
  bool opt_dumpSecondarySeries{false};
  E_Dump_Level opt_dumpLevel{E_Dump_Level::STUDY};

  constexpr int LONGCOL{20};
  constexpr int SHORTCOL{4};
  cmd.setParamColumn(LONGCOL + SHORTCOL + 4);
  cmd.addParam(
      "in-directory",
      "input directory containing DICOM files or directories with DICOM files");

  cmd.setOptionColumns(LONGCOL, SHORTCOL);
  cmd.addGroup("general options:", LONGCOL, SHORTCOL + 2);
  cmd.addOption("--help", "-h", "print this help text and exit",
                OFCommandLine::AF_Exclusive);
  cmd.addOption("--version", "print version information and exit",
                OFCommandLine::AF_Exclusive);

  OFLog::addOptions(cmd);

  cmd.addOption("--series-level", "-series",
                "dump tags at series level (default at study level)");
  cmd.addGroup("Tag options:");
  cmd.addOption("--tag", "-t", 1,
                "tag: gggg,eeee=\"string\" or name=\"string\"",
                "DICOM tag key");
  cmd.addOption(
      "--dump-secondary-series", "-dss",
      "include tag dump from secondary/report series (default: false)");

  cmd.addGroup("output options:");
  // cmd.addOption("--out-directory", "-od", 1,
  //               "directory: string (default: `./` (current directory))",
  //               "write output dump file to output directory");
  cmd.addOption("--filepath", "-fp", 1,
                "filepath: string (default: \"./dumped_tags.csv\")",
                "path of the dump file excluding extension");

  prepareCmdLineArgs(argc, argv, FNO_CONSOLE_APPLICATION);
  if (app.parseCommandLine(cmd, argc, argv)) {
    if (cmd.hasExclusiveOption()) {
      if (cmd.findOption("--version")) {
        app.printHeader(OFTrue);
        return 0;
      }
    }

    cmd.getParam(1, opt_inDirectory);

    OFLog::configureFromCommandLine(cmd, app);

    if (cmd.findOption("--series-level")) {
      opt_dumpLevel = E_Dump_Level::SERIES;
      opt_inputTags.emplace_back(DcmTag{DCM_SeriesInstanceUID});
      OFLOG_INFO(logger, "writing tags at series level");
    } else {
      opt_inputTags.emplace_back(DcmTag{DCM_StudyInstanceUID});
      OFLOG_INFO(logger, "writing tags at study level");
    }

    if (cmd.findOption("--tag", 0, OFCommandLine::FOM_FirstFromLeft)) {
      std::string tagString{};
      do {
        app.checkValue(cmd.getValue(tagString));

        if (tagString == "PatientID" || tagString == "StudyInstanceUID" ||
            tagString == "SeriesInstanceUID") {
          OFLOG_WARN(logger, "not adding tag `" << tagString
                                                << "` included by default");
          continue;
        }

        DcmTag tag = parseTagKey(app, tagString);
        if (tag.getGroup() == 0xffff || tag.getElement() == 0xffff)
          continue;

        opt_inputTags.emplace_back(tag);
      } while (cmd.findOption("--tag", 0, OFCommandLine::FOM_NextFromLeft));
    }

    if (cmd.findOption("--dump-secondary-series")) {
      opt_dumpSecondarySeries = true;
    }

    // if (cmd.findOption("--out-directory")) {
    //   app.checkValue(cmd.getValue(opt_outDirectory));
    // }

    if (cmd.findOption("--filepath")) {
      app.checkValue(cmd.getValue(opt_dumpFilepath));
    }
  }

  if (std::filesystem::exists(opt_inDirectory)) {
    if (std::filesystem::is_regular_file(opt_inDirectory)) {
      OFLOG_ERROR(logger,
                  fmt::format("input directory path `{}` is not directory",
                              opt_inDirectory));
      return EXITCODE_COMMANDLINE_SYNTAX_ERROR;
    }

    if (std::filesystem::is_empty(opt_inDirectory)) {
      OFLOG_ERROR(logger,
                  fmt::format("input directory path `{}` has no DICOM files",
                              opt_inDirectory));
      return EXITCODE_COMMANDLINE_SYNTAX_ERROR;
    }
  } else {
    OFLOG_ERROR(logger, fmt::format("input directory path `{}` does not exist",
                                    opt_inDirectory));
    return EXITCODE_COMMANDLINE_SYNTAX_ERROR;
  }

  if (opt_inputTags.size() == 2)
    OFLOG_WARN(logger, "No additional tags to given, writing only PatientID "
                       "and Study/SeriesInstanceUID");

  gatherTags(opt_inDirectory, opt_inputTags, opt_dumpFilepath, opt_dumpLevel);

  return 0;
}

DcmTag parseTagKey(OFConsoleApplication &app, const std::string &input_tag) {
  unsigned short group{0xffff};   // default unknown tag
  unsigned short element{0xffff}; // default unknown tag

  const std::size_t comma = input_tag.find(',');

  std::string errorMsg{};
  if (comma != std::string::npos) {
    // parse input tag in hex format: 0010,0020
    group = std::stoul(input_tag.substr(0, comma), nullptr, 16);
    element = std::stoul(input_tag.substr(comma + 1), nullptr, 16);
  } else {
    // parse input tag in name/dict format: PatientID
    const DcmDataDictionary &globalDataDict = dcmDataDict.rdlock();
    const DcmDictEntry *dicentry = globalDataDict.findEntry(input_tag.c_str());
    dcmDataDict.rdunlock();

    if (dicentry != nullptr) {
      group = dicentry->getGroup();
      element = dicentry->getElement();
    } else {
      errorMsg = "bad key format or dictionary name not found in dictionary: " +
                 input_tag;
      app.printError(errorMsg.c_str());
      errorMsg.clear();
    }
  }

  DcmTag tag{group, element};

  if (tag.error() != EC_Normal) {
    errorMsg = fmt::format("unknown tag key: ({:04x},{:04x}) for input tag: {}",
                           group, element, input_tag);
    app.printError(errorMsg.c_str());
    errorMsg.clear();
  }

  return tag;
}
