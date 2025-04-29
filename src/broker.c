#include <stdio.h>
#include <netdb.h>
#include <fcntl.h>  
#include <net/if.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <net/if.h>
#include <ifaddrs.h>
#include <pthread.h>
#include <arpa/inet.h>
#include <sys/socket.h>

#define SOLICITUD_DESCUBRIMIENTO "DISCOVERY_REQUEST"
#define PUERTO_DESCUBRIMIENTO 12345
#define PUERTO_BROKER 50000
#define TAM_BUFFER 1024
#define MAX_CLIENTES 50

int clientes[MAX_CLIENTES];
int total_clientes = 0;
pthread_mutex_t mutex_clientes = PTHREAD_MUTEX_INITIALIZER;

int contador_productores = 0;
pthread_mutex_t mutex_productores = PTHREAD_MUTEX_INITIALIZER;

void eliminar_cliente(int socket_cliente) {
    pthread_mutex_lock(&mutex_clientes);
    for (int i = 0; i < total_clientes; i++) {
        if (clientes[i] == socket_cliente) {
            for (int j = i; j < total_clientes - 1; j++) {
                clientes[j] = clientes[j + 1];
            }
            total_clientes--;
            break;
        }
    }
    pthread_mutex_unlock(&mutex_clientes);
}

void guardar_mensaje(const char *mensaje) {
    FILE *f = fopen("mensajes.log", "a");
    if (f) {
        fprintf(f, "%s\n", mensaje);
        fclose(f);
    } else {
        perror("Error al abrir mensajes.log");
    }
}

void registrar_productor(int id) {
    FILE *f = fopen("productores.log", "a");
    if (f) {
        fprintf(f, "Productor %d mandó un mensaje\n", id);
        fclose(f);
    } else {
        perror("Error al abrir productores.log");
    }
}

void *manejar_udp_descubrimiento(void *arg) {
    int socket_udp = *(int *)arg;
    struct sockaddr_in direccion_cliente;
    socklen_t tam_direccion = sizeof(direccion_cliente);
    char buffer[TAM_BUFFER];
    char ip_broker[INET_ADDRSTRLEN] = {0};

    struct ifaddrs *interfaces, *interfaz;
    if (getifaddrs(&interfaces) != -1) {
        for (interfaz = interfaces; interfaz != NULL; interfaz = interfaz->ifa_next) {
            if (interfaz->ifa_addr && interfaz->ifa_addr->sa_family == AF_INET &&
                !(interfaz->ifa_flags & IFF_LOOPBACK)) {
                void *direccion = &((struct sockaddr_in *)interfaz->ifa_addr)->sin_addr;
                inet_ntop(AF_INET, direccion, ip_broker, INET_ADDRSTRLEN);
                break;
            }
        }
        freeifaddrs(interfaces);
    }

    if (strlen(ip_broker) == 0) {
        perror("No se encontró una IP válida");
        return NULL;
    }

    printf("IP del Broker: %s\n", ip_broker);

    while (1) {
        recvfrom(socket_udp, buffer, TAM_BUFFER, 0, (struct sockaddr *)&direccion_cliente, &tam_direccion);
        char respuesta[TAM_BUFFER]; snprintf(respuesta, sizeof(respuesta), "%s:%d", ip_broker, PUERTO_BROKER);
        sendto(socket_udp, respuesta, strlen(respuesta), 0, (struct sockaddr *)&direccion_cliente, tam_direccion);
    }

    return NULL;
}

void *manejar_cliente(void *arg) {
    int socket_cliente = *(int *)arg;
    free(arg);
    char buffer[TAM_BUFFER];
    int bytes_recibidos;

    while (1) {
        memset(buffer, 0, TAM_BUFFER);
        bytes_recibidos = recv(socket_cliente, buffer, TAM_BUFFER - 1, 0);

        if (bytes_recibidos <= 0) {
            printf("Cliente desconectado\n");
            close(socket_cliente);
            eliminar_cliente(socket_cliente);
            break;
        }

        buffer[bytes_recibidos] = '\0';

        if (strcmp(buffer, "CONSUMIDOR") == 0) {
            continue; 
        }

        printf("Mensaje recibido: %s\n", buffer);
        pthread_mutex_lock(&mutex_productores);
        int id_productor = ++contador_productores;
        pthread_mutex_unlock(&mutex_productores);
        registrar_productor(id_productor); 
        guardar_mensaje(buffer);

        pthread_mutex_lock(&mutex_clientes);
        for (int i = 0; i < total_clientes; i++) {
            if (clientes[i] != socket_cliente) {
                if (send(clientes[i], buffer, strlen(buffer), 0) < 0)
                    perror("Error al reenviar mensaje");
            }
        }
        pthread_mutex_unlock(&mutex_clientes);
    }

    return NULL;
}

void *manejar_conexiones_tcp(void *arg) {
    int socket_tcp = *(int *)arg;
    struct sockaddr_in direccion_cliente;
    socklen_t tam_direccion = sizeof(direccion_cliente);

    while (1) {
        int nuevo_cliente = accept(socket_tcp, (struct sockaddr *)&direccion_cliente, &tam_direccion);
        if (nuevo_cliente < 0) {
            perror("Error en accept");
            continue;
        }

        pthread_mutex_lock(&mutex_clientes);
        if (total_clientes < MAX_CLIENTES) {
            clientes[total_clientes++] = nuevo_cliente;
            printf("Nuevo cliente: %s\n", inet_ntoa(direccion_cliente.sin_addr));

            pthread_t hilo_cliente;
            int *socket_ptr = malloc(sizeof(int));
            *socket_ptr = nuevo_cliente;
            pthread_create(&hilo_cliente, NULL, manejar_cliente, socket_ptr);
            pthread_detach(hilo_cliente);  
        } else {
            printf("Máximo de clientes alcanzado\n");
            close(nuevo_cliente);
        }
        pthread_mutex_unlock(&mutex_clientes);
    }

    return NULL;
}


int main() {
    int socket_udp, socket_tcp;
    pthread_t hilo_udp, hilo_tcp;

    socket_udp = socket(AF_INET, SOCK_DGRAM, 0);
    struct sockaddr_in direccion_broker = { .sin_family = AF_INET, .sin_port = htons(PUERTO_DESCUBRIMIENTO), .sin_addr.s_addr = INADDR_ANY };
    bind(socket_udp, (struct sockaddr *)&direccion_broker, sizeof(direccion_broker));

    socket_tcp = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in direccion_tcp = {
        .sin_family = AF_INET,
        .sin_port = htons(PUERTO_BROKER),
        .sin_addr.s_addr = INADDR_ANY
    };

    bind(socket_tcp, (struct sockaddr *)&direccion_tcp, sizeof(direccion_tcp));
    listen(socket_tcp, 5);

    printf("Broker escuchando en el puerto TCP %d...\n", PUERTO_BROKER);

    pthread_create(&hilo_udp, NULL, manejar_udp_descubrimiento, &socket_udp);
    pthread_create(&hilo_tcp, NULL, manejar_conexiones_tcp, &socket_tcp);

    pthread_join(hilo_udp, NULL);
    pthread_join(hilo_tcp, NULL);

    close(socket_tcp);
    close(socket_udp);
    return 0;
}
