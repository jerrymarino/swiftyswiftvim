#include <string>
#include <string>
#include <vector>

namespace ssvim {

/**
 * An unsaved file.
 *
 * In practice, this file is in the users vim buffer
 * and changes have been made to it, but they have not
 * been written to disk yet
 */
class UnsavedFile {
public:
  std::string contents;
  std::string fileName;
};

/**
 * Yield complitions in the form of json string.
 *
 * The completions match the API used within sourcekit
 */
class SwiftCompleter {
public:
  SwiftCompleter();
  ~SwiftCompleter();

  std::string
  CandidatesForLocationInFile(const std::string &filename, int line, int column,
                              const std::vector<UnsavedFile> &unsavedFiles,
                              const std::vector<std::string> &flags);

  std::string
  GetDeclarationLocation(const std::string &filename, int line, int column,
                         const std::vector<UnsavedFile> &unsavedFiles,
                         const std::vector<std::string> &flags,
                         bool reparse = true);
};
}
