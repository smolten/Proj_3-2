#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include <time.h>

#include "event.h"
#include <string>
#include <cstring>
#include <iterator>
#include <map>
#include <cstdlib>
#include <fstream>
#include <sstream>
using namespace std;

//Close sockets
int sockfd_tx = 0;
int sockfd_rx = 0;
void sigintHandler(void) {
    close(sockfd_tx);
    close(sockfd_rx);
    printf("sockets closed\n");
    exit(0);
}

// This is a data structure that holds important information on a session.
struct session {

    string client_id; // Assume the client ID is less than 32 characters.
    struct sockaddr_in client_addr; // IP address and port of the client
                                    // for receiving messages from the 
                                    // server.
    time_t last_time; // The last time when the server receives a message
                      // from this client.
    string token;    // The token of this session.
    OnlineState state;        // The state of this session, 0 is "OFFLINE", etc.

};

bool hasNumArguments(string buffer, int num) {
    return count_char(buffer, '<') == num && count_char(buffer, '>') == num+1;
}

map<string, session*> session_pointers_by_token;
struct session *current_session;
struct session* get_session_from_username(string username) {
    for(map<string, session*>::iterator it = session_pointers_by_token.begin();
        it != session_pointers_by_token.end();
        it++) {
        session* s = it->second;
        if (s->client_id == username)
            return s;
    }
    return NULL;
}

bool validate_login(string username, string password) {
    ifstream usersFile("USERS.txt");
    string line;
    bool matchFound = false;
    while(getline(usersFile, line)) {
        stringstream ss(line);
        string thisUser, thisPass;
        if ((ss >> thisUser >> thisPass) == false) {
            printf("ERROR: USERS.txt must have user and pass on each line.");
            break;
        } 

        if (thisUser == username && thisPass == password) {
            matchFound = true;
            break;
        }
    }

    usersFile.close();
    return matchFound;
}

int main() {

    //Redirect close to custom function
    std::atexit(sigintHandler);

    printf("Server starting...\n");

    int ret;
    struct sockaddr_in serv_addr, cli_addr;
    char recv_buffer[1024];
    char send_buffer[1024];
    int recv_len;
    socklen_t len;

    const char* client_id;

    sockfd_tx = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd_tx < 0) {
    printf("socket() error: %s.\n", strerror(errno));
        return -1;
    }

    sockfd_rx = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd_rx < 0) {
        printf("socket() error: %s.\n", strerror(errno));
        return -1;
    }

    // The servaddr is the address and port number that the server will 
    // keep receiving from.
    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    serv_addr.sin_port = htons(32000);

    int retBind = bind(sockfd_rx, (struct sockaddr *) &serv_addr, sizeof(serv_addr));
    if (retBind < 0) {
        printf("bind() error: %s.\n", strerror(errno));
        return -1;
    }

    while (1) {

        // Note that the program will still block on recvfrom()
        // You may call select() only on this socket file descriptor with
        // a timeout, or set a timeout using the socket options.

        len = sizeof(cli_addr);
        recv_len = recvfrom(sockfd_rx, // socket file descriptor
                 recv_buffer,       // receive buffer
                 sizeof(recv_buffer),  // number of bytes to be received
                 0,
                 (struct sockaddr *) &cli_addr,  // client address
                 &len);             // length of client address structure

        if (recv_len <= 0) {
            printf("recvfrom() error: %s.\n", strerror(errno));
            return -1;
        }
        recv_buffer[strcspn(recv_buffer,"\r\n")] = 0; //remove newlines
        string recv_string(recv_buffer);
        printf("RECV: \"%s\"\n", recv_string.c_str());
		
        time_t current_time = time(NULL);

        bool respond = false;
        bool loggedOff = false;
        string username, password, ip, port;
        Event event = parse_event_from_string(recv_string);
        switch(event) {
            case Login:
                username = parse_until(recv_string, '-');
                if (hasNumArguments(recv_string, 3) == false) {
                    respond = true;
                    sprintf(send_buffer, "server->%s#ERROR: Malformed login request.", username.c_str());
                    break;
                }

                password = parse_between(recv_string, '<', '>', 0);
                ip      = parse_between(recv_string, '<', '>', 1);
                port    = parse_between(recv_string, '<', '>', 2);
                cli_addr.sin_port = htons( stoi(port, NULL, 10) );

                printf("%s trying to log on. pass: %s ip: %s port: %s\n", 
                    username.c_str(), password.c_str(), ip.c_str(), port.c_str());
                if (validate_login(username, password) == false) {
                    respond = true;
                    sprintf(send_buffer, "server->%s#ERROR: Password does not match!", username.c_str());
                    break;
                } else {
                    respond = true;
                    string token = generate_token();
                    sprintf(send_buffer, "server->%s#Success<%s>", username.c_str(), token.c_str());
                    
                    //Begin session
                    current_session = (struct session*) malloc(sizeof(struct session));
                    current_session->client_id = username;
                    memcpy(&(current_session->client_addr), &cli_addr, sizeof(struct sockaddr_in));
                    current_session->last_time = current_time;
                    current_session->token = token;
                    current_session->state = Online;
                    session_pointers_by_token[token] = current_session;

                    break;
                }

            break;
            case Logoff:
                username = parse_until(recv_string, '-');
                printf("%s trying to log off\n", username.c_str());

                if (hasNumArguments(recv_string, 1)) {
                    string token = parse_between(recv_string, '<', '>', 0);
                    map<string, session*>::iterator it = session_pointers_by_token.find(token);
                    if (it != session_pointers_by_token.end()) {
                        loggedOff = true;
                        session_pointers_by_token.erase(it);
                        printf("%s successfully logged off\n", username.c_str());
                    }
                }
                if (loggedOff == false) {
                    printf("%s WAS NOT successfully logged off. Bad command or token not found.\n",
                        username.c_str());
                }
                break;
            case Message:
                username = parse_until(recv_string, '-');
                if (hasNumArguments(recv_string, 2)) {
                    string token = parse_between(recv_string, '<', '>', 0);
                    string messageId = parse_between(recv_string, '<', '>', 1);
                    map<string, session*>::iterator it = session_pointers_by_token.find(token);

                    bool noSession = it == session_pointers_by_token.end();
                    if (noSession == false) {
                        current_session = it->second;
                        current_session->last_time = current_time;
                        memcpy(&cli_addr, &(current_session->client_addr), sizeof(struct sockaddr_in));
                    }

                    if ( noSession || username != it->second->client_id) {
                        
                        //Can't find speaker
                        respond = true;
                        sprintf(send_buffer, "server->%s#<%s><%s>ERROR: Username/Token error.\n", 
                            username.c_str(), token.c_str(), messageId.c_str());
                        printf("ERROR: Token error\n");
                    } else {

                        //Send message
                        string target = parse_between(recv_string, '>', '#');
                        session* target_session = get_session_from_username(target);
                        if (target_session == NULL || target_session->state == Offline) {
                            //Can't find target
                            respond = true;
                            sprintf(send_buffer, "server->%s#<%s><%s>ERROR: Destination offline.\n", 
                                username.c_str(), token.c_str(), messageId.c_str());
                            printf("ERROR: Destination offline\n");
                        }
                    }
                }
                break;
            default:
                printf("UNIMPL\n");
                break;
        }

        if (respond) {
            ret = sendto(sockfd_tx, send_buffer, sizeof(send_buffer), 0, 
                    (struct sockaddr *) &cli_addr, sizeof(cli_addr));
            printf("SENT: \"%s\" to port %d\n", send_buffer, ntohs(cli_addr.sin_port));
        }


        // Now you may check the time of all clients. 
        // For each session, if the current time has passed 5 minutes plus 
        // the last time of the session, the session expires.
        // TODO: check session liveliness
        for(map<string, session*>::iterator it = session_pointers_by_token.begin();
            it != session_pointers_by_token.end();
            it++) {
            current_session = it->second;

            double secondsDiff = difftime(current_time, current_session->last_time);
            double fiveMinutes = 60 * 5;
            if (secondsDiff > fiveMinutes) {
                current_session->state = Offline;
                printf("%s timed out\n", current_session->client_id.c_str());
            }
        }

    } // This is the end of the while loop

    return 0;
} // This is the end of main()

