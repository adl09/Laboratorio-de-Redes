#include <stdint.h>
#include <assert.h>
#include <string.h>
#include <arpa/inet.h>

typedef enum {
    TYPE_RESERVED,
    TYPE_CONNECT,
    TYPE_CONNACK,
    TYPE_PUBLISH,
    TYPE_PUBACK,
    TYPE_PUBREC,
    TYPE_PUBREL,
    TYPE_PUBCOMP,
    TYPE_SUBSCRIBE,
    TYPE_SUBACK,
    TYPE_UNSUBSCRIBE,
    TYPE_UNSUBACK,
    TYPE_PINGREQ,
    TYPE_PINGRESP,
    TYPE_DISCONNECT,
    TYPE_AUTH,
} Type;

typedef struct __attribute__((__packed__)) 
{
    uint16_t len;
    uint8_t str[0];
} VString;


class MQTTMsg {
    public:
        // Fixed header
        Type type;
        uint8_t flags;
        int remaining_length;
        // Variable header and payload

        MQTTMsg(){}
        virtual ~MQTTMsg() {}
            
        // Type getType() const { return type; }
        // uint8_t getFlags() const { return flags; }
        // int getRemainingLength() const { return remaining_length; }

        virtual int toBuffer(uint8_t *buffer){return 0;}; // Convert the message to a buffer for sending
        virtual int fromBuffer(const uint8_t *buffer, int length){return 0;}; // Convert the buffer to a message for receiving
};

int rcvMsg(int sockfd, MQTTMsg *msg);
int sndMsg(int sockfd, MQTTMsg *msg);

class CONNECT:public MQTTMsg {
    public:
        VString protocol_name;
        uint8_t protocol_level;
        uint8_t connect_flags;
        uint16_t keep_alive;

        
};