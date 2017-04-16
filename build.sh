SRCROOT=$PWD

mkdir -p build
cd build

# Build out core library
clang++ -std=c++11 \
-F `xcode-select --print-path`/Toolchains/XcodeDefault.xctoolchain/usr/lib/ \
-c \
$SRCROOT/SwiftCompleter.cpp  

clang++ -shared  -F `xcode-select --print-path`/Toolchains/XcodeDefault.xctoolchain/usr/lib/ \
-framework sourcekitd \
-rpath `xcode-select --print-path`/Toolchains/XcodeDefault.xctoolchain/usr/lib \
-o Utils.so SwiftCompleter.o

# Build python interface
PYTHON_VERSION=2.7
PYTHON_INTERFACE=PythonExtensions
BOOST_INC=/usr/local/include
BOOST_LIB=/usr/local/lib

clang++ \
-std=c++11 \
-isystem /usr/local/Frameworks \
-F /usr/local/Frameworks \
-I/usr/local/Frameworks/Python.framework/Headers/ \
-I $BOOST_INC -fPIC \
-c $SRCROOT/$PYTHON_INTERFACE.cpp 

clang++ -shared \
-L$BOOST_LIB \
-isystem /usr/local/Frameworks \
-F /usr/local/Frameworks \
-framework Python \
-I/usr/local/Frameworks/Python.framework/Headers/ \
-lboost_python \
-lpython$PYTHON_VERSION \
-install_name $PYTHON_INTERFACE.so \
-o swiftvi.so \
$PYTHON_INTERFACE.o Utils.so

# Python code includes build
touch __init__.py

