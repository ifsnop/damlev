
ARCH := $(shell getconf LONG_BIT)

CPP_FLAGS_32 := -m32
CPP_FLAGS_64 := -m64

CPP_FLAGS := $(CPP_FLAGS_$(ARCH)) -Wall \
	-I/usr/local/include/mysql \
	-L/usr/lib/mysql \
	-L/usr/local/lib/mysql \
	-I/usr/include/mysql \
	-lmysqlclient \
	-DMYSQL_DYNAMIC_PLUGIN \
	-DDEBUG_MYSQL=1

CC=gcc

DESTDIR = $(shell mysql_config --plugindir)
TARGET = mysqldamlevlim.so
SOURCES = $(shell find . -name "dam*.c")

all: mysqldamlevlim

debug: CPP_FLAGS += -DDEBUG -g
debug: $(SOURCES)
	$(CC) $(CPP_FLAGS) -o bin/$(TARGET) $(SOURCES)

mysqldamlevlim: $(SOURCES)
	$(CC) $(CPP_FLAGS) -pipe -O3 -shared -o lib/$(TARGET) $(SOURCES)

install: mysqldamlevlim
	install lib/$(TARGET) $(DESTDIR)

clean:
	-rm lib/$(TARGET)
	-rm $(DESTDIR)/$(TARGET)
