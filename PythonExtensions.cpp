
#include <boost/python.hpp>
#include <string>
#include "SwiftCompleter.h"
#include <boost/python/suite/indexing/vector_indexing_suite.hpp>


using namespace boost::python;
using namespace YouCompleteMe;
typedef std::vector<std::string> StringList;

struct Runner
{
    void set(std::string fileName,
            std::string fileContents,
            std::vector<std::string> flags,
            unsigned line,
            unsigned column) { 
        this->fileName = fileName;
        unsavedFile.contents_ = fileContents;
        unsavedFile.filename_ = fileName;
        this->column = column;
        this->line = line;
        this->flags = flags;
    }

    std::string setAndRun(std::string fileName,
            std::string fileContents,
            std::vector<std::string> flags,
            unsigned line,
            unsigned column) {
        set(fileName, fileContents, flags, line, column);
        return run();
    }

    std::string run() { 
        auto completer = SwiftCompleter();        
        auto flags = this->flags;
        auto files = std::vector<UnsavedFile>();
        files.push_back(unsavedFile);
        auto result = completer.CandidatesForLocationInFile(
                fileName,
                column,
                line,
                files,
                flags);

        return result;
    
    }

    std::string fileName;
    std::vector<std::string> flags;
    UnsavedFile unsavedFile;
    unsigned line;
    unsigned column;
};

BOOST_PYTHON_MODULE(swiftvi)
{
    class_<StringList>("StringList")
        .def(vector_indexing_suite<StringList>() );
    class_<Runner>("Runner")
        .def("run", &Runner::run)
        .def("complete", &Runner::setAndRun)
        .def("setFile", &Runner::set)
    ;
};
