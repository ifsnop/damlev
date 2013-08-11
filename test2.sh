#make debug && valgrind --track-origins=yes --leak-check=full --leak-resolution=high bin/mysqldamlevlim.so

echo "running performance test"

for run in {1..30};
do
echo -n .
mysql warez -e "SELECT SQL_NO_CACHE now(),id,word,damlevlim(word,'hataraxiada',12) AS cuenta,score FROM lxk_operational WHERE damlevlim(word,'hataraxiada',12)<=4 ORDER BY cuenta, score desc LIMIT 10;" > /dev/null
done

echo
echo "old     0m34.461s"
echo "current 0m16.705s"

# profiling code
#valgrind --tool=callgrind bin/mysqldamlev.so
#callgrind_annotate --inclusive=yes --tree=both --auto=yes callgrind.out.916
