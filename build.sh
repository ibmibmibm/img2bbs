#!/bin/bash

[ -d build ] || mkdir build
cd build
cmake .. && make -j && cp -fv src/img2bbs ..
