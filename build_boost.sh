#!/bin/bash
# Do a build of boost using the boost infra.
# Libs go into build/boost/lib
set -e

DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"

SSVIM_BUILD_DIR=$DIR/build

# Headers to /vendor/boost/include
# Libs to /vendor/boost/lib
BOOST_PREFIX=$SSVIM_BUILD_DIR/vendor/boost/
BOOST_LIB_DIR=$SSVIM_BUILD_DIR/vendor/boost/lib/

if [[ -d $BOOST_LIB_DIR ]] && [[ -z $CI ]]; then
    echo "Boost probably is built, skipping"
    exit 0
fi

mkdir -p $BOOST_LIB_DIR

# Wants to build from here.
cd $DIR/vendor/boost
./bootstrap.sh --prefix=$BOOST_PREFIX \
    --libdir=$BOOST_LIB_DIR --without-icu

./b2 --prefix=$BOOST_PREFIX \
    --libdir=$BOOST_LIB_DIR \
    --user-config=user-config.jam \
    -d2 \
    -j4 \
    --layout-tagged install \
    threading=multi \
    link=static \
    cxxflags=-stdlib=libc++ linkflags=-stdlib=libc++ 

