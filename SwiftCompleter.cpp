#import <assert.h>
#import <dispatch/dispatch.h>
#import <fstream>
#import <functional>
#import <future>
#import <iostream>
#import <map>
#import <sourcekitd/sourcekitd.h>
#import <sstream>
#import <string>
#import <thread>
#import <vector>

#import "Logging.hpp"
#import "SwiftCompleter.hpp"

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

class FutureChannel {
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

using HandlerFunc = std::function<bool(sourcekitd_response_t)>;

namespace ssvim {

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

  std::vector<std::string> DefaultOSXArgs() {
    return {
        "-sdk",
        "/Applications/Xcode.app/Contents/Developer/Platforms/"
        "MacOSX.platform/Developer/SDKs/MacOSX.sdk",
        "-target", "x86_64-apple-macosx10.12",
    };
  }
};

class SourceKitService {
  Logger _logger;

public:
  SourceKitService(LogLevel logLevel);
  int CompletionUpdate(CompletionContext &ctx, char **oresponse);
  int CompletionOpen(CompletionContext &ctx, char **oresponse);
  int EditorOpen(CompletionContext &ctx, char **oresponse);
  int EditorReplaceText(CompletionContext &ctx, char **oresponse);
};
} // namespace ssvim

static char *PrintResponse(sourcekitd_response_t resp) {
  auto dict = sourcekitd_response_get_value(resp);
  auto JSONString = sourcekitd_variant_json_description_copy(dict);
  return JSONString;
}

// A Future channel for Semantic notifications.
// This channel is shared across all SourceKitService instances
// and SwiftCompleter instances
static FutureChannel SemaFutureChannel;

// There is a single notification receiver per sourcekitd session
// and currently, there is a single session per server
// @see SourceKitService::SourceKitService()
static void NotificationReceiver(ssvim::Logger logger,
                                 sourcekitd_response_t resp) {
  sourcekitd_response_description_dump(resp);
  sourcekitd_variant_t payload = sourcekitd_response_get_value(resp);

  // In order to get the semantic info, we have to wait for the editor to be
  // ready. It will notify us on the main thread and we can make another
  // request. This needs to happen after various requests ( mainly only used
  // for editor.replacetext now.
  auto semaName = sourcekitd_variant_dictionary_get_string(payload, KeyName);
  logger << "DID_GET_SEMA: " << semaName;
  sourcekitd_object_t edReq =
      sourcekitd_request_dictionary_create(nullptr, nullptr, 0);
  sourcekitd_request_dictionary_set_uid(
      edReq, KeyRequest,
      sourcekitd_uid_get_from_cstr("source.request.editor.replacetext"));
  sourcekitd_request_dictionary_set_string(edReq, KeyName, semaName);
  sourcekitd_request_dictionary_set_string(edReq, KeySourceText, "");

  // Send the request in the notification
  auto semaResponse = sourcekitd_send_request_sync(edReq);
  sourcekitd_response_description_dump(semaResponse);
  sourcekitd_request_release(edReq);
  logger << "SEMA_DONE";
  SemaFutureChannel.set(semaName, PrintResponse(semaResponse));
}

#pragma mark - SourceKit Completion Request Helper Functions

static sourcekitd_object_t CreateBaseRequest(sourcekitd_uid_t requestUID,
                                             const char *name,
                                             unsigned offset) {
  sourcekitd_object_t request =
      sourcekitd_request_dictionary_create(nullptr, nullptr, 0);
  sourcekitd_request_dictionary_set_uid(request, KeyRequest, requestUID);
  sourcekitd_request_dictionary_set_int64(request, KeyOffset, offset);
  sourcekitd_request_dictionary_set_string(request, KeyName, name);
  return request;
}

static bool SendRequestSync(sourcekitd_object_t request, HandlerFunc func) {
  auto response = sourcekitd_send_request_sync(request);
  bool result = func(response);
  sourcekitd_response_dispose(response);
  return result;
}

static bool CodeCompleteRequest(sourcekitd_uid_t requestUID, const char *name,
                                unsigned offset, const char *sourceText,
                                std::vector<std::string> compilerArgs,
                                const char *filterText, HandlerFunc func) {
  auto request = CreateBaseRequest(requestUID, name, offset);
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
  bool result = SendRequestSync(request, func);
  sourcekitd_request_release(request);
  return result;
}

using namespace ssvim;

// Get a clean file and offset for completion.
//
// The file ends after the first interesting character, which may prevent
// completing symbols declared after the offset.
//
// This seemed necessary on Swift V2 when it was first written, but hopefully
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
  while (std::getline(sourceFile, someLine)) {
    if (currentLine + 1 == line) {
      // Enumerate from the column to an interesting point
      for (int i = column; i >= 0; i--) {
        char someChar = '\0';
        if (someLine.length() > i) {
          someChar = someLine.at(i);
        }

        if (someChar == ' ' || someChar == '.' || i == 0) {
          // Include the character in the partial file
          std::string partialLine = "";
          if (i == 0) {
            partialLine = someLine;
          } else if (someLine.length() > i) {
            partialLine = someLine.substr(0, i + 1);
          }
          CleanFile->append(partialLine);
          *offset = CleanFile->length();
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

SourceKitService::SourceKitService(ssvim::LogLevel logLevel)
    : _logger(logLevel, "SKT") {
  // Initialize SourceKitD resource
  //
  // Here, we are set the notification to register for callbacks around editor
  // updates. This callback is invoked on the main thread.
  //
  // It is currently designed to have a single instance "initialized" for a
  // given program. It is lazily started, and never torn down. It manages
  // caching internally per session. There is an issue in SourceKitD that
  // causes us to never tear it down.
  static dispatch_once_t onceToken;
  dispatch_once(&onceToken, ^{
    ssvim::Logger sharedNotificationLogger(logLevel, "SKT");
    sourcekitd_initialize();
    // WARNING ( called on dispatch_main_queue ) by sourcekitd
    sourcekitd_set_notification_handler(^(sourcekitd_response_t resp) {
      dispatch_async(dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_HIGH, 0),
                     ^{
                       NotificationReceiver(sharedNotificationLogger, resp);
                     });
    });
  });
}

// Update the file and get latest results.
int SourceKitService::CompletionUpdate(CompletionContext &ctx,
                                       char **oresponse) {
  _logger << "WILL_COMPLETION_UPDATE";
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
        _logger.log(LogLevelExtreme, *oresponse);
        return false;
      });
  _logger << "DID_COMPLETION_UPDATE";
  return isError;
}

// Open the connection and get the first set of results.
int SourceKitService::CompletionOpen(CompletionContext &ctx, char **oresponse) {
  _logger << "WILL_COMPLETION_OPEN";
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
        _logger.log(LogLevelExtreme, *oresponse);
        return false;
      });
  _logger << "DID_COMPLETION_OPEN";
  return isError;
}

// Open sourcekit in editor mode
// On success, this returns a list of after the contents have
// gone through parsing.
int SourceKitService::EditorOpen(CompletionContext &ctx, char **oresponse) {
  _logger << "WILL_EDITOR_OPEN";
  auto contents = ctx.unsavedFiles[0].contents.c_str();
  bool isError =
      BasicRequest(sourcekitd_uid_get_from_cstr("source.request.editor.open"),
                   ctx.sourceFilename.data(), contents, ctx.compilerArgs(),
                   [&](sourcekitd_object_t response) -> bool {
                     if (sourcekitd_response_is_error(response)) {
                       return true;
                     }
                     *oresponse = PrintResponse(response);
                     _logger.log(LogLevelExtreme, *oresponse);
                     return false;
                   });
  _logger << "DID_EDITOR_OPEN";
  return isError;
}

// Editor replace text.
// This command puts sourcekitd into semantic mode to get full
// diagnostics.
int SourceKitService::EditorReplaceText(CompletionContext &ctx,
                                        char **oresponse) {
  std::string CleanFile;
  _logger << "WILL_EDITOR_REPLACETEXT";
  auto contents = ctx.unsavedFiles[0].contents.c_str();
  bool isError = BasicRequest(
      sourcekitd_uid_get_from_cstr("source.request.editor.replacetext"),
      ctx.sourceFilename.data(), contents, ctx.compilerArgs(),
      [&](sourcekitd_object_t response) -> bool {
        if (sourcekitd_response_is_error(response)) {
          return true;
        }
        *oresponse = PrintResponse(response);
        _logger.log(LogLevelExtreme, *oresponse);
        return false;
      });
  _logger << "DID_EDITOR_REPLACETEXT";
  return isError;
}

#pragma mark - SwiftCompleter

namespace ssvim {

// SwiftCompleter composes SourceKitService requests together
// to implement the higher level API.
SwiftCompleter::SwiftCompleter(LogLevel logLevel)
    : _logger(logLevel, "COMPLETER") {
}

SwiftCompleter::~SwiftCompleter() {
}

const std::string SwiftCompleter::CandidatesForLocationInFile(
    const std::string &filename, int line, int column,
    const std::vector<UnsavedFile> &unsavedFiles,
    const std::vector<std::string> &flags) {
  CompletionContext ctx;
  ctx.sourceFilename = filename;
  ctx.line = line;
  ctx.column = column;
  ctx.unsavedFiles = unsavedFiles;
  ctx.flags = flags;

  SourceKitService sktService(_logger.level());
  char *response = NULL;
  sktService.CompletionOpen(ctx, &response);
  sktService.CompletionUpdate(ctx, &response);
  return response;
}

const std::string
SwiftCompleter::DiagnosticsForFile(const std::string &filename,
                                   const std::vector<UnsavedFile> &unsavedFiles,
                                   const std::vector<std::string> &flags) {
  CompletionContext ctx;
  ctx.sourceFilename = filename;
  ctx.unsavedFiles = unsavedFiles;
  ctx.flags = flags;
  ctx.line = 0;
  ctx.column = 0;

  SourceKitService sktService(_logger.level());
  char *response = NULL;
  sktService.EditorOpen(ctx, &response);
  sktService.EditorReplaceText(ctx, &response);

  // We need to wait until:
  // - the document is updated ( NotificationReceiver fires )
  // - send a request for semantic info
  // - the semantic request completes
  auto future = SemaFutureChannel.future(filename);
  auto semaresult = future.get();
  return semaresult;
}
} // namespace ssvim
