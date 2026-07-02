git submodule update --init --recursive
mkdir build
cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
# config可以支持Debug Release RelWithDebInfo MinSizeRel
cmake --build . --target install --config MinSizeRel


