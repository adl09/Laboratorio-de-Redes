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
#include <chrono>
#include <set>
#include <algorithm>
#include <fcntl.h>
#include <errno.h>
#include <memory>
#include <unordered_map>
#include <poll.h>
#include "MQTT.hpp"
#include <signal.h>

using namespace std;

uint8_t buffer[BUFF_SIZE];


thread th_polling;
mutex mtx_topics, mtx_exit;
thread th_timing;

int global_sockfd;

void signal_handler(int signum)
{
    if (signum == SIGINT || signum == SIGTERM)
    {
        cout << "\nSignal received." << endl;
        DISCONNECT *disc_msg = new DISCONNECT();
        try
        {
            int msgsize = ((DISCONNECT *)disc_msg)->toBuffer(buffer, BUFF_SIZE);
            msgsize = sndMsg(global_sockfd, buffer, msgsize);
        }
        catch (const std::runtime_error &e)
        {
            cerr << "Error creating DISCONNECT message: " << e.what() << endl;
            delete disc_msg;
            exit(1);
        }
        exit(0);
    }
}

void setup_signal_handler(int sockfd)
{
    global_sockfd = sockfd;
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
}

void subscriber_routine(int sockfd)
{
    SUBSCRIBE *sub_msg = nullptr;
    SUBACK *suback_msg = nullptr;
    UNSUBSCRIBE *unsub_msg = nullptr;
    UNSUBACK *unsuback_msg = nullptr;
    PINGREQ *pingreq_msg = nullptr;
    PINGRESP *pingresp_msg = nullptr;
    PUBLISH *pub_msg = nullptr;
    DISCONNECT *disc_msg = nullptr;

    int msgsize;
    Type type;
    uint8_t type_flags, keepalive;

    set<string> subscribed;
    unordered_map<uint16_t, vector<string>> pending_sub;
    unordered_map<uint16_t, vector<string>> pending_unsub;
    uint16_t msg_id = 1;

    int stdin_fd = fileno(stdin);
    auto last_ping = chrono::steady_clock::now();
    struct pollfd fds[2];
    fds[0] = {sockfd, POLLIN, 0};
    fds[1] = {stdin_fd, POLLIN, 0};

    // CONNECT/CONNACK
    connection_procedure(sockfd, buffer, &keepalive);

    setup_signal_handler(sockfd);

    // SUBSCRIBE/UNSUBSCRIBE
    while (true)
    {
        int n = poll(fds, 2, keepalive * 1000);
        if (n < 0)
        {
            perror("poll");
            break;
        }
        if (n == 0)
        {
            try
            {
                pingreq_msg = new PINGREQ();
                msgsize = pingreq_msg->toBuffer(buffer, BUFF_SIZE);
                msgsize = sndMsg(sockfd, buffer, msgsize);
                delete pingreq_msg;
                pingreq_msg = nullptr;
                continue;
            }
            catch (const std::exception &e)
            {
                cerr << "Error sending PINGREQ: " << e.what() << endl;
                continue;
            }
        }
        // stdin
        if (fds[1].revents & POLLIN)
        {
            string cmd;
            cin >> cmd;
            vector<string> topics;
            string t;
            while (cin.peek() != '\n' && cin >> t)
                topics.push_back(t);

            uint16_t id = msg_id++;

            try
            {
                if (cmd == "SUB")
                {
                    pending_sub[id] = topics;
                    sub_msg = new SUBSCRIBE(&topics, id);
                    msgsize = ((SUBSCRIBE *)sub_msg)->toBuffer(buffer, BUFF_SIZE);
                    msgsize = sndMsg(sockfd, buffer, msgsize);
                }
                else if (cmd == "UNSUB")
                {
                    pending_unsub[id] = topics;
                    unsub_msg = new UNSUBSCRIBE(&topics, id);
                    msgsize = ((UNSUBSCRIBE *)unsub_msg)->toBuffer(buffer, BUFF_SIZE);
                    msgsize = sndMsg(sockfd, buffer, msgsize);
                }
                else if (cmd == "LIST")
                {
                    if (subscribed.empty())
                    {
                        cout << "No topics subscribed." << endl;
                    }
                    else
                    {
                        cout << "Subscribed topics:" << endl;
                        for (const auto &topic : subscribed)
                        {
                            cout << topic << endl;
                        }
                    }
                }
                else if (cmd == "PING")
                {
                    pingreq_msg = new PINGREQ();
                    msgsize = pingreq_msg->toBuffer(buffer, BUFF_SIZE);
                    msgsize = sndMsg(sockfd, buffer, msgsize);
                    delete pingreq_msg;
                    pingreq_msg = nullptr;
                }
                else if (cmd == "EXIT")
                {
                    disc_msg = new DISCONNECT();
                    msgsize = ((DISCONNECT *)disc_msg)->toBuffer(buffer, BUFF_SIZE);
                    msgsize = sndMsg(sockfd, buffer, msgsize);
                    delete disc_msg;
                    disc_msg = nullptr;
                }
            }
            catch (const std::runtime_error &e)
            {
                cerr << "Error: " << e.what() << endl;
                continue;
            }
        }

        // socket
        if (fds[0].revents & POLLIN)
        {
            try
            {
                uint8_t buf[BUFF_SIZE], flags;
                int rem;
                int len = rcvMsg(sockfd, &flags, &rem, buf, BUFF_SIZE);
                if (len == 0 || (fds[0].revents & (POLLHUP | POLLERR)))
                {
                    cerr << "Socket closed... Bye." << endl;
                    close(sockfd);
                    return;
                }
                if (len <= 0)
                    continue;

                auto type = Type(flags >> 4);

                auto it = pending_sub.end();
                switch (type)
                {
                case TYPE_PUBLISH:
                    pub_msg = new PUBLISH(flags, rem);
                    pub_msg->fromBuffer(buf, len);
                    cout << pub_msg->topic_name << "/" << pub_msg->value << endl;
                    delete pub_msg;
                    pub_msg = nullptr;
                    break;

                case TYPE_SUBACK:
                    suback_msg = new SUBACK(sub_msg);
                    suback_msg->fromBuffer(buf, len);
                    cout << "SUBACK received" << endl;

                    it = pending_sub.find(suback_msg->msg_id);
                    if (it != pending_sub.end())
                    {
                        for (auto &topic : it->second)
                            subscribed.insert(topic);
                        pending_sub.erase(it);
                    }
                    delete suback_msg, sub_msg;
                    sub_msg = nullptr;
                    suback_msg = nullptr;

                    break;

                case TYPE_UNSUBACK:
                    unsuback_msg = new UNSUBACK(unsub_msg);
                    unsuback_msg->fromBuffer(buf, len);
                    unsuback_msg->msg_id = (buf[0] << 8) | buf[1];
                    cout << "UNSUBACK received" << endl;

                    it = pending_unsub.find(unsuback_msg->msg_id);
                    if (it != pending_unsub.end())
                    {
                        for (auto &topic : it->second)
                            subscribed.erase(topic);
                        pending_unsub.erase(it);
                    }

                    delete unsuback_msg, unsub_msg;
                    unsub_msg = nullptr;
                    unsuback_msg = nullptr;
                    break;

                case TYPE_PINGRESP:
                    pingresp_msg = new PINGRESP();
                    pingresp_msg->fromBuffer(buf, len);
                    cout << "PINGRESP received" << endl;
                    delete pingresp_msg;
                    pingresp_msg = nullptr;
                    break;

                default:
                    break;
                }
            }
            catch (const std::runtime_error &e)
            {
                cerr << "Error receiving message: " << e.what() << endl;
                continue;
            }
        }
    }
}

void print_usage()
{
    cout << "Usage:" << endl;
    cout << "  SUB <topic1> <topic2> ... - Subscribe to topics" << endl;
    cout << "  UNSUB <topic1> <topic2> ... - Unsubscribe from topics" << endl;
    cout << "  LIST - List subscribed topics" << endl;
    cout << "  PING - Send a PINGREQ message" << endl;
    cout << "  EXIT - Disconnect and exit\n"
         << endl;
}

int main(int argc, char *argv[])
{
    if (argc != 3)
    {
        cerr << "Usage: ./client_sub <hostname> <port>" << endl;
        return 1;
    }

    print_usage();

    int sockfd;
    int portno = atoi(argv[2]);
    struct sockaddr_in serv_addr;
    struct hostent *server;

    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0)
    {
        cerr << "Error opening socket" << endl;
        return 1;
    }
    server = gethostbyname(argv[1]);
    if (server == NULL)
    {
        cerr << "Error, no such host" << endl;
        return 1;
    }
    bzero((char *)&serv_addr, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    bcopy((char *)server->h_addr, (char *)&serv_addr.sin_addr.s_addr, server->h_length);
    serv_addr.sin_port = htons(portno);
    if (connect(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0)
    {
        cerr << "Error connecting" << endl;
        return 1;
    }

    int flags = fcntl(sockfd, F_GETFL, 0);
    fcntl(sockfd, F_SETFL, flags | O_NONBLOCK);

    subscriber_routine(sockfd);

    close(sockfd);

    return 0;
}
