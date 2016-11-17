#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <inttypes.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#define SUPERBLOCK_OFFSET 1024
#define SUPERBLOCK_SIZE 1024
#define BUFFER_SIZE 2048
//file descriptor for the input disk image. 
int fd1 = 0;
uint32_t block_size;
int num_groups;

//super block
struct super_block {
  uint32_t s_inodes_count;                   // 4 byte inodes count
  uint32_t s_blocks_count;                   // 4 byte blocks count
  uint32_t s_r_blocks_count;                 // 4 byte reserved blocks count
  uint32_t s_free_blocks_count;              // 4 byte free blocks count
  uint32_t s_free_inodes_count;              // 4 byte free inodes count 
  uint32_t s_first_data_block;               // 4 byte first data block
  uint32_t s_log_block_size;                 // 4 byte block size
  uint32_t s_log_frag_size;                  // 4 byte fragment size 
  uint32_t s_blocks_per_group;               // 4 byte blocks per group
  uint32_t s_frags_per_group;                // 4 byte fragments per group
  uint32_t s_inodes_per_group;               // 4 byte inodes per group
  uint32_t s_mtime;                          // 4 byte mount time 
  uint32_t s_wtime;                          // 4 byte write time 
  uint16_t s_mnt_count;                      // 2 byte mount count  
  uint16_t s_max_mnt_count;                  // 2 byte maximal mount count 
  uint16_t s_magic;                          // 2 byte magic number 
  uint16_t s_state;                          // 2 byte file system state
  uint16_t s_errors;                         // 2 byte error 
  uint16_t s_minor_rev_level;                // 2 byte minor revision level
  uint32_t s_lastcheck;                      // 4 byte last file system check
  uint32_t s_checkinterval;                  // 4 byte maximum time interval 
  uint32_t s_creator_os;                     // 4 byte OS identifier
  uint32_t s_rev_level;                      // 4 byte revision level value 
  uint16_t s_def_resuid;                     // 2 byte default user id 
  uint16_t s_def_resgid;                     // 2 byte default group id
                                             // totol of 84 bytes declared
                                             //rest are specific implementations
  uint8_t restvar[SUPERBLOCK_SIZE - 84];    
} superblock;

//group descriptor table 
struct group_desc_table {
  uint32_t bg_block_bitmap;                 // 4 byte id of first block bitmap block
  uint32_t bg_inode_bitmap;                 // 4 byte id of first inode bitmap block
  uint32_t bg_inode_table;                  // 4 byte id of first inode table block
  uint16_t bg_free_blocks_count;            // 2 byte total number of free blocks
  uint16_t bg_free_inodes_count;            // 2 byte total number of free inodes
  uint16_t bg_used_dirs_count;              // 2 byte number of inodes allocated
  uint16_t bg_pad;                          // 2 byte used for padding
  uint32_t bg_reserved[3];                  // 12 byte of reserved space 
};

struct group_desc_table* table;


//inode structure for ext2
struct inode {
  uint16_t i_mode;                         // 2 byte format of file and access right
  uint16_t i_uid;                          // 2 byte user id
  uint32_t i_size;                         // 4 byte size of file in bytes 
  uint32_t i_atime;                        // 4 byte seconds access - 1970-1-1
  uint32_t i_ctime;                        // 4 byte seconds create - 1970-1-1
  uint32_t i_mtime;                        // 4 byte seconds modify - 1970-1-1
  uint32_t i_dtime;                        // 4 byte seconds delete - 1970-1-1
  uint16_t i_gid;                          // 2 byte group id
  uint16_t i_links_count;                  // 2 byte how many times linked
  uint32_t i_blocks;                       // 4 byte total number of blocks reserved
  uint32_t i_flags;                        // 4 byte file flags
  uint32_t i_osd1;                         // 4 byte OS dependent value
  uint32_t i_block[15];                   // array of blocks: 60 bytes
  uint32_t i_generation;                   // 4 byte file version
  uint32_t i_file_acl;                     // 4 byte block number extended attributes
  uint32_t i_dir_acl;                      // 4 byte block number dir attribute
  uint32_t i_faddr;                        // 4 byte fragment location
  uint16_t i_osd2[6];                      // 12 byte OS dependent structure 
} inode;

//linked list directory
struct dir {
  uint32_t inode;                         // 4 byte inode number of file entry
  uint16_t rec_len;                       // 2 byte displacement to next directory
  uint8_t name_len;                       // 1 byte byte length in name
  uint8_t file_type;                      // 1 byte file type
  uint8_t name[255];                      // <= 255 byte name 
};


//superblock info
void superblock_info() {
  int ret = pread(fd1, &superblock, SUPERBLOCK_SIZE, SUPERBLOCK_OFFSET);
  if (ret < SUPERBLOCK_SIZE) {
    perror("failure reading super block.\n");
    exit(1);
  }
  //compute the block size 
  block_size = 1024 << superblock.s_log_block_size;

  //compute the fragment size
  int32_t shift_val = (int32_t) superblock.s_log_frag_size;
  uint32_t fragment_size = 0;
  if (shift_val > 0 )
    fragment_size = 1024 << shift_val;
  else
    fragment_size = 1024 << -shift_val;
  int fd2 = creat("super.csv", 0666);
  if (fd2 < 0) {
    perror("failure creating superblock info file\n");
    exit(1);
  }
  char output_buffer[BUFFER_SIZE];
  //output are magic number, total number of inodes, total number of blocks, block size,
  //fragment size, blocks per group, inodes per group, fragments per group
  //first data block. 
  ret = sprintf(output_buffer, "%x,%"PRIu32",%"PRIu32",%"PRIu32",%"PRIu32",%"PRIu32",%"PRIu32",%"PRIu32",%"PRIu32"\n", superblock.s_magic, superblock.s_inodes_count, superblock.s_blocks_count, block_size, fragment_size, superblock.s_blocks_per_group, superblock.s_inodes_per_group, superblock.s_frags_per_group, superblock.s_first_data_block);
   write(fd2, output_buffer, ret);
}

//group descriptor info
void group_descriptor_info() {
  //calculate the pread region
  int start = superblock.s_first_data_block + 1;
  //calculate number of groups for iteration purpose. 
  num_groups = superblock.s_blocks_count / superblock.s_blocks_per_group;
  table = malloc(sizeof(struct group_desc_table)* num_groups);
  //check pread status
  int total_size = sizeof(struct group_desc_table) * num_groups;
  int ret = pread(fd1, table, total_size, start * block_size);
  if (ret < total_size)
    {
      perror("failure reading the part about group descriptor.\n");
      exit(1);
    }
  
  int fd2 = creat("group.csv", 0666);
  if (fd2 < 0) {
    perror("failure creating group descriptor info file\n");
    exit(1);
  }
  char output_buffer[BUFFER_SIZE];
  //iterate each group to add information to the output
  int i = 0;
  ret = 0;
  for (i = 0; i < num_groups; i++) {
    uint32_t blocks_in_group = superblock.s_blocks_per_group;
    //do a tiny calculation for the last group.
    if ( i == (num_groups - 1)) {
      //get the number of blocks in the last group. 
      blocks_in_group = superblock.s_blocks_count - i * superblock.s_blocks_per_group;
    }
    ret = sprintf(output_buffer, "%"PRIu32",%"PRIu32",%"PRIu32",%"PRIu32",%x,%x,%x\n", blocks_in_group,table[i].bg_free_blocks_count, table[i].bg_free_inodes_count, table[i].bg_used_dirs_count, table[i].bg_inode_bitmap, table[i].bg_block_bitmap, table[i].bg_inode_table);
    
    ret += write(fd2, output_buffer, ret);
  }
}



//free bitmap entry
//bitmap's first block are stored in the table's bg_block_bitmap
//and bg_inode_bitmap, and we go by block_size iteration to find
//bit value : if it is a zero, we increment the free counter.
void free_bitmap_entry_info() {
  
  int inode_index = 0; 
  int block_index = 0;
  int i = 0;
  int j = 0;
  int k = 0;
  int ret = 0;
  int fd2 = creat("bitmap.csv", 0666);
  if (fd2 < 0) {
    perror("failure creating the file for bitmap.\n");
    exit(1);
  }

  char output_buffer[BUFFER_SIZE];
  //buffer for block bitmap
  uint8_t *block_bitmap = (uint8_t *)malloc(sizeof(uint8_t) * block_size);
  //buffer for inode bitmap
  uint8_t *inode_bitmap = (uint8_t *)malloc(sizeof(uint8_t) * block_size);
  
  //keep a bound for inode to check only the first n of them.
  int inode_bound_for_group = 0;
  int inode_out_bound = 0;

  for (i = 0; i < num_groups; i++) {
    //the size of bitmap is determined by blocks_per_group / inodes_per_group.
    //for each group. 
    inode_bound_for_group += superblock.s_inodes_per_group;
    
    //read the block bitmap in ith group 
    pread(fd1, block_bitmap, block_size, table[i].bg_block_bitmap * block_size);
    //go through the block bitmap to check for free blocks.
    //first level, byte-wise.
    for (j = 0; j < block_size; j++) {
      //second level, bit-wise. check zero for a free block. 
      for (k = 0; k < 8; k++) {
	if ((block_bitmap[j] & (1 << k)) == 0) {
	  ret = sprintf(output_buffer, "%x,%"PRIu32"\n",table[i].bg_block_bitmap, block_index);
	  write(fd2, output_buffer, ret);
	}
	block_index++;
      }
    }
    
    //reset for inode bitmap in ith group.
    //check only the inodes in bound. 
    inode_out_bound = 0;
    pread(fd1, inode_bitmap, block_size, table[i].bg_inode_bitmap * block_size);
    //go through block to check free inodes: bytewise.
    for (j = 0; j < block_size; j++) {
      if (inode_out_bound) {
	 break;
       }
      //go through each bit value
      for (k = 0; k < 8; k++) {
	if (inode_index <= inode_bound_for_group) {
	  if ((inode_bitmap[j] & (1 << k)) == 0) {
	    ret = sprintf(output_buffer, "%x,%"PRIu32"\n", table[i].bg_inode_bitmap, inode_index);
	    write (fd2, output_buffer, ret);
	  }
	  inode_index ++;
	}
	//we reach the out-of-bound case.
	else {
	  inode_out_bound = 1;
	  break;
	}
      }
    }
  }
  free(inode_bitmap);
  free(block_bitmap);
}

//inode info
int inode_info() {
}

//directory entry info
int directory_entry_info() {
}

//indirect block entry info
int indirect_block_entry_info() {
}



int main(int argc, char** argv)
{
  if (argc < 2) {
    printf("no input disk image.\n");
    exit(1);
  }

  fd1 = open(argv[1], O_RDONLY);
  if (fd1 < 0) {
    perror("Failure opening the disk image\n");
    exit(1);
  }

  //get the superblock info
  superblock_info();
  //get the group descriptor info
  group_descriptor_info();
  //get the bitmap info
  free_bitmap_entry_info();
  //get the inodes table info
  
  return 0;
}
