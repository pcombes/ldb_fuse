#define FUSE_USE_VERSION 26


#include <string>
#include <iostream>
#include <fuse.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/types.h>
#include <sstream>
#include <iomanip>
#include <list>
#include <iosfwd>
#include <unordered_map>
#include "leveldb/db.h"
#include "leveldb/env.h"
#include "leveldb/slice.h"
#include "leveldb/write_batch.h"
#include "fdb_fuse.h"

int FDBDir::db_open() {

  leveldb::Options options;
  leveldb::Status status;

  options.create_if_missing = true;

  status = leveldb::DB::Open(options, dbdir, &mydirdb);
}


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
    string newdir;
    string dirname;

    log_write("Entered fdb_readdir with path: %s \n", path);

    //if (strcmp(path, "/") != 0)
    //   return -ENOENT;

   if(dirmap.find(path) == dirmap.end()) {
      log_write("Creating a new class for %s\n", path);
      newdir = path;
      newdir.erase(newdir.begin());
      newdir = dbroot + newdir;
      dirname = path;
      dirmap[path] = new FDBDir(newdir.c_str(), dirname.c_str());
      dirmap[path]->db_open();  
     }

    cwd = dirmap[path];

    fill(buffer, ".", NULL, 0);
    fill(buffer, "..", NULL, 0);

    tmppath = path;
    tmppath = "#" + tmppath;
    db_read_iter(tmppath.c_str(), &rbuffers);
    tmppath.erase(tmppath.begin());
    tmppath.erase(tmppath.begin());
    log_write("rbuffer size: %d\n", rbuffers.size());
    if(rbuffers.size() > 0) {
     for (string read_buffer : rbuffers) {
        read_buffer.erase(read_buffer.begin());
        read_buffer.erase(read_buffer.begin());
	log_write("Got this back from iter: %s\n", read_buffer.c_str());
        //remove the leading path stuff
        tmppath = tmppath + "/";
        string::size_type i = read_buffer.find(tmppath);
        if(i != string::npos) {
         read_buffer.erase(i, tmppath.length());
        }
        log_write("Filling buffer with %s\n", read_buffer.c_str());
    	fill(buffer, read_buffer.c_str(), NULL, 0);
     }
    }
    else {
     log_write("Empty dir\n");
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

  log_write("Entered fdb_open for %s\n", path);

  res = db_read(tmppath.c_str(), &readbuf); 

  if(res == 1) {
	log_write("fdb_open returns no entry\n");
	return -ENOENT;
  }

  istringstream iss(readbuf); 
  conv_fromByteString(iss, reinterpret_cast<unsigned char*>(&stbuf), sizeof(stbuf)); 
  //Test for permissions

  
  fi->fh = stbuf.st_size;
  fi->nonseekable = 1;

  log_write("Left fdb_open\n"); 
  return res;

}

static int fdb_close() {
 return 0;
}

static int fdb_write(const char *path, const char *buffer, size_t size, off_t offset, struct fuse_file_info *fi) {

  struct stat stbuf;  
  static time_t the_time;
  string tmppath = path;
  string metapath = path;
  tmppath = "^" + tmppath;
  metapath = "#" + metapath;

  string write_data = buffer;
  string emptybuf;
  string tmpbuf;
  ostringstream oss;

  log_write("Entered fdb_write for path: %s\n", path);

  //Get the old attributes
  fdb_getattr(path, &stbuf);

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

 //Update stbuf
  the_time = time(NULL);
  stbuf.st_mtime = the_time;
  stbuf.st_size = write_data.size() + stbuf.st_size;

  conv_toByteString(oss, reinterpret_cast<const unsigned char*>(&stbuf), sizeof(stbuf));

  db_delete(metapath.c_str());
  db_write(metapath.c_str(), oss.str());

  return write_data.size();
}

static int fdb_mkdir(const char *path, mode_t mode) {
 
  struct stat stbuf;
  ostringstream oss;
  string tmppath;
  string fspath = dbroot;
  static time_t the_time;

  the_time = time(NULL);

  log_write("Entered mkdir for path: %s\n", path);

  stbuf.st_mode = S_IFDIR | 0777;
  stbuf.st_nlink = 2;
  stbuf.st_size = 4096;
  stbuf.st_atime = the_time;
  stbuf.st_ctime = the_time;
  stbuf.st_mtime = the_time;

  conv_toByteString(oss, reinterpret_cast<const unsigned char*>(&stbuf), sizeof(stbuf));
  char b[sizeof(oss)];
  memcpy(b, &oss, sizeof(oss));
  
  tmppath = path;
  tmppath = "#" + tmppath; 

  log_write("Writing %s\n", tmppath.c_str());

  db_write(tmppath.c_str(), oss.str());

  log_write("Creating class and ldb (if it doesn't exist) for key %s\n", path);
  fspath = fspath + path;
  dirmap[path] = new FDBDir(fspath.c_str(), path);
  dirmap[path]->db_open();
 
  log_write("Leaving mkdir\n");  
  
  return 0;
}

static int fdb_rmdir(const char *path) {

  string tmppath;

  log_write("Entered rmdir - removing directory %s\n", path);
 
  tmppath = path;
  tmppath = "#" + tmppath;

  db_delete(tmppath.c_str());
  log_write("Leaving rmdir\n");

  return 0;
}

static int fdb_chown(const char *path, uid_t uid, gid_t gid) {
 
  struct stat stbuf; 
  log_write("Entered fdb_chown for %s\n", path);

  fdb_getattr(path, &stbuf);
  
  stbuf.st_uid = uid;
  stbuf.st_gid = gid;

  return fdb_update_metadata(path, &stbuf); 
   
}
 
static int fdb_update_metadata(const char *path, struct stat* stbuf) {
  
  ostringstream oss;
  leveldb::WriteBatch batch;
  leveldb::Status s;
  string tmppath;

  log_write("Updating metadata for %s\n", path);

  conv_toByteString(oss, reinterpret_cast<const unsigned char*>(stbuf), sizeof(stbuf));

  tmppath = path;
  tmppath = "#" + tmppath; 

  log_write("Batching for %s\n", tmppath.c_str());

  batch.Delete(tmppath.c_str());
  batch.Put(tmppath.c_str(), oss.str());

  log_write("Leaving update_metadata\n");

  return db_write_batch(&batch);   

}
 
static int fdb_create(const char *path, mode_t mode, fuse_file_info *fi) {

  string tmppath;
  string datapath;
  struct stat stbuf;
  ostringstream oss;
  static time_t the_time;
  string nonsense = "";

  the_time = time(NULL);
  log_write("Entering create for %s\n", path);
  stbuf.st_mode = S_IFREG | mode;
  stbuf.st_nlink = 1;
  stbuf.st_size = 2;
  stbuf.st_atime = the_time;
  stbuf.st_mtime = the_time;
  stbuf.st_ctime = the_time;
  stbuf.st_uid = getuid();
  stbuf.st_gid = getgid();

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

static int fdb_opendir(const char *dir, struct fuse_file_info *fi) {

  log_write("Entered fdb_opendir for dir %s\n", dir); 

 if(dirmap.find(dir) == dirmap.end()) {
   log_write("Opendir didn't have entry for %s\n", dir);
   string fsdir = dir;
   fsdir = dbroot + fsdir;
   dirmap[dir] = new FDBDir(fsdir.c_str(),dir);
   dirmap[dir]->db_open();
   cwd = dirmap[dir];
  }

  log_write("Left fdb_opendir\n");
}

int db_write(const char* key, const leveldb::Slice value) {

   cwd->db_write(key, value);
}

int FDBDir::db_write(const char* key, const leveldb::Slice value) {

   leveldb::Status s = mydirdb->Put(leveldb::WriteOptions(), key, value);
   if(s.ok()) 
    return 0;
   else
    return 1;
}

int db_write_batch(leveldb::WriteBatch *batch) {

 return cwd->db_write_batch(batch);

}

int FDBDir::db_write_batch(leveldb::WriteBatch *batch) {

  leveldb::Status s = mydirdb->Write(leveldb::WriteOptions(), batch); 
  if(s.ok())
    return 0;
   else
    return 1;
}


static int db_read(const char* key, string *read_buffer) {

  // log_write("Entered db_read with key: %s\n", key);

   //leveldb::Status s = fuse_db->Get(leveldb::ReadOptions(), key, read_buffer);
   //New way
  return  cwd->db_read(key, read_buffer);
}

int FDBDir::db_read(const char* key, string *read_buffer) {

  // log_write("Entered db_read with key: %s\n", key);

   leveldb::Status s = mydirdb->Get(leveldb::ReadOptions(), key, read_buffer);

   //log_write("leaving db_read\n");

   if(s.ok()) 
    return 0;
   else
    return 1;
}

static int db_delete(const char* key) {

   log_write("Entered db_delete with key: %s\n", key);
   
   return cwd->db_delete(key);

}

int FDBDir::db_delete(const char *key) {
   leveldb::Status s = mydirdb->Delete(leveldb::WriteOptions(), key);
 
   if(s.ok())
    return 0;
   else
    return 1;
}
  
int FDBDir::db_read_iter(const char *key, list<string>* rbuffers) {

  const char *limiter="^";

  if(mydirdb == NULL) {
   log_write("Empty mydirdb\n");
   return 1;
  }

  log_write("Entered class iter\n");
  leveldb::Iterator* it = mydirdb->NewIterator(leveldb::ReadOptions());
  log_write("Created iter\n");
  for (it->Seek(key); it->Valid() && it->key().ToString() < limiter; it->Next()) {
        rbuffers->push_front(it->key().ToString());
   }

  log_write("Exited for loop\n");
  assert(it->status().ok());
  delete it;

  return 0;
}
 
//provides an iterative read to the db
static int db_read_iter(const char *key, list<string>* rbuffers) {


  log_write("Entered db_read_iter with key: %s\n", key); 
 
  log_write("CWD is thought to be %s\n", cwd->mydir.c_str());
 
  cwd->db_read_iter(key, rbuffers);
 
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
   operations.opendir = fdb_opendir;
   operations.chown = fdb_chown;
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

  static fuse_operations operations;
  struct stat stbuf; 
  ostringstream oss;
  string fakepathdata = "get the hose";
  static time_t the_time;
  string rootdb = dbroot;

  the_time = time(NULL);

 
  if(argc > 20){
   printf("Usage: %s <path to LevelDB>\n", argv[0]);
   return 1;
  }

  rootdb = rootdb + "/fdb1";
  dirmap["/"] = new FDBDir(rootdb.c_str(), "/");
  dirmap["/"]->db_open();

  cwd = dirmap["/"];

  stbuf.st_mode = S_IFREG | 0777;
  stbuf.st_nlink = 1;
  stbuf.st_size = strlen(fakepath);
  stbuf.st_atime = the_time;
  stbuf.st_mtime = the_time;
  stbuf.st_ctime = the_time;
  stbuf.st_uid = getuid();
  stbuf.st_gid = getgid();

  conv_toByteString(oss, reinterpret_cast<const unsigned char*>(&stbuf), sizeof(stbuf)); 

  db_write(fakepath, oss.str());
  db_write(fakepath1, oss.str());
  db_write(fakedata, fakepathdata);

  std::string somereturn;

  init_log(); 
  init_operations(operations);
  log_write("Operations initialized\n"); 

  return fuse_main(argc, argv, &operations, NULL);
}
 
