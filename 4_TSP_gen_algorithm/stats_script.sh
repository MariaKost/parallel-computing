for population in $(seq 10000 10000 100000)
do
(./run 6 $population 10 --file graph100.txt | awk '{print $2, $4, $6, $8}' && head -1 stats.txt) | python3 plots_create.py;
done
