make debug && valgrind --track-origins=yes --leak-check=full --leak-resolution=high bin/mysqldamlevlim.so

# profiling code
#valgrind --tool=callgrind bin/mysqldamlev.so
#callgrind_annotate --inclusive=yes --tree=both --auto=yes callgrind.out.916
