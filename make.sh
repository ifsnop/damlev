
#http://www.oreillynet.com/onlamp/blog/2006/01/turning_mysql_data_in_latin1_t.html

#DROP FUNCTION mysql.damlevlim256;
#DROP FUNCTION mysql.damlevlim256u;
#CREATE FUNCTION damlevlim256 RETURNS INT SONAME 'mysqldamlevlim256.so';
#CREATE FUNCTION damlevlim256u RETURNS INT SONAME 'mysqldamlevlim256u.so';

#USE MySQL CHAR_LENGTH TO FIND ROWS WITH MULTI-BYTE CHARACTERS:
#SELECT name FROM clients WHERE LENGTH(name) != CHAR_LENGTH(name);

#USE MySQL HEX and PHP bin2hex
#SELECT name, HEX(name) FROM clients;
#Get the result back into PHP, and run a bin2hex on the string, compare it to MySQL’s hex of that same string

# SHOW VARIABLES LIKE 'plugin_dir'
# in my.cnf: #plugin_dir = /usr/lib/mysql/plugin


# -DDEBUG
g++ -m32 -Wall                          -I/usr/local/include/mysql -O -pipe -o lib/mysqldamlevlim256u32.so -shared -L/usr/lib/mysql -L/usr/local/lib/mysql -I/usr/include/mysql -lmysqlclient src/damlevlim256u.cpp
g++ -m64 -Wall -fPIC -L/usr/lib64/mysql -I/usr/local/include/mysql -O -pipe -o lib/mysqldamlevlim256u64.so -shared -L/usr/local/lib64/mysql -I/usr/include/mysql -lmysqlclient src/damlevlim256u.cpp
#cp mysqldamlevlim256.so /usr/lib
#cp mysqldamlevlim256u*.so /usr/lib/mysql/plugin

#g++ -o test test.cpp
#gcc -m32 -o test2 test2.c
