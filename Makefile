CC=gcc
CXX=g++
CFLAGS+=-O3 -Wall
CXXFLAGS=$(CFLAGS)

sidstream: sidstream.o cpu.o
	gcc -o $@ $^
	strip $@
