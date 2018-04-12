#Yazhe Feng
#HW6

1. How to use
 - Compile with 'make' command.

 - To defragment a disk, just type in './defrag <input_disk>', where 'input_disk' is the disk image to be defragmented.

 - If '-DDUMP' flag is specified during compilation, then the program requires an extra input filename (name for creating new file). The program can be run in this way: './defrag diskimg datafile', where the 'diskimg' is the disk image to be defragmented, and 'datafile' will be a newly generated file that contains only the file data of the disk 'diskimg'.

 - Makefile for './defrag' right now contains the '-DDUMP' flag.

 - Type in './defrag -h' for more information about how to use the defragmenter.

 2. Files
  - program files: 'defragment.c', 'defragment.h', 'Makefile'
