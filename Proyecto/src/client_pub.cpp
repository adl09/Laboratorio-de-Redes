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
    uint8_t keepalive;

    // CONNECT/CONNACK
    connection_procedure(sockfd, buffer,&keepalive);

    // PUBLISH
    uint8_t type_flags = FPUBLISH_DEF_TYPEFLAG | (*retain_flag << 0); // 1 for RETAIN
    pub_msg = new PUBLISH(topic, value, type_flags);
    msgsize = (pub_msg)->toBuffer(buffer, BUFF_SIZE);
    msgsize = sndMsg(sockfd, buffer, msgsize);
    delete pub_msg;
    pub_msg = nullptr;
    
    // DISCONNECT
    disc_msg = new DISCONNECT();
    msgsize = ((DISCONNECT*)disc_msg)->toBuffer(buffer, BUFF_SIZE);
    msgsize = sndMsg(sockfd, buffer, msgsize);
    delete disc_msg;
    disc_msg = nullptr;
}


int main(int argc, char *argv[])
{
    if(argc < 5 || argc > 6) {
        cout << "Usage: " << argv[0] << " <hostname> <port> <topic> <value> [<retain_flag>]" << endl;
        return 1;
    }
    
    int sockfd;
    int portno = atoi(argv[2]);
    struct sockaddr_in serv_addr;
    struct hostent *server;
    int retain_flag = 0;

    // Conexión a server
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if(sockfd < 0) {
        perror("Error opening socket");
        return 1;
    }
    server = gethostbyname(argv[1]);
    if(server == NULL) {
        fprintf(stderr, "Error, no such host\n");
        return 1;
    }
    bzero((char *) &serv_addr, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    bcopy((char *)server->h_addr, (char *)&serv_addr.sin_addr.s_addr, server->h_length);
    serv_addr.sin_port = htons(portno);
    if (connect(sockfd, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0) {
        perror("Error connecting");
        return 1;
    }

    string topic = argv[3];
    string value = argv[4];
    if (argc == 6){
        retain_flag = atoi(argv[5]);
    }
    publisher_routine(sockfd, &topic, &value, &retain_flag);
        
    close(sockfd);
        
    return 0;
}
