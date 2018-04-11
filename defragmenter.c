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
void* inode_region = NULL;
// data_pointer always points to the starting point of data region in original data
void* data_pointer = NULL;
struct superblock* sb;
struct inode* cur_inode = NULL;

//here testing
FILE* inputd;
//here testing


int blocksize = 0;
int inode_size = 0;
//long input_offset = 0;
//long output_offset = 0;
// file_offset keeps track of the current file position in the output file
long file_offset = 0;
long disksize = 0;
int inode_num = 0;
int filedata_index = 0;

int main(int argc, char** argv) {
  if(argc < 2) {
    printf("Input format: ./defrag <fragmented disk file>.\n Type in './defrag -h' for more information. \n");
    return FALSE;
  }

  if(strcmp(argv[1], "-h") == SUCCESS) {
    print_info();
    return TRUE;
  }
  
  char postfix[8] = "-defrag";
  int outfile_len = strlen(argv[1]) + strlen(postfix) + 1;
  // needs to free outfile
  char* outfile = (char*)malloc(sizeof(char) * outfile_len);
  outfile[outfile_len -1] = '\0';
  strcpy(outfile, argv[1]);
  strcat(outfile, postfix);
  printf("outfile is %s\n", outfile);

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

  outputfile = fopen(outfile, "wb+");
  if(outputfile == NULL) {
    perror("Open output file failed: ");
    free(outfile);
    free(input_buffer);
    fclose(inputfile);
    exit(EXIT_FAILURE);
  }

  //here testing
  inputd = fopen("myresultfile", "wb+");
  //here testing
  input_buffer = malloc(disksize);

  if(read_sysinfo() == FAIL) {
    printf("Read in boot block, super block, and inodes region failed. \n");
    fclose(inputfile);
    fclose(outputfile);
    free(outfile);
    free(input_buffer);
    exit(EXIT_FAILURE);
  }

  //printf("input_offset is %ld output_offset is %ld\n", input_offset, output_offset);
  if(readin_inodes() == FAIL) {
    printf("Read in and write files failed. \n");
    fclose(inputfile);
    fclose(outputfile);
    free(outfile);
    free(input_buffer);
    exit(EXIT_FAILURE);
  }
  printf("data offset is %d\n", filedata_index);
  if(write_free_blocks() == FAIL) {
    fclose(inputfile);
    fclose(outputfile);
    free(outfile);
    free(input_buffer);
    exit(EXIT_FAILURE);
  }

  if(write_swap_region() == FAIL) {
    fclose(inputfile);
    fclose(outputfile);
    free(outfile);
    free(input_buffer);
    exit(EXIT_FAILURE);
  }

  if(update_inodes_spblock() == FAIL) {
    fclose(inputfile);
    fclose(outputfile);
    free(outfile);
    free(input_buffer);
    exit(EXIT_FAILURE);
  }
  printf("final file size is %ld\n", file_offset);

  // testing...
  fseek(outputfile, 1024, SEEK_SET);
  void* inodes = malloc(4*512);
  fread(inodes, 1, 4*512, outputfile);
  for(int i = 0; i < inode_num; i++) {
    struct inode* start = (struct inode*)(inodes + inode_size * i);
    printf(" %d inode direct offset next is %d and direct offset is %d\n", i, start->next_inode, start->nlink);
    if(start->nlink != 0) {
      printf("iblock first one: %d\n", start->iblocks[0]);
    }
  }
  free(inodes);
  //testing..

  fclose(inputfile);
  fclose(outputfile);
  free(outfile);
  free(input_buffer);
}

// read in whole file, and write boot, super blocks, and inode regions
int read_sysinfo(){
  // read in the whole file
  if(fread(input_buffer, 1, disksize, inputfile) != disksize) {
    perror("Read in disk failed: ");
    return FAIL;
  }
  // write boot block, super block and inode_region
  sb = (struct superblock*)(input_buffer + BOOTSIZE);
  blocksize = sb->size;
  printf("block size is %d inode offset %d data offset %d\n", blocksize, sb->inode_offset, sb->data_offset);
  file_offset = BOOTSIZE + SUPERBSIZE + sb->data_offset * blocksize;
  data_pointer = input_buffer + file_offset;

  // write boot, super block, and inode region to the outputfile
  if(fwrite(input_buffer, 1, file_offset, outputfile) != file_offset) {
    perror("Write bootblock, superblock, and inodes region failed: ");
    return FAIL;
  }
  //input_offset += file_offset;
  //output_offset += file_offset;

  inode_region = input_buffer + BOOTSIZE + SUPERBSIZE + sb->inode_offset * blocksize;
  return SUCCESS;
}

int readin_inodes() {
  // inode struct size
  inode_size = sizeof(struct inode);
  inode_num = ((sb->data_offset - sb->inode_offset) * blocksize) / inode_size;
  printf("inode size is %d and number is %d\n", inode_size, inode_num);
  for(int i = 0; i < inode_num; i++) {
    cur_inode = (struct inode*)(inode_region + i * inode_size);
    // if inode is used
    if(cur_inode->nlink != 0) {
      if(read_write_file() == FAIL) {
        return FAIL;
      } else {
        printf("--------Finished %i writing files file_offset is %ld\n", i, file_offset);
      }
    } else {
      printf("----------- inode %d\n", i);
    }
  }
  return SUCCESS;
}

int read_write_file() {
  int file_counter = cur_inode->size;
  // write in direct blocks
  for(int i = 0; i < N_DBLOCKS; i++) {
    if(file_counter > 0) {
      printf("direct %dth before is %d\n", i, cur_inode->dblocks[i]);
      void* read_file = data_pointer + cur_inode->dblocks[i] * blocksize;
      //fseek(outputfile, file_offset, SEEK_SET);

      // here testing
      fwrite(read_file, 1, blocksize, inputd);
      //here testing
      
      if(fwrite(read_file, 1, blocksize, outputfile) != blocksize) {
        perror("Write file data to output failed: ");
        return FAIL;
      }
      cur_inode->dblocks[i] = filedata_index;
      filedata_index ++;
      file_offset += blocksize;
      file_counter -= blocksize;
      printf("direct %dth after is %d\n", i, cur_inode->dblocks[i]);
    }
  }

  // max index in a block-sized table
  int max_index = blocksize / POINTERSIZE;
  // read from 1-level indirect blocks and write to output file
  if(file_counter > 0) {
    for(int i = 0; i < N_IBLOCKS; i++) {
      if(file_counter <= 0) {
        break;
      }
      // dblocks refers to the array of direct indices
      printf("In 1-level the original %dth iblock value is %d\n", i, cur_inode->iblocks[i]);
      int* dblocks= (int*)(data_pointer + cur_inode->iblocks[i] * blocksize);

      // write each block to output file
      for(int j = 0; j < max_index; j++) {
        if(file_counter <= 0) {
          break;
        }
        void* read_file = data_pointer + dblocks[j] * blocksize;

	 // here testing
	fwrite(read_file, 1, blocksize, inputd);
	//here testing                                                                                                                        	
        //fseek(outputfile, file_offset, SEEK_SET);
        if(fwrite(read_file, 1, blocksize, outputfile) != blocksize) {
          perror("Write file data to output failed: ");
          return FAIL;
        }
        dblocks[j] = filedata_index;
        printf("In 1-level the dblocks %dth iblock value is %d\n", i, dblocks[j]);
        filedata_index ++;
        file_offset += blocksize;
        file_counter -= blocksize;
      }

      cur_inode->iblocks[i] = filedata_index;
      printf("In 1-level the the  %dth iblock value is %d\n", i, cur_inode->iblocks[i]);
      // write dblocks indices table to disk
      if(fwrite(dblocks, 1, blocksize, outputfile) != blocksize) {
        perror("Write index table for 'iblocks'failed: ");
        return FAIL;
      }
      filedata_index ++;
      file_offset += blocksize;

    }
  } else {
    return SUCCESS;
  }

  // read from 2-level indirect blocks
  if(file_counter > 0) {
    printf("second layer original i2block is %d\n", cur_inode->i2block);
    int* in2block = (int*)(data_pointer + cur_inode->i2block * blocksize);

    // outtest indirect table
    for(int i = 0; i < max_index; i++) {
      if(file_counter <= 0) {
        break;
      }
      // direct data indices table
      int* dblocks = (int*)(data_pointer + in2block[i] * blocksize);

      for(int j = 0; j < max_index; j++) {
        if(file_counter <= 0) {
          break;
        }
        void* read_file = data_pointer + dblocks[j] * blocksize;

	 // here testing
         fwrite(read_file, 1, blocksize, inputd);
	 //here testing                                                                                                                        	
	printf("second layer dblock %d is %d\n", j, dblocks[j]);
        //fseek(outputfile, file_offset, SEEK_SET);
        if(fwrite(read_file, 1, blocksize, outputfile) != blocksize) {
          perror("Write file data to output failed: ");
          return FAIL;
        }
	dblocks[j] = filedata_index;
        filedata_index ++;
        file_offset += blocksize;
        file_counter -= blocksize;
      }

      in2block[i] = filedata_index;
      printf("second layer in2blocks %d is %d\n", i, in2block[i]);
      // write dblocks indices table to disk
      if(fwrite(dblocks, 1, blocksize, outputfile) != blocksize) {
        perror("Write index table for 'iblocks'failed: ");
        return FAIL;
      }
      filedata_index ++;
      file_offset += blocksize;
    }

    cur_inode->i2block = filedata_index;
    printf("second layer after i2block is %d\n", cur_inode->i2block);
    // write in2block indices table to disk
    if(fwrite(in2block, 1, blocksize, outputfile) != blocksize) {
      perror("Write index table for 'iblocks'failed: ");
      return FAIL;
    }
    filedata_index ++;
    file_offset += blocksize;

  } else {
    return SUCCESS;
  }

  if(file_counter > 0) {
    printf("third layer original i333block is %d\n", cur_inode->i2block);
    int* in3block = (int*)(data_pointer + cur_inode->i3block * blocksize);

    // out-most indirect table
    for(int i = 0; i < max_index; i++) {
      if(file_counter <= 0) {
        break;
      }

      int* in2block = (int*)(data_pointer + in3block[i] * blocksize);
      for(int j = 0; j < max_index; j++) {
        if(file_counter <= 0) {
          break;
        }

        int* dblocks = (int*)(data_pointer + in2block[j] * blocksize);
        for(int z = 0; z < max_index; z++) {
          if(file_counter <= 0) {
            break;
          }
          void* read_file = data_pointer + dblocks[z] * blocksize;

	   // here testing
	  fwrite(read_file, 1, blocksize, inputd);
	  //here testing                                                                                                                        	  
	  //fseek(outputfile, file_offset, SEEK_SET);
	  printf("third layer original dblocks %d aaaafter is %d\n", i, in3block[i]);
          if(fwrite(read_file, 1, blocksize, outputfile) != blocksize) {
            perror("Write file data to output failed: ");
            return FAIL;
          }
	  dblocks[z] = filedata_index;
          filedata_index ++;
          file_offset += blocksize;
          file_counter -= blocksize;
        }

        in2block[j] = filedata_index;
        printf("third layer original in22block %d aaaafter is %d\n", i, in3block[i]);
        // write in2block indices table to disk
        if(fwrite(dblocks, 1, blocksize, outputfile) != blocksize) {
          perror("Write index table for 'iblocks'failed: ");
          return FAIL;
        }
        filedata_index ++;
        file_offset += blocksize;
      }

      in3block[i] = filedata_index;
      printf("third layer original in3block %d aaaafter is %d\n", i, in3block[i]);
      // write in2block indices table to disk
      if(fwrite(in2block, 1, blocksize, outputfile) != blocksize) {
        perror("Write index table for 'iblocks'failed: ");
        return FAIL;
      }
      filedata_index ++;
      file_offset += blocksize;
    }

    cur_inode->i3block = filedata_index;
    printf("third layer original i333block aaaafter is %d\n", cur_inode->i3block);
    // write in3block indices table to disk
    if(fwrite(in3block, 1, blocksize, outputfile) != blocksize) {
      perror("Write index table for 'iblocks'failed: ");
      return FAIL;
    }
    filedata_index ++;
    file_offset += blocksize;

  } else {
    return SUCCESS;
  }
  return SUCCESS;
}

int update_inodes_spblock() {
  printf("updating......\n");
  fseek(outputfile, BOOTSIZE, SEEK_SET);
  if(fwrite((void*)sb, 1, SUPERBSIZE, outputfile) != SUPERBSIZE) {
    perror("Update super block data failed: ");
    return FAIL;
  }

  int inode_regionsize = (sb->data_offset - sb->inode_offset) * blocksize;
  fseek(outputfile, BOOTSIZE + SUPERBSIZE + sb->inode_offset * blocksize, SEEK_SET);
  if(fwrite(inode_region, 1, inode_regionsize, outputfile) != inode_regionsize) {
    perror("Update inode region data failed: ");
    return FAIL;
  }
  fseek(outputfile, 0L, SEEK_END);
  return SUCCESS;
}

int write_free_blocks(){
  int free_block_nums = sb->swap_offset - sb->data_offset - filedata_index;
  sb->free_block = filedata_index;
  printf("Free block number : %d and file_offset before free is %ld\n", free_block_nums, file_offset);
  void* freeblock_region = input_buffer + file_offset;
  void* freeblock = freeblock_region;
  int freeblock_index = sb->free_block;
  for(int i = 0; i < free_block_nums; i++) {
    int* value = (int*)freeblock;
    if(i + 1 == free_block_nums) {
      *value = -1;
    } else {
      *value = freeblock_index + 1;
    }
    printf("%d freeblock pointer now is %d\n", i, *value);
    if(fwrite(freeblock, 1, blocksize, outputfile) != blocksize) {
      perror("Write free blocks to file failed: ");
      return FAIL;
    }
    freeblock_index ++;
    file_offset += blocksize;
    freeblock += blocksize;
  }
  printf("after copying all free blocks, file offset is %ld\n", file_offset);
  printf("swap region offset is %d\n", sb->swap_offset);
  return SUCCESS;
}

int write_swap_region() {
  void* swapregion = input_buffer + file_offset;
  int swapregion_size = disksize - sb->swap_offset * blocksize - BOOTSIZE - SUPERBSIZE;
  printf("......swapregion size is %d \n", swapregion_size);

  if(fwrite(swapregion, 1, swapregion_size, outputfile) != swapregion_size) {
    perror("Write swap region failed: ");
    return FAIL;
  }
  file_offset += swapregion_size;
  return SUCCESS;
}

void print_info() {
  printf("* Information about usage of disk defragmenter. \n");
  printf("* To defragment a disk, run the defragmenter program with \n\t'./defrag <input-disk-image>'\n");
  printf("* The resulting defragmented disk image will be stored under the original disk image name with postfix '-defrag'. \n");
  printf("* Example: \n\tcommand line input: './defrag myfile' \n\toutput file: 'myfile-defrag'\n");
}
