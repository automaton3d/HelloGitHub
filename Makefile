CC      = gcc
CXX     = g++
CFLAGS  = -O2 -Wall -Wextra $(shell pkg-config --cflags sdl3)
CXXFLAGS= -O2 -Wall -Wextra
LDFLAGS = -lm $(shell pkg-config --libs sdl3)

all: hello mytry

hello: hello.cpp
	$(CXX) $(CXXFLAGS) -o $@ $<

mytry: mytry.c pulsating.h
	$(CC) $(CFLAGS) -o $@ $< $(LDFLAGS)

clean:
	rm -f hello mytry

.PHONY: all clean
