clang++ -std=c++11 \
-F `xcode-select --print-path`/Toolchains/XcodeDefault.xctoolchain/usr/lib/ \
-framework sourcekitd \
-rpath `xcode-select --print-path`/Toolchains/XcodeDefault.xctoolchain/usr/lib \
SwiftCompleter.cpp  \
-o \
SwiftCompleter
