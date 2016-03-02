
using namespace std;
using std::cout;

#define DATACHAR "^"
#define MDCHAR "#"
#define BLOCKSIZE 4096

static leveldb::DB* fuse_db;
//std::string read_buffer;
//file metadata will use a # and data a ^
static const char *logfile = "/tmp/fdb.log";
static const char *rootpath = "#/";
static const char *dbroot = "/tmp/ldbdirs";
static const char *fakepath="#/firstfile";
static const char *fakepath1="#/different";
static const char *fakedata="^/firstfile";

int db_write(const char* key, const leveldb::Slice value);
int db_write_batch(leveldb::WriteBatch *batch);
static int db_read(const char* key, string *read_buffer);
static int db_delete(const char* key);
static int db_read_iter(const char *key, list<string>* rbuffers);
static void log_write(const char *format, ...);
bool conv_fromByteString(istream &inputstream, unsigned char *outchar, int size);
bool conv_toByteString(ostream &outstream, const unsigned char *inchar, int size);
static int fdb_update_metadata(const char *path, struct stat* stbuf);

class FDBDir {
   leveldb::DB* mydirdb;

   public:
   std::string mydir;
   std::string dbdir;

   FDBDir(const char* dir, const char* dirname) : dbdir(dir), mydir(dirname) {}
   int db_open();
   int db_close();
   int db_write(const char* key, const leveldb::Slice value);
   int db_read(const char* key, string *read_buffer);
   int db_delete(const char* key);
   int db_read_iter(const char *key, list<string>* rbuffers);
   int db_write_batch(leveldb::WriteBatch *batch);
   int delete_db();
};

FDBDir* cwd;
unordered_map<string, FDBDir* > dirmap;
unordered_map<string , int> stringmap;

