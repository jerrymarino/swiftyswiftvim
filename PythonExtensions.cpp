#include <boost/python.hpp>
#include <boost/python/suite/indexing/vector_indexing_suite.hpp>
#include <string>
#include "SwiftCompleter.h"

using namespace boost::python;
using namespace ssvim;

// Wrap a std::vector<std::string> for Python Consumers
typedef std::vector<std::string> StringList;

struct Runner
{
    std::string complete(std::string fileName,
            std::string fileContents,
            std::vector<std::string> flags,
            unsigned line,
            unsigned column) {
        auto completer = SwiftCompleter();        
        auto files = std::vector<UnsavedFile>();
        auto unsavedFile = UnsavedFile();
        unsavedFile.contents = fileContents;
        unsavedFile.fileName = fileName;

        files.push_back(unsavedFile);
        auto result = completer.CandidatesForLocationInFile(
                fileName,
                column,
                line,
                files,
                flags);

        return result;
    
    }
};

BOOST_PYTHON_MODULE(swiftvi)
{
    class_<StringList>("StringList")
        .def(vector_indexing_suite<StringList>() );
    class_<Runner>("Runner")
        .def("complete", &Runner::complete)
    ;
};
