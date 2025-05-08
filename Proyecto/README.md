IDEA PROYECTO:
    Obfuscador UDP into TCP. (Para simplificar puede hacerse unidireccional).
    1. Server-Client: fakeTCP entre servidor y cliente. Luego ver como evitar los ACK, que funcione similar UDP.
    2. Un proceso en PC server envía datos al proceso(obfuscador) server, este lo procesa (tiene una cola) y lo manda a la red por TCP.
    3. Se tiene que calcular checksum para que los paquetes fake-tcp se vean reales.
    4. Creo una interfaz sobre la que escucha el proceso y por la que dirijo todo el tráfico de la PC. Sobre esta interfaz se setean rutas que redirijan el trafico saliente (fakeTCP) por la interfaz física.
    5. Múltiples clientes.