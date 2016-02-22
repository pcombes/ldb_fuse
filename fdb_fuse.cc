#define FUSE_USE_VERSION 26

#include <stdio.h>
#include <string>
#include <iostream>
#include <fuse.h>
#include <errno.h>
#include <fcntl.h>
#include <sstream>
#include <iomanip>
#include <list>
#include <iosfwd>
#include "leveldb/db.h"
#include "leveldb/env.h"
#include "leveldb/slice.h"

using namespace std;
using std::cout;

struct patrick {
	int somenumber;
	int somevalue;
	const char *somestr;
	int somethird;
};

      
static leveldb::DB* fuse_db;
//std::string read_buffer;
//file metadata will use a # and data a ^
static const char *logfile = "/tmp/fdb.log";
static const char *rootpath = "/";
static const char *fakepath="#/firstfile";
static const char *fakepath1="#/different";

int db_write(const char* key, const leveldb::Slice value);
static int db_read(const char* key, string *read_buffer);
static int db_read_iter(const char *key, list<string>* rbuffers);
static void log_write(const char *format, ...);
bool conv_fromByteString(istream &inputstream, unsigned char *outchar, int size);
bool conv_toByteString(ostream &outstream, const unsigned char *inchar, int size);


static int fdb_getattr(const char *path, struct stat *stbuf) {

    int res=0;
    std::string read_buffer;
    leveldb::Slice slicebuf;
    char *data;
    struct stat newst;
    

    log_write("entered getattr for %s\n", path);
    memset(stbuf, 0, sizeof(struct stat));
    memset(&newst, 0, sizeof(struct stat));


    if (strcmp(path, "/") == 0) {
		stbuf->st_mode = S_IFDIR | 0750;
		stbuf->st_nlink = 2;
		return res;
	} 
    else {
      //log_write("Calling db_read with path: %s \n", path);
      string tmppath = path;
      tmppath = "#" + tmppath;
      log_write("Calling db_read with path: %s \n", tmppath.c_str());
      res = db_read(tmppath.c_str(), &read_buffer);
      if (res == 1) {
	return -ENOENT;
      }
      istringstream iss(read_buffer); 
      conv_fromByteString(iss, reinterpret_cast<unsigned char*>(&newst), sizeof(newst)); 
	stbuf->st_mode = newst.st_mode;
	stbuf->st_nlink = newst.st_nlink;
        stbuf->st_size = strlen(fakepath);
    }
   
   return res;
}

static int fdb_readdir(const char *path, void *buffer, fuse_fill_dir_t fill, off_t offset, struct fuse_file_info *fi) {

    int res=0;
    string read_buffer;
    leveldb::Slice slicebuf;
    char *data;
    list<string> rbuffers;
    list<string>::iterator it;

    log_write("Entered fdb_readdir with path: %s anything\n", path);

    if (strcmp(path, "/") != 0)
       return -ENOENT;

    fill(buffer, ".", NULL, 0);
    fill(buffer, "..", NULL, 0);

    db_read_iter(path, &rbuffers);
    for (it=rbuffers.begin(); it != rbuffers.end(); ++it) {
        read_buffer = *it;
        read_buffer.erase(read_buffer.begin());
        read_buffer.erase(read_buffer.begin());
	log_write("Got this back from iter: %s\n", read_buffer.c_str());
    	fill(buffer, read_buffer.c_str(), NULL, 0);
    }


    log_write("Leaving fdb_readdir\n");
    return res;
    
}
static int fdb_open(const char *path, struct fuse_file_info *fi) {
 return 0;
}

static int fdb_close() {
 return 0;
}

static int fdb_mkdir(const char *path, mode_t mode) {
 
  struct stat stbuf;
  ostringstream oss;
  string tmppath;

  log_write("Entered mkdir for path: %s\n", path);

  stbuf.st_mode = S_IFDIR | 0404;
  stbuf.st_nlink = 2;
  stbuf.st_size = strlen(fakepath);

  conv_toByteString(oss, reinterpret_cast<const unsigned char*>(&stbuf), sizeof(stbuf));
  char b[sizeof(oss)];
  memcpy(b, &oss, sizeof(oss));
  
  tmppath = path;
  tmppath = "#" + tmppath; 

  log_write("Writing %s\n", tmppath.c_str());

  db_write(tmppath.c_str(), oss.str());
  log_write("Leaving mkdir\n");  
  
  return 0;
}

static int fdb_create() {
 
 return 0;
}

static int fdb_read(const char *path, char *buffer, size_t length, off_t offset, struct fuse_file_info *fi) {
 return 0;
}

int db_write(const char* key, const leveldb::Slice value) {

   leveldb::Status s = fuse_db->Put(leveldb::WriteOptions(), key, value);
   if(s.ok()) 
    return 0;
   else
    return 1;
}

static int db_read(const char* key, string *read_buffer) {

//   leveldb::DB* fuse_db1;
  //leveldb::Options options;
  //leveldb::Status status;
 //status = leveldb::DB::Open(options, "/tmp/fdb", &fuse_db1);
  // struct stat stbuf;


   log_write("Entered db_read with key: %s\n", key);

   //string read_buffer;
   leveldb::Status s = fuse_db->Get(leveldb::ReadOptions(), key, read_buffer);
   //memcpy(&stbuf, read_buffer.data(), sizeof(read_buffer.data()));
   //memcpy(&later_pbuf, read_buffer.data(), sizeof(read_buffer.data()));

   //log_write("About to memcopy\n");
   //slicebuf = (char *) malloc(sizeof(read_buffer.data()));
   //memcpy(slicebuf, read_buffer.data(), sizeof(read_buffer.data()));
   
   //istringstream iss(*read_buffer); 
   //conv_fromByteString(iss, reinterpret_cast<unsigned char*>(&stbuf), sizeof(stbuf)); 
   //if (stbuf.st_nlink == 1) {
//	log_write("stbuf good from db_read\n");
 //   }


   log_write("leaving db_read\n");



//   delete fuse_db1;
 if(s.ok()) 
    return 0;
   else
    return 1;

}

static int db_read_iter(const char *key, list<string>* rbuffers) {

  log_write("Entered db_read_iter\n"); 
  leveldb::Iterator* it = fuse_db->NewIterator(leveldb::ReadOptions());

  for (it->SeekToFirst(); it->Valid(); it->Next()) {
        //log_write("Found key: %s\n", it->key().ToString().data());
	rbuffers->push_front(it->key().ToString());
   }

  assert(it->status().ok());
  delete it;
   
  log_write("Leaving db_read_iter\n"); 
  return 0; 
}


static bool init_log() {

  FILE *log = fopen(logfile, "w");

  if(log == NULL)
    return false;

  fprintf(log, "Started log\n");

  fclose(log);

  return true;
}

static void log_write(const char *format, ...) {

  FILE *log = fopen(logfile, "a");

  va_list arguments;
  va_start(arguments, format);

  vfprintf(log, format, arguments);

 
  fclose(log);
}

 

static int init_operations(fuse_operations& operations) { 
   //Set everything to NULL, prevents non-initialized ops from being active
   memset(&operations, 0, sizeof(fuse_operations));

   operations.open = fdb_open;
   operations.read = fdb_read;
   operations.getattr = fdb_getattr;
   operations.readdir = fdb_readdir;
   operations.mkdir = fdb_mkdir;
}

bool conv_toByteString(ostream &outstream, const unsigned char *inchar, int size) {

      ostream::fmtflags oflags = outstream.flags();
      char ofill = outstream.fill();

      outstream << hex << setfill('0');

      for(int i = 0; size > i; ++i) {
         if(0 < i)
            outstream << ' ';
         outstream << "0x" << setw(2) << static_cast<int>(inchar[i]);
      }

      outstream.flags(oflags);
      outstream.fill(ofill);

      return true;
}

bool conv_fromByteString(istream &inputstream, unsigned char *outchar, int size) {

     bool ret = true;
     string line;
     unsigned int ubite = 0;
     int i = 0;

     log_write("conv_fromByte running\n");
     getline(inputstream, line);
 
     istringstream iss_convert(line);
     iss_convert >> hex;

     while((iss_convert >> ubite) && (i < size)) {
        if((0x00 <= ubite) && (0xff >= ubite))
	   outchar[i++] = static_cast<unsigned char>(ubite);
        else
	   return false;
      }

     log_write("conv_fromByte exiting\n");
     return true;
}
      
  

int main(int argc, char** argv) {

  leveldb::Options options;
  leveldb::Status status;
  static fuse_operations operations;
  struct stat stbuf; 
  ostringstream oss;


  options.create_if_missing = true;
 
  if(argc > 20){
   printf("Usage: %s <path to LevelDB>\n", argv[0]);
   return 1;
  }
 
  status = leveldb::DB::Open(options, "/tmp/fdb1", &fuse_db);

  stbuf.st_mode = S_IFREG | 0444;
  stbuf.st_nlink = 1;
  stbuf.st_size = strlen(fakepath);
 
  conv_toByteString(oss, reinterpret_cast<const unsigned char*>(&stbuf), sizeof(stbuf)); 
  //char b[sizeof(stbuf)];
  //memcpy(b, &stbuf, sizeof(stbuf));   
  char b[sizeof(oss)];
  memcpy(b, &oss, sizeof(oss));   


  db_write(fakepath, oss.str());
  db_write(fakepath1, oss.str());
  //db_write("0firstfile", oss);


  std::string somereturn;
// db_read("/my/path", &sombuf);

 

 //delete fuse_db;
 //
  init_log(); 
  init_operations(operations);
  log_write("Operations initialized\n"); 

  return fuse_main(argc, argv, &operations, NULL);
}
 
