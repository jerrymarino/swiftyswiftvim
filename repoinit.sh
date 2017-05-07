#!/bin/bash
mkdir -p vendor/
cd vendor/

# Beast
curl -Lo BeastDL https://github.com/vinniefalco/Beast/archive/6d5547a32c50ec95832c4779311502555ab0ee1f.zip
unzip BeastDL
mkdir -p beast
ditto Beast-6d5547a32c50ec95832c4779311502555ab0ee1f/ beast/
rm -rf BeastDL
rm -rf Beast-6d5547a32c50ec95832c4779311502555ab0ee1f

