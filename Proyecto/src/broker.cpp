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

#include "broker.hpp"
#include "MQTT.hpp"

using namespace std;

Broker::Broker(int portnumber)
{
    serv_sockfd = socket(AF_INET, SOCK_STREAM, 0);
    assert(serv_sockfd >= 0 && "Error opening socket");
    bzero((char *)&serv_addr, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = INADDR_ANY;
    serv_addr.sin_port = htons(portnumber);
    assert(bind(serv_sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) >= 0 && "Error on binding");
    listen(serv_sockfd, 5);
    cout << "Broker listening on port " << portnumber << endl;
}

void Broker::toclient_routine(Client* client){
    cout << "Client thread started for client: " << inet_ntoa(client->addr.sin_addr) << ":" << ntohs(client->addr.sin_port) << endl;
    
    // Receive messages from the client
    MQTTMsg *msg = new MQTTMsg();
    int n = rcvMsg(client->sockfd, msg);
    assert(n >= 0 && "Error receiving message");
    // process CONNECT message
    // send CONNACK message

    ///////////
    // Routine:
    // Wait for a message from the client...
    // If SUBSCRIBE, send SUBACK message and add client to the list of subscribers of that topic
    // If UNSUBSCRIBE, remove client from the list of subscribers of that topic
    // If PUBLISH, send PUBLISH message to all subscribers
    

    

}

void Broker::acceptClients()
{
    int sockfd;
    struct sockaddr_in client_addr;
    socklen_t client_len = sizeof(client_addr);

    while (true)
    {
        sockfd = accept(serv_sockfd, (struct sockaddr *)&client_addr, &client_len);
        assert(sockfd >= 0 && "Error on accept"); // Modificar, manejo de error sin romper todo.
        Client *newclient = new Client(sockfd, client_addr, client_len);
        this->clients_active.push_back(newclient);
        this->client_thread_active.emplace_back(Broker::toclient_routine,newclient);
        cout << "New client connected: " << inet_ntoa(client_addr.sin_addr) << ":" << ntohs(client_addr.sin_port) << endl;
    
    }
}

Broker::~Broker()
{
    close(serv_sockfd);
}

