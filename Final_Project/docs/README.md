## Project Overview

This project explores key concepts in computer networks, focusing on the implementation and experimentation with the MQTT protocol. MQTT (Message Queuing Telemetry Transport) is a lightweight messaging protocol commonly used for IoT (Internet of Things) applications due to its efficiency and simplicity.

The project includes:

- Implementation of MQTT clients and a broker to facilitate publish/subscribe communication.
- Demonstrations of how clients can publish messages to specific topics and subscribe to receive relevant data.
- Experiments and analysis of data transmission, error handling, and network performance using MQTT.

## Using the MQTT Clients and Server

The `src` directory contains three main components:

- **server**: Acts as the MQTT broker. It manages topic subscriptions and routes messages from publishers to subscribers.
- **client_pub**: An MQTT client that publishes messages to a specified topic.
- **client_sub**: An MQTT client that subscribes to a topic and receives messages published to it.

### How to Use

1. **Start the server (broker):**
    ```bash
    ./server
    ```

2. **Start a subscriber client:**
    ```bash
    ./client_sub <ip> <port>
    ```
    After starting, you can interact with the program to subscribe (`SUB <topic>`) or unsubscribe (`UNSUB <topic>`) from topics as needed.

3. **Start a publisher client:**
    ```bash
    ./client_pub <ip> <port> <topic> <message> <retain>
    ```
    Replace `<topic>` with the topic to publish to, and `<message>` with the message content, and `<retain>` with the retain flag (0 or 1).

You can run multiple subscribers and publishers simultaneously to test publish/subscribe functionality.

## MQTT Clients and Broker

- **MQTT Clients:** These are devices or applications that connect to the MQTT broker. Clients can act as publishers (sending messages to topics) or subscribers (receiving messages from topics they are interested in). In this project, you will find sample client implementations that demonstrate both roles.
- **MQTT Broker:** The broker is the central server that manages message distribution. It receives messages from publishers and forwards them to the appropriate subscribers based on topic subscriptions. The broker ensures reliable and efficient message delivery between clients.

This setup allows for scalable, real-time communication between multiple devices, making it ideal for networked applications and IoT systems.
