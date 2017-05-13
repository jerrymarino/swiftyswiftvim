message("SSVIM CMAKE START")

cmake_minimum_required(VERSION 2.8)

# Boost

# Optionally the user may provide a custom boost.
# For now, we default to homebrew provided boosts.
# FIXME: This isn't really an ideal default, but it is
# what it has.
if(NOT DEFINED ENV{OVERRIDE_BOOST})
    message("Attempting to use brew boost")
    # if this command fails, the build will explode in a future step
    execute_process (
        COMMAND bash -c "${CMAKE_CURRENT_SOURCE_DIR}/install_brew_boost_if_needed.sh"
        OUTPUT_VARIABLE XCODE_PATH
    )
    set(BOOST_INCLUDEDIR /usr/local/include/boost)
    set(BOOST_ROOT /usr/local/lib)
endif()

## Boost includes

set(CMAKE_INCLUDE_PATH ${CMAKE_INCLUDE_PATH} ${BOOST_INCLUDEDIR})
set(CMAKE_INCLUDE_PATH ${CMAKE_INCLUDE_PATH} ${BOOST_ROOT})

include_directories(SYSTEM ${Boost_INCLUDE_DIRS})
include_directories(${BOOST_INCLUDEDIR})

option(Boost_USE_STATIC_LIBS "Use static libraries for boost" ON)
set(BOOST_USE_MULTITHREADED ON)

find_package(Boost REQUIRED COMPONENTS
    coroutine
    context
    filesystem
    program_options
    system
    thread
    )

link_libraries(${Boost_LIBRARIES})
add_definitions(-DBOOST_COROUTINES_NO_DEPRECATION_WARNING=1) # for asio

## Beast HTTP

include_directories(vendor/Beast/include/)

set(THREADS_PREFER_PTHREAD_FLAG ON)

find_package(Threads)

# Include vendored deps

include_directories(vendor)

# Sourcekit

# Try using Xcode's sourcekitd build.
# We provide this as a fallback ( and sensible default ) so that the user
# doesn't have to build SourceKit from source.
macro(TryXcodeSourceKit)
    message("Attempting to find system SourceKit")
    if (NOT ${CMAKE_SYSTEM_NAME} MATCHES "Darwin")
        message (FATAL_ERROR "SourceKit default option requires OSX.")
    endif()
    execute_process(
        COMMAND bash -c "xcode-select --print-path | tr -d '\n'"
        OUTPUT_VARIABLE XCODE_PATH
    )
    if("${XCODE_PATH}" STREQUAL "")
        message(FATAL_ERROR "Cannot find Xcode install.")
    endif()

    message("Using Sourcekit from Xcode install at: ${XCODE_PATH}.")

    set(SKT_FLAGS "-framework sourcekitd")
    set(SKT_FLAGS " ${SKT_FLAGS} -F ${XCODE_PATH}/Toolchains/XcodeDefault.xctoolchain/usr/lib")
    set(SKT_FLAGS " ${SKT_FLAGS} -rpath ${XCODE_PATH}/Toolchains/XcodeDefault.xctoolchain/usr/lib")
endmacro()

# The user can provide a SourceKit via SOURCEKIT_FLAGS
if(DEFINED ENV{SOURCEKIT_FLAGS})
    message("Using user provided SourceKit")
    set(SKT_FLAGS ENV{SOURCEKIT_FLAGS})
else()
    TryXcodeSourceKit()
endif()

set(GLOBAL_CXX_FLAGS "-std=c++1z -stdlib=libc++")
set(GLOBAL_CXX_FLAGS "${GLOBAL_CXX_FLAGS} -Wall -Wextra -Wpedantic  -Wno-unused-parameter")
set(GLOBAL_CXX_FLAGS "${GLOBAL_CXX_FLAGS} -Wno-import-preprocessor-directive-pedantic -Wno-unused-command-line-argument")

add_definitions(${GLOBAL_CXX_FLAGS})
set(CMAKE_CXX_FLAGS ${SKT_FLAGS})

add_executable(http_server
    file_body.hpp
    Logging.hpp
    SemanticHTTPServer.hpp
    SemanticHTTPServer.cpp
    SwiftCompleter.hpp
    SwiftCompleter.cpp
    HTTPServerMain.cpp
)

add_executable(test_driver
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

message("SSVIM CMAKE FINISH")
