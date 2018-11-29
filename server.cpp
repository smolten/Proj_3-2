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
    int state;        // The state of this session, 0 is "OFFLINE", etc.

};


map<string, session*> session_pointers_by_client_id;
struct session *current_session;
struct session* get_session_from_client_id(const char* client_id) {
    return NULL;
}
const char* get_client_id_from_string(const char* string) {
    return "";
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
string generate_token() {
    string token = "";
    for(int i=0; i<6; i++) {
        char c = (char) (65+rand() % 28);
        token += c;
    }
    return token;
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


		// Now we know there is an event from the network
        // TODO: Figure out which event and process it according to the
        // current state of the session referred.

        bool respond = false;
        string username, password, ip, port;
        Event event = parse_event_from_string(recv_string);
        switch(event) {
            case Login:
                username = parse_until(recv_string, '-');
                if (count_char(recv_string, '<') != 3 || count_char(recv_string, '>') != 4) {
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
                    sprintf(send_buffer, "server->%s#ERROR: Bad User/ Pass combo.", username.c_str());
                    break;
                } else {
                    respond = true;
                    sprintf(send_buffer, "server->%s#Success<%s>", username.c_str(), generate_token().c_str());
                    break;
                }

            break;
            case Logoff:
                username = parse_until(recv_string, '-');
                printf("%s trying to log off\n", username.c_str());
                printf("UNIMPL\n");
                break;
            case Message:
                printf("UNIMPL\n");
                break;
            default:
                printf("UNIMPL\n");
                break;
        }

        if (respond) {
            ret = sendto(sockfd_tx, send_buffer, sizeof(send_buffer), 0, 
                    (struct sockaddr *) &cli_addr, sizeof(cli_addr));
            printf("SENT: %s\n", send_buffer);
        }

        client_id = get_client_id_from_string(recv_buffer);
        current_session = get_session_from_client_id(client_id);
        if (current_session != NULL) {
            // Record the last time that this session is active.
            current_session->last_time = time(NULL);

            if (event == 0 /* login event */) {
                if (current_session->state == 0 /* OFFLINE state */) {

                    // TODO: take the corresponding action
                } else if (current_session->state == 0) {

                    // TODO: hand errors if the event happens in some state
                    // that is not expected.

                }

            } else if (event == 0) {

                // TODO: process other events

            }
        }

        time_t current_time = time(NULL);

        // Now you may check the time of all clients. 
        // For each session, if the current time has passed 5 minutes plus 
        // the last time of the session, the session expires.
        // TODO: check session liveliness


    } // This is the end of the while loop

    return 0;
} // This is the end of main()

