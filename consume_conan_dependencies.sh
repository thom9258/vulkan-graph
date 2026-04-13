#!/bin/bash

[[ ! -d build ]] && mkdir build
conan install . --output-folder=build --build=missing -pr=conanprofile.txt
cd build ; cmake .. -DCMAKE_TOOLCHAIN_FILE=build/Release/generators/conan_toolchain.cmake -DCMAKE_BUILD_TYPE=Release
