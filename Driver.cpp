#import "Logging.hpp"
#import "SwiftCompleter.hpp"
#import <dispatch/dispatch.h>
#import <iostream>
#import <string>

using namespace ssvim;
using namespace std;

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

std::string contents = "// \n\
//  some_swift.swift \n\
//  Swift Completer \n\
// \n\
//  Created by Jerry Marino on 4/30/16. \n\
//  Copyright © 2016 Jerry Marino. All rights reserved. \n\
// \n\
 \n\
 \n\
    func someOtherFunc(){ \n\
    } \n\
 \n\
    func anotherFunction(){ \n\
    someOther()\n\
    } \n\
\n";

using namespace ssvim;
static ssvim::Logger logger(LogLevelInfo);

int wrapped_main() {
  Runner runner;
  std::cout << contents;
  vector<string> flags;
  flags.push_back("-sdk");
  flags.push_back("/Applications/Xcode.app/Contents/Developer/Platforms/"
                  "MacOSX.platform/Developer/SDKs/MacOSX.sdk");
  flags.push_back("-target");
  flags.push_back("x86_64-apple-macosx10.12");

  auto exampleFilePath = "/tmp/x";
  auto result = runner.complete(exampleFilePath, contents, flags, 19, 13);
  logger << result;
  logger << "Done";
  exit(0);
}

int main() {
  logger << "Running Test Driver";
  dispatch_async(dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_HIGH, 0), ^{
    wrapped_main();
  });
  dispatch_main();
}
