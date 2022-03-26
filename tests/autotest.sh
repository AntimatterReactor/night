echo "Testing Debug"
cd ..
bash autobuild.sh
build/debug/night tests/test.night &> tests/dump_release.txt
build/release/night tests/test.night &> tests/dump_debug.txt

echo "Differences between release and debug:"
diff tests/dump_release.txt tests/dump_debug.txt

echo "Differences between release and expected:"
diff tests/dump_release.txt tests/release_expected.txt

echo "Differences between debug and expected:"
diff tests/dump_debug.txt tests/debug_expected.txt
