#!/bin/bash
set -e
if [ ! -d "ext/duckdb-1.4.3" ]; then
  git clone --depth 1 --branch v1.4.3 https://github.com/duckdb/duckdb.git ext/duckdb-1.4.3
fi
mkdir -p build/duckdb-1.4.3
pushd build
pushd duckdb-1.4.3

DUCKDB_OPTS="\
-DBUILD_SHARED_LIBS=OFF \
-DBUILD_UNITTESTS=OFF \
-DBUILD_SHELL=OFF \
-DBUILD_EXTENSIONS='' \
-DENABLE_SANITIZER=OFF \
-DENABLE_UBSAN=OFF \
-DBUILD_BENCHMARKS=OFF"

if [[ "$OSTYPE" == "msys" ]]; then

cmake ../../ext/duckdb-1.4.3 -G "Visual Studio 18 2026" -A x64 $DUCKDB_OPTS
cmake --build . --config RelWithDebInfo --target duckdb_static

elif [[ "$OSTYPE" == "darwin"* ]]; then

cmake ../../ext/duckdb-1.4.3 $DUCKDB_OPTS
cmake --build . --config Release --parallel --target duckdb_static

elif [[ "$OSTYPE" == "linux-gnu"* ]]; then

cmake ../../ext/duckdb-1.4.3 $DUCKDB_OPTS
cmake --build . --config Release -j1 --target duckdb_static

fi

popd
popd