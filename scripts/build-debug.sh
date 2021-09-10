mkdir -p Debug
cd Debug
cmake -DCMAKE_BUILD_TYPE=Debug ..
cp compile_commands.json ..
make
status=$?
[ $status -eq 0 ] || exit $status
cd ..
