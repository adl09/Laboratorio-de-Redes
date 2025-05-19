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

using namespace std;

class Client{
    public:
        int sockfd;
        sockaddr_in addr;
        socklen_t len;

        Client(int sockfd_, sockaddr_in addr_,socklen_t len_) : sockfd(sockfd_), addr(addr_), len(len_){}
        ~Client(){}
        // topics list;
};

class Broker{
    private:
        int serv_sockfd;
        struct sockaddr_in serv_addr;

        // listaclientes(threads)
        vector<Client*> clients_active;
        vector<thread> client_thread_active;

        // listatopicos con link a clientes suscriptos (map)

    public:
        Broker(int portnumber);
        void acceptClients();
        static void toclient_routine(Client* client);
        ~Broker();
        
};
