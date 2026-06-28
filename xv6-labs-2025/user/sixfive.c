#include "kernel/types.h"
#include "kernel/fcntl.h"
#include "user/user.h"


int main(int argc, char *argv[])
{
    char *separators = "-\r\t\n./";

    for (int j = 1; j < argc; j++) {
      int fd;
      if ((fd = open(argv[j], O_RDONLY)) < 0) {
        fprintf(2, "sixfive: read error\n");
        exit(1);
      }
  
      char c;
      int pre_num = -1;
      int count = 0;
      int result = 0;
      int n;
  
      while ((n = read(fd, &c, sizeof(c))) > 0) {
        if (c >= '0' && c <= '9') {
          result = result * 10 + c - '0';
          count++;
        } else if (strchr(separators, c)) {
          if ((pre_num == -1 || strchr(separators, c)) && count > 0) {
            if ((result % 5 == 0) || (result % 6 == 0)) 
              printf("%d\n", result);
          } 
          result = 0;
          count = 0;
          pre_num = c;
        } else {
          pre_num = c;
        }
      }
  
      if ((pre_num == -1 || strchr(separators, pre_num)) && count > 0) {
        if ((result % 5 == 0) || (result % 6 == 0)) 
          printf("%d\n", result);
      }
  
      close(fd);
 
    }

   exit(0);
}

