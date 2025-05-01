#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <ifaddrs.h>
#include <net/if.h>
#include <time.h> 
  
#define SOLICITUD_DESCUBRIMIENTO "DISCOVERY_REQUEST" 
#define LOG_FILE "log_broker.txt" 
#define PUERTO_DESCUBRIMIENTO 12345 
#define PUERTO_BROKER 50000 
#define TAM_BUFFER 1024 
#define MAX_CLIENTES 50 
#define MAX_MENSAJES 1000 
#define MAX_GRUPOS 20
 
typedef struct {
    char mensajes[MAX_MENSAJES][TAM_BUFFER];
    int inicio;
    int fin;
    int total;
    pthread_mutex_t mutex;
    pthread_cond_t cond;
} ColaMensajes;
 
ColaMensajes cola = {.inicio = 0, .fin = 0, .total = 0, .mutex = PTHREAD_MUTEX_INITIALIZER, .cond = PTHREAD_COND_INITIALIZER};
   
typedef struct {
    char nombre[50];
    int offset;
 
} GrupoOffset;
  
FILE* log_fp = NULL;
   
void get_timestamp(char* ts, size_t size) {
    time_t now = time(NULL);
    struct tm* t = localtime(&now);
    strftime(ts, size, "[%H:%M:%S]", t);
 
}
 
void write_log(const char* msg) {
    char timestamp[32];
    get_timestamp(timestamp, sizeof(timestamp));
    fprintf(log_fp, "%s %s\n", timestamp, msg);
    fflush(log_fp);
}
 
GrupoOffset grupos[MAX_GRUPOS]; 
int num_grupos = 0; 
pthread_mutex_t mutex_grupos = PTHREAD_MUTEX_INITIALIZER;
  
int clientes[MAX_CLIENTES]; 
int total_clientes = 0;
pthread_mutex_t mutex_clientes = PTHREAD_MUTEX_INITIALIZER;
 
void encolar_mensaje(const char *mensaje) {
    pthread_mutex_lock(&cola.mutex);
    if (cola.total < MAX_MENSAJES) {
        strncpy(cola.mensajes[cola.fin], mensaje, TAM_BUFFER);
        cola.fin = (cola.fin + 1) % MAX_MENSAJES;
        cola.total++;
        pthread_cond_broadcast(&cola.cond);
        write_log(mensaje); 
    }
    pthread_mutex_unlock(&cola.mutex);
}
 
int obtener_offset(const char *grupo) {
    pthread_mutex_lock(&mutex_grupos);
    for (int i = 0; i < num_grupos; i++) {
        if (strcmp(grupos[i].nombre, grupo) == 0) {
            pthread_mutex_unlock(&mutex_grupos);
            return grupos[i].offset;
        }
    }
 
    strncpy(grupos[num_grupos].nombre, grupo, 50);
    grupos[num_grupos].offset = 0;
    num_grupos++;
    pthread_mutex_unlock(&mutex_grupos);
    return 0;
 
}
 
void actualizar_offset(const char *grupo) {
    pthread_mutex_lock(&mutex_grupos);
    for (int i = 0; i < num_grupos; i++) {
        if (strcmp(grupos[i].nombre, grupo) == 0) {
            grupos[i].offset++;
            break;
        }
    }
    pthread_mutex_unlock(&mutex_grupos);
}
 
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
        char respuesta[TAM_BUFFER];
        snprintf(respuesta, sizeof(respuesta), "%s:%d", ip_broker, PUERTO_BROKER);
        sendto(socket_udp, respuesta, strlen(respuesta), 0, (struct sockaddr *)&direccion_cliente, tam_direccion);
 
    }
    return NULL;
}
 
void *manejar_cliente(void *arg) {
    int socket_cliente = *(int *)arg;
    free(arg);
    char buffer[TAM_BUFFER];
    int bytes_recibidos;
 
    bytes_recibidos = recv(socket_cliente, buffer, TAM_BUFFER - 1, 0);
 
    if (bytes_recibidos <= 0) {
        close(socket_cliente);
        eliminar_cliente(socket_cliente);
        return NULL;
    }
 
    buffer[bytes_recibidos] = '\0';
  
    if (strcmp(buffer, "CONSUMIDOR") == 0) {// ASIGNACIÓN ALEATORIA DE GRUPO
        char grupo = 'A' + (rand() % 3);  // 'A', 'B' o 'C'
        char grupo_str[2] = {grupo, '\0'};
 
        if (send(socket_cliente, grupo_str, strlen(grupo_str), 0) < 0) { // Envia El grupo que le asigno el broker al consumidor.
            perror("Error al enviar grupo al consumidor");
            close(socket_cliente);
            eliminar_cliente(socket_cliente);
            return NULL;
        }
 
        // Registrar la asignación de grupo
        char log_msg[100];
        snprintf(log_msg, sizeof(log_msg), "Consumidor asignado al grupo: %s", grupo_str);
        write_log(log_msg); // Registrar la asignación del grupo
 
        char grupo_nombre[50]; 
        strncpy(grupo_nombre, grupo_str, 50); // USAR grupo_str como identificador del grupo

        while (1) {
            pthread_mutex_lock(&cola.mutex);
            while (obtener_offset(grupo_nombre) >= cola.total) {
                pthread_cond_wait(&cola.cond, &cola.mutex);
            }
 
            int offset = obtener_offset(grupo_nombre);
            int idx = (cola.inicio + offset) % MAX_MENSAJES;
            char mensaje[TAM_BUFFER];
            strncpy(mensaje, cola.mensajes[idx], TAM_BUFFER);
            actualizar_offset(grupo_nombre);
            pthread_mutex_unlock(&cola.mutex);
 
            if (send(socket_cliente, mensaje, strlen(mensaje), 0) < 0) break;
        }
 
        close(socket_cliente);
        eliminar_cliente(socket_cliente);
        return NULL;
 
    } else {
        pthread_mutex_lock(&mutex_clientes); 
        encolar_mensaje(buffer);
        pthread_mutex_unlock(&mutex_clientes);
        close(socket_cliente);
        return NULL;
    }
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
            pthread_t hilo_cliente;
            int *socket_ptr = malloc(sizeof(int));
            *socket_ptr = nuevo_cliente;
            pthread_create(&hilo_cliente, NULL, manejar_cliente, socket_ptr);
            pthread_detach(hilo_cliente);
            write_log("Nuevo cliente conectado"); // Registrar la conexión del cliente
        } else {
            printf("Máximo de clientes alcanzado\n");
            close(nuevo_cliente);
        }
        pthread_mutex_unlock(&mutex_clientes);
    }
    return NULL;
}
 
int main() {
    srand(time(NULL));  // Semilla para rand()
 
    int socket_udp, socket_tcp;
    pthread_t hilo_udp, hilo_tcp;
 
    log_fp = fopen(LOG_FILE, "a+"); // Setup log file
 
    if (!log_fp){
        exit(EXIT_FAILURE);
    }
 
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
    fclose(log_fp);
 
    return 0;
}