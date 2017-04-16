#!/usr/bin/env python

import swiftvi

runner = swiftvi.Runner()
fileName = "/Users/jerry/Projects/swiftvi/Examples/some_swift.swift"
contents = str(open(fileName).read())
runner.setFile(fileName, contents, 0, 0)

print (runner.run())
