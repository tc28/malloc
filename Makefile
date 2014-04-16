CC = gcc
CFLAGS = -Wall -Wextra -Wunused -O2 -m32 -Werror -g

OBJS = mdriver.o memlib.o fsecs.o fcyc.o clock.o ftimer.o

all: mdriver mdriver-naive

mdriver: $(OBJS) rk4.o
	$(CC) $(CFLAGS) -o mdriver $(OBJS) rk4.o

rk4.o: rk4.c 

