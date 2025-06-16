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

#include "broker.hpp"
#include "MQTT.hpp"

using namespace std;

int main(int argc, char *argv[])
{
    assert(argc >= 2 && "Error: no port provided");
    assert(argc <= 3 && "Error: too many arguments");
    
    int portnumber=atoi(argv[1]);
    Broker* broker = new Broker(portnumber);
    assert(broker!=NULL && "Error initializing broker");

    broker->acceptClients(); // Inicia la rutina de aceptar clientes.
    

    return 0;
}
