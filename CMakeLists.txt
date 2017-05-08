message("__SSVI CMAKE START")

cmake_minimum_required(VERSION 2.8)

# Boost
#

SET(CMAKE_INCLUDE_PATH ${CMAKE_INCLUDE_PATH} /usr/local/include/boost)
SET(CMAKE_INCLUDE_PATH ${CMAKE_INCLUDE_PATH} /usr/local/lib)

option (Boost_USE_STATIC_LIBS "Use static libraries for boost" ON)

set (BOOST_INCLUDEDIR /usr/local/Cellar/boost/1.64.0_1)
set (BOOST_ROOT /usr/local/lib)
set (BOOST_USE_MULTITHREADED ON)

find_package (Boost REQUIRED COMPONENTS
    coroutine
    context
    filesystem
    program_options
    system
    thread
    )

include_directories (SYSTEM ${Boost_INCLUDE_DIRS})
include_directories(/usr/local/include/boost)
link_libraries (${Boost_LIBRARIES})
add_definitions (-DBOOST_COROUTINES_NO_DEPRECATION_WARNING=1) # for asio

## Beast HTTP

include_directories(vendor/Beast/include/)

set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads)

# Include vendored deps

include_directories(vendor)

# Sourcekit

# OSX First
# Use Xcode'd sourcekitd build.
execute_process (
    COMMAND bash -c "xcode-select --print-path | tr -d '\n'"
    OUTPUT_VARIABLE XCODE_PATH
)
 
message(${XCODE_PATH})

set(SKT_FLAGS "-framework sourcekitd")
set(SKT_FLAGS " ${SKT_FLAGS} -F ${XCODE_PATH}/Toolchains/XcodeDefault.xctoolchain/usr/lib")
set(SKT_FLAGS " ${SKT_FLAGS} -rpath ${XCODE_PATH}/Toolchains/XcodeDefault.xctoolchain/usr/lib")

set(GLOBAL_CXX_FLAGS "-std=c++1y -stdlib=libc++")
set(GLOBAL_CXX_FLAGS "${GLOBAL_CXX_FLAGS} -Wall -Wextra -Wpedantic  -Wno-unused-parameter")
set(GLOBAL_CXX_FLAGS "${GLOBAL_CXX_FLAGS} -Wno-unused-command-line-argument")

add_definitions(${GLOBAL_CXX_FLAGS})
set(CMAKE_CXX_FLAGS ${SKT_FLAGS})

add_executable (http_server
    file_body.hpp
    Logging.hpp
    SemanticHTTPServer.hpp
    SemanticHTTPServer.cpp
    SwiftCompleter.hpp
    SwiftCompleter.cpp
    HTTPServerMain.cpp
)

add_executable (test_driver
    Logging.hpp
    SwiftCompleter.hpp
    SwiftCompleter.cpp
    Driver.cpp
)

target_link_libraries(http_server ${Boost_LIBRARIES} Threads::Threads)

INSTALL( TARGETS http_server
    RUNTIME DESTINATION bin )
INSTALL( FILES scripts/activate
    DESTINATION . )

message("__SSVI CMAKE FINISH")
