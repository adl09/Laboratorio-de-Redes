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

int rcvMsg(int sockfd, uint8_t *first, int *remlen, uint8_t *buffer, uint16_t sz8) // retornar cantidad de bytes leidos
{
    bzero(buffer, BUFF_SIZE);
    uint8_t *ptr_buff = (uint8_t *)buffer;

    uint32_t toRecv = 1; // First byte
    ssize_t recvd = 0;
    ssize_t totalRecvd = 0;

    // Receive the first byte of the fixed header
    // set flags to nonblock
    
    recvd = recv(sockfd, first, toRecv, 0);
    if ((recvd == -1 && (errno == EWOULDBLOCK || errno == EAGAIN)))
        return -2;
    if ((recvd == -1 && errno != EINTR) || recvd == 0)
        return 0;
    
    totalRecvd += recvd;

    // Calculate the remaining length to receive
    int count = 0;
    *remlen = getEncodedLength(sockfd, &count);
    if (*remlen == -1)
        return -1;

    toRecv = *remlen;

    // Receive the variable header and payload
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
    // Send
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
    // Convert the CONNECT message to a buffer for sending
    int i = 0;
    bzero(buffer, BUFF_SIZE); // Clear the buffer

    type2buff(buffer);                                                  // Set the message type
    flags2buff(buffer);                                                 // Set the flags
    encodeLength(remaining_length, buffer + 1, &i);                     // Remaining length
    i += 1;                                                             // i = 1 byte (first) + encoded_len counter bytes
    len2buff(protocol_name_len, buffer + i);                            // Protocol name length
    i += 2;                                                             // i = 1 byte (first) + encoded_len counter bytes
    memcpy(buffer + i, this->protocol_name.c_str(), protocol_name_len); // Protocol name
    i += protocol_name_len;
    buffer[i++] = protocol_level;                               // Protocol level
    buffer[i++] = connect_flags;                                // Connect flags
    len2buff(keep_alive, buffer + i);                           // Keep alive
    i += 2;                                                     // i = 1 byte (first) + encoded_len counter bytes
    len2buff(client_id_len, buffer + i);                        // Client ID length
    i += 2;                                                     // i = 1 byte (first) + encoded_len counter bytes
    memcpy(buffer + i, this->client_id.c_str(), client_id_len); // Client ID
    i += client_id_len;
    return i; // Return the number of bytes written to the buffer
}

int CONNECT::fromBuffer(const uint8_t *buffer, ssize_t sz8)
{
    // // Convert the buffer to a CONNECT message
    protocol_name_len = (buffer[0] << 8) | buffer[1];         // Protocol name length
    memcpy(&protocol_name[0], buffer + 2, protocol_name_len); // Protocol name
    int i = 2;
    i += protocol_name_len;
    protocol_level = buffer[i++];                  // Protocol level
    connect_flags = buffer[i++];                   // Connect flags
    keep_alive = (buffer[i] << 8) | buffer[i + 1]; // Keep alive

    // AGREGAR CLIENT_ID ...

    return i + 1; // Return the number of bytes read from the buffer
}

int CONNACK::toBuffer(uint8_t *buffer, ssize_t sz8)
{
    // Convert the CONNACK message to a buffer for sending
    int i = 0;
    bzero(buffer, BUFF_SIZE); // Clear the buffer

    type2buff(buffer);                              // Set the message type
    flags2buff(buffer);                             // Set the flags
    encodeLength(remaining_length, buffer + 1, &i); // Remaining length
    i += 1;                                         // i = 1 byte (first) + encoded_len counter bytes
    buffer[i++] = session_present;                  // Session present
    buffer[i++] = return_code;                      // Return code
    return i;                                       // Return the number of bytes written to the buffer
}

int CONNACK::fromBuffer(const uint8_t *buffer, ssize_t sz8)
{
    // Convert the buffer to a CONNACK message
    session_present = buffer[0]; // Session present
    return_code = buffer[1];     // Return code
    return 0;
}

void connection_procedure(int sockfd, uint8_t *buffer, uint8_t *keepalive)
{
    CONNECT *con_msg = nullptr;
    CONNACK *conack_msg = nullptr;
    int msgsize, remlen, totalRecvd;

    // Create CONNECT
    con_msg = new CONNECT();
    msgsize = ((CONNECT *)con_msg)->toBuffer(buffer, BUFF_SIZE);
    msgsize = sndMsg(sockfd, buffer, msgsize);
    *keepalive = con_msg->keep_alive;
    delete con_msg;
    con_msg = nullptr;

    // Wait to receive CONNACK from server
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
    // Convert the PUBLISH message to a buffer for sending
    int i = 0;
    bzero(buffer, BUFF_SIZE); // Clear the buffer

    type2buff(buffer);                                            // Set the message type
    flags2buff(buffer);                                           // Set the flags
    encodeLength(remaining_length, buffer + 1, &i);               // Remaining length
    i += 1;                                                       // i = 1 byte (first) + encoded_len counter bytes
    len2buff(topic_name_len, buffer + i);                         // Topic name length
    i += 2;                                                       // i = 1 byte (first) + encoded_len counter bytes
    memcpy(buffer + i, this->topic_name.c_str(), topic_name_len); // Topic name
    i += topic_name_len;
    memcpy(buffer + i, this->value.c_str(), value.length()); // Payload
    i += value.length();
    return i; // Return the number of bytes written to the buffer
}

int PUBLISH::fromBuffer(const uint8_t *buffer, ssize_t sz8)
{
    // Convert the buffer to a PUBLISH message
    topic_name_len = (buffer[0] << 8) | buffer[1];           // Topic name length
    topic_name = string((char *)buffer + 2, topic_name_len); // Topic name
    int i = topic_name_len + 2;
    value = string((char *)buffer + i, this->remaining_length - i); // Payload
    i += value.length();

    return 0;
}

int SUBSCRIBE::toBuffer(uint8_t *buffer, ssize_t sz8)
{
    // Convert the SUBSCRIBE message to a buffer for sending
    int i = 0;
    bzero(buffer, BUFF_SIZE); // Clear the buffer

    type2buff(buffer);                              // Set the message type
    flags2buff(buffer);                             // Set the flags
    encodeLength(remaining_length, buffer + 1, &i); // Remaining length
    i += 1;                                         // i = 1 byte (first) + encoded_len counter bytes
    // msg_id
    len2buff(msg_id, buffer + i); // Message ID
    i += 2; // i = 1 byte (first) + encoded_len counter bytes

    for (int j = 0; j < (int)this->topics->size(); ++j)
    {
        len2buff((*this->topics)[j].topic_len, buffer + i);                                      // Topic name length
        i += 2;                                                                                  // i = 1 byte (first) + encoded_len counter bytes
        memcpy(buffer + i, (*this->topics)[j].topic_name.c_str(), (*this->topics)[j].topic_len); // Topic name
        i += (*this->topics)[j].topic_len;
        buffer[i++] = (*this->topics)[j].qos; // QoS level
    }

    return i; // Return the number of bytes written to the buffer
}
int SUBSCRIBE::fromBuffer(const uint8_t *buffer, ssize_t sz8)
{
    // Convert the buffer to a SUBSCRIBE message
    int i = 0;
    int rem_length = this->remaining_length;
    msg_id = (buffer[i] << 8) | buffer[i + 1]; // Message ID
    i += 2;                                    // i = 1 byte (first) + encoded_len counter bytes
    rem_length -= 2;                           // Message ID length
    do
    {
        topics_struct topic;
        topic.topic_len = (buffer[i] << 8) | buffer[i + 1]; // Topic name length
        i += 2;
        topic.topic_name = string((char *)buffer + i, topic.topic_len); // Topic name
        i += topic.topic_len;
        topic.qos = buffer[i++]; // QoS level
        topics->push_back(topic);
        rem_length -= (topic.topic_len + 3);
        if (rem_length < 0)
        {
            cout << "Malformed SUBSCRIBE message" << endl; // MANEJO ERRORES
            return -1;
        }
    } while (rem_length > 0);
    return 0;
}



int SUBACK::toBuffer(uint8_t *buffer, ssize_t sz8)
{
    // Convert the SUBACK message to a buffer for sending
    int i = 0;
    bzero(buffer, BUFF_SIZE); // Clear the buffer

    type2buff(buffer);                              // Set the message type
    flags2buff(buffer);                             // Set the flags
    encodeLength(remaining_length, buffer + 1, &i); // Remaining length
    i += 1;                                         // i = 1 byte (first) + encoded_len counter bytes
    len2buff(msg_id, buffer + i);                   // Message ID
    i += 2;                                         // i = 1 byte (first) + encoded_len counter bytes
    for (int j = 0; j < (int)this->return_codes->size(); ++j)
    {
        buffer[i++] = (*this->return_codes)[j]; // Return code
    }
    return i; // Return the number of bytes written to the buffer
}

int SUBACK::fromBuffer(const uint8_t *buffer, ssize_t sz8)
{
    // Convert the buffer to a SUBACK message
    msg_id = (buffer[0] << 8) | buffer[1]; // Message ID
    int i = 2;                             // i = 1 byte (first) + encoded_len counter bytes
    for (int j = 0; j < this->remaining_length - 2; ++j)
    {
        return_codes->push_back(buffer[i++]); // Return code
    }
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
    // Add 

    return;
}

int PINGREQ::toBuffer(uint8_t *buffer, ssize_t sz8)
{
    // Convert the PINGREQ message to a buffer for sending
    int i = 0;
    bzero(buffer, BUFF_SIZE); // Clear the buffer

    type2buff(buffer);                              // Set the message type
    flags2buff(buffer);                             // Set the flags
    encodeLength(remaining_length, buffer + 1, &i); // Remaining length
    i += 1;                                         // i = 1 byte (first) + encoded_len counter bytes
    return i;                                       // Return the number of bytes written to the buffer
}

int PINGREQ::fromBuffer(const uint8_t *buffer, ssize_t sz8)
{
    // Convert the buffer to a PINGREQ message
    return 0;
}

int PINGRESP::toBuffer(uint8_t *buffer, ssize_t sz8)
{
    // Convert the PINGRESP message to a buffer for sending
    int i = 0;
    bzero(buffer, BUFF_SIZE); // Clear the buffer

    type2buff(buffer);                              // Set the message type
    flags2buff(buffer);                             // Set the flags
    encodeLength(remaining_length, buffer + 1, &i); // Remaining length
    i += 1;                                         // i = 1 byte (first) + encoded_len counter bytes
    return i;                                       // Return the number of bytes written to the buffer
}

int PINGRESP::fromBuffer(const uint8_t *buffer, ssize_t sz8)
{
    // Convert the buffer to a PINGRESP message
    return 0;
}
int DISCONNECT::toBuffer(uint8_t *buffer, ssize_t sz8)
{
    // Convert the DISCONNECT message to a buffer for sending
    int i = 0;
    bzero(buffer, BUFF_SIZE); // Clear the buffer

    type2buff(buffer);                              // Set the message type
    flags2buff(buffer);                             // Set the flags
    encodeLength(remaining_length, buffer + 1, &i); // Remaining length
    i += 1;                                         // i = 1 byte (first) + encoded_len counter bytes
    return i;                                       // Return the number of bytes written to the buffer
}
int DISCONNECT::fromBuffer(const uint8_t *buffer, ssize_t sz8)
{
    // Convert the buffer to a DISCONNECT message

    return 0;
}

int UNSUBSCRIBE::toBuffer(uint8_t *buffer, ssize_t sz8)
{
    // Convert the UNSUBSCRIBE message to a buffer for sending
    int i = 0;
    bzero(buffer, BUFF_SIZE); // Clear the buffer

    type2buff(buffer);                              // Set the message type
    flags2buff(buffer);                             // Set the flags
    encodeLength(remaining_length, buffer + 1, &i); // Remaining length
    i += 1;                                         // i = 1 byte (first) + encoded_len counter bytes
    len2buff(msg_id, buffer + i);                   // Message ID
    i += 2;                                         // i = 1 byte (first) + encoded_len counter bytes

    for (int j = 0; j < (int)this->topics->size(); ++j)
    {
        len2buff((*this->topics)[j].topic_len, buffer + i);                                      // Topic name length
        i += 2;                                                                                  // i = 1 byte (first) + encoded_len counter bytes
        memcpy(buffer + i, (*this->topics)[j].topic_name.c_str(), (*this->topics)[j].topic_len); // Topic name
        i += (*this->topics)[j].topic_len;
    }
    return i; // Return the number of bytes written to the buffer
}

int UNSUBSCRIBE::fromBuffer(const uint8_t *buffer, ssize_t sz8)
{
    // Convert the buffer to a UNSUBSCRIBE message
    int i = 0;
    int rem_length = this->remaining_length;
    msg_id = (buffer[i] << 8) | buffer[i + 1]; // Message ID
    i += 2;                                    // i = 1 byte (first) + encoded_len counter bytes
    rem_length -= 2;                           // Message ID length
    do
    {
        topics_struct topic;
        topic.topic_len = (buffer[i] << 8) | buffer[i + 1]; // Topic name length
        i += 2;
        topic.topic_name = string((char *)buffer + i, topic.topic_len); // Topic name
        i += topic.topic_len;
        topics->push_back(topic);
        rem_length -= (topic.topic_len + 2);
        if (rem_length < 0)
        {
            cout << "Malformed UNSUBSCRIBE message" << endl; // MANEJO ERRORES
            return -1;
        }
    } while (rem_length > 0);
    return 0;
}

int UNSUBACK::fromBuffer(const uint8_t *buffer, ssize_t sz8)
{
    // Convert the buffer to a UNSUBACK message
    msg_id = (buffer[0] << 8) | buffer[1]; // Message ID
    return 0;
}

int UNSUBACK::toBuffer(uint8_t *buffer, ssize_t sz8)
{
    // Convert the UNSUBACK message to a buffer for sending
    int i = 0;
    bzero(buffer, BUFF_SIZE); // Clear the buffer

    type2buff(buffer);                              // Set the message type
    flags2buff(buffer);                             // Set the flags
    encodeLength(remaining_length, buffer + 1, &i); // Remaining length
    i += 1;                                         // i = 1 byte (first) + encoded_len counter bytes
    len2buff(msg_id, buffer + i);                   // Message ID
    i += 2;                                         // i = 1 byte (first) + encoded_len counter bytes
    return i;                                       // Return the number of bytes written to the buffer
}
