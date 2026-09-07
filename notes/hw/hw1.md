# Read `cat` how does the system keep track of the connections between the string filename `argv[i]` passed to `open()` and the resulting integer file descriptor `fd`? What does the integer file descriptor number refer to?

Every process has a `struct file *ofiles[NOFILE]` array

```c
struct file {
  enum { FD_NONE, FD_PIPE, FD_INODE, FD_DEVICE } type;
  int ref; // reference count
  char readable;
  char writable;
  struct pipe *pipe; // FD_PIPE
  struct inode *ip;  // FD_INODE and FD_DEVICE
  uint off;          // FD_INODE
  short major;       // FD_DEVICE
};
```

`struct file` fills in the `ofiles[fd]`, `struct file` based on the corresponding `argv[i]` passed to `open()`. 

```txt
argv[i]
  │
  │ open(argv[i], ...)
  ▼
sys_open()
  │
  │ namei(path)
  ▼
struct inode *ip
  │
  │ filealloc()
  ▼
struct file *f
  │
  │ f->ip = ip
  │
  │ fdalloc(f)
  ▼
p->ofile[fd] = f
  │
  ▼
fd
```

The integer file descriptor 'fd' is the index of the `ofiles[fd]` in the `ofiles` array.
