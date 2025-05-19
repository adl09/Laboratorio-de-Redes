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


void publisher_routine(int sockfd, const char* topic)
{   
    // Create MQTT msg
    MQTTMsg *msg = new MQTTMsg();
    
    // Send the message
    int n = sndMsg(sockfd, msg);
    
    assert(n >= 0 && "Error sending message");


}


int main(int argc, char *argv[])
{
    assert((argc >= 5 || argc <= 6) && "Usage: ./client_pub <hostname> <port> <topic> <message>");
    
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

    // Send CONNECT message
    // Wait for CONNACK message

    // Send PUBLISH message:
    // format: getline <topic> <message> <retain>
    publisher_routine(sockfd, argv[3]);
    



    //getchar();

    // printf("Please enter the message: ");
    // bzero(buffer,256);

    // while (true) {
    //     printf("Please enter the message: ");
    //     bzero(buffer, 256);
    //     fgets(buffer, 255, stdin);
    //     n = write(sockfd, buffer, strlen(buffer));
    //     assert(n >= 0 && "Error writing to socket");
    // }
    // assert(n >= 0 && "Error writing to socket");

    close(sockfd);
        
    return 0;
}
