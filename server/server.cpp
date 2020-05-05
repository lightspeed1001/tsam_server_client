#include "server.h"
#include <string.h>
#include <iostream>
#include <sys/types.h>  
#include <sys/socket.h>
#include <netinet/in.h>
#include <stdlib.h>  
#include <errno.h>  
#include <unistd.h>
#include <arpa/inet.h> 
#include <sstream>
#include <signal.h>

using namespace std;

#define GROUP_INITIALS "MS"
#define SERVER_PREFIX  "SERVER> "
#define ERR_NAME       SERVER_PREFIX"That name is invalid!"


Server::Server(const int port)
{
    _single_port_mode = true;
    _require_knock    = false;
    _port             = port;
    
    SetDefaults();
}

Server::Server(const int min_port, const int max_port, const int port_count = 3)
{
    _single_port_mode = false;
    _require_knock    = true;
    _min_port         = min_port;
    _max_port         = max_port;
    _port_count       = port_count;
    
    SetDefaults();
}

void Server::SetDefaults()
{
    _welcome_no_name = WELCOME_NO_NAME;
    _welcome = WELCOME;
    _bye     = BYE;

    SetServerID();
}

int Server::SetWelcomeMessage(string msg)
{
    _welcome = msg;
    return 0;
}

int Server::SetDisconnectMessage(string msg)
{
    _bye = msg;
    return 0;
}

int Server::SetKnockSequence(vector<int> seq)
{
    _port_sequence = seq;
    return 0;
}

string Server::GetCurrentTimeString(string format)
{
    //I hate c++
    time_t rawtime;
    struct tm * timeinfo;
    char buffer [80];

    time (&rawtime);
    timeinfo = localtime (&rawtime);

    strftime (buffer,80,format.c_str(),timeinfo);
    string retval(buffer);
    return retval;
}

int Server::StartServer()
{
    signal(SIGPIPE, SIG_IGN);
    if(_single_port_mode)
    {
        //Make sure port is free
        if(!IsPortAvailable(_port))
        {
            cout << "Please choose a different port" << endl;
            return -1;
        }
        
        struct sockaddr_in address;
        socklen_t address_length = sizeof(address);
        _master_socket = SetupSocketOnPort(_port, &address);
        cout << "Setup done. Waiting for connections..." << endl;

        //Various things we need in the infinite loop
        int max_socket_descriptor, activity;
        
        fd_set readfds;
        while(true)
        {
            //Clear the socket set
            FD_ZERO(&readfds);
            //Add the master socket back in
            FD_SET(_master_socket, &readfds);
            //Used in the select function to read from clients
            max_socket_descriptor = _master_socket;
            
            //add our clients back to the fd_set
            for(CLIENT_MAP::iterator it = _clientsmap.begin(); it != _clientsmap.end(); ++it)
            {
                FD_SET(it->first, &readfds);
                
                if(it->first > max_socket_descriptor) max_socket_descriptor = it->first;
            }

            //wait forever for something to happen
            activity = select(max_socket_descriptor + 1, &readfds, NULL, NULL, NULL);
            cout << "Something yo" << endl;
            if((activity < 0) && (errno!=EINTR))
            {
                cout << "Something went wrong with the select function" << endl;
            }

            //If it's not a new connection, it's something else on some other socket
            HandleClientSockets(&readfds);

            //If something happened on the master socket, it's a new incoming connection
            //Since this is single port mode, just accept it
            if(FD_ISSET(_master_socket, &readfds))
            {   
                cout << "Incoming connection!" << endl;
                int new_socket = accept(_master_socket, (struct sockaddr*)&address, &address_length);
                if(new_socket < 0)
                {
                    cout << "Couldn't accept incoming connection. Try again?" << endl;
                }
                else
                {
                    WelcomeUser(address, new_socket);
                }
            }//new connection
            
        }
    }
    else
    {
        //First, we need to find n sequential ports
        for(int i = _min_port; i < _max_port; i++)
        {
            //Make sure it's not in use
            if(IsPortAvailable(i))
            {
                _ports.push_back(i);
                if(_ports.size() == (size_t)_port_count) break;
            }
            else
            {
                //Was in use, the next ports won't be sequential, so we purge the set
                _ports.clear();
            }
        }
        //Sort it, because why not
        sort(_ports.begin(), _ports.end());

        //Now that we found our sequential ports, we need to create the sequence
        //The default sequence is 3 1 2
        if(_port_sequence.empty())
            SetKnockSequence(KNOCK_SEQUENCE);
        
        //Make sure we found enough ports
        if(_port_sequence.size() != _ports.size())
        {
            cout << "Invalid sequence size or not enough open ports in range" << endl;
            return -1;
        }

        //Convert our list of ports into the correct sequence
        vector<int> tmp;
        for(size_t i = 0; i < _ports.size(); i++)
        {
            int index = _port_sequence[i];
            tmp.push_back(_ports[index]);
        }
        _port_sequence = tmp;

        //These are used later, to check for knocking
        //It's just easy access
        int last_port = _port_sequence[_port_sequence.size() - 1];
        int first_port = _port_sequence[0];

        //We need to track a lot of info, so I made a struct for it
        vector<ConnectionInfo> c_info;
        //Register the sockets we need
        for(size_t i = 0; i < _port_sequence.size(); i++)
        {
            ConnectionInfo c;
            struct sockaddr_in address;
            socklen_t address_length = sizeof(address);
            c.address = address;
            c.address_length = address_length;
            c.port = _port_sequence[i];
            c.socket = SetupSocketOnPort(_port_sequence[i], &c.address); //Magic happens here I suppose
            c.sequence = i;
            c_info.push_back(c);
            
            if(c.socket < 0)
            {
                cout << "Something went horribly wrong" << endl;
                return -1;
            }
        }

        //Output our sequence for easy use
        cout << "Your sequence is: " << endl;
        for(auto derp : c_info)
        {
            cout << "port: " << derp.port << "; sequence: " << derp.sequence << endl;
        }

        //Keep track of the largest socket
        int max_socket_descriptor, activity;
        //Our set of connections
        fd_set readfds;
        while(true)
        {
            //Zero it out so it's clean
            FD_ZERO(&readfds);
            
            max_socket_descriptor = c_info[0].socket;
            
            //Add the master sockets back in
            for(auto info : c_info)
            {
                FD_SET(info.socket, &readfds);
                if(info.socket > max_socket_descriptor) max_socket_descriptor = info.socket;
            }

            //add our clients back to the fd_set
            cout << "num clients = " << _clientsmap.size() << endl;;
            for(CLIENT_MAP::iterator it = _clientsmap.begin(); it != _clientsmap.end(); ++it)
            {
                FD_SET(it->first, &readfds);
                cout << it->first << endl;
                if(it->first > max_socket_descriptor) max_socket_descriptor = it->first;
            }

            //wait forever for something to happen
            //Acknowledgement: I do not have any timeouts in my code
            //This means that any one can prod our ports over the period of a few days and hope to connect. In theory at least.
            activity = select(max_socket_descriptor + 1, &readfds, NULL, NULL, NULL);
            
            if(activity < 0) 
            {   
                cout << "select went wrong" << endl;
                return -1;
            }

            //Deal with our client connections
            HandleClientSockets(&readfds);
            
            //If something happened on the master socket, it's a new incoming connection
            //We need to enforce our sequence
            for(size_t i = 0; i < c_info.size(); i++)
            {
                ConnectionInfo c = c_info[i];
                //Find the socket that was connected to
                if(FD_ISSET(c.socket, &readfds))
                {
                    //Get the incoming IP
                    int new_socket = accept(c.socket, (struct sockaddr*)&c.address, &c.address_length);
                    string new_ip = inet_ntoa(c.address.sin_addr);
                    
                    //First port hit, add to our map of clients
                    //I used states to determine what port the client has tried already
                    //The states are 1 (brand new), 2 (got the second port), 3 (you're in, not used)
                    if(c.port == first_port)
                    {
                        cout << new_ip << " is on the first port" << endl;
                        //grats, you're on the first state
                        _clientstates[new_ip] = 1;
                        close(new_socket);
                    }
                    //Check to see if the connection is to the last port
                    else if(c.port == last_port)
                    {
                        cout << new_ip << " is poking the last port" << endl;
                        
                        //If this returns something, the user has already poked a few ports
                        auto found = _clientstates.find(new_ip);
                        if(found != _clientstates.end())
                        {
                            //Make sure the ip is in the correct state
                            if((size_t)_clientstates[new_ip] == _port_sequence.size() - 1)
                            {
                                //Grats, you got in
                                cout << new_ip << " got in with the right sequence" << endl;
                                WelcomeUser(c.address, new_socket);
                            }
                            //Remove the client from the state checker. They're already in and we don't need to keep tabs on them
                            _clientstates.erase(found);
                        }
                    }
                    else
                    {
                        cout << new_ip << " is poking " << c.port << endl;
                        //It's not the first or last, make sure dude is on the correct stage
                        auto found = _clientstates.find(new_ip);
                        if(found != _clientstates.end())
                        {
                            //Make sure the client is on the right state to continue
                            cout << "Are you on the right track? " << _clientstates[new_ip] << " " << c.sequence << endl;
                            if(c.sequence == _clientstates[new_ip])
                            {
                                cout << "You're on the right track" << endl;
                                _clientstates[new_ip]++;
                            }
                            else //wrong state, goodbye
                            {
                                cout << "you're not on the right track" << endl;
                                _clientstates.erase(found);
                            }
                                
                        }
                        //either way, close the connection
                        close(new_socket);
                    }
                }

            }
        }
    }
    
    return 0;
}

int Server::WelcomeUser(sockaddr_in address, int sock)
{
    //Store the IP because why not
    string new_ip = inet_ntoa(address.sin_addr);
    cout << "New connection made! Socket: " << sock << "; IP: " << new_ip << ";" << endl;

    //Create a name for our new user
    //defaults to anon[socket]
    //users can change with either /connect or /name
    string new_username = "anon" + to_string(sock);

    //Construct the welcome message to the new client
    string w_msg = SERVER_PREFIX + _welcome_no_name; //Welcome <new user>! Please choose a name
    StrEZReplace(w_msg, REPLACE_NAME, new_username);
    //Send it
    size_t msg_fail = SendMessage(sock, w_msg);
    if(msg_fail != w_msg.length() )
    {
        cout << "Error sending welcome message to the new user" << endl;
        return -1;
    }

    //Let people know some one new joined
    //This message is slightly different from the one the new client gets
    w_msg = SERVER_PREFIX + _welcome; //Welcome anon!
    StrEZReplace(w_msg, REPLACE_NAME, new_username);
    BlastMessageExcept(w_msg, sock);
    //We haven't added the new user to our map, so he won't get welcome twice

    //Add our new user to the map
    _clientsmap[sock] = "anon" + to_string(sock);
    return 0;
}

//Deals with all of the client related sockets
int Server::HandleClientSockets(fd_set* readfds)
{
    //Deleting inside that loop is bad
    vector<int> delete_us;
    //Go through all of the sockets we have
    for(CLIENT_MAP::iterator it = _clientsmap.begin(); it != _clientsmap.end(); ++it)
    {
        if(FD_ISSET(it->first, readfds))
        {
            cout << "Activity on " << it->first << endl;

            //Get the message
            //returns (message, success)
            auto msg_suc = ReadMessage(it->first);
            string msg = get<0>(msg_suc);
            //For debugging mostly
            cout << msg << endl;
            bool success = get<1>(msg_suc);

            //If the read failed, drop the connection
            if(!success)
            {
                delete_us.push_back(it->first);
            }
            else
            {
                //Check if it's a command
                if(msg[0] == CMD_TOKEN[0])
                {
                    cout << "handling cmd" << endl;
                    HandleCommand(msg, it->first);
                }
                else
                {
                    //If it's just a message, blast it to every one
                    //Construct the string first, so that people know who sent the message 
                    msg = it->second + ": " + msg;
                    BlastMessage(msg);
                }
            }
        }
    }
    //Go through the clients we want to drop and drop them
    for(int del : delete_us)
    {
        DisconnectUser(del);
    }
    //Cleanup
    delete_us.clear();

    return 0;
}

//Creates a socket connection
//Returns the socket we made
int Server::SetupSocketOnPort(int port, struct sockaddr_in* addr)
{
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    //Create master socket
    cout << "Creating master socket..." << endl;
    
    //Make sure nothing went wrong
    if(sock == 0)
    {
        cout << "Couldn't create socket. Try different port?" << endl;
        return -1;
    }
    //Allow more than one connection
    int option = 1;
    cout << "Allowing more than one connection..." << endl;
    int multi_con = setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, (char*)&option, sizeof(option));
    if(multi_con < 0)
    {
        cout << "Couldn't allow more than one connection. Aborting" << endl;
        return -1;
    }

    addr->sin_family = AF_INET;           //use ipv4
    addr->sin_addr.s_addr = INADDR_ANY;   //allow any ip to connect
    addr->sin_port = htons(port);         //listen to port
    
    cout << "Binding master socket to localhost:" << port << "..." << endl;
    if(bind(sock, (struct sockaddr*) addr, sizeof(*addr)) < 0)
    {
        cout << errno << endl;
        cout << "Error binding socket" << endl;
        return -1;
    }

    cout << "Setting max waiting connections..." << endl;
    if(listen(sock, MAX_WAIT_CONNECTIONS) < 0)
    {
        cout << "Error listening to waiting connections" << endl;
        return -1;
    }

    return sock;
}

void Server::DisconnectUser(int sock)
{
    //Get the username, so we can let people know who dropped
    string username = _clientsmap[sock];
    auto it = _clientsmap.find(sock);
    if(it == _clientsmap.end())
    {
        //Client does not exist
        cout << "Client doesn't exist" << endl;
        return;
    }
    cout << "Disconnecting socket " << it->first << "..." << endl;
    close(it->first);
    //Remove from our map, so we don't prod that socket
    _clientsmap.erase(it);
    //Let people know who left
    BlastMessage(SERVER_PREFIX + username + " disconnected");
}

//This is run every startup and also by sending /change_id
void Server::SetServerID()
{
    //Run fortune -s and pipe it to stderr
    FILE* pipe = popen("fortune -s 2>&1", "r");
    array<char, 256> buffer;
    string result = "";
    //Read from stderr
    std::cout << "Reading fortune..." << std::endl;
    while (fgets(buffer.data(), 128, pipe) != NULL) 
    {
        result += buffer.data();
    }
    cout << "Fortune: " << result << endl;
    //Trims newline characters
    result.erase(std::remove(result.begin(), result.end(), '\n'), result.end());
    
    //Construct the new id
    string new_id = result + "-" + GROUP_INITIALS + "-" + GetCurrentTimeString("%F %T");
    cout << "New id: " << new_id << endl;
    _server_id = new_id;
}

//Handles any commands the users send
void Server::HandleCommand(string msg, int sock)
{
    cout << "erasing token" << endl;
    //Removes the starting /
    msg.erase(msg.begin());
    //Probably a better method to do this, but this is what I came up with
    //Lots of if checks to find the correct command
    if(StartsWithCaseInsensitive(msg, CMD_NAME) || StartsWithCaseInsensitive(msg, CMD_CONNECT))
    {
        //Changes the client's name
        string new_name = msg.substr(msg.find(' ')+1);
        cout << new_name << endl;
        //Make sure the name is legit
        if(new_name.find(' ') == string::npos && new_name.length() <= 9)
        {
            string old_name = _clientsmap[sock];
            //update our map
            _clientsmap[sock] = new_name;
            //Let people know who changed and to what
            BlastMessage(SERVER_PREFIX + old_name + " changed to " + new_name);
        }
        else
        {
            //Name is invalid
            SendMessage(sock, ERR_NAME);
        }
    }
    else if(StartsWithCaseInsensitive(msg, CMD_WHO))
    {
        //sends them the list of connected users
        //start with number of users
        SendMessage(sock, to_string(_clientsmap.size()));
        //then send them all the names
        for(auto usr : _clientsmap)
        {
            SendMessage(sock, usr.second);
        }
    }
    else if(StartsWithCaseInsensitive(msg, CMD_CHANGE_ID))
    {
        //Simply changes the server id
        SetServerID();
        BlastMessage(string(SERVER_PREFIX) + " ID changed: " + _server_id);
    }
    else if(StartsWithCaseInsensitive(msg, CMD_ID))
    {
        //Returns the server id
        msg = SERVER_PREFIX + GetServerID();
        SendMessage(sock, msg);
    }
    else if(StartsWithCaseInsensitive(msg, CMD_LEAVE))
    {
        //Handled client side
        //DisconnectUser(sock);
    }
    else if(StartsWithCaseInsensitive(msg, CMD_MSG))
    {
        //Sends a private message to some one

        //Get the target user
        msg.erase(0, string(CMD_MSG).length() + 1);
        auto next_space = msg.find(' ');
        string to = msg.substr(0, next_space);
        msg.erase(0, next_space);
        //Send the message to the person (if they exist)
        PrivateMessage(sock, to, msg);
    }
    cout << "done handling cmd" << endl;
}

//Found this on SO
//Checks if the string starts with some other string
//Case insensitive, because I like it better that way
bool Server::StartsWithCaseInsensitive(std::string mainStr, std::string toMatch)
{
    // Convert mainStr to lower case
    std::transform(mainStr.begin(), mainStr.end(), mainStr.begin(), ::tolower);
    // Convert toMatch to lower case
    std::transform(toMatch.begin(), toMatch.end(), toMatch.begin(), ::tolower);

    if(mainStr.find(toMatch) == 0)
        return true;
    else
        return false;
}

//Found this on StackOverflow.
//Faster than writing my own. 
//Takes in a string, something to find and something to replace what we found with
bool Server::StrEZReplace(std::string& str, const std::string& from, const std::string& to) 
{
    size_t start_pos = str.find(from);
    if(start_pos == std::string::npos)
        return false;
    str.replace(start_pos, from.length(), to);
    return true;
}

bool Server::IsPortAvailable(int port)
{
    //Quick and dirty method
    //Just connect to localhost using that port
    //If it fails, it's not in use

    cout << "Checking availability of port " << port << "..." << endl;

    struct sockaddr_in client;
    client.sin_family = AF_INET;
    client.sin_port = htons(port);
    inet_aton("localhost", &client.sin_addr);

    int sock = (int)socket(AF_INET, SOCK_STREAM, 0);
    int result = connect(sock, (struct sockaddr*) &client, sizeof(client));
    //cout << result << endl;
    if(result < 0) //returns -1 on error
    {
        cout << "Port " << port << " is available" << endl;
        close(sock);
    
        return true;
    }
    close(sock);
    
    cout << "Port " << port << " is NOT available" << endl;
    return false;
}

//Sends a private message from some one to some one
//No checking if it's the same person
int Server::PrivateMessage(int from, string to, string msg)
{
    msg = _clientsmap[from] + "->" + to + ": " + msg;
    for(CLIENT_MAP::iterator it = _clientsmap.begin(); it != _clientsmap.end(); ++it)
    {
        if(it->second == to)
        {
            SendMessage(from, msg);
            return SendMessage(it->first, msg);
        }
    }
    return -1;
}

//Sends a message to every one connected
void Server::BlastMessage(string msg)
{
    cout << "Blasting " << msg << " to everyone" << endl;
    for(CLIENT_MAP::iterator it = _clientsmap.begin(); it != _clientsmap.end(); ++it)
    {
        SendMessage(it->first, msg);
    }
}

//Sends a message to every one except for one dude
//Not used?
void Server::BlastMessageExcept(string msg, int sock)
{
    cout << "Blasting " << msg << " to everyone except for " << sock << endl;
    for(CLIENT_MAP::iterator it = _clientsmap.begin(); it != _clientsmap.end(); ++it)
    {
        if(it->first != sock)
            SendMessage(it->first, msg);
    }
}
