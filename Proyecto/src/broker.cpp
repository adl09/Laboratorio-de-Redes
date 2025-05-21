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
uint8_t buffer[BUFF_SIZE];

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

void Broker::toclient_routine(Client *client)
{
    cout << "Client thread started for client: " << inet_ntoa(client->addr.sin_addr) << ":" << ntohs(client->addr.sin_port) << endl;

    uint8_t type_flags;
    Type type;
    int remlen;
    int totalRecvd;
    int msgsize,n;
    MQTTMsg *msg = nullptr;

    PUBLISH *pub_msg = nullptr;
    SUBSCRIBE *sub_msg = nullptr;
    SUBACK *suback = nullptr;

    try
    {
        // Receive CONNECT from client
        totalRecvd = rcvMsg(client->sockfd, &type_flags, &remlen, buffer, BUFF_SIZE);
        msg = new CONNECT(type_flags, remlen);
        msg->fromBuffer(buffer, BUFF_SIZE);
        // VER QUE HARÍA EL SERVER CON EL CONNECT (set keepalive, etc)

        // Send CONNACK message
        CONNACK *connack = new CONNACK();
        msgsize = connack->toBuffer(buffer, BUFF_SIZE);
        n = sndMsg(client->sockfd, buffer, msgsize);
        cout << "CONNACK SENT: " << n << endl;

    }
    catch (const std::runtime_error &e)
    {
        cout << "Runtime error: " << e.what() << endl;
        cout << "Client thread finished for client: " << inet_ntoa(client->addr.sin_addr) << ":" << ntohs(client->addr.sin_port) << endl;

        // VER COMO SALIR DEL THREAD CORRECTAMETE
        return;
    }

    while (true)
    {
        // Receive MESSAGES from client
        try
        {
            totalRecvd = rcvMsg(client->sockfd, &type_flags, &remlen, buffer, BUFF_SIZE);
            type = (Type)(type_flags >> 4);
        }
        catch (const std::runtime_error &e)
        {
            cout << "Error receiving message from client: " << inet_ntoa(client->addr.sin_addr) << ":" << ntohs(client->addr.sin_port) << endl;
            cout << "Skipping message..." << endl;
            break;
        }

        switch (type)
        {
        case TYPE_PUBLISH:
            // Add publication to list and send to all subscribers
            pub_msg = new PUBLISH(type_flags, remlen);
            pub_msg->fromBuffer(buffer, BUFF_SIZE);
            cout << "Received PUBLISH message: " << pub_msg->topic_name << " " << pub_msg->value << endl;
 
            // MANEJAR EL PUBLISH: AGREGAR TOPIC, ETC...

            delete pub_msg;
            type_flags = 0;
            break;

        case TYPE_SUBSCRIBE:
            // Add client to list of subscribers of the topic
            sub_msg = new SUBSCRIBE(type_flags, remlen);
            sub_msg->fromBuffer(buffer, BUFF_SIZE);
            
            // cout << "Received SUBSCRIBE message: " << sub_msg->topics->at(0).topic << endl;
            // //cout << "Received SUBSCRIBE message: " << sub_msg->topics->at(1).topic << endl;

            // MANEJAR EL SUBSCRIBE: AGREGAR CLIENTE A LA LISTA DE SUSCRIPTORES DEL TOPIC

            suback = new SUBACK(sub_msg);
            msgsize = suback->toBuffer(buffer, BUFF_SIZE);
            n = sndMsg(client->sockfd, buffer, msgsize);
            
            //cout << "SUBACK SENT: " << n << endl;

            type_flags = 0;
            break;

        case TYPE_UNSUBSCRIBE:
            // Remove client from list of subscribers of the topic
            

            type_flags = 0;
            break;

        case TYPE_PINGREQ:
            // Send PINGRESP message
            

            type_flags = 0;
            break;

        default: // Including DISCONNECT request and another CONNECT message.
            return;
            // VER COMO SALIR DEL THREAD CORRECTAMETE
        }
    }

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
        this->client_thread_active.emplace_back(Broker::toclient_routine, newclient);
    }
}

Broker::~Broker()
{
    close(serv_sockfd);
}
