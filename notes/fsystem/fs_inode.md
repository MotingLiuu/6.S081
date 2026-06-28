# Inode

A file's name is distinct from the file itself. The same underlying file, called an ***inode*** can have multiple names, called ***links***. 

Each link consists of an entry in a dir. The entry contains a file name and a reference to an inode. An inode holds ***metadata*** about a file. including its type, its length, the location of the file's content on disk, and the number of links to a file.


`kernel/fstat.h`
```c
#define T_DIR     1   // Directory
#define T_FILE    2   // File
#define T_DEVICE  3   // Device

struct stat {
  int dev;     // File system's disk device
  uint ino;    // Inode number
  short type;  // Type of file
  short nlink; // Number of links to file
  uint64 size; // Size of file in bytes
};
```

`link` system call creates another file system name referrring to the same inode as an existing file.

```c
open("a", O_CREATE|O_WRONLY);
link("a", "b");
```

Each inode is identified by a unique number ***inode number***.

```c
#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#include "kernel/fs.h"
#include "kernel/fcntl.h"

int main(int argc, char *argv[]){
  int fd;
  struct stat st1, st2, st3;

  if((fd = open("sixfive.txt", O_RDONLY)) < 0){
    fprintf(2, "open error\n");
    exit(1);
  }
  if (fstat(fd, &st1) < 0){
    fprintf(2, "fstat error\n");
    exit(1);
  }
  link("sixfive.txt", "sixfive.txt.bak");
  close(fd);

  if((fd = open("sixfive.txt", O_RDONLY)) < 0){
    fprintf(2, "open error\n");
    exit(1);
  }
  if (fstat(fd, &st2) < 0){
    fprintf(2, "fstat error\n");
    exit(1);
  }
  close(fd);

  if((fd = open("sixfive.txt.bak", O_RDONLY)) < 0){
    fprintf(2, "open error\n");
    exit(1);
  }
  if (fstat(fd, &st3) < 0){
init: starting sh
    fprintf(2, "fstat error\n");
    exit(1);
  }

  printf("st1.dev: %d\n", st1.dev);
  printf("st1.ino: %d\n", st1.ino);
  printf("st1.type: %d\n", st1.type);
  printf("st1.nlink: %d\n", st1.nlink);
  printf("st1.size: %ld\n", st1.size);

  printf("st2.dev: %d\n", st2.dev);
  printf("st2.ino: %d\n", st2.ino);
  printf("st2.type: %d\n", st2.type);
  printf("st2.nlink: %d\n", st2.nlink);
  printf("st2.size: %ld\n", st2.size);

  printf("st3.dev: %d\n", st3.dev);
  printf("st3.ino: %d\n", st3.ino);
  printf("st3.type: %d\n", st3.type);
  printf("st3.nlink: %d\n", st3.nlink);
  printf("st3.size: %ld\n", st3.size);

  exit(0);
}
```

```bash
$ fstat_ex1
st1.dev: 1
st1.ino: 4
st1.type: 2
st1.nlink: 1
st1.size: 19
st2.dev: 1
st2.ino: 4
st2.type: 2
st2.nlink: 2
st2.size: 19
st3.dev: 1
st3.ino: 4
st3.type: 2
st3.nlink: 2
st3.size: 19
```

Unix provides file utilities callable from the shell as user-level programs. except `cd`. `cd` must change the current working dir of the shell. if `cd` is a regular utilities file, the shell would fork a child process, child would run `cd`, cd can only change the child's working dir.

