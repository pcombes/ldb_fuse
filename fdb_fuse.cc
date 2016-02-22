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
#include "fdb_fuse.h"


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
      string tmppath = path;
      tmppath = "#" + tmppath;
      log_write("Calling db_read with path: %s \n", tmppath.c_str());
      res = db_read(tmppath.c_str(), &read_buffer);
      if (res == 1) {
	return -ENOENT;
      }
      istringstream iss(read_buffer); 
      conv_fromByteString(iss, reinterpret_cast<unsigned char*>(&newst), sizeof(newst)); 
//	stbuf->st_mode = newst.st_mode;
//	stbuf->st_nlink = newst.st_nlink;
 //       stbuf->st_size = newst.st_size;
        memcpy(stbuf, &newst, sizeof(newst));
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
    string tmppath;

    log_write("Entered fdb_readdir with path: %s \n", path);

    if (strcmp(path, "/") != 0)
       return -ENOENT;

    fill(buffer, ".", NULL, 0);
    fill(buffer, "..", NULL, 0);

    tmppath = path;
    tmppath = "#" + tmppath;
    db_read_iter(tmppath.c_str(), &rbuffers);
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

//Begin the FUSE functions
static int fdb_open(const char *path, struct fuse_file_info *fi) {

  int res = 0;
  string tmppath = path;
  string readbuf;
  struct stat stbuf;

  tmppath = "#" + tmppath;

  log_write("Entered fdb_open\n");

  res = db_read(tmppath.c_str(), &readbuf); 

  if(res == 1) {
	log_write("fdb_open returns no entry\n");
	return -ENOENT;
  }

  istringstream iss(readbuf); 
  conv_fromByteString(iss, reinterpret_cast<unsigned char*>(&stbuf), sizeof(stbuf)); 
  fi->fh = stbuf.st_size;
 
  return res;

}

static int fdb_close() {
 return 0;
}

static int fdb_write(const char *path, const char *buffer, size_t size, off_t offset, struct fuse_file_info *fi) {
  
  string tmppath = path;
  string metapath = path;
  tmppath = "^" + tmppath;
  metapath = "#" + metapath;

  string write_data = buffer;
  string emptybuf;
  string tmpbuf;

  log_write("Entered fdb_write for path: %s\n", path);

  if((db_read(tmppath.c_str(), &tmpbuf)) == 1) {
	log_write("no such path %s\n", metapath.c_str());
	return -ENOENT;
   }

  if((db_delete(tmppath.c_str())) == 1) {
	log_write("fdb_write Didn't delete\n");
	return -ENOENT;
  }

  tmpbuf.insert(offset, write_data);
  log_write("Writing to %s\n", tmppath.c_str());
  log_write("With data: %s\n", tmpbuf.c_str());

  db_write(tmppath.c_str(), tmpbuf);

  return write_data.size();
}

static int fdb_mkdir(const char *path, mode_t mode) {
 
  struct stat stbuf;
  ostringstream oss;
  string tmppath;

  log_write("Entered mkdir for path: %s\n", path);

  stbuf.st_mode = S_IFDIR | 0777;
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

static int fdb_rmdir(const char *path) {

  string tmppath;

  log_write("Entered rmdir\n");
 
  tmppath = path;
  tmppath = "#" + tmppath;

  db_delete(tmppath.c_str());
  log_write("Leaving rmdir\n");

  return 0;
}
  
static int fdb_create(const char *path, mode_t mode, fuse_file_info *fi) {

  string tmppath;
  string datapath;
  struct stat stbuf;
  ostringstream oss;
  static time_t the_time;
  string nonsense = "";

  the_time = time(NULL);
  log_write("Entering create\n");
  stbuf.st_mode = S_IFREG | mode;
  stbuf.st_nlink = 1;
  stbuf.st_size = 2;
  stbuf.st_atime = the_time;
  stbuf.st_mtime = the_time;
  stbuf.st_ctime = the_time;

  conv_toByteString(oss, reinterpret_cast<const unsigned char*>(&stbuf), sizeof(stbuf)); 

  tmppath = path;
  tmppath = "#" + tmppath;

  datapath = path;
  datapath = "^" + datapath;
 
  db_write(tmppath.c_str(), oss.str()); 
  db_write(datapath.c_str(), nonsense.c_str()); 

  return 0;
}

static int fdb_read(const char *path, char *buffer, size_t length, off_t offset, struct fuse_file_info *fi) {

  string tmppath;
  string read_buffer;

  log_write("Entered fdb_read\n");
  
  tmppath=path;
  tmppath = "^" + tmppath;
  
  db_read(tmppath.c_str(), &read_buffer);

  memcpy(buffer, read_buffer.data(), read_buffer.size()); 

  return read_buffer.size();
}

int db_write(const char* key, const leveldb::Slice value) {

   leveldb::Status s = fuse_db->Put(leveldb::WriteOptions(), key, value);
   if(s.ok()) 
    return 0;
   else
    return 1;
}

static int db_read(const char* key, string *read_buffer) {

   log_write("Entered db_read with key: %s\n", key);

   leveldb::Status s = fuse_db->Get(leveldb::ReadOptions(), key, read_buffer);

   log_write("leaving db_read\n");

   if(s.ok()) 
    return 0;
   else
    return 1;

}

static int db_delete(const char* key) {

   log_write("Entered db_delete with key: %s\n", key);
    
   leveldb::Status s = fuse_db->Delete(leveldb::WriteOptions(), key);

   if(s.ok())
    return 0;
   else
    return 1;
}

//db_read_iter provides an iterative read to the db
static int db_read_iter(const char *key, list<string>* rbuffers) {

  const char *limiter="^";

  log_write("Entered db_read_iter with key: %s\n", key); 
  leveldb::Iterator* it = fuse_db->NewIterator(leveldb::ReadOptions());

  for (it->Seek(key); it->Valid() && it->key().ToString() < limiter; it->Next()) {
	rbuffers->push_front(it->key().ToString());
   }

  assert(it->status().ok());
  delete it;
   
  log_write("Leaving db_read_iter\n"); 
  return 0; 
}


// Logging operations. Create a logfile
static bool init_log() {

  FILE *log = fopen(logfile, "w");

  if(log == NULL)
    return false;

  fprintf(log, "Started log\n");

  fclose(log);

  return true;
}

//Write to the logfile
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
   operations.write = fdb_write;
   operations.getattr = fdb_getattr;
   operations.readdir = fdb_readdir;
   operations.mkdir = fdb_mkdir;
   operations.rmdir = fdb_rmdir;
   operations.create = fdb_create;
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
  string fakepathdata = "get the hose";
  static time_t the_time;

  the_time = time(NULL);

  options.create_if_missing = true;
 
  if(argc > 20){
   printf("Usage: %s <path to LevelDB>\n", argv[0]);
   return 1;
  }
 
  status = leveldb::DB::Open(options, "/tmp/fdb1", &fuse_db);

  stbuf.st_mode = S_IFREG | 0777;
  stbuf.st_nlink = 1;
  stbuf.st_size = strlen(fakepath);
  stbuf.st_atime = the_time;
  stbuf.st_mtime = the_time;
  stbuf.st_ctime = the_time;

  conv_toByteString(oss, reinterpret_cast<const unsigned char*>(&stbuf), sizeof(stbuf)); 

  db_write(fakepath, oss.str());
  db_write(fakepath1, oss.str());

  db_write(fakedata, fakepathdata);

  std::string somereturn;


 //delete fuse_db;
  init_log(); 
  init_operations(operations);
  log_write("Operations initialized\n"); 

  return fuse_main(argc, argv, &operations, NULL);
}
 
