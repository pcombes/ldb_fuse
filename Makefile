CC=gcc
LEVELDB=/usr/local/src/leveldb
INCLUDE += $(LEVELDB)/include
LIBRARIES += $(LEVELDB)/out-shared
LINK=-lleveldb -lfuse
CXXFLAGS += -I $(INCLUDE) -L $(LIBRARIES) $(LINK) -D_FILE_OFFSET_BITS=64 -std=c++11 -g

$(SOURCES:.cc=.o)

fdb_fuse := $(CC) $(CXXFLAGS) fdb_fuse.o -o $@


all: fdb_fuse
