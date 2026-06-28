// On-disk file system format.
// Both the kernel and user programs use this header file.


#define ROOTINO  1   // root i-number
#define BSIZE 1024  // block size

/*
 * every block has 1024 bytes
 */

// Disk layout:
// [ boot block | super block | log | inode blocks |
//                                          free bit map | data blocks]
//

/*  boot block is the first 1024 bytes. QEMU loads the ELF kernel. 
 *  what is the ELF kernel? what QEMU does?
 *
 *  what is mkfs?
 *  super block just contains 8 uints? yes
 *  what is magic in super block? a magic number identifying the FS type.
 *  what does boot lock contain? 
 */

// mkfs computes the super block and builds an initial file system. The
// super block describes the disk layout:
struct superblock {
  uint magic;        // Must be FSMAGIC
  uint size;         // Size of file system image (blocks)
  uint nblocks;      // Number of data blocks
  uint ninodes;      // Number of inodes.
  uint nlog;         // Number of log blocks
  uint logstart;     // Block number of first log block
  uint inodestart;   // Block number of first inode block
  uint bmapstart;    // Block number of first free map block
};

#define FSMAGIC 0x10203040

/*
 * what is FSMAGIC
 */

#define NDIRECT 12
#define NINDIRECT (BSIZE / sizeof(uint))
#define MAXFILE (NDIRECT + NINDIRECT)

/*
 * what is NDIRECT? Number of direct blocks are in addrs[0..NDIRECT-1]
 *
 * what is NINDIRECT? addrs[NDIRECT] stores the number of a block which 
 * contains all of the indirect block numbers.
 *
 * what is MAXFILE?
 * So, the maxfile is ND + NIND
 */

// On-disk inode structure
struct dinode {
  short type;           // File type T_FILE, T_DIR, T_DEVICE (defined in stat.h)
  short major;          // Major device number (T_DEVICE only)
  short minor;          // Minor device number (T_DEVICE only)
  short nlink;          // Number of links to inode in file system
  uint size;            // Size of file (bytes)
  uint addrs[NDIRECT+1];   // Data block addresses
                           // This contains block numbers.
};

/*
 * what is major and minor? What is T_DEVICE?
 * only meaningful for T_DEVICE, they pick which device driver handles the reads/writes (UNIX tradition).
 * when do mknod /dev/console 1 0, the kernel creates an inode with type = T_DEVICE, major = 1, minor = 0
 *
 * why does inode #1 is the root directory? every directory is a file? and has a inode?
 * At boot, ther kernel reads the superblock, walks to inodestart, and treats inode 1 as the root.
 * A directory is just a file with type T_DIR, whose data blocks contain an array of dirent structs. must contain . and ..
 */

// Inodes per block.
#define IPB           (BSIZE / sizeof(struct dinode))

// Block containing inode i
#define IBLOCK(i, sb)     ((i) / IPB + sb.inodestart)

/*
 * what is sb?
 * a struct superblock instance, kernel keeps a global sb.
 */

// Bitmap bits per block
#define BPB           (BSIZE*8)

// Block of free map containing bit for block b
#define BBLOCK(b, sb) ((b)/BPB + sb.bmapstart)
/*
 * what is bitmap? what does one bit per data block, 1=free mean?
 * A bit array tracking which data blocks are free. One black = 1024 bytes = 8192 bits.
 */

// Directory is a file containing a sequence of dirent structures.
#define DIRSIZ 14

struct dirent {
  ushort inum;
  char name[DIRSIZ];
};

/*
 * dirent means directory entry. It contains an inode number and a name.
 */

