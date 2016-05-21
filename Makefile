CFLAGS = -I ./include
LFLAGS = -lrt -lX11 -lGLU -lGL -pthread -lm #-lXrandr

all: bats

bats: bats.cpp ppm.cpp log.cpp
	g++ $(CFLAGS) bats.cpp ppm.cpp log.cpp libggfonts.a -Wall -Wextra $(LFLAGS) -o bats

clean:
	rm -f bats
	rm -f *.o

