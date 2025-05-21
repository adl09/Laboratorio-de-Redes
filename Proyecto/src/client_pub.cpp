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

uint8_t buffer[BUFF_SIZE];


void publisher_routine(int sockfd, string *topic, string *value)
{
    // Routine is CONNECT--WAIT CONNACK--PUBLISH--DISCONECT REQUEST
    
    // Create CONNECT
    MQTTMsg *msg = new CONNECT();
    int msgsize = ((CONNECT*)msg)->toBuffer(buffer, BUFF_SIZE);

    // Send the message
    int n = sndMsg(sockfd, buffer, msgsize);
    cout << "snd return: " << n << endl;

    sleep(1); // Wait for CONNACK message
    
    PUBLISH *msg2 = new PUBLISH(topic, value);
    
    msgsize = (msg2)->toBuffer(buffer, BUFF_SIZE);
    cout << "PUBLISH message size: " << msgsize << endl;
    // Send the message
    n = sndMsg(sockfd, buffer, msgsize);
    cout << "snd return: " << n << endl;

    // Receive PUBACK message
    sleep(2);

}


int main(int argc, char *argv[])
{
    assert((argc >= 5 || argc <= 6) && "Usage: ./client_pub <hostname> <port> <topic> <message>");
    
    int sockfd;
    int portno = atoi(argv[2]);
    struct sockaddr_in serv_addr;
    struct hostent *server;

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
    // argv[3] = topic
    string topic = argv[3];
    string value = argv[4];
    publisher_routine(sockfd, &topic, &value);
    

    close(sockfd);
        
    return 0;
}
