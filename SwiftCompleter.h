#include <string>
#include <string>
#include <vector>

namespace ssvim {

class UnsavedFile {
    public:
    std::string contents;
    std::string fileName;
};

class SwiftCompleter {
public:
  SwiftCompleter();
  ~SwiftCompleter();

  std::string  CandidatesForLocationInFile(
    const std::string &filename,
    int line,
    int column,
    const std::vector< UnsavedFile > &unsavedFiles,
    const std::vector< std::string > &flags );

  std::string GetDeclarationLocation(
    const std::string &filename,
    int line,
    int column,
    const std::vector< UnsavedFile > &unsavedFiles,
    const std::vector< std::string > &flags,
    bool reparse = true );
};
}
