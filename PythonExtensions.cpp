
#include <boost/python.hpp>
#include <string>
#include "SwiftCompleter.h"


using namespace YouCompleteMe;

struct Runner
{
    void set(std::string fileName, std::string fileContents, unsigned line, unsigned column) { 
        this->fileName = fileName;
        unsavedFile.contents_ = fileContents;
        unsavedFile.filename_ = fileName;
        this->column = column;
        this->line = line;
    }
    std::string run() { 
        auto completer = SwiftCompleter();        
        auto flags = std::vector<std::string>();
        flags.push_back(std::string("Some"));
        auto files = std::vector<UnsavedFile>();
//        auto result = completer.CandidatesForLocationInFile(
 //               filename,
         //       column,
   //             line,
     //           files,
       //         flags)

        return "";
    
    }

    std::string fileName;
    UnsavedFile unsavedFile;
    unsigned line;
    unsigned column;
};

using namespace boost::python;

BOOST_PYTHON_MODULE(swiftvi)
{
    class_<Runner>("Runner")
        .def("run", &Runner::run)
        .def("setFile", &Runner::set)
    ;
};
