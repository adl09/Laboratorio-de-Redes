# MQTT
Protocolo Publish/Subscribe
Tiene que ser compatible con Mosquitto.

Buffers de 

# Broker
- A este se conectan los clientes que se suscriban o publiquen a un tópico. Tópico: cadenas de caracteres (tópico,valor).
- Puedo utilizar https://test.mosquitto.org/ como broker de test.
- main()
- Multiprocesos:
    - Atender a múltiples conexiones: fork, thread, select.
    - Al recibir una nueva conexión: creo un nuevo socket con accept.
    - Cuando un thread recibe un publish, revisa la tabla de clientes suscriptos y les reenvia el mensaje.
    - Para ver los hilos usar: top -H -p PID (PID LO OBTENGO CON ps ax | grep server)
- Tablas:
    - Conexiones (clientes).
    - Tópicos a retener (tópico, valor).
    - 


# Client
- main() -> opciones para suscribir/publicar
- No creo necesitar multithreading.
- Tengo una cola de envío y otra de recepción.

Subscriber:


Publisher:


# TO-DO
1. Implementación protocolo. Output: Mensajes.
2. Manejo conexiones, cliente-server. 


================================================
18-05-25
./server 1234
./client_sub localhost 1234 topic
./client_pub localhost 1234 topic value

TENEMOS:
- ./server crea un objeto broker (acá se inicializa el socket en 1234 y se pone listen) y llama al método acceptClients() (while(1) accept, esto es bloqueante, pero se bloquea el hilo principal solamente. A cada cliente nuevo se lo maneja en un hilo nuevo).

- 

================================================
20-05-25
# Requisitos:
1. CONNECT: 
    a. Client2Server
    b. Servidor: espera solo UN connect, si viene otro cierra la conexión.
    c. Flags usadas: username, password, willretain, cleansession=1 (no voy a guardar estados), willmessage.
    d. 
2. CONNACK:
    a. Server2Client
    b. 