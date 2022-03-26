mkdir -v -p build/{release,debug}
cmake -B build/debug -S . -G "Unix Makefiles" -DCMAKE_BUILD_TYPE=Debug \
-DCMAKE_CXX_COMPILER=g++
cmake -B build/release -S . -G "Unix Makefiles" -DCMAKE_BUILD_TYPE=Release \
-DCMAKE_CXX_COMPILER=g++
make -j4 -C build/debug
make -j4 -C build/release
