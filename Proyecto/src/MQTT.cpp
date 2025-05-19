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

int rcvMsg(int sockfd, MQTTMsg *msg){
    // Read the fixed header
    uint8_t fixed_header[2];
    bzero(fixed_header, sizeof(fixed_header));
    size_t toRecv = sizeof(fixed_header);
    ssize_t recvd;
    uint8_t *ptr = (uint8_t *)fixed_header;
    bool headerRecvd = false;
    
    uint8_t *hdr_payload = NULL;


    // Receive the fixed header
    while(toRecv){
        recvd = recv(sockfd, ptr, toRecv, 0);
        assert(recvd != -1 && "Error receiving fixed header");
        if(recvd != -1){
            toRecv -= recvd;
            
            // TEST
            // std::cout << "Received (hex): ";
            // for (size_t i = 0; i < recvd; ++i) {
            //     std::cout << std::hex << std::uppercase << (int)ptr[i] << " ";
            // }
            // std::cout << std::dec << std::endl;

            ptr += recvd;
            if(toRecv == 0 && !headerRecvd){
                headerRecvd = true;
                // Process the fixed header
                msg->type = (Type)((fixed_header[0] >> 4) & 0x0F); // Get the type
                msg->flags = fixed_header[0] & 0x0F; // Get the flags
                msg->remaining_length = fixed_header[1]; // Get the remaining length
                toRecv = msg->remaining_length; // Set the remaining length to receive
                hdr_payload = new uint8_t[toRecv]; // Allocate memory for the variable header and the payload
                break;
            }        
        }
    }

    if(headerRecvd){
        // Process the variable header and payload
        std::cout << "Received message of type: " << ((int)msg->type) << std::endl;
        std::cout << "Flags: " << (int)msg->flags << std::endl;
        std::cout << "Remaining length: " << msg->remaining_length << std::endl;
    }

    // Receive the variable header and payload
    assert(hdr_payload != NULL && "Error allocating memory for header and payload");
    while(toRecv){
        recvd = recv(sockfd, hdr_payload, toRecv, 0);
        



    }
    // COMPLETAR!
    return 0;
}

int sndMsg(int sockfd, MQTTMsg *msg){
    // Send the fixed header
    uint8_t fixed_header[2];
    bzero(fixed_header, sizeof(fixed_header));
    fixed_header[0] = (msg->type << 4) | (msg->flags & 0x0F); // Set the type and flags
    fixed_header[1] = msg->remaining_length; // Set the remaining length
    ssize_t sent = send(sockfd, fixed_header, sizeof(fixed_header), 0);
    assert(sent != -1 && "Error sending fixed header");
    if(sent == -1){
        std::cerr << "Error sending fixed header" << std::endl;
        return -1;
    }
    // Send the variable header and payload
    // For now, we will just print the sent message
    std::cout << "Sent message of type: " << (int)msg->type << std::endl;
    std::cout << "Flags: " << (int)msg->flags << std::endl;
    std::cout << "Remaining length: " << msg->remaining_length << std::endl;

    return 0;
}