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
