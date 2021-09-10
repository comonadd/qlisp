mkdir -p Release
cd Release
cmake -DCMAKE_BUILD_TYPE=Release ..
cp compile_commands.json ..
make
cd ..
