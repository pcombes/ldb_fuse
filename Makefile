CC=gcc
LEVELDB=/home/dumb/src/leveldb
INCLUDE += $(LEVELDB)/include
LIBRARIES += $(LEVELDB)/out-shared
LINK=-lleveldb -lfuse
CXXFLAGS += -I $(INCLUDE) -L $(LIBRARIES) $(LINK) -D_FILE_OFFSET_BITS=64 -std=c++11

$(SOURCES:.cc=.o)

fdb_fuse := $(CC) $(CXXFLAGS) fdb_fuse.o -o $@


all: fdb_fuse
