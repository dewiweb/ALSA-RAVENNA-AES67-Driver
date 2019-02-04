# define some Makefile variables for the compiler and compiler flags
# to use Makefile variables later in the Makefile: $()
#
#  -g    adds debugging information to the executable file
#  -Wall turns on most, but not all, compiler warnings
#
# for C++ define  CC = g++
CC = g++
CFLAGS = -g -Wall -pthread -DLINUX

default: main

main:  main.cpp ipc.o
	$(CC) $(CFLAGS) -o mt_alsa_d_msg_test main.cpp MTAL_IPC.o

	
ipc.o: ../../common/MTAL_IPC.c ../../common/MTAL_IPC.h
	$(CC) $(CFLAGS) -c ../../common/MTAL_IPC.c
	
clean: 
	$(RM) myprog