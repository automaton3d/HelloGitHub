CC      = gcc
CXX     = g++
CFLAGS  = -O2 -Wall -Wextra
CXXFLAGS= -O2 -Wall -Wextra
LDFLAGS = -lm

all: hello mytry

hello: hello.cpp
	$(CXX) $(CXXFLAGS) -o $@ $<

mytry: mytry.c
	$(CC) $(CFLAGS) -o $@ $< $(LDFLAGS)

clean:
	rm -f hello mytry

.PHONY: all clean
