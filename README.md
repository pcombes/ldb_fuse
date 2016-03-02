# ldb_fuse
Build

Edit the LEVELDB variable in the makefile to reflect where the out-shared directory, a result of the LevelDB build process, is located. After that just type ‘make’ and it should build. Note that a number of c++11 constructs and syntax is used so gcc needs to have support for it available.

Usage

Create a directory to act as the mount point, wherever your user has permission. Also create a directory in /tmp on the host called ‘ldbdirs’ (/tmp/ldbdirs) for storage of the leveldb databases. Add the location of the leveldb shared libraries to LD_LIBRARY_PATH. Then start fdb_fuse by just running it with the mount point directory as the only argument.

Approach

fdb_fuse will create an initial leveldb for the “root” directory and populate it with some global metadata like the block counter. If an existing leveldb is found, creation and initialization will be skipped and the DB re-used.

fdb_fuse creates a new leveldb database for each subdirectory created by the user. This makes directory renames and movements much easier and also segments the database up enough to support a wider number of stored objects.

Within each “directory” database there are three types of entries:
      KV pairs that correspond to the filename and FS metadata respectively
      KV pairs that correspond to the filename and the ordered list of blocks belonging to that file
      KV pairs of the block id and up to 4k of the file data

When writing, the offset of the write divided by the block size determines the placement within the ordered block list. The block id itself is taken from a global counter, incremented after each use. The existing block list for the file (if any) is then read out and updated with the new block information.

When reading, the offset again determines which block in the block list will be read out. And the input buffer size is ignored as data will be returned in blocks of up to 4k.

Open operations set the nonseekable flag for files, so operations that require re-positioning the file pointer will complain.

<B>Known Issues</B>

Doesn’t seem to like reading back files > than 8k (two blocks)
Not encoding/byte converting content just yet, although that is easy enough to change
