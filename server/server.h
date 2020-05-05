#ifndef _SERVER_H
#define _SERVER_H

#include <string>
#include <vector>
#include <map>
#include "../message_protocol.h"

#define DEFAULT_MIN 31337
#define DEFAULT_MAX (DEFAULT_MIN + 100)
#define MAX_WAIT_CONNECTIONS 3
#define MAX_CONNECTED_CLIENTS 64
#define KNOCK_SEQUENCE {2, 0, 1}
#define CLIENT_MAP std::map<int, std::string>
//#define HEAD_LEN int32_t

#define CMD_TOKEN       "/"
#define CMD_ID          "id"
#define CMD_CONNECT     "connect"
#define CMD_NAME        "name"
#define CMD_LEAVE       "leave"
#define CMD_WHO         "who"
#define CMD_MSG         "msg" //private message
//Not implementing msg all, since that is default behaviour
#define CMD_CHANGE_ID   "change_id"


#define REPLACE_NAME    "<NAME>"
#define WELCOME         "Welcome <NAME>!"
#define BYE             "Bye <NAME>!"
#define WELCOME_NO_NAME "Welcome <NAME>! Please register a username with " CMD_TOKEN CMD_NAME" yournamehere"

struct ClientState
{
    int state;
    std::string ip_string;
};

struct ConnectionInfo
{
    int port, socket, sequence;
    struct sockaddr_in address;
    socklen_t address_length;
};

class Server : MessageProtocol
{
    public:
        Server(const int port);     // Single port mode
        Server(const int min_port, const int max_port, const int port_count);   // Search for available ports

        int SetWelcomeMessage(std::string msg);
        int SetDisconnectMessage(std::string msg);
        int SetKnockSequence(std::vector<int> seq);      // Set the knock sequence manually. It defaults to the one defined above
        
        int StartServer();
    
    private:
        std::string _server_id;
        int _port, _min_port, _max_port, _port_count;   // _port is only used in single port mode
        int _master_socket;
        CLIENT_MAP _clientsmap;
        std::map<std::string, int> _clientstates;

        std::vector<int> _ports;             // List of our ports used when sequencing things
        std::vector<int> _port_sequence;
        bool _require_knock, _single_port_mode;
        std::string _welcome, _bye, _welcome_no_name;

        // Used during setup
        bool IsPortAvailable(int port);
        void SetDefaults();
        
        bool StrEZReplace(std::string& str, const std::string& from, const std::string& to);
        void BlastMessageExcept(std::string msg, int sock);
        void HandleCommand(std::string msg, int sock);
        bool StartsWithCaseInsensitive(std::string mainStr, std::string toMatch);
        void DisconnectUser(int sock);
        string GetCurrentTimeString(string format);
        void SetServerID();
        int SetupSocketOnPort(int port, struct sockaddr_in* addr);
        int HandleClientSockets(fd_set* readfds);
        int WelcomeUser(sockaddr_in address, int sock);

        // API compliance
        //Not listed commands are somewhere else
        std::string GetServerID() { return _server_id; }; // id
        int PrivateMessage(int from, std::string to, std::string msg); // msg <username>
        void BlastMessage(std::string msg); // sends a message to all connected users; msg all
};

#endif //server.h