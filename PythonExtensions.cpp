
#include <boost/python.hpp>
#include <string>

struct Runner
{
    void set(std::string msg) { mMsg = msg; }
    std::string run() { return mMsg; }
    std::string mMsg;
};

using namespace boost::python;

BOOST_PYTHON_MODULE(swiftvi)
{
    class_<Runner>("Runner")
        .def("run", &Runner::run)
        .def("set", &Runner::set)
    ;
};
