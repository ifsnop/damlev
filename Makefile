
ARCH := $(shell getconf LONG_BIT)

CPP_FLAGS_32 := -m32
CPP_FLAGS_64 := -m64

CPP_FLAGS := $(CPP_FLAGS_$(ARCH)) -Wall \
	-I/usr/local/include/mysql \
	-L/usr/lib/mysql \
	-L/usr/local/lib/mysql \
	-I/usr/include/mysql \
	-lmysqlclient \
	-DMYSQL_DYNAMIC_PLUGIN

CC=g++

DESTDIR = $(shell mysql_config --plugindir)
TARGET = mysqldamlev.so
SOURCES = $(shell find . -name "dam*.cpp")

all: mysqldamlev

debug: CPP_FLAGS += -DDEBUG -g
debug: $(SOURCES)
	$(CC) $(CPP_FLAGS) -o bin/$(TARGET) $(SOURCES)

mysqldamlev: $(SOURCES)
	$(CC) $(CPP_FLAGS) -pipe -O2 -shared -o lib/$(TARGET) $(SOURCES)

install: mysqldamlev
	install lib/$(TARGET) $(DESTDIR)

clean:
	-rm lib/$(TARGET)
	-rm $(DESTDIR)/$(TARGET)


#!/bin/bash
# -DDEBUG -g
#echo building mysqldamlevlim256_32
#g++ -m32 -Wall                          -I/usr/local/include/mysql -O -pipe -o lib/mysqldamlevlim256u32.so -shared -L/usr/lib/mysql -L/usr/local/lib/mysql -I/usr/include/mysql -lmysqlclient src/damlevlim256u.cpp
#echo building mysqldamlevlim256_64
#g++ -O -m64 -Wall -fPIC -L/usr/lib64/mysql -I/usr/local/include/mysql -pipe -o lib/mysqldamlevlim256u64.so -shared -L/usr/local/lib64/mysql -I/usr/include/mysql -lmysqlclient src/damlevlim256u.cpp
#cp mysqldamlevlim256.so /usr/lib
#cp mysqldamlevlim256u*.so /usr/lib/mysql/plugin
#some tests, not needed
#g++ -o test test.cpp
#gcc -m32 -o test2 test2.c
