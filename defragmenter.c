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
#define POINTERSIZE 4

FILE* inputfile;
FILE* outputfile;

void* input_buffer = NULL;
// data_pointer always points to the starting point of data region in original data
void* data_pointer = NULL;
struct superblock* sb;
struct inode* cur_inode = NULL;

int blocksize = 0;
int inode_size = 0;
long input_offset = 0;
long output_offset = 0;
// file_offset keeps track of the current file position in the output file
long file_offset = 0;
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
  printf("disk size is %ld\n", disksize);
  if(disksize <= 0) {
    printf("Input file has no content. \n");
    free(outfile);
    fclose(inputfile);
    exit(EXIT_FAILURE);
  }
  rewind(inputfile);
  input_buffer = malloc(disksize);

  outputfile = fopen(outfile, "wb+");
  if(outputfile == NULL) {
    perror("Open output file failed: ");
    free(outfile);
    free(input_buffer);
    fclose(inputfile);
    exit(EXIT_FAILURE);
  }

  if(read_sysinfo() == FAIL) {
    printf("Read in boot block and super block failed. \n");
    fclose(inputfile);
    fclose(outputfile);
    free(outfile);
    free(input_buffer);
    exit(EXIT_FAILURE);
  }

  printf("input_offset is %ld output_offset is %ld\n", input_offset, output_offset);

  fclose(inputfile);
  fclose(outputfile);
  free(outfile);
  free(input_buffer);
  free(outfile);
}

// read in whole file, and write boot and super block
int read_sysinfo(){
  // read in the whole file
  if(fread(input_buffer, 1, disksize, inputfile) != disksize) {
    perror("Read in disk failed: ");
    return FAIL;
  }

  //write boot block to new disk
  if(fwrite(input_buffer, 1, BOOTSIZE, outputfile) != BOOTSIZE) {
    perror("Write boot block failed: ");
    return FAIL;
  }
  input_offset += BOOTSIZE;
  output_offset += BOOTSIZE;

  sb = (struct superblock*)(input_buffer + BOOTSIZE);
  blocksize = sb->size;
  printf("block size is %d\n", blocksize);

  //write super block first to new disk
  if(fwrite((void*)(input_buffer + input_offset), 1, SUPERBSIZE, outputfile) != SUPERBSIZE) {
    perror("Write super block failed: ");
    return FAIL;
  }
  input_offset += SUPERBSIZE;
  output_offset += SUPERBSIZE;
  file_offset = BOOTSIZE + SUPERBSIZE + sb->data_offset * blocksize;
  data_pointer = input_buffer + file_offset;
  return SUCCESS;
}

int readin_inodes() {
  // write to file the region before inode-region
  if(fwrite((void*)(input_buffer + input_offset), 1, sb->inode_offset * blocksize, outputfile) != sb->inode_offset * blocksize) {
    perror("Write to output file failed: ");
    return FAIL;
  }
  input_offset += sb->inode_offset * blocksize;
  output_offset += sb->inode_offset * blocksize;

  // inode struct size
  inode_size = sizeof(struct inode);
  printf("inode size is %d\n", inode_size);
  inode_num = ((sb->data_offset - sb->inode_offset) * blocksize) / inode_size;
  cur_inode = (struct inode*)(input_buffer + input_offset);
  for(int i = 0; i < inode_num; i++) {
    // if inode is not used
    if(cur_inode->nlink == 0) {
      if(fwrite((void*)cur_inode, 1, inode_size, outputfile) != inode_size) {
        perror("Write inode to output file failed: ");
        return FAIL;
      }
      input_offset += inode_size;
      output_offset += inode_size;
      cur_inode = (struct inode*)(input_buffer + input_offset);
    } else { // if inode is used
      if(read_write_file() == FAIL) {


      }
    }
  }

  return SUCCESS;
}

int read_write_file() {
  int file_counter = cur_inode->size;
  // write in direct blocks
  for(int i = 0; i < N_DBLOCKS; i++) {
    if(file_counter > 0) {
        void* read_file = data_pointer + cur_inode->dblocks[i] * blocksize;
        fseek(outputfile, file_offset, SEEK_SET);
        int bytesread = file_counter > blocksize ? blocksize : file_counter;
        if(fwrite(read_file, 1, bytesread, outputfile) != bytesread) {
          perror("Write file data to output failed: ");
          return FAIL;
        }
        file_offset += bytesread;
        file_counter -= bytesread;
    }
  }

  // read from 1-level indirect blocks and write to output file
  if(file_counter > 0) {
    for(int i = 0; i < N_IBLOCKS; i++) {
      if(file_counter <= 0) {
        return SUCCESS;
      }
      // dblocks refers to the array of direct indices
      int* dblocks= (int*)(data_pointer + cur_inode->iblocks[i] * blocksize);
      int max_index = blocksize / POINTERSIZE;
      // write each block to output file
      for(int j = 0; j < max_index; j++) {
        if(file_counter <= 0) {
          return SUCCESS;
        }
        void* read_file = data_pointer + dblocks[j] * blocksize;
        fseek(outputfile, file_offset, SEEK_SET);
        int bytesread = file_counter > blocksize ? blocksize : file_counter;
        if(fwrite(read_file, 1, bytesread, outputfile) != bytesread) {
          perror("Write file data to output failed: ");
          return FAIL;
        }
        file_offset += bytesread;
        file_counter -= bytesread;
      }
    }
  } else {
    return SUCCESS;
  }

  // read from 2-level indirect blocks





}
