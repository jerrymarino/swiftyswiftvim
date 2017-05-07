#include <assert.h>
#include <dispatch/dispatch.h>
#include <fstream>
#include <functional>
#include <future>
#include <iostream>
#include <map>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

#include "SwiftCompleter.hpp"
#include "sourcekitd/sourcekitd.h"

#pragma mark - Futures

using promise_ty = std::promise<std::string> *;

// future channel sets values on registered futures.
// it operates in a shared key space.
//
// usage:
// in the example of working with the document update notifications,
// a key would be formed based on the name and notification
//
// {
//  key.notification: source.notification.editor.documentupdate,
//  key.name: "/Users/aprilmarino/swiftyswiftvim/Examples/some_swift.swift"
//  }

class future_channel {
  std::map<std::string, std::vector<promise_ty>> _promises;
  std::mutex _shared_mutex;

public:
  void set(std::string key, const std::string value) {
    std::lock_guard<std::mutex> lock(_shared_mutex);
    auto entries = _promises.find(key);
    if (entries != _promises.end()) {
      for (auto promise : entries->second) {
        promise->set_value(value);
        delete promise;
      }
      _promises.erase(key);
    }
  }

  std::future<std::string> future(std::string key) {
    auto promise = new std::promise<std::string>;
    std::lock_guard<std::mutex> lock(_shared_mutex);
    _promises[key].push_back(promise);
    return promise->get_future();
  }
};

#pragma mark - SourceKitD

static auto KeyRequest = sourcekitd_uid_get_from_cstr("key.request");
static auto KeyCompilerArgs = sourcekitd_uid_get_from_cstr("key.compilerargs");
static auto KeyOffset = sourcekitd_uid_get_from_cstr("key.offset");
static auto KeyLength = sourcekitd_uid_get_from_cstr("key.length");
static auto KeyCodeCompleteOptions =
    sourcekitd_uid_get_from_cstr("key.codecomplete.options");
static auto KeyUseImportDepth =
    sourcekitd_uid_get_from_cstr("key.codecomplete.sort.useimportdepth");
static auto KeyFilterText =
    sourcekitd_uid_get_from_cstr("key.codecomplete.filtertext");
static auto KeyHideLowPriority =
    sourcekitd_uid_get_from_cstr("key.codecomplete.hidelowpriority");
static auto KeySourceFile = sourcekitd_uid_get_from_cstr("key.sourcefile");
static auto KeySourceText = sourcekitd_uid_get_from_cstr("key.sourcetext");
static auto KeyName = sourcekitd_uid_get_from_cstr("key.name");

#pragma mark - SourceKitD Notifications

static void NotificationReceiver(sourcekitd_response_t resp);

// Initialize SourceKitD
//
// Here, we are set the notification to register for callbacks around editor
// updates. This callback is invoked on the main thread.
//
// It is currently designed to have a single instance "initialized" for a given
// program. It is lazily started, and never torn down.
//
// This means that:
// - Don't do any level of processing or blocking i/o in NotificationHandler
// FIXME!!! ( it currently does this )
static void InitSourceKitD() {
  static dispatch_once_t onceToken;
  dispatch_once(&onceToken, ^{
    sourcekitd_initialize();
    sourcekitd_set_notification_handler(^(sourcekitd_response_t resp) {
      NotificationReceiver(resp);
    });
  });
}

static void ShutdowSourceKitD() {}

static char *PrintResponse(sourcekitd_response_t resp) {
  auto dict = sourcekitd_response_get_value(resp);
  auto JSONString = sourcekitd_variant_json_description_copy(dict);
  return JSONString;
}

// A Future channel for Semantic notificaitons.
static future_channel sema_future_channel;

static void NotificationReceiver(sourcekitd_response_t resp) {
  sourcekitd_response_description_dump(resp);
  sourcekitd_variant_t payload = sourcekitd_response_get_value(resp);
  // In order to get the semantic info, we have to wait for the editor to be
  // ready
  // FIXME: factor out all of the diagnostic code out into a new file
  // Ideally, there should be a nice way to init a sourcekitd session in a way
  // that has a handler only for semantic info
  auto semaName = sourcekitd_variant_dictionary_get_string(payload, KeyName);
  std::cout << "DID_GET_SEMA: " << semaName;
  std::cout.flush();
  sourcekitd_object_t edReq =
      sourcekitd_request_dictionary_create(nullptr, nullptr, 0);
  sourcekitd_request_dictionary_set_uid(
      edReq, KeyRequest,
      sourcekitd_uid_get_from_cstr("source.request.editor.replacetext"));
  sourcekitd_request_dictionary_set_string(edReq, KeyName, semaName);
  sourcekitd_request_dictionary_set_string(edReq, KeySourceText, "");
  auto semaResponse = sourcekitd_send_request_sync(edReq);
  sourcekitd_response_description_dump(semaResponse);
  sourcekitd_request_release(edReq);
  std::cout << "__SEMA_DONE";
  std::cout.flush();
  sema_future_channel.set(semaName, PrintResponse(semaResponse));
}

#pragma mark - SourceKitD Completion Requests

static sourcekitd_object_t createBaseRequest(sourcekitd_uid_t requestUID,
                                             const char *name,
                                             unsigned offset) {
  sourcekitd_object_t request =
      sourcekitd_request_dictionary_create(nullptr, nullptr, 0);
  sourcekitd_request_dictionary_set_uid(request, KeyRequest, requestUID);
  sourcekitd_request_dictionary_set_int64(request, KeyOffset, offset);
  sourcekitd_request_dictionary_set_string(request, KeyName, name);
  return request;
}

using HandlerFunc = std::function<bool(sourcekitd_response_t)>;

static bool SendRequestSync(sourcekitd_object_t request, HandlerFunc func) {
  auto response = sourcekitd_send_request_sync(request);
  bool result = func(response);
  // FIXME:!
  //  sourcekitd_response_dispose(response);
  return result;
}

std::vector<std::string> DefaultOSXArgs() {
  return {
      "-sdk",
      "/Applications/Xcode.app/Contents/Developer/Platforms/"
      "MacOSX.platform/Developer/SDKs/MacOSX.sdk",
      "-target", "x86_64-apple-macosx10.12",
  };
}

static bool CodeCompleteRequest(sourcekitd_uid_t requestUID, const char *name,
                                unsigned offset, const char *sourceText,
                                std::vector<std::string> compilerArgs,
                                const char *filterText, HandlerFunc func) {
  auto request = createBaseRequest(requestUID, name, offset);
  sourcekitd_request_dictionary_set_string(request, KeySourceFile, name);
  sourcekitd_request_dictionary_set_string(request, KeySourceText, sourceText);

  auto opts = sourcekitd_request_dictionary_create(nullptr, nullptr, 0);
  {
    if (filterText) {
      sourcekitd_request_dictionary_set_string(opts, KeyFilterText, filterText);
    }
  }
  sourcekitd_request_dictionary_set_value(request, KeyCodeCompleteOptions,
                                          opts);
  sourcekitd_request_release(opts);

  auto args = sourcekitd_request_array_create(nullptr, 0);
  {
    sourcekitd_request_array_set_string(args, SOURCEKITD_ARRAY_APPEND, name);

    for (auto arg : compilerArgs)
      sourcekitd_request_array_set_string(args, SOURCEKITD_ARRAY_APPEND,
                                          arg.c_str());
  }
  sourcekitd_request_dictionary_set_value(request, KeyCompilerArgs, args);
  sourcekitd_request_release(args);
  bool result = SendRequestSync(request, func);
  sourcekitd_request_release(request);
  return result;
}

static bool BasicRequest(sourcekitd_uid_t requestUID, const char *name,
                         const char *sourceText,
                         std::vector<std::string> compilerArgs,
                         HandlerFunc func) {

  auto request = sourcekitd_request_dictionary_create(nullptr, nullptr, 0);
  sourcekitd_request_dictionary_set_uid(request, KeyRequest, requestUID);
  sourcekitd_request_dictionary_set_string(request, KeyName, name);
  sourcekitd_request_dictionary_set_string(request, KeySourceText, sourceText);
  auto KeySyntacticOnly = sourcekitd_uid_get_from_cstr("key.syntactic_only");
  auto KeyEnableSubStructure =
      sourcekitd_uid_get_from_cstr("key.enablesubstructure");

  sourcekitd_request_dictionary_set_int64(request, KeyEnableSubStructure, 1);
  sourcekitd_request_dictionary_set_int64(request, KeySyntacticOnly, 0);

  auto args = sourcekitd_request_array_create(nullptr, 0);
  {
    sourcekitd_request_array_set_string(args, SOURCEKITD_ARRAY_APPEND, name);

    for (auto arg : compilerArgs)
      sourcekitd_request_array_set_string(args, SOURCEKITD_ARRAY_APPEND,
                                          arg.c_str());
  }
  sourcekitd_request_dictionary_set_value(request, KeyCompilerArgs, args);
  sourcekitd_request_release(args);
  std::cout << "\n__SENDNOW\n";
  bool result = SendRequestSync(request, func);
  sourcekitd_request_release(request);
  std::cout << "\n__ENDSEND\n";
  return result;
}

using namespace ssvim;

// Context for a given completion

struct CompletionContext {
  // The current source source file's absolute path
  std::string sourceFilename;

  // Position of the completion
  unsigned line;
  unsigned column;

  std::vector<std::string> flags;

  // Unsaved files
  std::vector<UnsavedFile> unsavedFiles;

  // Return the args based on the current flags
  // and default to the OSX SDK if none.
  std::vector<std::string> compilerArgs() {
    if (flags.size() == 0) {
      return DefaultOSXArgs();
    }

    return flags;
  }
};

// Get a clean file and offset for completion.
//
// The file ends after the first interesting character, which may prevent
// completing symbols declared after the offset.
//
// This seemend necessary on Swift V2 when it was first written, but hopefully
// it can be improved.
static void GetOffset(CompletionContext &ctx, unsigned *offset,
                      std::string *CleanFile) {
  unsigned line = ctx.line;
  unsigned column = ctx.column;
  auto fileName = ctx.sourceFilename;

  std::string unsavedInput;

  for (auto unsavedFile : ctx.unsavedFiles) {
    if (unsavedFile.fileName == fileName) {
      unsavedInput = unsavedFile.contents;
      break;
    }
  }

  assert(unsavedInput.length() && "Missing unsaved file");

  std::istringstream sourceFile(unsavedInput);
  std::string someLine;
  unsigned currentLine = 0;
  unsigned outOffset = 0;

  while (std::getline(sourceFile, someLine)) {
    if (currentLine + 1 == line) {
      // Enumerate from the column to an interesting point
      for (int i = column; i >= 0; i--) {
        char someChar = '\0';

        if (someLine.length() > i) {
          someChar = someLine.at(i);
        }

        if (someChar == ' ' || someChar == '.' || i == 0) {
          unsigned forwardIdx = i + 1;
          // Include the character in the partial file
          std::string partialLine = "";
          if (i == 0) {
            partialLine = someLine;
          } else if (someLine.length() > forwardIdx) {
            partialLine = someLine.substr(0, forwardIdx);
          } else if (someLine.length() > i) {
            partialLine = someLine.substr(0, i);
          }
          outOffset += partialLine.length();
          CleanFile->append(partialLine);
          outOffset = CleanFile->length();

          *offset = outOffset;
          return;
        }
      }
    } else {
      currentLine++;
      CleanFile->append(someLine);
      CleanFile->append("\n");
    }
  }
}

// Update the file and get latest results.
static int CompletionUpdate(CompletionContext &ctx, char **oresponse) {
  sourcekitd_uid_t RequestCodeCompleteUpdate =
      sourcekitd_uid_get_from_cstr("source.request.codecomplete.update");
  unsigned CodeCompletionOffset = 0;
  std::string CleanFile;
  GetOffset(ctx, &CodeCompletionOffset, &CleanFile);

  bool isError = CodeCompleteRequest(
      RequestCodeCompleteUpdate, ctx.sourceFilename.data(),
      CodeCompletionOffset, CleanFile.c_str(), ctx.compilerArgs(), nullptr,
      [&](sourcekitd_object_t response) -> bool {
        if (sourcekitd_response_is_error(response)) {
          return true;
        }
        *oresponse = PrintResponse(response);
        return false;
      });

  return isError;
}

// Open the connection and get the first set of results.
static int CompletionOpen(CompletionContext &ctx, char **oresponse) {
  sourcekitd_uid_t RequestCodeCompleteOpen =
      sourcekitd_uid_get_from_cstr("source.request.codecomplete.open");
  unsigned CodeCompletionOffset = 0;
  std::string CleanFile;
  GetOffset(ctx, &CodeCompletionOffset, &CleanFile);

  bool isError = CodeCompleteRequest(
      RequestCodeCompleteOpen, ctx.sourceFilename.data(), CodeCompletionOffset,
      CleanFile.c_str(), ctx.compilerArgs(), nullptr,
      [&](sourcekitd_object_t response) -> bool {
        if (sourcekitd_response_is_error(response)) {
          return true;
        }
        *oresponse = PrintResponse(response);
        return false;
      });
  return isError;
}

// Open sourcekit in editor mode
// On success, this returns a list of after the contents have
// gone through parsing.
static int EditorOpen(CompletionContext &ctx, char **oresponse) {
  auto contents = ctx.unsavedFiles[0].contents.c_str();
  std::cout << "__CTX:";
  std::cout << ctx.sourceFilename;
  std::cout << contents;
  for (auto &arg : ctx.compilerArgs()) {
    std::cout << "__ARG:";
    std::cout << arg;
  }
  std::cout.flush();
  bool isError =
      BasicRequest(sourcekitd_uid_get_from_cstr("source.request.editor.open"),
                   ctx.sourceFilename.data(), contents, ctx.compilerArgs(),
                   [&](sourcekitd_object_t response) -> bool {
                     if (sourcekitd_response_is_error(response)) {
                       return true;
                     }
                     *oresponse = PrintResponse(response);
                     return false;
                   });
  std::cout << "isError:";
  std::cout << isError;
  return isError;
}

// Editor replace text.
// This command puts sourcekitd into semantic mode to get full
// diagnostics.
static int EditorReplaceText(CompletionContext &ctx, char **oresponse) {
  std::string CleanFile;
  auto contents = ctx.unsavedFiles[0].contents.c_str();
  bool isError = BasicRequest(
      sourcekitd_uid_get_from_cstr("source.request.editor.replacetext"),
      ctx.sourceFilename.data(), contents, ctx.compilerArgs(),
      [&](sourcekitd_object_t response) -> bool {
        if (sourcekitd_response_is_error(response)) {
          return true;
        }
        *oresponse = PrintResponse(response);
        return false;
      });
  return isError;
}

#pragma mark - SwiftCompleter

namespace ssvim {

SwiftCompleter::SwiftCompleter() { InitSourceKitD(); }

SwiftCompleter::~SwiftCompleter() { ShutdowSourceKitD(); }

std::string SwiftCompleter::CandidatesForLocationInFile(
    const std::string &filename, int line, int column,
    const std::vector<UnsavedFile> &unsavedFiles,
    const std::vector<std::string> &flags) {
  CompletionContext ctx;
  ctx.sourceFilename = filename;
  ctx.line = line;
  ctx.column = column;
  ctx.unsavedFiles = unsavedFiles;
  ctx.flags = flags;
  char *response = NULL;
  CompletionOpen(ctx, &response);
  CompletionUpdate(ctx, &response);
  return response;
}

std::string
SwiftCompleter::DiagnosticsForFile(const std::string &filename,
                                   const std::vector<UnsavedFile> &unsavedFiles,
                                   const std::vector<std::string> &flags) {
  CompletionContext ctx;
  ctx.sourceFilename = filename;
  ctx.unsavedFiles = unsavedFiles;
  ctx.flags = flags;
  ctx.line = 0;
  ctx.column = 0;
  std::cout << "WillOpen\n";
  char *response = NULL;
  EditorOpen(ctx, &response);
  std::cout << "DidOpen\n";
  std::cout << response;
  EditorReplaceText(ctx, &response);
  std::cout << response;
  std::cout << "\n";

  auto future = sema_future_channel.future(filename);
  auto semaresult = future.get();
  std::cout << semaresult;
  std::cout << "\n";
  return semaresult;
}
} // namespace ssvim
