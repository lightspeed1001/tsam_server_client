#include "server.h"

#define PORT_MIN 31337

using namespace std;

int main(int argc , char *argv[])   
{
    //Server s(PORT_MIN);
    Server s(PORT_MIN, PORT_MIN+100, 3);
    s.StartServer();
}