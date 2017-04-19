#!/usr/bin/env python

import build.swiftvi as swiftvi


def run(col, line):
    flags = swiftvi.StringList()
    flags.append("-sdk")
    flags.append(
        "/Applications/Xcode.app/Contents/Developer/Platforms/MacOSX.platform/Developer/SDKs/MacOSX.sdk")
    flags.append("-target")
    flags.append("x86_64-apple-macosx10.12")

    runner = swiftvi.Runner()
    fileName = "/Users/jerry/Projects/swiftvi/Examples/some_swift.swift"
    contents = str(open(fileName).read())
    print(runner.complete(fileName, contents, flags, col, line))


run(19, 15)
