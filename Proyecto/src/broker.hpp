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

        list<Client*> clients_active;
        mutex mtx_clients;

        mutex mtx_rettopics;

        /* GESTIÓN DE MENSAJES
        1. Cuando se recibe un SUBSCRIBE de un cliente nuevo, se busca el topic en el mapa clients_subscribed_topics, si no existe se agrega el cliente a la lista de clientes suscritos a ese topic.

            a. Verificar si el topic ya existe en el mapa retained_topics, si existe se busca el valor y se envía al cliente.

        2. Cuando se recibe un PUBLISH de un cliente:
            a. Se busca en el mapa clients_subscribed_topics si hay clientes suscritos al topic y se los envía.
            b. Se mira si hay que retener este mensaje, si es así se agrega al mapa retained_topics.
            c. 

        */
        // lista de clientes
        unordered_map<string, list<Client*>> clients_subscribed_topics;

        // listatopicos con topic,value para ubicar a partir de un topic, el valor correspondiente
        // 1. Cuando se recibe un 
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
