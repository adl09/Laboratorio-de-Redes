#include <stdint.h>
#include <assert.h>
#include <cstring>
#include <iostream>
#include <arpa/inet.h>

#define BUFF_SIZE 1024

using namespace std;

typedef enum
{
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

#define TYPE_MSK 0xF0
#define FLAGS_MSK 0x0F

class MQTTMsg
{
public:
    // Fixed header
    Type type;
    uint8_t flags;
    int remaining_length;
    // Variable header and payload

    MQTTMsg() {}
    virtual ~MQTTMsg() {}

    void type2buff(uint8_t *buffer) const
    {
        *buffer = ((this->type << 4) & TYPE_MSK) | (*buffer & FLAGS_MSK);
        return;
    }

    void flags2buff(uint8_t *buffer) const
    {
        *buffer = (*buffer & TYPE_MSK) | (this->flags & FLAGS_MSK);
        return;
    }

    void len2buff(uint16_t len, uint8_t *buffer) const
    {
        // 2 bytes encode
        *buffer = (len >> 8) & 0xFF; // high byte
        *(buffer + 1) = len & 0xFF;  // low byte
        return;
    }

    // Type getType() const { return type; }
    // uint8_t getFlags() const { return flags; }
    // int getRemainingLength() const { return remaining_length; }

    virtual int toBuffer(uint8_t *buffer, ssize_t sz8) { return 0; };         // Convert the message to a buffer for sending
    virtual int fromBuffer(const uint8_t *buffer, ssize_t sz8) { return 0; }; // Convert the buffer to a message for receiving
};

void connection_procedure(int sockfd, uint8_t *buffer, uint8_t *keepalive=0);
void subscribe_procedure(int sockfd, uint8_t *buffer, vector<string> *topic);


void encodeLength(int value, uint8_t *encoded, int *pcount);
int getEncodedLength(int sockfd, int *pcount);
int rcvMsg(int sockfd, uint8_t *type, int *remlen, uint8_t *buffer, uint16_t sz8);
int sndMsg(int sockfd, uint8_t *buffer, int length);

#define FCONNECT_DEF_TYPEFLAG (uint8_t)0x10
#define FCONNECT_DEF_REMLENGTH 12
#define FCONNECT_CLEAN_SESSION 0x02
#define FCONNECT_WILL_RETAIN 0x20
#define FCONNECT_WILL_QOS 0x18
#define FCONNECT_WILL 0x04
#define FCONNECT_PASSWORD 0x40
#define FCONNECT_USER_NAME 0x80

class CONNECT : public MQTTMsg {
public:
    // Variable header
    uint16_t protocol_name_len;
    string protocol_name;
    uint8_t protocol_level;
    uint8_t connect_flags;
    uint16_t keep_alive;
    // Payload
    uint16_t client_id_len;
    string client_id;

    CONNECT(uint8_t type_flags = FCONNECT_DEF_TYPEFLAG,
            int remlength = FCONNECT_DEF_REMLENGTH,
            int keepalive_ = 20)
    {
        this->type = (Type)(type_flags >> 4);
        this->flags = (type_flags & FLAGS_MSK);
        if (this->type != TYPE_CONNECT)
        {
            throw std::runtime_error("Invalid type for CONNECT message");
        }
        this->remaining_length = remlength;
        protocol_name_len = 4;
        protocol_name = "MQTT";
        protocol_level = 4;
        connect_flags = (FCONNECT_CLEAN_SESSION);
        keep_alive = keepalive_;
        client_id_len = 0;
        client_id = "";
    }
    int toBuffer(uint8_t *buffer, ssize_t sz8) override;
    int fromBuffer(const uint8_t *buffer, ssize_t sz8) override;
};

class CONNACK : public MQTTMsg
{
public:
    // Variable header
    uint8_t session_present;
    uint8_t return_code;

    CONNACK(uint8_t type_flags = 0x20,
            int remlength = 2)
    {
        this->type = (Type)(type_flags >> 4);
        this->flags = (type_flags & FLAGS_MSK);

        if (this->type != TYPE_CONNACK)
        {
            throw std::runtime_error("Invalid type for CONNACK message");
        }

        this->remaining_length = remlength;
        session_present = 0; // Clean session (bit=0)
        return_code = 0;
    }

    int toBuffer(uint8_t *buffer, ssize_t sz8) override;
    int fromBuffer(const uint8_t *buffer, ssize_t sz8) override;
};

#define FPUBLISH_DEF_TYPEFLAG (uint8_t)0x30

class PUBLISH : public MQTTMsg
{
public:
    // Variable header
    uint16_t topic_name_len;
    string topic_name;
    // uint16_t packet_id; // Not present in QoS 0
    //  Payload
    string value;

    PUBLISH(uint8_t type_flags,
            int remlength)
    {
        this->type = (Type)(type_flags >> 4);
        this->flags = (type_flags & FLAGS_MSK); // 1 for RETAIN

        if (this->type != TYPE_PUBLISH)
        {
            throw std::runtime_error("Invalid type for PUBLISH message");
        }

        this->remaining_length = remlength;
        topic_name_len = 0;
        topic_name = "";
        value = "";
    }

    PUBLISH(string *topic,
            string *value_,
            uint8_t type_flags = FPUBLISH_DEF_TYPEFLAG,
            int remlength = 0)
    {
        this->type = (Type)(type_flags >> 4);
        this->flags = (type_flags & FLAGS_MSK); // 1 for RETAIN

        if (this->type != TYPE_PUBLISH)
        {
            throw std::runtime_error("Invalid type for PUBLISH message");
        }

        if (remlength == 0)
        {
            this->remaining_length = strlen(topic->c_str()) + strlen(value_->c_str()) + 2; // 2 bytes for topic name length
        }

        topic_name_len = strlen(topic->c_str());
        topic_name = *topic;
        value = *value_;
    }

    int toBuffer(uint8_t *buffer, ssize_t sz8) override;
    int fromBuffer(const uint8_t *buffer, ssize_t sz8) override;
};

struct topics_struct
{
    uint16_t topic_len;
    string topic_name;
    uint8_t qos;
};

#define FSUBSCRIBE_DEF_TYPEFLAG (uint8_t)0x82
#define FSUBSCRIBE_DEF_REMLENGTH 0

class SUBSCRIBE : public MQTTMsg
{
public:
    // Payload
    vector<topics_struct> *topics;
    uint16_t msg_id; // packet id

    SUBSCRIBE(uint8_t type_flags = FSUBSCRIBE_DEF_TYPEFLAG,
              int remlength = FSUBSCRIBE_DEF_REMLENGTH,
              vector<string> *topics_ = nullptr, uint16_t msg_id_ = 0)
    {
        this->type = (Type)(type_flags >> 4);
        this->flags = (type_flags & FLAGS_MSK);

        if (this->type != TYPE_SUBSCRIBE)
        {
            throw std::runtime_error("Invalid type for SUBSCRIBE message");
        }

        this->remaining_length = remlength;
        this->topics = new vector<topics_struct>();
        if (topics_ != nullptr)
        {
            for (auto &topic : *topics_)
            {
                topics_struct t;
                t.topic_name = topic;
                t.topic_len = topic.length();
                t.qos = 0; // QoS 0
                this->topics->push_back(t);
                this->remaining_length += t.topic_len + 3; // 2 bytes for topic name length + 1 byte for QoS
            }
        }
    }

    SUBSCRIBE(vector<string> *topics_,
              uint16_t msg_id_ = 0)
    {
        this->type = TYPE_SUBSCRIBE;
        this->flags = 2;            // FLAGS FIELD MUST BE 0010
        this->msg_id = msg_id_;           // packet id
        this->remaining_length = 2; // 2 bytes for packet id
        // string to topics_struct
        this->topics = new vector<topics_struct>();
        for (auto &topic : *topics_)
        {
            topics_struct t;
            t.topic_name = topic;
            t.topic_len = topic.length();
            t.qos = 0; // QoS 0
            this->topics->push_back(t);
            this->remaining_length += t.topic_len + 3; // 2 bytes for topic name length + 1 byte for QoS
        }
    }

    int toBuffer(uint8_t *buffer, ssize_t sz8) override;
    int fromBuffer(const uint8_t *buffer, ssize_t sz8) override;
};

class SUBACK : public MQTTMsg
{
public:
    // Variable header
    uint16_t msg_id; // packet id
    // Payload
    vector<uint8_t> *return_codes;

    SUBACK(SUBSCRIBE *sub_msg = nullptr,
           uint8_t type_flags = 0x90,
           int remlength = 0,
           uint16_t msg_id_ = 0)
    {
        this->type = (Type)(type_flags >> 4);
        this->flags = (type_flags & FLAGS_MSK);

        if (this->type != TYPE_SUBACK)
        {
            throw std::runtime_error("Invalid type for SUBACK message");
        }

        this->remaining_length = remlength;
        //this->msg_id = msg_id_; // packet id
        this->return_codes = new vector<uint8_t>();
        if (sub_msg != nullptr)
        {
            this->msg_id = sub_msg->msg_id;
            for (int i = 0; i < (int)sub_msg->topics->size(); ++i)
            {
                this->return_codes->push_back((*sub_msg->topics)[i].qos);
            }
            this->remaining_length += this->return_codes->size() + 2; // 2 bytes for packet id
        }
    }

    int toBuffer(uint8_t *buffer, ssize_t sz8) override;
    int fromBuffer(const uint8_t *buffer, ssize_t sz8) override;
};

class PINGREQ : public MQTTMsg
{
public:
    PINGREQ(uint8_t type_flags = 0xC0,
            int remlength = 0)
    {
        this->type = (Type)(type_flags >> 4);
        this->flags = (type_flags & FLAGS_MSK);

        if (this->type != TYPE_PINGREQ)
        {
            throw std::runtime_error("Invalid type for PINGREQ message");
        }

        this->remaining_length = remlength;
    }
    int toBuffer(uint8_t *buffer, ssize_t sz8) override;
    int fromBuffer(const uint8_t *buffer, ssize_t sz8) override;
};

class PINGRESP : public MQTTMsg
{
public:
    PINGRESP(uint8_t type_flags = 0xD0,
             int remlength = 0)
    {
        this->type = (Type)(type_flags >> 4);
        this->flags = (type_flags & FLAGS_MSK);

        if (this->type != TYPE_PINGRESP)
        {
            throw std::runtime_error("Invalid type for PINGRESP message");
        }

        this->remaining_length = remlength;
    }
    int toBuffer(uint8_t *buffer, ssize_t sz8) override;
    int fromBuffer(const uint8_t *buffer, ssize_t sz8) override;
};
class DISCONNECT : public MQTTMsg
{
public:
    DISCONNECT(uint8_t type_flags = 0xE0,
               int remlength = 0)
    {
        this->type = (Type)(type_flags >> 4);
        this->flags = (type_flags & FLAGS_MSK);

        if (this->type != TYPE_DISCONNECT)
        {
            throw std::runtime_error("Invalid type for DISCONNECT message");
        }

        this->remaining_length = remlength;
    }
    int toBuffer(uint8_t *buffer, ssize_t sz8) override;
    int fromBuffer(const uint8_t *buffer, ssize_t sz8) override;
};

class UNSUBSCRIBE : public MQTTMsg
{
public:
    // Payload
    vector<topics_struct> *topics;
    uint16_t msg_id; // packet id

    UNSUBSCRIBE(uint8_t type_flags = 0xA2,
                int remlength = 0,
                vector<string> *topics_ = nullptr,
                uint16_t msg_id_ = 0)
    {
        this->type = (Type)(type_flags >> 4);
        this->flags = (type_flags & FLAGS_MSK);

        if (this->type != TYPE_UNSUBSCRIBE)
        {
            throw std::runtime_error("Invalid type for UNSUBSCRIBE message");
        }

        this->remaining_length = remlength;
        this->topics = new vector<topics_struct>();
        if (topics_ != nullptr)
        {
            for (auto &topic : *topics_)
            {
                topics_struct t;
                t.topic_name = topic;
                t.topic_len = topic.length();
                t.qos = 0; // QoS is not used in UNSUBSCRIBE
                this->topics->push_back(t);
                this->remaining_length += t.topic_len + 2; // 2 bytes for topic name length
            }
        }
    }

    UNSUBSCRIBE(vector<string> *topics_,
                uint16_t msg_id_ = 0)
    {
        this->type = TYPE_UNSUBSCRIBE;
        this->flags = 2;            // FLAGS FIELD MUST BE 0010
        this->msg_id = msg_id_;           // packet id
        this->remaining_length = 2; // 2 bytes for packet id
        // string to topics_struct
        this->topics = new vector<topics_struct>();
        for (auto &topic : *topics_)
        {
            topics_struct t;
            t.topic_name = topic;
            t.topic_len = topic.length();
            t.qos = 0; // QoS is not used in UNSUBSCRIBE
            this->topics->push_back(t);
            this->remaining_length += t.topic_len + 2; // 2 bytes for topic name length
        }
    }

    int toBuffer(uint8_t *buffer, ssize_t sz8) override;
    int fromBuffer(const uint8_t *buffer, ssize_t sz8) override;
};

class UNSUBACK : public MQTTMsg
{
    public:
    UNSUBACK(UNSUBSCRIBE *unsub_msg=nullptr,
            uint8_t type_flags = 0xB0,
            int remlength = 2,
            uint16_t msg_id_ = 0)
    {
        this->type = (Type)(type_flags >> 4);
        this->flags = (type_flags & FLAGS_MSK);
        this->msg_id = msg_id_;

        if (this->type != TYPE_UNSUBACK)
        {
            throw std::runtime_error("Invalid type for UNSUBACK message");
        }

        if (unsub_msg != nullptr)
        {
            this->msg_id = unsub_msg->msg_id;
        }

        

        this->remaining_length = remlength;
    }
    uint16_t msg_id;

    int toBuffer(uint8_t *buffer, ssize_t sz8) override;
    int fromBuffer(const uint8_t *buffer, ssize_t sz8) override;
  
};