#!/bin/bash
set -e
path_wt="$(pwd)/install/wt"
echo "Wt at: $path_wt"

if [[ "$OSTYPE" == "msys"* ]]; then
    path_boost="$(pwd)/build/boost_1_88_0"
    echo "Boost at: $path_boost"
fi

mkdir -p build/us.elections
pushd build/us.elections

if [[ "$OSTYPE" == "msys"* ]]; then
  cmake ../.. --fresh -G "Visual Studio 18 2026" -A x64 \
	-DWT_INCLUDE="$path_wt/include" \
	-DBOOST_INCLUDE_DIR="$path_boost/include/boost-1_88" \
	-DBOOST_LIB_DIRS="$path_boost/lib"
	
  cmake --build . --config RelWithDebInfo 
else
  cmake ../.. --fresh \
	-DWT_INCLUDE="$path_wt/include"
	
  cmake --build . --config Release -j1
  cmake --install . --config Release
fi

echo "open browser http://localhost:8080"
if [[ "$OSTYPE" == "msys"* ]]; then
./RelWithDebInfo/elections --http-address=0.0.0.0 --http-port=8080  --docroot=.
else
./elections --http-address=0.0.0.0 --http-port=8080  --docroot=.
fi

popd
popd