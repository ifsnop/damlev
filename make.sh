#!/bin/bash

# -DDEBUG -g
echo building mysqldamlevlim256_32
g++ -m32 -Wall                          -I/usr/local/include/mysql -O -pipe -o lib/mysqldamlevlim256u32.so -shared -L/usr/lib/mysql -L/usr/local/lib/mysql -I/usr/include/mysql -lmysqlclient src/damlevlim256u.cpp

echo building mysqldamlevlim256_64
g++ -O -m64 -Wall -fPIC -L/usr/lib64/mysql -I/usr/local/include/mysql -pipe -o lib/mysqldamlevlim256u64.so -shared -L/usr/local/lib64/mysql -I/usr/include/mysql -lmysqlclient src/damlevlim256u.cpp
#cp mysqldamlevlim256.so /usr/lib
#cp mysqldamlevlim256u*.so /usr/lib/mysql/plugin


#some tests, not needed
#g++ -o test test.cpp
#gcc -m32 -o test2 test2.c
