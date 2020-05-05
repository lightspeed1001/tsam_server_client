Author: mikael12@ru.is
        Mikael Sigmundsson
        101291-2359
Skel:   The source code is on skel, under ~/project2/

This was originally compiled on Windows with MSYS2.
The makefile should suffice to build on skel.
The server has a lot of output, mostly for me while debugging.
I tried my best to remove any useless info, but some may have slipped through.

In case the makefile doesn't work, here are the commands I used (in MSYS2).
Note that windows automatically adds .exe to the output, while linux does not. Add .out when compiling on linux.
The client REQUIRES libcurses (skel has version 5.9). It also starts on a black screen until a key is pressed.
I ran into this when I last used ncurses and couldn't fix it then. Just press a button and it'll refresh.

Client:
    g++ -Wall -std=c++11 client/*.cpp -lncurses -o client
Server:
    g++ -Wall -std=c++11 server/*.cpp -o server

The server/client expects a specific protocol when talking. It is described in message_protocol.h,
but a tl;dr version would be: 
    send the length of the message, then send the message
    read the length of the message, then read the message

I have provided windows executables, compiled with the makefile provided.