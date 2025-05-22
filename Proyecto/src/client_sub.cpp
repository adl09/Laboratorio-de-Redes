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
#include <chrono>
#include <set>

#include "MQTT.hpp"

using namespace std;

uint8_t buffer[BUFF_SIZE];

void subscriber_routine(int sockfd, vector<string> *topics)
{
    CONNECT *con_msg = nullptr;
    CONNACK *conack_msg = nullptr;
    SUBSCRIBE *sub_msg = nullptr;
    SUBACK *suback_msg = nullptr;
    PINGREQ *pingreq_msg = nullptr;
    PINGRESP *pingresp_msg = nullptr;
    PUBLISH *pub_msg = nullptr;
    DISCONNECT *disc_msg = nullptr;

    int msgsize, remlen, totalRecvd;
    Type type;
    uint8_t type_flags, keepalive;

    // CONNECT/CONNACK
    connection_procedure(sockfd, buffer);
    

    // SUBSCRIBE/SUBACK
    subscribe_procedure(sockfd, buffer, topics);
    
    




    // // start timer
    // auto start = std::chrono::high_resolution_clock::now();



    // while (true)
    // {
    //     while(true){
    //         totalRecvd = rcvMsg(sockfd, &type_flags, &remlen, buffer, BUFF_SIZE);
    //         type = (Type)(type_flags >> 4);
            
    //         if(type == TYPE_PUBLISH){
    //             pub_msg = new PUBLISH(type_flags,remlen);
    //             pub_msg->fromBuffer(buffer, totalRecvd);
    //             cout << "Received PUBLISH message: " << pub_msg->topic_name << " " << pub_msg->value << endl;
    //             type = TYPE_RESERVED;
    //         }
    
    //     }

    //     if (keepalive > 0)
    //     {
    //         // Wait for keepalive
    //         auto now = std::chrono::high_resolution_clock::now();
    //         auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - start).count();
    //         if (elapsed >= keepalive)
    //         {
    //             // Send PINGREQ message
    //             pingreq = new PINGREQ();
    //             pingreq->type = TYPE_PINGREQ;
    //             pingreq->remaining_length = 0;
    //             msgsize = pingreq->toBuffer(buffer, BUFF_SIZE);
    //             int n = sndMsg(sockfd, buffer, msgsize);
    //             start = std::chrono::high_resolution_clock::now(); // Reset timer
    //             // // Wait for PINGRESP message
    //             // n = rcvMsg(sockfd, buffer, &msgsize, buffer, BUFF_SIZE);
    //             // if (n > 0)
    //             // {
    //             //     pingresp = new PINGRESP();
    //             //     pingresp->fromBuffer(buffer, msgsize);
    //             //     cout << "PINGRESP received" << endl;
    //             // }
    //         }
    //     }
    // }
}

int main(int argc, char *argv[])
{
    assert((argc >= 4 || argc <= 5) && "Usage: ./client_sub <hostname> <port> <topics list>");

    int sockfd;
    int portno = atoi(argv[2]);
    struct sockaddr_in serv_addr;
    struct hostent *server;

    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    assert(sockfd >= 0 && "Error opening socket");
    server = gethostbyname(argv[1]);
    assert(server != NULL && "Error, no such host");
    bzero((char *)&serv_addr, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    bcopy((char *)server->h_addr, (char *)&serv_addr.sin_addr.s_addr, server->h_length);
    serv_addr.sin_port = htons(portno);
    assert(connect(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) >= 0 && "Error connecting");

    vector<string> topics;
    for (int i = 3; i < argc; ++i)
    {
        topics.push_back(argv[i]);
    }

    subscriber_routine(sockfd, &topics);

    close(sockfd);

    return 0;
}
