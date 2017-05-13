#!/bin/bash
set -e

EXPECTED_BOOST=/usr/local/include/boost/
if [[ ! -d $EXPECTED_BOOST ]]; then
    echo "Missing Boost $EXPECTED_BOOST. \
        trying $ brew install boost 1.64"
    brew install boost@1.64
fi

