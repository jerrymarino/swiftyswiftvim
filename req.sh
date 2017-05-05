#!/bin/bash
contents()
{
src="// \n\
//  some_swift.swift \n\
//  Swift Completer \n\
// \n\
//  Created by Jerry Marino on 4/30/16. \n\
//  Copyright © 2016 Jerry Marino. All rights reserved. \n\
// \n\
 \n\
 \n\
    func someOtherFunc(){ \n\
    } \n\
 \n\
    func anotherFunction(){ \n\
    someOther()\n\
    } \n\
\n"
echo '{"line":0,"column":0,"file_name":"'$PWD'/Examples/some_swift.swift", "contents": "'$src'",  "flags": ["-sdk", "/Applications/Xcode.app/Contents/Developer/Platforms/MacOSX.platform/Developer/SDKs/MacOSX.sdk", "-target", "x86_64-apple-macosx10.12" ]}'
}

# Manually test /completions or /diagnostics
# TODO: write some integration tests
curl -i \
-H "Content-Type: application/json" \
-X POST \
-d "$(contents)" "http://0.0.0.0:8080/diagnostics"
