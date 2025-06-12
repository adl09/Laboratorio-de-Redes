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
    if (serv_sockfd < 0)
    {
        cerr << "Error opening socket" << endl;
        exit(EXIT_FAILURE);
    }
    bzero((char *)&serv_addr, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = INADDR_ANY;
    serv_addr.sin_port = htons(portnumber);
    if (bind(serv_sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0)
    {
        cerr << "Error on binding" << endl;
        exit(EXIT_FAILURE);
    }
    listen(serv_sockfd, 5);
    cout << "Broker listening on port " << portnumber << endl;
}

void Broker::toclient_routine(Client *client)
{
    cout << "Client thread started for client: " << inet_ntoa(client->addr.sin_addr) << ":" << ntohs(client->addr.sin_port) << endl;

    uint8_t type_flags;
    Type type;
    int msgsize, remlen, totalRecvd;
    uint16_t keepalive=0;

    CONNECT *con_msg = nullptr;
    CONNACK *conack_msg = nullptr;
    PUBLISH *pub_msg = nullptr;
    SUBSCRIBE *sub_msg = nullptr;
    SUBACK *suback_msg = nullptr;
    UNSUBSCRIBE *unsub_msg = nullptr;
    UNSUBACK *unsuback_msg = nullptr;
    PINGRESP *pingresp_msg = nullptr;

    try
    {
        // Receive CONNECT from client
        totalRecvd = rcvMsg(client->sockfd, &type_flags, &remlen, buffer, BUFF_SIZE);
        con_msg = new CONNECT(type_flags, remlen);
        con_msg->fromBuffer(buffer, BUFF_SIZE);
        con_msg->keep_alive = keepalive;
        delete con_msg;
        con_msg = nullptr;

        // Send CONNACK message
        conack_msg = new CONNACK();
        msgsize = conack_msg->toBuffer(buffer, BUFF_SIZE);
        msgsize = sndMsg(client->sockfd, buffer, msgsize);
        delete conack_msg;
        conack_msg = nullptr;
    }
    catch (const std::runtime_error &e)
    {
        cout << "Runtime error: " << e.what() << endl;
        cout << "Client thread finished for client: " << inet_ntoa(client->addr.sin_addr) << ":" << ntohs(client->addr.sin_port) << endl;
        removeClient(client);
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
            addPublish(&pub_msg->topic_name, &pub_msg->value, &type_flags);

            delete pub_msg;
            pub_msg = nullptr;
            type_flags = 0;
            break;

        case TYPE_SUBSCRIBE:
            // Add client to list of subscribers of the topic
            sub_msg = new SUBSCRIBE(type_flags, remlen);
            sub_msg->fromBuffer(buffer, BUFF_SIZE);
            for (auto &topic : *sub_msg->topics)
            {
                clients_subscribed_topics[topic.topic_name].push_back(client);
                client->subscribe_topics.insert(topic.topic_name);
                if (retained_topics.find(topic.topic_name) != retained_topics.end())
                {
                    // Send retained message to subscriber
                    PUBLISH *pub_msg = new PUBLISH(&topic.topic_name, &retained_topics[topic.topic_name], FPUBLISH_DEF_TYPEFLAG, 0);
                    msgsize = pub_msg->toBuffer(buffer, BUFF_SIZE);
                    msgsize = sndMsg(client->sockfd, buffer, msgsize);
                    delete pub_msg;
                }
            }

            suback_msg = new SUBACK(sub_msg);
            sub_msg->msg_id = suback_msg->msg_id;
            msgsize = suback_msg->toBuffer(buffer, BUFF_SIZE);
            msgsize = sndMsg(client->sockfd, buffer, msgsize);


            delete sub_msg;
            sub_msg = nullptr;
            delete suback_msg;
            suback_msg = nullptr;
            type_flags = 0;
            break;

        case TYPE_UNSUBSCRIBE:

            // Remove client from list of subscribers of the topic
            unsub_msg = new UNSUBSCRIBE(type_flags, remlen);
            unsub_msg->fromBuffer(buffer, BUFF_SIZE);
            for (auto &topic : *unsub_msg->topics)
            {
                auto it = clients_subscribed_topics.find(topic.topic_name);
                if (it != clients_subscribed_topics.end())
                {
                    it->second.remove(client);
                    client->subscribe_topics.erase(topic.topic_name);
                    if (it->second.empty())
                    {
                        clients_subscribed_topics.erase(it);
                    }
                }
            }
            unsuback_msg = new UNSUBACK();
            unsuback_msg->msg_id = unsub_msg->msg_id;
            msgsize = unsuback_msg->toBuffer(buffer, BUFF_SIZE);
            msgsize = sndMsg(client->sockfd, buffer, msgsize);
            // TRY

            delete unsub_msg;
            unsub_msg = nullptr;
            delete unsuback_msg;
            unsuback_msg = nullptr;

            type_flags = 0;
            break;

        case TYPE_PINGREQ:
            // Send PINGRESP message
            pingresp_msg = new PINGRESP();
            msgsize = pingresp_msg->toBuffer(buffer, BUFF_SIZE);
            msgsize = sndMsg(client->sockfd, buffer, msgsize);
            delete pingresp_msg;
            pingresp_msg = nullptr;

            type_flags = 0;
            break;

        case TYPE_DISCONNECT:
            removeClient(client);
            return;
            
        case TYPE_CONNECT:
            // Already handled
            removeClient(client);
            break;

        default:
            removeClient(client);
            return;
        }
    }
}

void Broker::addClient(int sockfd, const sockaddr_in &client_addr)
{
    Client *newclient = new Client(this, sockfd, client_addr);
    unique_lock<mutex> lck(mtx_clients);
    this->clients_active.push_back(newclient);
}

void Broker::removeClient(Client *client)
{
    unique_lock<mutex> lck(mtx_clients);
    for (const auto &topic : client->subscribe_topics)
    {
        auto it = clients_subscribed_topics.find(topic);
        if (it != clients_subscribed_topics.end())
        {
            it->second.remove(client);
            if (it->second.empty())
            {
                clients_subscribed_topics.erase(it);
            }
        }
    }
    cout << "Client thread finished for client: " << inet_ntoa(client->addr.sin_addr) << ":" << ntohs(client->addr.sin_port) << endl;
    close(client->sockfd);
    client->th.detach();
    clients_active.remove(client);
}

void Broker::acceptClients()
{
    int sockfd;
    struct sockaddr_in client_addr;
    socklen_t client_len = sizeof(client_addr);

    while (true)
    {
        sockfd = accept(serv_sockfd, (struct sockaddr *)&client_addr, &client_len);
        if (sockfd < 0) 
        {
            cerr << "Error on accept" << endl;
            continue;
        }
        addClient(sockfd, client_addr);
    }
}

Broker::~Broker()
{
    close(serv_sockfd);
}

Broker::Client::Client(Broker *br_, int sockfd_, const sockaddr_in &addr_)
    : br(br_),
      sockfd(sockfd_),
      addr(addr_),
      th(&Broker::toclient_routine, br, this)
{
}

void Broker::addPublish(string *topic, string *value, uint8_t *flags)
{

    int msgsize;
    if (*flags & 1) // RETAIN flag is set
    {
        unique_lock<mutex> lck(mtx_rettopics);
        retained_topics[*topic] = *value;
    }
    // Send message to all subscribers
    PUBLISH *pub_msg = new PUBLISH(topic, value, *flags);

    if (clients_subscribed_topics.find(*topic) != clients_subscribed_topics.end())
    {
        for (auto &subscriber : clients_subscribed_topics[*topic])
        {
            msgsize = pub_msg->toBuffer(buffer, BUFF_SIZE);
            msgsize = sndMsg(subscriber->sockfd, buffer, msgsize);
        }
    }
}