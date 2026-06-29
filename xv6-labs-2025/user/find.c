#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#include "kernel/fs.h"
#include "kernel/fcntl.h"

int find(char *path, char *name) {

  /*printf("DEBUG: find: looking for %s in %s\n", name, path); */

  int fd;
  char buf[512], *p;
  struct dirent de;
  struct stat st;

  if ((fd = open(path, O_RDONLY)) < 0) {
    fprintf(2, "find: cannot open %s\n", path);
    return -1;
  }
  if (fstat(fd, &st) < 0) {
    fprintf(2, "find: cannot stat %s\n", path);
    return -1;
  }

  /*printf("DEBUG: path: %s\n st.type: %d\n", path, st.type); */

  switch(st.type) {
    case T_DEVICE:
    case T_FILE:
      fprintf(2, "pattern: find name. name should be a dir\n");
      return -1;
    case T_DIR:

      /*printf("DEBUG: path is T_DIR\n"); */

      strcpy(buf, path);
      p = buf + strlen(buf);
      *p++ = '/';
      while (read(fd, &de, sizeof(de)) == sizeof(de)) {
        if (de.inum == 0)
          continue;
        memmove(p, de.name, DIRSIZ);
        p[DIRSIZ] = 0;

        /*printf("DEBUG: finding %s in %s, current name %s\n", name, path, buf); */

        if (strcmp(p, ".") == 0)
          continue;
        if (strcmp(p, "..") == 0)
          continue;
        if (stat(buf, &st) < 0) {
          fprintf(2, "find: cannot stat %s\n", buf);
          continue;
        }
        if (st.type == T_FILE) {

          /*printf("DEBUG: Comparing %s and %s\n", p, name); */

          if (strcmp(p, name) == 0) {
            printf("%s\n", buf);
            continue;
          }
        } else if (st.type == T_DIR) {

          /*printf("DEBUG: Recursing into %s\n", buf); */

          if (find(buf, name) < 0) {
            return -1;
          }
        } else {
          continue;
        }
      }
    }
  close(fd);
  return 0;
}

int main(int argc, char *argv[]) {
  if (argc < 2) {
    fprintf(2, "usage: find name\n");
    exit(1);
  } else if (argc == 2) {
    if (find(".", argv[1]) < 0)
      exit(1);
  } else if (argc == 3) {
    if (find(argv[1], argv[2]) < 0)
      exit(1);
  } else {
    fprintf(2, "usage: find name\n");
    exit(1);
  } 
  exit(0);
}
