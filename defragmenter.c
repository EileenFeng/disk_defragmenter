#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "defragmenter.h"

#define TRUE 1
#define FALSE 0
#define SUCCESS 0
#define FAIL -1
#define BOOTSIZE 512
#define SUPERBSIZE 512

FILE* inputfile;
FILE* outputfile;

struct superblock* sb;
struct inode* cur_inode;

long blocksize = 0;
long inode_size = 0;
long input_offset = 0;
long output_offset = 0;
long disksize = 0;
int inode_num = 0;


int main(int argc, char** argv) {
  if(argc < 2) {
    printf("Input format: ./defrag <fragmented disk file> \n");
    return FALSE;
  }
  char postfix[] = "-defrag";
  int outfile_len = strlen(argv[1]) + strlen(postfix) + 1;
  // needs to free outfile
  char* outfile = (char*)malloc(sizeof(char) * outfile_len);
  strncpy(outfile, argv[1], strlen(argv[1]));
  strncat(outfile, postfix, strlen(postfix));
  outfile[outfile_len - 1] = '\0';

  inputfile = fopen(argv[1], "rb");
  if(inputfile == NULL) {
    perror("Open input file failed: ");
    free(outfile);
    exit(EXIT_FAILURE);
  }

  // if file size <= 0 then exit
  fseek(inputfile, 0L, SEEK_END);
  disksize = ftell(inputfile); 

  if(disksize <= 0) {
    printf("Input file has no content. \n");
    free(outfile);
    fclose(inputfile);
    exit(EXIT_FAILURE);
  }
  rewind(inputfile);
  
  outputfile = fopen(outfile, "wb+");
  if(outputfile == NULL) {
    perror("Open output file failed: ");
    free(outfile);
    fclose(inputfile);
    exit(EXIT_FAILURE);
  }

  if(read_sysinfo() == FAIL) {
    printf("Read in boot block and super block failed. \n");
    fclose(inputfile);
    fclose(outputfile);
    free(sb);
    free(outfile);
    exit(EXIT_FAILURE);
  }

  inode_size = sizeof(struct inode);


  fclose(inputfile);
  fclose(outputfile);
  free(cur_inode);
  free(sb);
  free(outfile);
}

int read_sysinfo(){
  // read in the boot block
  void* bootblock = malloc(BOOTSIZE);
  if(fread(bootblock, 1, BOOTSIZE, inputfile) != BOOTSIZE) {
    perror("Read in boot block failed: ");
    return FAIL;
  }
  input_offset += BOOTSIZE;
  
  //write boot block to new disk
  if(fwrite(bootblock, 1, BOOTSIZE, outputfile) != BOOTSIZE) {
    perror("Write boot block failed: ");
    return FAIL;
  }
  output_offset += BOOTSIZE;

  // read in super block
  sb = malloc(SUPERBSIZE);
  if(fread((void*)sb, 1, SUPERBSIZE, inputfile) != SUPERBSIZE) {
    perror("Read in super block failed: ");
    return FAIL;
  }
  input_offset += SUPERBSIZE;
  blocksize = sb->size;

  //write super block to new disk
  if(fwrite((void*)sb, 1, SUPERBSIZE, outputfile) != SUPERBSIZE) {
    perror("Write super block failed: ");
    return FAIL;
  }
  output_offset += SUPERBSIZE;
  free(bootblock);
  return SUCCESS;
}

int readin_inodes() {
  // inode struct size
  inode_size = sizeof(struct inode);
  cur_inode = malloc(inode_size);
  int inode_offset = sb->inode_offset;
  inode_num = ((sb->data_offset - inode_offset) * blocksize) / inode_size; 

  long beforenodes = inode_offset * blocksize;
  void* before_inodes = malloc(beforenodes);
  if(fread(before_inodes, 1, beforenodes, inputfile) != beforenodes) {
    perror("Read in data before inode region failed: ");
    free(before_inodes);
    return FAIL;
  }
  input_offset += beforenodes;

  for(int index = 0; index < inode_num; index++) {
    if(fread(cur_inode, 1, inode_size, inputfile) != inode_num) {
      perror("Read in i-node failed: ");
      free(before_inodes);
      return FAIL;
    }
    input_offset += inode_size;
    if(cur_inode->nlink == 0) {
      if(fwrite(cur_inode, 1, inode_size, outputfile) != inode_size) {
        perror("Write i-node failed: ");
        free(before_inodes);
        return FAIL;
      }
    } else {
      printf("needs to read in file info. \n");
    }
  }
  return SUCCESS;
}