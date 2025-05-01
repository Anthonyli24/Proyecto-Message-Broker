#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <netinet/in.h>
 
#define MENSAJE_DESCUBRIMIENTO "DISCOVERY_REQUEST"
#define MENSAJE_IDENTIFICACION "CONSUMIDOR"
#define PUERTO_DESCUBRIMIENTO 12345
#define TAM_BUFFER 1024
 
int crear_socket_udp_broadcast() {
    int socket_udp = socket(AF_INET, SOCK_DGRAM, 0);
    int habilitar_broadcast = 1;
    setsockopt(socket_udp, SOL_SOCKET, SO_BROADCAST, &habilitar_broadcast, sizeof(habilitar_broadcast));
    return socket_udp;
}
 
void enviar_mensaje_descubrimiento(int socket_udp) {
    struct sockaddr_in direccion_broadcast = {
        .sin_family = AF_INET,
        .sin_port = htons(PUERTO_DESCUBRIMIENTO),
        .sin_addr.s_addr = INADDR_BROADCAST
    };
    sendto(socket_udp, MENSAJE_DESCUBRIMIENTO, strlen(MENSAJE_DESCUBRIMIENTO), 0,
           (struct sockaddr *)&direccion_broadcast, sizeof(direccion_broadcast));
}
 
void recibir_info_broker(int socket_udp, char *ip, int *puerto) {
    struct sockaddr_in direccion_broker;
    socklen_t tam_direccion = sizeof(direccion_broker);
    char buffer[TAM_BUFFER];
    int n = recvfrom(socket_udp, buffer, sizeof(buffer), 0,
                     (struct sockaddr *)&direccion_broker, &tam_direccion);
    buffer[n] = '\0';
    sscanf(buffer, "%15[^:]:%d", ip, puerto);
}
 
int establecer_conexion_tcp(const char *ip, int puerto) {
    int socket_tcp = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in direccion_broker = {
        .sin_family = AF_INET,
        .sin_port = htons(puerto)
    };
    inet_pton(AF_INET, ip, &direccion_broker.sin_addr);
    connect(socket_tcp, (struct sockaddr *)&direccion_broker, sizeof(direccion_broker));
    return socket_tcp;
}
 
void escuchar_mensajes(int socket_tcp, const char *grupo) {
    char buffer[TAM_BUFFER];
    printf("Esperando mensajes del Broker (grupo %s)...\n", grupo);
    while (1) {
        int bytes_recibidos = recv(socket_tcp, buffer, sizeof(buffer) - 1, 0);
        if (bytes_recibidos > 0) {
            buffer[bytes_recibidos] = '\0';
            printf("Mensaje [%s]: %s\n", grupo, buffer);
        } else if (bytes_recibidos == 0) {
            printf("El Broker cerró la conexión.\n");
            break;
        } else {
            perror("Error al recibir");
            break;
        }
    }
}
 
int main() {
    char ip_broker[INET_ADDRSTRLEN];
    int puerto_broker;
 
    // 1. Descubrimiento vía UDP
    int socket_udp = crear_socket_udp_broadcast();
    enviar_mensaje_descubrimiento(socket_udp);
    recibir_info_broker(socket_udp, ip_broker, &puerto_broker);
    close(socket_udp);
 
    // 2. Conexión TCP al broker
    int socket_tcp = establecer_conexion_tcp(ip_broker, puerto_broker);
 
    // 3. Identificarse como consumidor
    send(socket_tcp, MENSAJE_IDENTIFICACION, strlen(MENSAJE_IDENTIFICACION), 0);
 
    // 4. Recibir grupo asignado
    char grupo[2] = {0}; // Un carácter más terminador nulo
    int bytes = recv(socket_tcp, grupo, 1, 0);
    if (bytes <= 0) {
        printf("Error al recibir grupo\n");
        close(socket_tcp);
        return 1;
    }
    grupo[1] = '\0';
    printf("Conectado al grupo asignado por el broker: %s\n", grupo);
 
    // 5. Escuchar mensajes del broker
    escuchar_mensajes(socket_tcp, grupo);
 
    close(socket_tcp);
    return 0;
}
 