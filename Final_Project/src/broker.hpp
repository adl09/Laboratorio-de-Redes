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
#include <map>
#include <list>
#include <unordered_map>
#include <set>


using namespace std;


class Broker{
    private:
        class Client{
            public:
                Broker *br;
                const int sockfd;
                const sockaddr_in addr;
                thread th;

                set<string> subscribe_topics;
        
                Client(Broker *br_, int sockfd_, const sockaddr_in &addr_);
                ~Client(){}
        
        };

        int serv_sockfd;
        struct sockaddr_in serv_addr;

        

        mutex mtx_clients;
        mutex mtx_rettopics;

        list<Client*> clients_active;
        unordered_map<string, list<Client*>> clients_subscribed_topics;
        unordered_map<string,string> retained_topics;
        


    public:
        Broker(int portnumber);
        void acceptClients();
        void addClient(int sockfd, const sockaddr_in &client_addr);
        void removeClient(Client* client);
        void toclient_routine(Client* client);
        void addPublish(string* topic, string* value, uint8_t *flags);

        ~Broker();
        

};
