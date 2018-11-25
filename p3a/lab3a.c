//NAME: Jianzhi Liu, Yinsheng Zhou
//EMAIL: ljzprivate@yahoo.com, jacobzhou@g.ucla.edu
//ID: 204742214, 004817743
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include <time.h>
#include "ext2_fs.h"

char* fsimg = NULL;
int fd_fsimg = 0;

#define SUPERBLOCK_OFFSET 1024
#define SUPERBLOCK_LENGTH 1024
#define GROUP_DESC_OFFSET 2048
#define GROUP_DESC_LENGTH 32

struct ext2_super_block superblock;
struct ext2_group_desc group_desc;
struct ext2_inode inode;
struct ext2_dir_entry dir_entry;

int BLOCK_SIZE = -1;

void error_exit(char *msg){
  fprintf(stderr,"%s",msg);
  exit(2);
}

void superblock_summary() {
  /* Read the following information and output to stdout
  1. SUPERBLOCK
  2. total number of blocks (decimal)
  3. total number of i-nodes (decimal)
  4. block size (in bytes, decimal)
  5. i-node size (in bytes, decimal)
  6. blocks per group (decimal)
  7. i-nodes per group (decimal)
  8. first non-reserved i-node (decimal)
  */
  int rv = pread(fd_fsimg, &superblock, SUPERBLOCK_LENGTH, SUPERBLOCK_OFFSET);
  if (rv < 0) {
    fprintf(stderr, "Failed to read from fd%d: %s\n", fd_fsimg, strerror(errno));
    exit(2);
  }
  if (rv != SUPERBLOCK_LENGTH) {
    fprintf(stderr, "Failed to read superblock: %d/1024 bytes read\n", rv);
    exit(2);
  }
  printf("SUPERBLOCK,%d,%d,%d,%d,%d,%d,%d\n",
	 superblock.s_blocks_count,
	 superblock.s_inodes_count,
	 EXT2_MIN_BLOCK_SIZE << (superblock.s_log_block_size),
	 superblock.s_inode_size,
	 superblock.s_blocks_per_group,
	 superblock.s_inodes_per_group,
	 superblock.s_first_ino);
  BLOCK_SIZE = EXT2_MIN_BLOCK_SIZE << (superblock.s_log_block_size);
}

void group_summary() {
  /* Read the following information and output to stdout
  1. GROUP
  2. group number (decimal, starting from zero)
  3. total number of blocks in this group (decimal)
  4. total number of i-nodes in this group (decimal)
  5. number of free blocks (decimal)
  6. number of free i-nodes (decimal)
  7. block number of free block bitmap for this group (decimal)
  8. block number of free i-node bitmap for this group (decimal)
  9. block number of first block of i-nodes in this group (decimal)
  */
  int rv = pread(fd_fsimg, &group_desc, GROUP_DESC_LENGTH, GROUP_DESC_OFFSET);
  if (rv < 0) {
    fprintf(stderr, "Failed to read from fd%d: %s\n", fd_fsimg, strerror(errno));
    exit(2);
  }
  if (rv != GROUP_DESC_LENGTH) {
    fprintf(stderr, "Failed to read group descriptor: %d/32 bytes read\n", rv);
    exit(2);
  }
  printf("GROUP,%d,%d,%d,%d,%d,%d,%d,%d\n",
	 0,
	 superblock.s_blocks_count,
	 superblock.s_inodes_count,
	 group_desc.bg_free_blocks_count,
	 group_desc.bg_free_inodes_count,
	 group_desc.bg_block_bitmap,
	 group_desc.bg_inode_bitmap,
	 group_desc.bg_inode_table);
}

void free_block_entries() {
  /*Scan block bitmap for free blocks*/
  int BLOCK_BITMAP_OFFSET =
    group_desc.bg_block_bitmap * BLOCK_SIZE;
  char *block_bitmap_buf = malloc(BLOCK_SIZE);
  if (block_bitmap_buf == NULL) {
    fprintf(stderr, "Failed to malloc\n");
    exit(2);
  }

  int rv = pread(fd_fsimg, block_bitmap_buf, BLOCK_SIZE, BLOCK_BITMAP_OFFSET);
  if (rv < 0) {
    fprintf(stderr, "Failed to read from fd%d: %s\n", fd_fsimg, strerror(errno));
    exit(2);
  }
  if (rv != BLOCK_SIZE) {
    fprintf(stderr, "Failed to read bitmap: %d/1024 bytes read\n", rv);
    exit(2);
  }
  int i, j;
  int block_number = 0;
  int break_cond = 0;
  for (i = 0; i < BLOCK_SIZE; i++) {
    for (j = 0; j < 8; j++) {
      block_number++;
      if (block_number > (int) superblock.s_blocks_per_group) {
	break_cond = 1;
	break;
      }
      if ((block_bitmap_buf[i] & (1 << j)) == 0) {
	printf("BFREE,%d\n", block_number);
      }
    }
    if (break_cond) break;
  }
  free(block_bitmap_buf);
}

void format_time(__u32 time, char * result) {
  time_t raw_data = time;
  struct tm* gtime = gmtime(&raw_data);
  strftime(result, 30, "%m/%d/%g %H:%M:%S", gtime);
  //get the raw data and formats them in the desired order, storing the value in result
}

char find_type(__u16 i_mode){
  char file_type;
  if((i_mode & 0x4000) == 0x4000){
    file_type = 'd';
  }
  else if((i_mode & 0xA000) == 0xA000){
    file_type = 's';
  }
  else if((i_mode & 0x8000) == 0x8000){
    file_type = 'f';
  }
  else{
    file_type = '?';
  }
  return file_type;
}

void disp_inode(int inode_number) {
  /*
    INODE
    inode number (decimal)
    file type
    mode (low order 12-bits, octal ... suggested format "%o")
    owner (decimal)
    group (decimal)
    link count (decimal)
    time of last I-node change (mm/dd/yy hh:mm:ss, GMT)
    modification time (mm/dd/yy hh:mm:ss, GMT)
    time of last access (mm/dd/yy hh:mm:ss, GMT)
    file size (decimal)
    number of (512 byte) blocks of disk space (decimal) taken up by this file
  */
  char inode_file_type = find_type(inode.i_mode);
  char ctime[30], atime[30], mtime[30];
  format_time(inode.i_ctime,ctime);
  format_time(inode.i_atime,atime);
  format_time(inode.i_mtime,mtime);

  printf("INODE,%u,%c,%o,%u,%u,%u,%s,%s,%s,%u,%u",
	 inode_number,
	 inode_file_type,
	 inode.i_mode & 511,
	 inode.i_uid,
	 inode.i_gid,
	 inode.i_links_count,
	 ctime,
	 mtime,
	 atime,
	 inode.i_size,
	 inode.i_blocks);
  int i;
  for(i = 0; i < EXT2_N_BLOCKS; i++){
    fprintf(stdout,",%u",inode.i_block[i]);
  }
  fprintf(stdout,"\n");
}

void disp_directory(int inode_number) {
  /*
    DIRENT
    parent inode number (decimal)
    logical byte offset (decimal) of this entry within the directory
    inode number of the referenced file (decimal)
    entry length (decimal)
    name length (decimal)
    name (string, surrounded by single-quotes)
  */
  int DIR_OFFSET = 0;
  int i;
  for (i = 0; i < EXT2_NDIR_BLOCKS; i++) {
    DIR_OFFSET = 0;
    while (DIR_OFFSET < BLOCK_SIZE) {
      int INODE_OFFSET = inode.i_block[i] * BLOCK_SIZE + DIR_OFFSET;
      if (pread(fd_fsimg, &dir_entry, sizeof(dir_entry), INODE_OFFSET) == -1){
	fprintf(stderr, "Failed to read from fd%d: %s\n", fd_fsimg, strerror(errno));
	exit(2);
      }
      if (!dir_entry.inode) break;
      printf("DIRENT,%d,%d,%d,%d,%d,'%s'\n",
	     inode_number,
	     DIR_OFFSET,
	     dir_entry.inode,
	     dir_entry.rec_len,
	     dir_entry.name_len,
	     dir_entry.name);
      DIR_OFFSET += dir_entry.rec_len;
    }
  }
}

void disp_indirect_reference(int inode_number) {
  int n_references = BLOCK_SIZE / 4;
  
  // first level
  int first_lvl_ref_block = inode.i_block[12];
  if (first_lvl_ref_block != 0) {
    int INODE_OFFSET = first_lvl_ref_block * BLOCK_SIZE;
    // load the first level indirect reference block
    int *buf = malloc(BLOCK_SIZE);
    if (buf == NULL) {
      fprintf(stderr, "Failed to malloc\n");
      exit(2);
    }
    if (pread(fd_fsimg, buf, BLOCK_SIZE, INODE_OFFSET) == -1){
      fprintf(stderr, "Failed to read from fd%d: %s\n", fd_fsimg, strerror(errno));
      exit(2);
    }
    // display each reference
    int i;
    for (i = 0; i < n_references; i++) {
      if (buf[i]) {
	printf("INDIRECT,%d,%d,%d,%d,%d\n",
	       inode_number,
	       1,
	       12+i,
	       first_lvl_ref_block,
	       buf[i]);
      }
    }
    free(buf);
  }

  // second level
  int second_lvl_ref_block = inode.i_block[13];
  //printf("INDIRECT,SECOND LEVEL BLOCK NUMBER %d\n", second_lvl_ref_block);
  if (second_lvl_ref_block != 0) {
    int OFFSET1 = second_lvl_ref_block * BLOCK_SIZE;
    // load the second level indirect reference block
    int *buf1 = malloc(BLOCK_SIZE);
    if (buf1 == NULL) {
      fprintf(stderr, "Failed to malloc\n");
      exit(2);
    }
    if (pread(fd_fsimg, buf1, BLOCK_SIZE, OFFSET1) == -1){
      fprintf(stderr, "Failed to read from fd%d: %s\n", fd_fsimg, strerror(errno));
      exit(2);
    }
    // display each reference
    int i, j;
    for (i = 0; i < n_references; i++) {
      int OFFSET2 = buf1[i] * BLOCK_SIZE;
      int *buf2 = malloc(BLOCK_SIZE);
      if (buf2 == NULL) {
	fprintf(stderr, "Failed to malloc\n");
	exit(2);
      }
      if (pread(fd_fsimg, buf2, BLOCK_SIZE, OFFSET2) == -1){
	fprintf(stderr, "Failed to read from fd%d: %s\n", fd_fsimg, strerror(errno));
	exit(2);
      }
      for (j = 0; j < n_references; j++) {
	if (buf2[j]) {
	  printf("INDIRECT,%d,%d,%d,%d,%d\n",
	       inode_number,
	       2,
	       268,
	       second_lvl_ref_block,
	       buf1[i]); 
	  int OFFSETB = 0;
	  while (OFFSETB < n_references) {
	    if (pread(fd_fsimg, &dir_entry, sizeof(dir_entry), OFFSET2 + OFFSETB) == -1) {
	      fprintf(stderr, "Failed to read from fd%d: %s\n", fd_fsimg, strerror(errno));
	      exit(2);
	    }
	    printf("INDIRECT,%d,%d,%d,%d,%d\n",
		   inode_number,
		   1,
		   268+j,
		   buf1[i],
		   buf2[j]);
	    OFFSETB += 268;
	  }
	}
      }
      free(buf2);
    }
    free(buf1);
  }

  //third level
  int third_lvl_ref_block = inode.i_block[14];
  if (third_lvl_ref_block != 0) {
    int* buf1 = malloc(BLOCK_SIZE);
    int OFFSET1 = third_lvl_ref_block * BLOCK_SIZE;
    if (pread(fd_fsimg, buf1, BLOCK_SIZE, OFFSET1) == -1) {
      fprintf(stderr, "Failed to read from fd%d: %s\n", fd_fsimg, strerror(errno));
      exit(2);
    }
    int i, j, k;
    for (i = 0; i < n_references; i++){
      int* buf2 = malloc(BLOCK_SIZE);
      int OFFSET2 =  buf1[i] * BLOCK_SIZE;
      if (pread(fd_fsimg, buf2, BLOCK_SIZE, OFFSET2) == -1) {
	fprintf(stderr, "Failed to read from fd%d: %s\n", fd_fsimg, strerror(errno));
	exit(2);
      }
      for (j = 0; j < n_references; j++){
	int* buf3 = malloc(BLOCK_SIZE);
	int OFFSET3 = buf2[j] * BLOCK_SIZE;
	if (pread(fd_fsimg, buf3, BLOCK_SIZE, OFFSET3) == -1) {
	  fprintf(stderr, "Failed to read from fd%d: %s\n", fd_fsimg, strerror(errno));
	  exit(2);
	}
	for(k = 0; k < n_references; k++){
	  if (buf3[k] != 0){
	    int OFFSETB = 0;
	    while (OFFSETB < n_references) {
	      if (pread(fd_fsimg, &dir_entry, sizeof(dir_entry), OFFSET3 + OFFSETB) == -1) {
		fprintf(stderr, "Failed to read from fd%d: %s\n",
			fd_fsimg, strerror(errno));
		exit(2);
	      }    
	      int c = (256-i) * (256 - j) + (256 - k) + 14;
	      printf("INDIRECT,%d,%d,%d,%d,%d\n",
		     inode_number,
		     3,
		     c+i,
		     third_lvl_ref_block,
		     buf1[i]);
	      printf("INDIRECT,%d,%d,%d,%d,%d\n",
                     inode_number,
                     2,
                     c+i+j,
                     buf1[i],
                     buf2[j]);
	      printf("INDIRECT,%d,%d,%d,%d,%d\n",
                     inode_number,
                     1,
                     c+i+j+k,
                     buf2[j],
                     buf3[k]);
	      OFFSETB += 65536;
	    }
	  }
	}
	free(buf3);
      }
      free(buf2);
    }
    free(buf1);
  }
}

void handle_used_inode(int inode_number) {
  // load inode into buffer
  int INODE_LENGTH = superblock.s_inode_size;
  int INODE_OFFSET =
    group_desc.bg_inode_table * 1024 + (inode_number - 1) * INODE_LENGTH;
  if (pread(fd_fsimg, &inode, INODE_LENGTH, INODE_OFFSET) == -1){
    fprintf(stderr, "Failed to read from fd%d: %s\n", fd_fsimg, strerror(errno));
    exit(2);
  }
  // display the inode
  if(inode.i_mode != 0 && inode.i_links_count != 0){
    disp_inode(inode_number);
  }
  char file_type = find_type(inode.i_mode);
  if (file_type == 'd') {
    disp_directory(inode_number);
  }
  if (file_type == 'f' || file_type == 'd') {
    disp_indirect_reference(inode_number);
  }
}

void scan_inodes() {
  int INODE_BITMAP_OFFSET =
    group_desc.bg_inode_bitmap * BLOCK_SIZE;
  char *inode_bitmap_buf = malloc(BLOCK_SIZE);
  if (inode_bitmap_buf == NULL) {
    fprintf(stderr, "Failed to malloc\n");
    exit(2);
  }

  int rv = pread(fd_fsimg, inode_bitmap_buf, BLOCK_SIZE, INODE_BITMAP_OFFSET);
  if (rv < 0) {
    fprintf(stderr, "Failed to read from fd%d: %s\n", fd_fsimg, strerror(errno));
    exit(2);
  }
  if (rv != BLOCK_SIZE) {
    fprintf(stderr, "Failed to read bitmap: %d/1024 bytes read\n", rv);
    exit(2);
  }

  int i, j;
  int inode_number = 0;
  int break_cond = 0;
  for (i = 0; i < BLOCK_SIZE; i++) {
    for (j = 0; j < 8; j++) {
      inode_number++;
      if (inode_number > (int) superblock.s_inodes_per_group) {
	break_cond = 1;
	break;
      }
      if ((inode_bitmap_buf[i] & (1 << j)) == 0) {
	printf("IFREE,%d\n", inode_number);
      }
      else {
	handle_used_inode(inode_number);
      }
    }
    if (break_cond) break;
  }
  free(inode_bitmap_buf);
}

int main(int argc, char **argv) {
  char* usage = "Usage: ./lab3a file_system_image\n";
  if (argc != 2) {
    fprintf(stderr, "%s", usage);
    exit(1);
  }
  fsimg = argv[1];
  fd_fsimg = open(fsimg, O_RDONLY);
  if (fd_fsimg < 0) {
    fprintf(stderr, "Failed to open %s: %s\n", fsimg, strerror(errno));
    exit(1);
  }
  superblock_summary();
  group_summary();
  free_block_entries();
  scan_inodes();
  return 0;
}
