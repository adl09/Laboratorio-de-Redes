LDLIBS = -lm -pthread
CFLAGS = -g -Wall
SRC_DIR = ./src

TARGETS = server client_sub client_pub

all: ${TARGETS}

server: ${SRC_DIR}/server.cpp ${SRC_DIR}/broker.cpp ${SRC_DIR}/broker.hpp ${SRC_DIR}/MQTT.cpp
	@g++ -o server ${SRC_DIR}/server.cpp ${SRC_DIR}/broker.cpp ${SRC_DIR}/MQTT.cpp ${CFLAGS} ${LDLIBS}

client_sub: ${SRC_DIR}/client_sub.cpp ${SRC_DIR}/MQTT.cpp
	@g++ -o client_sub ${SRC_DIR}/client_sub.cpp ${SRC_DIR}/MQTT.cpp ${CFLAGS} ${LDLIBS}

client_pub: ${SRC_DIR}/client_pub.cpp ${SRC_DIR}/MQTT.cpp
	@g++ -o client_pub ${SRC_DIR}/client_pub.cpp ${SRC_DIR}/MQTT.cpp ${CFLAGS} ${LDLIBS}

clean:
	@rm -f ${TARGETS} core*
