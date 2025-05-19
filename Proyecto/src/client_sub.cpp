#include <iostream>
#include <unistd.h>
#include <sys/types.h> 
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <thread>
#include <mutex>
#include <cstring>
#include <cassert>
#include <vector>
#include <netdb.h>

#include "MQTT.hpp"


using namespace std;

void subscriber_routine(int sockfd, const char* topic)
{
    // // Subscribe to the topic
    // MQTTMsg *msg = new SUBSCRIBE(topic);
    // sndMsg(msg);
    // delete msg;

    // // Receive messages
    // while (true) {
    //     MQTTMsg *msg = rcvMsg(sockfd);
    //     if (msg->getType() == TYPE_PUBLISH) {
    //         PUBLISH *publish_msg = static_cast<PUBLISH*>(msg);
    //         cout << "Received message: " << publish_msg->getPayload() << endl;
    //         delete publish_msg;
    //     } else {
    //         cout << "Unknown message type: " << msg->getType() << endl;
    //         delete msg;
    //     }
    // }
}

int main(int argc, char *argv[])
{
    assert((argc >= 4 || argc <=5) && "Usage: ./client_sub <hostname> <port> <topic>");
    
    int sockfd, n;
    int portno = atoi(argv[2]);
    struct sockaddr_in serv_addr;
    struct hostent *server;
    char buffer[256];

    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    assert(sockfd >= 0 && "Error opening socket");
    server = gethostbyname(argv[1]);
    assert(server != NULL && "Error, no such host");
    bzero((char *) &serv_addr, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    bcopy((char *)server->h_addr, (char *)&serv_addr.sin_addr.s_addr, server->h_length);
    serv_addr.sin_port = htons(portno);
    assert(connect(sockfd, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) >= 0 && "Error connecting");

    subscriber_routine(sockfd, argv[3]);

    close(sockfd);
        
    return 0;
}
