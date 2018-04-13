CC = gcc
FLAGS = -Wall -g -DDUMP

defrag: defragmenter.c defragmenter.h
	$(CC) -o defrag $(FLAGS) defragmenter.c

clean:
	rm defrag *-defrag
