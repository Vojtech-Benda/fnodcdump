#include <string>
#include <vector>
#include <filesystem>
#include <set>
#include <ranges>
#include <chrono>

#include "fmt/format.h"
#include "fmt/ranges.h"
#include "fmt/os.h"
#include "fmt/chrono.h"

#include "dcmtk/config/osconfig.h"
#include "dcmtk/dcmdata/dctk.h"
#include "dcmtk/oflog/oflog.h"
#include "dcmtk/dcmdata/cmdlnarg.h"
#include "dcmtk/ofstd/ofconapp.h"

static DcmTag parseTagKey(OFConsoleApplication &app, const std::string &input_tag);

auto containsForbiddenSubstring = [](const OFString &str, const std::set<OFString> &words) {
     return std::ranges::any_of(words, [&str](const OFString &word) {
         return str.find(word) != OFString_npos;
     });
};

constexpr auto FNO_CONSOLE_APPLICATION{"fnodcdump"};
static OFLogger logger = OFLog::getLogger(fmt::format("fno.apps.{}", FNO_CONSOLE_APPLICATION).c_str());


int main(int argc, char *argv[]) {
    // constexpr auto    FNO_CONSOLE_APPLICATION{"fnodcdump"};
    constexpr auto    APP_VERSION{"0.1.0"};
    constexpr auto    RELEASE_DATE{"2025-06-10"};
    const std::string rcsid = fmt::format("${}: ver. {} rel. {}\n$dcmtk: ver. {} rel. {}",
                                          FNO_CONSOLE_APPLICATION,
                                          APP_VERSION,
                                          RELEASE_DATE,
                                          OFFIS_DCMTK_VERSION,
                                          OFFIS_DCMTK_RELEASEDATE);

    OFConsoleApplication app(FNO_CONSOLE_APPLICATION, "DICOM tag file dumping tool", rcsid.c_str());
    OFCommandLine        cmd;

    const char *opt_inDirectory{nullptr};
    // const char *opt_outDirectory{"./"};
    const char *opt_dumpFilepath{"./dumped_tags.csv"};
    std::vector<DcmTag> opt_inputTags{};

    constexpr int LONGCOL{20};
    constexpr int SHORTCOL{4};
    cmd.setParamColumn(LONGCOL + SHORTCOL + 4);
    cmd.addParam("in-directory", "input directory containing DICOM files or directories with DICOM files");

    cmd.setOptionColumns(LONGCOL, SHORTCOL);
    cmd.addGroup("general options:", LONGCOL, SHORTCOL + 2);
    cmd.addOption("--help", "-h", "print this help text and exit", OFCommandLine::AF_Exclusive);
    cmd.addOption("--version", "print version information and exit", OFCommandLine::AF_Exclusive);

    OFLog::addOptions(cmd);

    cmd.addGroup("Tag options:");
    cmd.addOption("--tag", "-t", 1, "tag: gggg,eeee=\"string\" or name=\"string\"", "DICOM tag key");

    cmd.addGroup("output options:");
    // cmd.addOption("--out-directory",
    //               "-od",
    //               1,
    //               "directory: string (default: \"./\" (current directory))",
    //               "write output dump file to output directory");
    cmd.addOption("--filepath",
                  "-f",
                  1,
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

        if (cmd.findOption("--tag", 0, OFCommandLine::FOM_FirstFromLeft)) {
            const char *tagString{nullptr};
            do {
                app.checkValue(cmd.getValue(tagString));

                DcmTag tag = parseTagKey(app, tagString);
                if (tag.getGroup() == 0xffff || tag.getElement() == 0xffff) continue;
                opt_inputTags.emplace_back(tag);

            } while (cmd.findOption("--tag", 0, OFCommandLine::FOM_NextFromLeft));
        }
        //
        // if (cmd.findOption("--out-directory")) {
        //     app.checkValue(cmd.getValue(opt_outDirectory));
        // }

        if (cmd.findOption("--filepath")) {
            app.checkValue(cmd.getValue(opt_dumpFilepath));
        }
    }

    if (std::filesystem::exists(opt_inDirectory)) {
        if (std::filesystem::is_regular_file(opt_inDirectory)) {
            OFLOG_ERROR(logger, fmt::format("input directory path \"{}\" is not directory", opt_inDirectory));
            return EXITCODE_COMMANDLINE_SYNTAX_ERROR;
        }

        if (std::filesystem::is_empty(opt_inDirectory)) {
            OFLOG_ERROR(logger, fmt::format("input directory path \"{}\" has no DICOM files", opt_inDirectory));
            return EXITCODE_COMMANDLINE_SYNTAX_ERROR;
        }
    } else {
        OFLOG_ERROR(logger, fmt::format("input directory path \"{}\" does not exist", opt_inDirectory));
        return EXITCODE_COMMANDLINE_SYNTAX_ERROR;
    }

    if (opt_inputTags.empty()) {
        OFLOG_WARN(logger, "No additional tags to write");
    }

    // const auto time = std::chrono::system_clock::now();
    const auto tt   = std::chrono::system_clock::to_time_t(
                                                         std::chrono::system_clock::now()
                                                        );
    const std::tm tm = *std::localtime(&tt);

    // std::filesystem::path outputPath{opt_outDirectory};
    const std::string dumpFilepath = fmt::format("{}-{:%Y-%m-%d-%H-%M-%S}.csv", opt_dumpFilepath, tm);
    // outputPath /= dumpFilename;
    auto filestream = fmt::output_file(dumpFilepath);

    std::string header{"PatientID;StudyInstanceUID;SeriesDescription"};

    auto iter = opt_inputTags.begin();
    while (iter != opt_inputTags.end()) {
        if (iter != opt_inputTags.end()) {
            header += ";";
        }
        header += iter->getTagName();
        ++iter;
    }

    const std::set<OFString> imagetypes_words{"secondary", "derived", "localizer"};
    const std::set<OFString> seriesdesc_words{"topog", "scout", "report", "dose", "protocol"};

    // main work
    DcmFileFormat dcmff;
    OFString currentSeriesuid{}, lastSeriesuid{};
    filestream.print("{}\n", header);
    for (const auto &entry : std::filesystem::recursive_directory_iterator(opt_inDirectory)) {
        if (entry.is_directory() || entry.path().filename() == "DICOMDIR") {
            OFLOG_DEBUG(logger, fmt::format("skipping entry \"{}\"", entry.path().string()));
            continue;
        }

        // load data just before pixel data, PixelData=(0x7fe0,0x0010)
        if (dcmff.loadFileUntilTag(entry.path().string().c_str(),
                                   EXS_Unknown,
                                   EGL_noChange,
                                   DCM_MaxReadLength,
                                   ERM_autoDetect,
                                   {0x7fdf, 0x0009}).bad()) {
            OFLOG_WARN(logger, fmt::format("failed loading file \"{}\"", entry.path().string()));
            dcmff.clear();
            continue;
        }

        DcmDataset *dataset = dcmff.getDataset();

        dataset->findAndGetOFString(DCM_SeriesInstanceUID, currentSeriesuid);
        if (currentSeriesuid == lastSeriesuid) {
            continue;
        }

        OFString patientid{}, studyuid{}, seriesdesc{}, imagetype{};
        dataset->findAndGetOFString(DCM_PatientID, patientid);
        dataset->findAndGetOFString(DCM_StudyInstanceUID, studyuid);
        dataset->findAndGetOFString(DCM_SeriesDescription, seriesdesc);
        dataset->findAndGetOFString(DCM_ImageType, imagetype);

        OFStandard::toLower(imagetype);
        OFStandard::toLower(seriesdesc);

        if (containsForbiddenSubstring(imagetype, imagetypes_words)) {
            OFLOG_INFO(logger,
                       fmt::format("dataset contains forbidden ImageType \"{}\", not writing tags", imagetype));
            continue;
        }

        if (containsForbiddenSubstring(seriesdesc, seriesdesc_words)) {
            OFLOG_INFO(logger,
                       fmt::format("dataset contains forbidden SeriesDescription \"{}\", not writing tags",
                           seriesdesc));
            continue;
        }

        OFString values{patientid + ";" + studyuid + ";" + seriesdesc};
        OFString val{};

        if (!opt_inputTags.empty()) {
            auto iter_val = opt_inputTags.begin();
            while (iter_val != opt_inputTags.end()) {
                if (iter_val != opt_inputTags.end()) {
                    values += ";";
                }
                dataset->findAndGetOFString(*iter_val, val);
                values += val;
                ++iter_val;
                val.clear();
            }
        }

        filestream.print("{}\n", values);
        fmt::print("written tags for {}, {}\n", patientid, seriesdesc);
        lastSeriesuid = currentSeriesuid;
    }

    fmt::print("tags written to {}\n", dumpFilepath);
    filestream.close();
    return 0;
}

DcmTag parseTagKey(OFConsoleApplication &app, const std::string &input_tag) {
    unsigned short group{0xffff}; // default unknown tag
    unsigned short element{0xffff}; // default unknown tag

    const std::size_t comma = input_tag.find(',');
    // const std::size_t equals = input_tag.find('=');

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
            errorMsg = "bad key format or dictionary name not found in dictionary: " + input_tag;
            app.printError(errorMsg.c_str());
            errorMsg.clear();
        }
    }

    DcmTag tag{group, element};
    const OFString tagname = tag.getTagName();
    if (tagname == "PatientID" ||
        tagname == "StudyInstanceUID" ||
        tagname == "SeriesDescription") {
        OFLOG_WARN(logger, "skipping -t " << tagname << "; already included by default");
        return {0xffff, 0xffff}; // return unknown tag
    }

    if (tag.error() != EC_Normal) {
        errorMsg = fmt::format("unknown tag key: ({:04x},{:04x}) for input tag: {}", group, element, input_tag);
        app.printError(errorMsg.c_str());
        errorMsg.clear();
    }

    return tag;
}
