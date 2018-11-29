#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "event.h"
#include <string>
#include <cstring>
#include <cstdlib>
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

int get_semirandom_port_number() {
    return 32001 + rand() % 100;
}


int main() {

    //Redirect close to custom function
    std::atexit(sigintHandler);

    printf("Client starting...\n");

    int ret;
    char send_buffer[1024];
    char recv_buffer[1024];
    struct sockaddr_in serv_addr;
    struct sockaddr_in my_addr;
    int port = get_semirandom_port_number();

    sockfd_tx = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd_tx < 0) {
	printf("socket() error: %s.\n", strerror(errno));
        return -1;
    }

    // The "serv_addr" is the server's address and port number, 
    // i.e, the destination address if the client need to send something. 
    // Note that this "serv_addr" must match with the address in the 
    // UDP receive code.
    // We assume the server is also running on the same machine, and 
    // hence, the IP address of the server is just "127.0.0.1".
    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = inet_addr("127.0.0.1");
    serv_addr.sin_port = htons(32000);

    sockfd_rx = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd_rx < 0) {
    printf("socket() error: %s.\n", strerror(errno));
        return -1;
    }

    // The "my_addr" is the client's address and port number used for  
    // receiving responses from the server.
    // Note that this is a local address, not a remote address.
    memset(&my_addr, 0, sizeof(my_addr));
    my_addr.sin_family = AF_INET;
    my_addr.sin_addr.s_addr = inet_addr("127.0.0.1");
    my_addr.sin_port = htons(port);
    printf("PORT:%d\n", ntohs(my_addr.sin_port));

    // Bind "my_addr" to the socket for receiving message from the server.
    int retBind = bind(sockfd_rx, (struct sockaddr *) &my_addr, sizeof(my_addr));
    if (retBind < 0) {
        printf("bind() error: %s.\n", strerror(errno));
        return -1;
    }

    struct timeval timeout;
    timeout.tv_sec = 1;
    timeout.tv_usec = 0;
    
    bool loggedIn = false;
    Event event;
    string token = ""; // Assume the token is a 6-character string

    while (1) {
    
        // Use select to wait on keyboard input or socket receiving.
        fd_set read_set;
        FD_ZERO(&read_set);
        FD_SET(fileno(stdin), &read_set);
        FD_SET(sockfd_rx, &read_set);
        select(sockfd_rx + 1, &read_set, NULL, NULL, &timeout);

        if (FD_ISSET(fileno(stdin), &read_set)) {

            // Now we know there is a keyboard input event
            // TODO: Figure out which event and process it according to the
            // current state
            fgets(send_buffer, sizeof(send_buffer), stdin);
            string send_str(send_buffer);
            event = parse_event_from_string(send_buffer);
            
            bool send = true;
            switch(event) {
                case Login:
                    if (loggedIn == false) {
                        if (count_char(send_str, '<')+count_char(send_str, '>') != 3) {
                            printf("ERROR: Malformed login request.\n");
                            break;
                        }

                        printf("Attempting login...\n");

                        char info[32];
                        sprintf(info, "<%s><%d>", "127.0.0.1", port);
                        //printf("Appending %s to login.\n", info);
                        int end = strcspn(send_buffer,"\r\n");
                        for (int i=0; i < 32; i++) {
                            send_buffer[end+i] = info[i];
                        }
                        send_buffer[end+32] = '\0';
                    } else {
                        printf("ERROR: You're already logged in.\n");
                    }
                    break;
                case Logoff:
                    if (loggedIn) {
                        printf("You are now logged out.\n");
                        token = "";
                        loggedIn = false;
                    } else {
                        printf("ERROR: You are not logged in.\n");
                    }
                    break;
                case Message:
                    break;
                case None: send = false; break;
                case LoginSuccess: send = false; break;
            }

            if (send) {
                ret = sendto(sockfd_tx, send_buffer, sizeof(send_buffer), 0, 
                    (struct sockaddr *) &serv_addr, sizeof(serv_addr));
                printf("SENT: %s\n", send_buffer);
            }

        }

        if (FD_ISSET(sockfd_rx, &read_set)) {
            // Now we know there is an event from the network
            // TODO: Figure out which event and process it according to the
            // current state

            ret = recv(sockfd_rx, recv_buffer, sizeof(recv_buffer), 0);
            string recv_string(recv_buffer);
            printf("RECV: %s\n", recv_string.c_str());
            event = parse_event_from_string(recv_string);

            switch(event) {
                case LoginSuccess:
                    token = parse_between(recv_string, '<', '>', 0);
                    loggedIn = true;
                    printf("Logged in! Token = %s\n", token.c_str());
                    break;
                case Message:
                    break;
                default: break;
            }
        }

        // Now we finished processing the pending event. Just go back to the
        // beginning of the loop and wait for another event.


    } // This is the end of the while loop

} // This is the end of main()