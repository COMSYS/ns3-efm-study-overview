#! /bin/bash
set -e

parent_path=$( cd "$(dirname "${BASH_SOURCE[0]}")" ; pwd -P )

result_proc_path="$parent_path/../raw-result-processing/"

cd "$result_proc_path/external/simdjson"
curl -L -o simdjson.cpp https://github.com/simdjson/simdjson/releases/download/v3.1.7/simdjson.cpp
curl -L -o simdjson.h https://github.com/simdjson/simdjson/releases/download/v3.1.7/simdjson.h


cd "$result_proc_path/external/"

curl -L -o alglib-source.zip https://www.alglib.net/translator/re/alglib-4.00.0.cpp.gpl.zip
unzip alglib-source.zip
mv alglib-cpp alglib
rm alglib-source.zip

echo "add_library(alglib
            src/solvers.cpp
            src/linalg.cpp
            src/interpolation.cpp
            src/alglibinternal.cpp
            src/alglibmisc.cpp
            src/ap.cpp
            )
target_include_directories(alglib PUBLIC \${CMAKE_CURRENT_SOURCE_DIR})" > "alglib/CMakeLists.txt"




cd $result_proc_path

mkdir -p build/Release

cd build/Release

cmake -DCMAKE_BUILD_TYPE=Release ../..

cmake --build .
