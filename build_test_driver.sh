SRCROOT=$PWD
set -e

mkdir -p build
cd build

# Build out core library
clang++ -std=c++11 \
-F `xcode-select --print-path`/Toolchains/XcodeDefault.xctoolchain/usr/lib/ \
-c \
$SRCROOT/SwiftCompleter.cpp  

# Build python interface
BOOST_INC=/usr/local/include
BOOST_LIB=/usr/local/lib

# TODO: Move to CMake
echo "Driver"
clang++ \
-std=c++11 \
-isystem /usr/local/Frameworks \
-F /usr/local/Frameworks \
-I/usr/local/Frameworks/Python.framework/Headers/ \
-I $BOOST_INC -fPIC \
-c $SRCROOT/Driver.cpp 

echo "App"
clang++ \
-L$BOOST_LIB \
-framework sourcekitd \
-F `xcode-select --print-path`/Toolchains/XcodeDefault.xctoolchain/usr/lib/ \
-rpath `xcode-select --print-path`/Toolchains/XcodeDefault.xctoolchain/usr/lib \
-isystem /usr/local/Frameworks \
-F /usr/local/Frameworks \
-o swiftvi \
SwiftCompleter.o Driver.o

