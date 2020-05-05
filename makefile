CC = g++
FLAGS = -Wall -std=c++11

.PHONY: client server

client:
	$(CC) $(FLAGS) client/*.cpp -lncurses -o $@.out
server:
	$(CC) $(FLAGS) server/*.cpp -o $@.out
all: client server