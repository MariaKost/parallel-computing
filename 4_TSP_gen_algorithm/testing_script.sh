make
./run 2 12 25 --file graph1000.txt | tail -3
echo ""
./run 2 12 25 --generate 1000 | tail -3
make clean
