#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#include "kernel/fs.h"
#include "kernel/fcntl.h"

enum {
  Match = 256,
  Split = 257
};

typedef struct State {
  int c;
  State *out;
  State *out1;
};

State matchstate = { Match };

