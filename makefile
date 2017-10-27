CC = gcc
CFLAGS = -Wall -Wextra -c
LFLAGS = -lm

mandelbrot: mandelbrot.o
	$(CC) mandelbrot.o $(LFLAGS) -o mandelbrot

mandelbrot.o: mandelbrot.c
	$(CC) $(CFLAGS) mandelbrot.c

all: mandelbrot
