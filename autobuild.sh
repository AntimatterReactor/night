mkdir -v -p build/{release,debug}
cmake -B build/debug -S . -G "Unix Makefiles" -DCMAKE_BUILD_TYPE=Debug
cmake -B build/release -S . -G "Unix Makefiles" -DCMAKE_BUILD_TYPE=Release
make -j4 -C build/debug
make -j4 -C build/release
