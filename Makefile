CC = gcc
FLAGS = -Wall -g

defrag: defragmenter.c defragmenter.h
	$(CC) -o defrag $(FLAGS) defragmenter.c
