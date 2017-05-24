#!/bin/bash
# Update the boost checkout to a minimal set of files to do
# fast builds.
#
# It's hardcoded for a version, so just change this or add variables in the
# future ;)
# This requires ( On OSX `brew install boost-bcp` )

set -e

rm -rf vendor/boost
mkdir -p vendor/boost

SSVIM_ROOT=$PWD

FETCH_ROOT=`mktemp -d`
cd $FETCH_ROOT

curl -LOk https://sourceforge.net/projects/boost/files/boost/1.64.0/boost_1_64_0.zip/download

unzip download > /dev/null

BOOST_TARGET_ROOT=$SSVIM_ROOT/vendor/boost

# Copy out packages.
# This unzips to boost_1_64_0
BOOST_ARCHIVE=$PWD/boost_1_64_0
bcp coroutine \
    context \
    config \
    filesystem \
    program_options \
    system \
    thread \
    utility \
    intrusive \
    property_tree \
    algorithm \
    --boost=$BOOST_ARCHIVE $BOOST_TARGET_ROOT > /dev/null

# Build system files need to be imported
cp -r $BOOST_ARCHIVE/tools $BOOST_TARGET_ROOT/tools

ditto $BOOST_ARCHIVE/bootstrap.sh $BOOST_TARGET_ROOT
ditto $BOOST_ARCHIVE/Jamroot $BOOST_TARGET_ROOT
ditto $BOOST_ARCHIVE/boostcpp.jam $BOOST_TARGET_ROOT
ditto $BOOST_ARCHIVE/boost-build.jam $BOOST_TARGET_ROOT

# This seems needed. Handle other platforms in the future when they
# are ready.
echo 'using darwin : : ;\n' > $BOOST_TARGET_ROOT/user-config.jam

cd $BOOST_TARGET_ROOT

# Blacklist directories
for X in $(find $BOOST_TARGET_ROOT -type d -name "test" \
    -or -type d -name "doc" \
    -or -type d -name "boostbook" \
    -or -type d -name "example" ); do
    rm -rf $X
done

# Blacklist known that are interspersed with source code
for X in $(find $BOOST_TARGET_ROOT -type f -name "*.html" \
    -or -type d -name "*.css" \
    -or -type d -name "*.png" \
    -or -type d -name "*.js" ); do
    rm -rf $X
done

