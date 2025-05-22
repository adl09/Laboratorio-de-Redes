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


void publisher_routine(int sockfd, string *topic, string *value, int *retain_flag=0)
{
    CONNECT *con_msg = nullptr;
    CONNACK *conack_msg = nullptr;
    PUBLISH *pub_msg = nullptr;
    DISCONNECT *disc_msg = nullptr;
    int msgsize, remlen, totalRecvd;

    // CONNECT/CONNACK
    connection_msgs(sockfd, buffer);

    // SEND PUBLISH message
    uint8_t type_flags = FPUBLISH_DEF_TYPEFLAG | (*retain_flag << 0); // 1 for RETAIN
    pub_msg = new PUBLISH(topic, value, type_flags);
    msgsize = (pub_msg)->toBuffer(buffer, BUFF_SIZE);
    msgsize = sndMsg(sockfd, buffer, msgsize);
    delete pub_msg;
    pub_msg = nullptr;
    
    // SEND DISCONNECT message
    disc_msg = new DISCONNECT();
    msgsize = ((DISCONNECT*)disc_msg)->toBuffer(buffer, BUFF_SIZE);
    msgsize = sndMsg(sockfd, buffer, msgsize);
    delete disc_msg;
    disc_msg = nullptr;
}


int main(int argc, char *argv[])
{
    assert((argc >= 5 || argc <= 7) && "Usage: ./client_pub <hostname> <port> <topic> <message> <retain?>");
    
    int sockfd;
    int portno = atoi(argv[2]);
    struct sockaddr_in serv_addr;
    struct hostent *server;
    int retain_flag = 0;

    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    assert(sockfd >= 0 && "Error opening socket");
    server = gethostbyname(argv[1]);
    assert(server != NULL && "Error, no such host");
    bzero((char *) &serv_addr, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    bcopy((char *)server->h_addr, (char *)&serv_addr.sin_addr.s_addr, server->h_length);
    serv_addr.sin_port = htons(portno);
    assert(connect(sockfd, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) >= 0 && "Error connecting");

    string topic = argv[3];
    string value = argv[4];
    if (argc == 6){
        retain_flag = atoi(argv[5]);
    }
    publisher_routine(sockfd, &topic, &value, &retain_flag);
    
    cout << "Message <" << value << "> sent to topic <" << topic << "> ";
    if (retain_flag)
        cout << "/ Retained" << endl;    
    
    close(sockfd);
        
    return 0;
}
