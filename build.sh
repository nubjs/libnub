#!/bin/bash

SRC_DIR="$( cd "$( dirname "$0" )" && pwd )"
GENERATOR=${GENERATOR:-make}

git submodule init
git submodule update

./tools/gyp/gyp --depth=$SRC_DIR -Duv_library=static_library -Icommon.gypi \
  --generator-output=$SRC_DIR/out/ -Goutput_dir=$SRC_DIR/out -f $GENERATOR nub.gyp
