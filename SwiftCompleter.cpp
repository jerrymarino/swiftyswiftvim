#include "sourcekitd/sourcekitd.h"
#include <dispatch/dispatch.h>
#include <assert.h>
#include <string>
#include <functional>
#include <map>
#include <vector>
#include <iostream>
#include <fstream>
#include <sstream>

#include "SwiftCompleter.h"

#pragma mark - SourceKitD

static sourcekitd_uid_t KeyRequest;
static sourcekitd_uid_t KeyCompilerArgs;
static sourcekitd_uid_t KeyOffset;
static sourcekitd_uid_t KeyLength;

static sourcekitd_uid_t KeyCodeCompleteOptions;
static sourcekitd_uid_t KeyUseImportDepth;
static sourcekitd_uid_t KeyHideLowPriority;
static sourcekitd_uid_t KeyFilterText;

static sourcekitd_uid_t KeySourceFile;
static sourcekitd_uid_t KeySourceText;
static sourcekitd_uid_t KeyName;

static void InitKeys() {
  KeyRequest = sourcekitd_uid_get_from_cstr( "key.request" );
  KeyCompilerArgs = sourcekitd_uid_get_from_cstr( "key.compilerargs" );
  KeyOffset = sourcekitd_uid_get_from_cstr( "key.offset" );
  KeyLength = sourcekitd_uid_get_from_cstr( "key.length" );

  KeyCodeCompleteOptions =
    sourcekitd_uid_get_from_cstr( "key.codecomplete.options" );
  KeyUseImportDepth =
    sourcekitd_uid_get_from_cstr( "key.codecomplete.sort.useimportdepth" );
  KeyFilterText = sourcekitd_uid_get_from_cstr( "key.codecomplete.filtertext" );
  KeyHideLowPriority =
    sourcekitd_uid_get_from_cstr( "key.codecomplete.hidelowpriority" );

  KeySourceFile = sourcekitd_uid_get_from_cstr( "key.sourcefile" );
  KeySourceText = sourcekitd_uid_get_from_cstr( "key.sourcetext" );
  KeyName = sourcekitd_uid_get_from_cstr( "key.name" );
}

static void NotificationReceiver( sourcekitd_response_t resp ) {
  if ( sourcekitd_response_is_error( resp ) ) {
    sourcekitd_response_description_dump( resp );
  }
}

static void InitSourceKitD() {
  sourcekitd_initialize();
  InitKeys();
  sourcekitd_set_notification_handler( ^( sourcekitd_response_t resp ) {
    NotificationReceiver( resp );
  } );
}

static void ShutdowSourceKitD() {
  sourcekitd_shutdown();
}

static char *PrintResponse( sourcekitd_response_t resp ) {
  auto dict = sourcekitd_response_get_value( resp );
  auto JSONString = sourcekitd_variant_json_description_copy( dict );
  return JSONString;
}

#pragma mark - SourceKitD Completion Requests

static sourcekitd_object_t createBaseRequest( sourcekitd_uid_t requestUID,
                                              const char *name,
                                              unsigned offset ) {
  sourcekitd_object_t request =
    sourcekitd_request_dictionary_create( nullptr, nullptr, 0 );
  sourcekitd_request_dictionary_set_uid( request, KeyRequest, requestUID );
  sourcekitd_request_dictionary_set_int64( request, KeyOffset, offset );
  sourcekitd_request_dictionary_set_string( request, KeyName, name );
  return request;
}

using HandlerFunc = std::function<bool( sourcekitd_response_t )>;

static bool SendRequestSync( sourcekitd_object_t request, HandlerFunc func ) {
  auto response = sourcekitd_send_request_sync( request );
  bool result = func( response );
  sourcekitd_response_dispose( response );
  return result;
}

std::vector<std::string> DefaultOSXArgs() {
  return {
    "-sdk",
    "/Applications/Xcode.app/Contents/Developer/Platforms/MacOSX.platform/Developer/SDKs/MacOSX10.11.sdk",
    "-target",
    "x86_64-apple-macosx10.11",
  };
}

static bool CodeCompleteRequest(
  sourcekitd_uid_t requestUID,
  const char *name,
  unsigned offset,
  const char *sourceText,
  std::vector < std::string > compilerArgs,
  const char *filterText,
  HandlerFunc func ) {
  auto request = createBaseRequest( requestUID, name, offset );
  sourcekitd_request_dictionary_set_string( request, KeySourceFile, name );
  sourcekitd_request_dictionary_set_string( request, KeySourceText, sourceText );

  auto opts = sourcekitd_request_dictionary_create( nullptr, nullptr, 0 );
  {
    if ( filterText ) {
      sourcekitd_request_dictionary_set_string( opts, KeyFilterText, filterText );
    }
  }
  sourcekitd_request_dictionary_set_value( request, KeyCodeCompleteOptions, opts );
  sourcekitd_request_release( opts );

  auto args = sourcekitd_request_array_create( nullptr, 0 );
  {
    sourcekitd_request_array_set_string( args, SOURCEKITD_ARRAY_APPEND, name );

    for ( auto arg : compilerArgs )
      sourcekitd_request_array_set_string( args, SOURCEKITD_ARRAY_APPEND, arg.c_str() );

  }
  sourcekitd_request_dictionary_set_value( request, KeyCompilerArgs, args );
  sourcekitd_request_release( args );

  // Send the request!
  bool result = SendRequestSync( request, func );
  sourcekitd_request_release( request );
  return result;
}

// Context for a given completion
struct CompletionContext {
  // The current source source file's absolute path
  std::string sourceFilename;

  // Position of the completion
  unsigned line;
  unsigned column;

  std::vector < std::string > flags;

  // Unsaved files
  std::vector < UnsavedFile > unsavedFiles;

  // Return the args based on the current flags
  // and default to the OSX SDK if none.
  std::vector < std::string > compilerArgs() {
    if ( flags.size() == 0 ) {
      return DefaultOSXArgs();
    }

    return flags;
  }
};

// Get a clean file and offset for completion.
// FIXME: The file ends after the first interesting
// character, which prevents completing symbols
// declared after the offset
static void GetOffset(
  CompletionContext &ctx,
  unsigned *offset,
  std::string *CleanFile ) {
  unsigned line = ctx.line;
  unsigned column = ctx.column;
  auto fileName = ctx.sourceFilename;

  std::string unsavedInput;

  for ( auto unsavedFile : ctx.unsavedFiles ) {
    if ( unsavedFile.filename_ == fileName ) {
      unsavedInput = unsavedFile.contents_;
      break;
    }
  }

  assert( unsavedInput.length() && "Missing unsaved file" );

  std::istringstream sourceFile( unsavedInput );
  std::string someLine;
  unsigned currentLine = 0;
  unsigned outOffset = 0;

  while ( std::getline( sourceFile, someLine ) ) {
    if ( currentLine + 1 == line ) {
      // Enumerate from the column to an interesting point
      for ( int i = column; i >= 0; i-- ) {
        char someChar = '\0';

        if ( someLine.length() > i ) {
          someChar = someLine.at( i );
        }

        if ( someChar == ' ' || someChar == '.' || i == 0 ) {
          unsigned forwardIdx = i + 1;
          //Include the character in the partial file
          std::string  partialLine = "";

          if ( i == 0 ) {
            partialLine = someLine;
          } else if ( someLine.length() > forwardIdx ) {
            partialLine = someLine.substr( 0, forwardIdx );
          } else if ( someLine.length() > i ) {
            partialLine = someLine.substr( 0, i );
          }

          outOffset += partialLine.length();
          CleanFile->append( partialLine );
          outOffset = CleanFile->length();

          *offset = outOffset;
          return;
        }
      }
    } else {
      currentLine++;
      CleanFile->append( someLine );
      CleanFile->append( "\n" );
    }
  }
}

// Update the file and get latest results.
static int CompletionUpdate( CompletionContext &ctx, char **oresponse ) {
  sourcekitd_uid_t RequestCodeCompleteUpdate =
    sourcekitd_uid_get_from_cstr( "source.request.codecomplete.update" );
  unsigned CodeCompletionOffset = 0;
  std::string CleanFile;
  GetOffset( ctx, &CodeCompletionOffset, &CleanFile );

  bool isError =  CodeCompleteRequest(
                    RequestCodeCompleteUpdate, ctx.sourceFilename.data(), CodeCompletionOffset,
                    CleanFile.c_str(), ctx.compilerArgs(), nullptr,
  [&]( sourcekitd_object_t response ) -> bool {
    if ( sourcekitd_response_is_error( response ) ) {
      sourcekitd_response_description_dump( response );
      return true;
    }
    *oresponse = PrintResponse( response );
    return false;
  } );

  return isError;
}

// Open the connection and get the first set of results.
static int CompletionOpen( CompletionContext &ctx, char **oresponse ) {
  sourcekitd_uid_t RequestCodeCompleteOpen =
    sourcekitd_uid_get_from_cstr( "source.request.codecomplete.open" );
  unsigned CodeCompletionOffset = 0;
  std::string CleanFile;
  GetOffset( ctx, &CodeCompletionOffset, &CleanFile );

  bool isError = CodeCompleteRequest(
                   RequestCodeCompleteOpen, ctx.sourceFilename.data(), CodeCompletionOffset,
                   CleanFile.c_str(), ctx.compilerArgs(), nullptr,
  [&]( sourcekitd_object_t response ) -> bool {
    if ( sourcekitd_response_is_error( response ) ) {
      sourcekitd_response_description_dump( response );
      return true;
    }
    *oresponse = PrintResponse( response );
    return false;
  } );
  return isError;
}

#pragma mark - SwiftCompleter

namespace YouCompleteMe {
static sourcekitd_uid_t KeyRequest;

SwiftCompleter::SwiftCompleter() {
  InitSourceKitD();
}

SwiftCompleter::~SwiftCompleter() {
  ShutdowSourceKitD();
}

bool OpenForFile = false;

// FIXME: add support for eagerly opening a file and pre
// populating the cache. It seems to take a long time to
// return the first result and results after that are
// faster
static bool openForFile( std::string file ) {
  return OpenForFile;
}

std::string SwiftCompleter::CandidatesForLocationInFile(
  const std::string &filename,
  int line,
  int column,
  const std::vector< UnsavedFile > &unsaved_files,
  const std::vector< std::string > &flags ) {
  CompletionContext ctx;
  ctx.sourceFilename = filename;
  ctx.line = line;
  ctx.column = column;
  ctx.unsavedFiles = unsaved_files;
  ctx.flags = flags;
  char *response = NULL;

  if ( openForFile( "" ) ) {
    CompletionOpen( ctx, &response );
    CompletionUpdate( ctx, &response );
  } else {
    CompletionOpen( ctx, &response );
    OpenForFile = true;
  }

  return response;
}

std::string SwiftCompleter::GetDeclarationLocation(
  const std::string &filename,
  int line,
  int column,
  const std::vector< UnsavedFile > &unsaved_files,
  const std::vector< std::string > &flags,
  bool reparse ) {
  return "SomeLocation";
}

}
