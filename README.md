# TSAM Project 2
Project 2 for Computer Networks at Reykjavik University. 

The aim was to make a server/client application that adhered to some standard of communications. I misunderstood this somehow and ended up making a very simple chatroom, reminiscent of an IRC client.

## Requirements
* g++
* libncurses

Works best on Linux or the Debian shell from Windows  (WSL). It can work in the Windows shell, but you'll have to take some extra steps for libncurses to work properly.

## Compiling and running the project
To compile both server and client, do `make all`. If you just want the server, `make server` and `make client` for just the client.
