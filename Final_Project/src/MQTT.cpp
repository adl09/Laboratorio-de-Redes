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
#include <set>
#include <fcntl.h>
#include <errno.h>

#include "MQTT.hpp"

using namespace std;

void encodeLength(int value, uint8_t *encoded, int *pcount)
{
    int encodedByte;
    do
    {
        encodedByte = value % 128;
        value = value / 128;
        if (value > 0)
            encodedByte = encodedByte | 128;
        *encoded = encodedByte;
        encoded++;
        (*pcount)++;
    } while (value > 0);
}

int getEncodedLength(int sockfd, int *pcount)
{
    int multiplier = 1;
    uint8_t encodedByte = 0;

    int totalLength = 0;
    do
    {
        ssize_t recvd = recv(sockfd, &encodedByte, 1, 0);
        if ((recvd == -1 && errno != EINTR) || recvd == 0)
            return -1;
        (*pcount)++;
        totalLength += (encodedByte & 127) * multiplier;

        multiplier *= 128;
        if (multiplier > 128 * 128 * 128)
        {
            cout << "Malformed Remaining Length" << endl; // MANEJO ERRORES
        }
    } while ((encodedByte & 128) != 0);

    return totalLength;
}

int rcvMsg(int sockfd, uint8_t *first, int *remlen, uint8_t *buffer, uint16_t sz8)
{
    bzero(buffer, BUFF_SIZE);
    uint8_t *ptr_buff = (uint8_t *)buffer;

    uint32_t toRecv = 1;
    ssize_t recvd = 0;
    ssize_t totalRecvd = 0;

    
    recvd = recv(sockfd, first, toRecv, 0);
    if ((recvd == -1 && (errno == EWOULDBLOCK || errno == EAGAIN)))
        return -2;
    if ((recvd == -1 && errno != EINTR) || recvd == 0)
        return 0;
    
    totalRecvd += recvd;

    int count = 0;
    *remlen = getEncodedLength(sockfd, &count);
    if (*remlen == -1)
        return -1;

    toRecv = *remlen;

    while (toRecv)
    {
        recvd = recv(sockfd, ptr_buff, toRecv, 0);
        if ((recvd == -1 && errno != EINTR) || recvd == 0)
            return 0;

        if (recvd != -1)
        {
            totalRecvd += recvd;
            toRecv -= recvd;
            ptr_buff += recvd;
        }
    }

    if (totalRecvd > sz8)
        throw std::runtime_error("Buffer overflow");
    if (totalRecvd < 0)
        throw std::runtime_error("Bad receive");

    return totalRecvd;
}

int sndMsg(int sockfd, uint8_t *buffer, int length)
{
    size_t toSend = length;
    ssize_t sent;
    uint8_t *ptr_buff = (uint8_t *)buffer;

    while (toSend)
    {
        sent = send(sockfd, ptr_buff, toSend, 0);
        if ((sent == -1 && errno != EINTR) || sent == 0)
            return sent;
        if (sent != -1)
        {
            toSend -= sent;
            ptr_buff += sent;
        }
    }
    return 0;
}

// MESSAGES

int CONNECT::toBuffer(uint8_t *buffer, ssize_t sz8)
{
    
    int i = 0;
    bzero(buffer, BUFF_SIZE); 

    type2buff(buffer);                                                  
    flags2buff(buffer);                                               
    encodeLength(remaining_length, buffer + 1, &i);                     
    i += 1;                                                            
    len2buff(protocol_name_len, buffer + i);                            
    i += 2;                                                             
    memcpy(buffer + i, this->protocol_name.c_str(), protocol_name_len); 
    i += protocol_name_len;
    buffer[i++] = protocol_level;                               
    buffer[i++] = connect_flags;                               
    len2buff(keep_alive, buffer + i);                          
    i += 2;                                                     
    len2buff(client_id_len, buffer + i);                        
    i += 2;                                                    
    memcpy(buffer + i, this->client_id.c_str(), client_id_len); 
    i += client_id_len;
    if (i > sz8) throw std::runtime_error("Buffer overflow");
    return i; 
}

int CONNECT::fromBuffer(const uint8_t *buffer, ssize_t sz8)
{
   
    protocol_name_len = (buffer[0] << 8) | buffer[1];        
    memcpy(&protocol_name[0], buffer + 2, protocol_name_len); 
    int i = 2;
    i += protocol_name_len;
    protocol_level = buffer[i++];                  
    connect_flags = buffer[i++];                   
    keep_alive = (buffer[i] << 8) | buffer[i + 1]; 

    if (i > sz8) throw std::runtime_error("Buffer overflow");
    return i + 1; 
}

int CONNACK::toBuffer(uint8_t *buffer, ssize_t sz8)
{
    int i = 0;
    bzero(buffer, BUFF_SIZE); 

    type2buff(buffer);                              
    flags2buff(buffer);                             
    encodeLength(remaining_length, buffer + 1, &i); 
    i += 1;                                         
    buffer[i++] = session_present;                  
    buffer[i++] = return_code;                      
    if (i > sz8) throw std::runtime_error("Buffer overflow");

    return i;                                     
}

int CONNACK::fromBuffer(const uint8_t *buffer, ssize_t sz8)
{
    int i=0;
    session_present = buffer[0]; 
    i += 1;
    return_code = buffer[1];    
    i += 1;
    if (i > sz8) throw std::runtime_error("Buffer overflow");

    return 0;
}

void connection_procedure(int sockfd, uint8_t *buffer, uint8_t *keepalive)
{
    CONNECT *con_msg = nullptr;
    CONNACK *conack_msg = nullptr;
    int msgsize, remlen, totalRecvd;

    con_msg = new CONNECT();
    msgsize = ((CONNECT *)con_msg)->toBuffer(buffer, BUFF_SIZE);
    msgsize = sndMsg(sockfd, buffer, msgsize);
    *keepalive = con_msg->keep_alive;
    delete con_msg;
    con_msg = nullptr;

    conack_msg = new CONNACK();
    while (true)
    {
        try
        {
            totalRecvd = rcvMsg(sockfd, &conack_msg->flags, &remlen, buffer, BUFF_SIZE);
            conack_msg->fromBuffer(buffer, BUFF_SIZE);
            if (conack_msg->type == TYPE_CONNACK)
                break;
        }
        catch (std::runtime_error &e)
        {
            cout << "Error receiving CONNACK: " << e.what() << endl;
            return;
        }
    }
    return;
}

int PUBLISH::toBuffer(uint8_t *buffer, ssize_t sz8)
{
    int i = 0;
    bzero(buffer, BUFF_SIZE); 

    type2buff(buffer);                                            
    flags2buff(buffer);                                           
    encodeLength(remaining_length, buffer + 1, &i);               
    i += 1;                                                       
    len2buff(topic_name_len, buffer + i);                        
    i += 2;                                                       
    memcpy(buffer + i, this->topic_name.c_str(), topic_name_len);
    i += topic_name_len;
    memcpy(buffer + i, this->value.c_str(), value.length()); 
    i += value.length();
    if (i > sz8) throw std::runtime_error("Buffer overflow");

    return i; 
}

int PUBLISH::fromBuffer(const uint8_t *buffer, ssize_t sz8)
{
    topic_name_len = (buffer[0] << 8) | buffer[1];           
    topic_name = string((char *)buffer + 2, topic_name_len);
    int i = topic_name_len + 2;
    value = string((char *)buffer + i, this->remaining_length - i); 
    i += value.length();
    if (i > sz8) throw std::runtime_error("Buffer overflow");
    return 0;
}

int SUBSCRIBE::toBuffer(uint8_t *buffer, ssize_t sz8)
{
    int i = 0;
    bzero(buffer, BUFF_SIZE); 

    type2buff(buffer);                             
    flags2buff(buffer);                             
    encodeLength(remaining_length, buffer + 1, &i); 
    i += 1;                                         
    len2buff(msg_id, buffer + i);
    i += 2; 

    for (int j = 0; j < (int)this->topics->size(); ++j)
    {
        len2buff((*this->topics)[j].topic_len, buffer + i);                                      
        i += 2;                                                                                  
        memcpy(buffer + i, (*this->topics)[j].topic_name.c_str(), (*this->topics)[j].topic_len);
        i += (*this->topics)[j].topic_len;
        buffer[i++] = (*this->topics)[j].qos;
    }
    if (i > sz8) throw std::runtime_error("Buffer overflow");
    return i; 
}
int SUBSCRIBE::fromBuffer(const uint8_t *buffer, ssize_t sz8)
{
    int i = 0;
    int rem_length = this->remaining_length;
    msg_id = (buffer[i] << 8) | buffer[i + 1];
    i += 2;                                    
    rem_length -= 2;                          
    do
    {
        topics_struct topic;
        topic.topic_len = (buffer[i] << 8) | buffer[i + 1]; 
        i += 2;
        topic.topic_name = string((char *)buffer + i, topic.topic_len); 
        i += topic.topic_len;
        topic.qos = buffer[i++]; 
        topics->push_back(topic);
        rem_length -= (topic.topic_len + 3);
        if (rem_length < 0)
        {
            cout << "Malformed SUBSCRIBE message" << endl; 
            return -1;
        }
    } while (rem_length > 0);
    if (i > sz8) throw std::runtime_error("Buffer overflow");
    return 0;
}



int SUBACK::toBuffer(uint8_t *buffer, ssize_t sz8)
{
   
    int i = 0;
    bzero(buffer, BUFF_SIZE); 

    type2buff(buffer);                              
    flags2buff(buffer);                             
    encodeLength(remaining_length, buffer + 1, &i); 
    i += 1;                                         
    len2buff(msg_id, buffer + i);                   
    i += 2;                                         
    for (int j = 0; j < (int)this->return_codes->size(); ++j)
    {
        buffer[i++] = (*this->return_codes)[j]; 
    }
    if (i > sz8) throw std::runtime_error("Buffer overflow");
    return i; 
}

int SUBACK::fromBuffer(const uint8_t *buffer, ssize_t sz8)
{
    
    msg_id = (buffer[0] << 8) | buffer[1]; 
    int i = 2;                            
    for (int j = 0; j < this->remaining_length - 2; ++j)
    {
        return_codes->push_back(buffer[i++]);
    }
    if (i > sz8) throw std::runtime_error("Buffer overflow");
    return 0;
}

void subscribe_procedure(int sockfd, uint8_t *buffer, vector<string> *topics)
{    
    SUBSCRIBE *sub_msg = nullptr;
    SUBACK *suback_msg = nullptr;
    int msgsize, remlen, totalRecvd;
        
    // Subscribe to the topic/s
    sub_msg = new SUBSCRIBE(topics);
    msgsize = ((SUBSCRIBE *)sub_msg)->toBuffer(buffer, BUFF_SIZE);
    msgsize = sndMsg(sockfd, buffer, msgsize);

    // Wait to receive SUBACK message
    suback_msg = new SUBACK();
    while (true)
    {
        try
        {
            totalRecvd = rcvMsg(sockfd, &suback_msg->flags, &remlen, buffer, BUFF_SIZE);
            suback_msg->fromBuffer(buffer, BUFF_SIZE);
            if (suback_msg->type == TYPE_SUBACK)
                break;
        }
        catch (std::runtime_error &e)
        {
            cout << "Error receiving SUBACK: " << e.what() << endl;
            return;
        }
    }

    return;
}

int PINGREQ::toBuffer(uint8_t *buffer, ssize_t sz8)
{
    
    int i = 0;
    bzero(buffer, BUFF_SIZE); 

    type2buff(buffer);                             
    flags2buff(buffer);                             
    encodeLength(remaining_length, buffer + 1, &i); 
    i += 1;                                         

    if (i > sz8) throw std::runtime_error("Buffer overflow");
    return i;                                       
}

int PINGREQ::fromBuffer(const uint8_t *buffer, ssize_t sz8)
{
    return 0;
}

int PINGRESP::toBuffer(uint8_t *buffer, ssize_t sz8)
{
    int i = 0;
    bzero(buffer, BUFF_SIZE); 

    type2buff(buffer);                              
    flags2buff(buffer);                             
    encodeLength(remaining_length, buffer + 1, &i); 
    i += 1;                                         
    if (i > sz8) throw std::runtime_error("Buffer overflow");
    return i;                               
}

int PINGRESP::fromBuffer(const uint8_t *buffer, ssize_t sz8)
{
    return 0;
}
int DISCONNECT::toBuffer(uint8_t *buffer, ssize_t sz8)
{
    int i = 0;
    bzero(buffer, BUFF_SIZE); 
    type2buff(buffer);                             
    flags2buff(buffer);                            
    encodeLength(remaining_length, buffer + 1, &i);
    i += 1;                                         
    if (i > sz8) throw std::runtime_error("Buffer overflow");
    return i;                                      
}
int DISCONNECT::fromBuffer(const uint8_t *buffer, ssize_t sz8)
{
    return 0;
}

int UNSUBSCRIBE::toBuffer(uint8_t *buffer, ssize_t sz8)
{
    int i = 0;
    bzero(buffer, BUFF_SIZE); 
    type2buff(buffer);                             
    flags2buff(buffer);                             
    encodeLength(remaining_length, buffer + 1, &i); 
    i += 1;                                        
    len2buff(msg_id, buffer + i);              
    i += 2;                                         

    for (int j = 0; j < (int)this->topics->size(); ++j)
    {
        len2buff((*this->topics)[j].topic_len, buffer + i);                                     
        i += 2;                                                                               
        memcpy(buffer + i, (*this->topics)[j].topic_name.c_str(), (*this->topics)[j].topic_len);
        i += (*this->topics)[j].topic_len;
    }
    if (i > sz8) throw std::runtime_error("Buffer overflow");
    return i;
}

int UNSUBSCRIBE::fromBuffer(const uint8_t *buffer, ssize_t sz8)
{
    int i = 0;
    int rem_length = this->remaining_length;
    msg_id = (buffer[i] << 8) | buffer[i + 1]; 
    i += 2;                                    
    rem_length -= 2;                          
    do
    {
        topics_struct topic;
        topic.topic_len = (buffer[i] << 8) | buffer[i + 1];
        i += 2;
        topic.topic_name = string((char *)buffer + i, topic.topic_len);
        i += topic.topic_len;
        topics->push_back(topic);
        rem_length -= (topic.topic_len + 2);
        if (rem_length < 0)
        {
            cout << "Malformed UNSUBSCRIBE message" << endl; 
            return -1;
        }
    } while (rem_length > 0);
    if (i > sz8) throw std::runtime_error("Buffer overflow");
    return 0;
}

int UNSUBACK::fromBuffer(const uint8_t *buffer, ssize_t sz8)
{
    msg_id = (buffer[0] << 8) | buffer[1]; 
    return 0;
}

int UNSUBACK::toBuffer(uint8_t *buffer, ssize_t sz8)
{
    int i = 0;
    bzero(buffer, BUFF_SIZE);
    type2buff(buffer);                          
    flags2buff(buffer);                        
    encodeLength(remaining_length, buffer + 1, &i);
    i += 1;                                
    len2buff(msg_id, buffer + i);                 
    i += 2;                                     
    if (i > sz8) throw std::runtime_error("Buffer overflow");
    return i;                                   
}
