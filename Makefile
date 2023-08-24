# Marcelina Oset 313940

CXXFLAGS = -std=gnu++17 -Wall -Wextra
CC = g++

all: traceroute

traceroute: traceroute.o

clean:
	rm traceroute.o

distclean:
	rm traceroute.o traceroute