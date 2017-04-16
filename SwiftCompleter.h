#include <string>
#include <string>
#include <vector>

class UnsavedFile {
    public:
    std::string contents_;
    std::string filename_;
};


namespace YouCompleteMe {


class SwiftCompleter {
public:
  SwiftCompleter();
  ~SwiftCompleter();

  std::string  CandidatesForLocationInFile(
    const std::string &filename,
    int line,
    int column,
    const std::vector< UnsavedFile > &unsaved_files,
    const std::vector< std::string > &flags );

  std::string GetDeclarationLocation(
    const std::string &filename,
    int line,
    int column,
    const std::vector< UnsavedFile > &unsaved_files,
    const std::vector< std::string > &flags,
    bool reparse = true );
};
}
