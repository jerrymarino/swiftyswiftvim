#import "Logging.hpp"
#import "SwiftCompleter.hpp"
#import <boost/algorithm/string/replace.hpp>
#import <boost/property_tree/json_parser.hpp>
#import <boost/property_tree/ptree.hpp>
#import <dispatch/dispatch.h>
#import <fstream>
#import <iostream>
#import <sstream>
#import <string>
#import <sys/socket.h>
#import <tuple>
#import <vector>

using namespace ssvim;
using namespace std;

std::string GetExamplesDir() {
  char cwd[1024];
  if (getcwd(cwd, sizeof(cwd)) != NULL) {
    return std::string(std::string(cwd) + "/Examples/");
  }
  return "";
}

std::string ReadFile(const std::string &fileName) {
  std::ifstream ifs(fileName.c_str(),
                    std::ios::in | std::ios::binary | std::ios::ate);

  std::ifstream::pos_type fileSize = ifs.tellg();
  ifs.seekg(0, std::ios::beg);

  std::vector<char> bytes(fileSize);
  ifs.read(&bytes[0], fileSize);
  return std::string(&bytes[0], fileSize);
}
using boost::property_tree::ptree;
using boost::property_tree::read_json;
using boost::property_tree::write_json;

ptree ReadJSON(std::string body) {
  ptree pt;
  std::istringstream is(body);
  read_json(is, pt);
  return pt;
}

// Write out an example DB.
// Template the __SRCROOT__ with an absolute
// return the ptree value
ptree SetupTemplateCompilationDB() {
  std::string exampleRoot = GetExamplesDir() + "iOS/Basic";
  auto exampleTemplate =
      exampleRoot + "/" + std::string("compile_commands.json.template");
  std::ofstream outFile(
      std::string(exampleRoot + "/" + std::string("compile_commands.json")));
  std::ifstream readFile(exampleTemplate);
  std::string line;
  std::string outStr;
  while (std::getline(readFile, line)) {
    boost::replace_all(line, std::string("__SRCROOT__"), exampleRoot);
    outFile << line << std::endl;
    outStr = outStr + line + "\n";
  }
  outStr += "\0";
  auto JSON = ReadJSON(outStr);
  return JSON;
}

// Lex a shell invocation
template <class C = std::vector<std::string>> C shlex(std::string s) {
  auto result = C{};

  auto accumulator = std::string{};
  auto quote = char{};
  auto escape = bool{};

  auto evictAccumulator = [&]() {
    if (!accumulator.empty()) {
      result.push_back(std::move(accumulator));
      accumulator = "";
    }
  };

  for (auto c : s) {
    if (escape) {
      escape = false;
      accumulator += c;
    } else if (c == '\\') {
      escape = true;
    } else if ((quote == '\0' && c == '\'') || (quote == '\0' && c == '\"')) {
      quote = c;
    } else if ((quote == '\'' && c == '\'') || (quote == '"' && c == '"')) {
      quote = '\0';
    } else if (!isspace(c) || quote != '\0') {
      accumulator += c;
    } else {
      evictAccumulator();
    }
  }

  evictAccumulator();

  return result;
}

auto BasicExampleNamed(std::string name) {
  std::string exampleRoot = GetExamplesDir() + "iOS/Basic";
  return exampleRoot + "/" + name;
}

class FlagTestSuite {
public:
  FlagTestSuite() {
    testBasicFlags();
    testDepFlags();
  }

  std::vector<std::string>
  getExampleCompileCommandForFile(std::string testFile) {
    auto compDB = SetupTemplateCompilationDB();
    std::cout << testFile;
    std::vector<std::string> command;
    for (auto item : compDB) {
      auto fileName = item.second.get<std::string>("file");
      auto commandStr = item.second.get<std::string>("command");
      if (fileName == testFile) {
        command = shlex(commandStr);
      }
    }
    assert(command.size() > 0);
    return command;
  }

  void testBasicFlags() {
    auto testFile = BasicExampleNamed("Basic/AppDelegate.swift");
    auto command = getExampleCompileCommandForFile(testFile);
    auto prepped = FlagsForCompileCommand(command);
    assert(prepped.at(0) == "-primary-file");
    assert(prepped.at(1) == testFile);
  }

  void testDepFlags() {
    auto testFile = BasicExampleNamed("Basic/ViewController.swift");
    auto depFile = BasicExampleNamed("Basic/AppDelegate.swift");
    auto command = getExampleCompileCommandForFile(testFile);
    auto prepped = FlagsForCompileCommand(command);
    assert(prepped.at(0) == "-primary-file");
    assert(prepped.at(1) == testFile);
    assert(prepped.at(2) == depFile);
  }
};

using namespace ssvim;
static ssvim::Logger logger(LogLevelInfo);

struct Runner {
  std::string complete(std::string fileName, std::string fileContents,
                       std::vector<std::string> flags, unsigned line,
                       unsigned column) {
    auto completer = SwiftCompleter(LogLevelExtreme);
    auto files = std::vector<UnsavedFile>();
    auto unsavedFile = UnsavedFile();
    unsavedFile.contents = fileContents;
    unsavedFile.fileName = fileName;

    files.push_back(unsavedFile);
    auto result = completer.CandidatesForLocationInFile(fileName, line, column,
                                                        files, flags);

    return result;
  }
};

class CompleterSmokeTestSuite {
public:
  CompleterSmokeTestSuite() {
    logger << "Running Smoke tests";
    Runner runner;
    vector<string> flags;
    flags.push_back("-sdk");
    flags.push_back("/Applications/Xcode.app/Contents/Developer/Platforms/"
                    "MacOSX.platform/Developer/SDKs/MacOSX.sdk");
    flags.push_back("-target");
    flags.push_back("x86_64-apple-macosx10.12");

    auto exampleDir = GetExamplesDir();
    auto exampleName = exampleDir + std::string("some_swift.swift");
    auto example = ReadFile(exampleName);
    std::cout << example;
    auto result = runner.complete(exampleName, example, flags, 19, 16);

    // FIXME: parse this response.
    // we should have a massive response here.
    assert(result.size() > 500);
    logger << "Ran Smoke tests";
  }
};

int wrapped_main() {
  FlagTestSuite suite;
  // TODO: this should be probably moved out of here. We are doing
  // assertions on code paths that we don't own
  CompleterSmokeTestSuite smokeTest;
  exit(0);
}

int main() {
  dispatch_async(dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_HIGH, 0), ^{
    wrapped_main();
  });
  dispatch_main();
}
