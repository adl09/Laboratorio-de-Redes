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

#include "MQTT.hpp"

using namespace std;

uint8_t buffer[BUFF_SIZE];

void subscriber_routine(int sockfd, vector<string> *topics)
{
    CONNECT *connect = nullptr;
    SUBSCRIBE *subscribe = nullptr;
    SUBACK *suback = nullptr;
    PUBLISH *publish = nullptr;
    CONNACK *connack = nullptr;
    PINGREQ *pingreq = nullptr;
    PINGRESP *pingresp = nullptr;

    // Routine is CONNECT--RECEIVE CONNACK--SEND SUBSCRIBE--RECEIVE SUBACK
    // STATIONARY: RECEIVE PUBLISHES(...)
    // IF KEEPALIVE WITHOUT MESSAGES, SEND PINGREQ -- WAIT PINGRESP -- BACK TO STATIONARY
    // IF CTRL+D -- SEND DISCONNECT REQUEST

    // Create CONNECT
    connect = new CONNECT();
    int keepalive = connect->keep_alive;
    int msgsize = connect->toBuffer(buffer, BUFF_SIZE);

    // Send the message
    int n = sndMsg(sockfd, buffer, msgsize);
    cout << "snd return: " << n << endl;

    sleep(1); // Wait for CONNACK message
    // Receive CONNACK message

    // // Subscribe to the topic
    subscribe = new SUBSCRIBE(topics);
    msgsize = ((SUBSCRIBE *)subscribe)->toBuffer(buffer, BUFF_SIZE);
    cout << "SUBSCRIBE message size: " << msgsize << endl;
    // Send the message
    n = sndMsg(sockfd, buffer, msgsize);

    // Receive SUBACK message

    // start timer
    auto start = std::chrono::high_resolution_clock::now();



    while (true)
    {
        if (keepalive > 0)
        {
            // Wait for keepalive
            auto now = std::chrono::high_resolution_clock::now();
            auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - start).count();
            if (elapsed >= keepalive)
            {
                // Send PINGREQ message
                pingreq = new PINGREQ();
                pingreq->type = TYPE_PINGREQ;
                pingreq->remaining_length = 0;
                msgsize = pingreq->toBuffer(buffer, BUFF_SIZE);
                int n = sndMsg(sockfd, buffer, msgsize);
                start = std::chrono::high_resolution_clock::now(); // Reset timer
                // // Wait for PINGRESP message
                // n = rcvMsg(sockfd, buffer, &msgsize, buffer, BUFF_SIZE);
                // if (n > 0)
                // {
                //     pingresp = new PINGRESP();
                //     pingresp->fromBuffer(buffer, msgsize);
                //     cout << "PINGRESP received" << endl;
                // }
            }
        }
    }
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
        topics.emplace_back(argv[i]);
    }

    subscriber_routine(sockfd, &topics);

    close(sockfd);

    return 0;
}
