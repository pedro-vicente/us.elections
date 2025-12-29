#!/bin/bash
set -e
remote=$(git config --get remote.origin.url)
echo "remote repository: $remote"
if [ ! -d "ext/wt-4.12.1" ]; then
    git -c advice.detachedHead=false clone -b 4.12.1 https://github.com/emweb/wt.git ext/wt-4.12.1
    rm -rf build/wt-4.12.1
else
    echo "ext/wt-4.12.1 already exists, skipping clone"
fi

sleep 2
mkdir -p build/wt-4.12.1
pushd build
pushd wt-4.12.1

echo "At: $(pwd)"

if [[ "$OSTYPE" == "msys" ]]; then

path_boost="../boost_1_88_0"
echo "Boost at: $path_boost"

cmake ../../ext/wt-4.12.1  \
 -DCMAKE_INSTALL_PREFIX="$(pwd)/../../install/wt" \
 -DBOOST_PREFIX="$path_boost" \
 -DBUILD_EXAMPLES=OFF \
 -DENABLE_QT5=OFF -DENABLE_QT6=OFF 
cmake --build .  --config RelWithDebInfo --parallel 
cmake --install . --config RelWithDebInfo

elif [[ "$OSTYPE" == "darwin"* ]]; then

cmake ../../ext/wt-4.12.1 \
 -DCMAKE_INSTALL_PREFIX="$(pwd)/../../install/wt" \
 -DBUILD_EXAMPLES=OFF \
 -DCMAKE_CXX_FLAGS="-Wno-deprecated -Wno-deprecated-declarations -Wno-deprecated-copy" \
 -DCMAKE_BUILD_TYPE=Release \
 -DBUILD_SHARED_LIBS=OFF \
 -DCMAKE_FIND_LIBRARY_SUFFIXES=".a" \
 -DBoost_USE_STATIC_LIBS=ON 
 
cmake --build . --config Release --parallel 
cmake --install . --config Release

elif [[ "$OSTYPE" == "linux-gnu"* ]]; then

cmake ../../ext/wt-4.12.1  \
 -DCMAKE_INSTALL_PREFIX="$(pwd)/../../install/wt" \
 -DBUILD_EXAMPLES=OFF \
 -DCMAKE_BUILD_TYPE=Release \
 -DOPENSSL_ROOT_DIR=/usr \
 -DOPENSSL_CRYPTO_LIBRARY=/usr/lib/x86_64-linux-gnu/libcrypto.so \
 -DOPENSSL_SSL_LIBRARY=/usr/lib/x86_64-linux-gnu/libssl.so \
 -DENABLE_SSL:BOOL=ON

cmake --build . --config Release -j1
cmake --install . --config Release

fi

popd 
popd