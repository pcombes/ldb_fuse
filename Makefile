CC=gcc
LEVELDB=/home/dumb/src/leveldb
INCLUDE += $(LEVELDB)/include
LIBRARIES += $(LEVELDB)/out-shared
LIBS=-lstdc++ -lleveldb -lfuse
CXXFLAGS += -I $(INCLUDE) -D_FILE_OFFSET_BITS=64 -std=c++11 -g
LDFLAGS += -L $(LIBRARIES) $(LIBS) 
SOURCES=base64.cc fdb_fuse.cc
OBJECTS=$(SOURCES:.cc=.o)

# fdb_fuse := $(CC) $(CXXFLAGS) -o $@
fdb_fuse: $(OBJECTS)
	$(CC) $(CXXFLAGS) $(LDFLAGS) $(OBJECTS) -o $@

.cc.o: 
	$(CC) $(CXXFLAGS) -c  $< -o $@


all: $(SOURCES) fdb_fuse
