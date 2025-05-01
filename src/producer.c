#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <time.h>
 
#define MENSAJE_DESCUBRIMIENTO "DISCOVERY_REQUEST"
#define PUERTO_DESCUBRIMIENTO 12345
#define TAM_BUFFER 1024
#define NUM_MENSAJES 10
 
const char *mensajes[NUM_MENSAJES] = {
    "Bienvenido al sistema.",
    "Buen dia para todos.",
    "Hola, soy el productor.",
    "Hola, ¿cómo estás?",
    "Saludos desde el productor.",
    "Mensaje de prueba.",
    "Este es un mensaje.",
    "Mensaje aleatorio.",
    "Hola desde el productor.",
    "Que tenga un buen día."
};
 
int crear_socket_udp() {
    int socket_udp = socket(AF_INET, SOCK_DGRAM, 0);
    int habilitar_broadcast = 1;
    setsockopt(socket_udp, SOL_SOCKET, SO_BROADCAST, &habilitar_broadcast, sizeof(habilitar_broadcast));
    return socket_udp;
}
 
void enviar_mensaje_descubrimiento(int socket_udp) {
    struct sockaddr_in direccion_broadcast = { .sin_family = AF_INET, .sin_port = htons(PUERTO_DESCUBRIMIENTO), .sin_addr.s_addr = INADDR_BROADCAST };
    sendto(socket_udp, MENSAJE_DESCUBRIMIENTO, strlen(MENSAJE_DESCUBRIMIENTO), 0, (struct sockaddr *)&direccion_broadcast, sizeof(direccion_broadcast));
}
 
void recibir_info_broker(int socket_udp, char *ip, int *puerto) {
    struct sockaddr_in direccion_broker;
    socklen_t tam_direccion = sizeof(direccion_broker);
    char buffer[TAM_BUFFER];
    int n = recvfrom(socket_udp, buffer, sizeof(buffer), 0, (struct sockaddr *)&direccion_broker, &tam_direccion);
    buffer[n] = '\0';
    sscanf(buffer, "%15[^:]:%d", ip, puerto);
}
 
int establecer_conexion_tcp(const char *ip, int puerto) {
    int socket_tcp = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in direccion_broker = { .sin_family = AF_INET, .sin_port = htons(puerto) };
    inet_pton(AF_INET, ip, &direccion_broker.sin_addr);
    connect(socket_tcp, (struct sockaddr *)&direccion_broker, sizeof(direccion_broker));
    return socket_tcp;
}
 
void enviar_mensaje_usuario(int socket_tcp) {
    int mensaje_aleatorio = rand() % NUM_MENSAJES;
    send(socket_tcp, mensajes[mensaje_aleatorio], strlen(mensajes[mensaje_aleatorio]), 0);
    printf("Mensaje enviado al Broker: %s\n", mensajes[mensaje_aleatorio]);
}
 
int main() {
    char ip_broker[INET_ADDRSTRLEN];
    int puerto_broker;
    srand(time(NULL));
 
    // Descubrimiento de la informacion del broker vía UDP broadcast
    int socket_udp = crear_socket_udp();
    enviar_mensaje_descubrimiento(socket_udp);
    recibir_info_broker(socket_udp, ip_broker, &puerto_broker);
    close(socket_udp);
 
    // Conexión TCP con el broker
    int socket_tcp = establecer_conexion_tcp(ip_broker, puerto_broker);
 
    // Enviar mensajes al broker
    enviar_mensaje_usuario(socket_tcp);
 
    close(socket_tcp);
    return 0;
}