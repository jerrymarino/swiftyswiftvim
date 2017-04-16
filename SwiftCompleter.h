#include "../DLLDefines.h"
#include "UnsavedFile.h"
#include <boost/utility.hpp>
#include <string>
#include <string>
#include <vector>

namespace YouCompleteMe {
class SwiftCompleter : boost::noncopyable {
public:
  YCM_DLL_EXPORT SwiftCompleter();
  YCM_DLL_EXPORT ~SwiftCompleter();

  YCM_DLL_EXPORT std::string  CandidatesForLocationInFile(
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
