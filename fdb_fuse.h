using namespace std;
using std::cout;


static leveldb::DB* fuse_db;
//std::string read_buffer;
//file metadata will use a # and data a ^
static const char *logfile = "/tmp/fdb.log";
static const char *rootpath = "/";
static const char *fakepath="#/firstfile";
static const char *fakepath1="#/different";

int db_write(const char* key, const leveldb::Slice value);
static int db_read(const char* key, string *read_buffer);
static int db_delete(const char* key);
static int db_read_iter(const char *key, list<string>* rbuffers);
static void log_write(const char *format, ...);
bool conv_fromByteString(istream &inputstream, unsigned char *outchar, int size);
bool conv_toByteString(ostream &outstream, const unsigned char *inchar, int size);

