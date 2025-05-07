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
#define NUM_THREADS 254
#define TASK_QUEUE_SIZE 50
 
// === Estructura de Cola de Mensajes ===
typedef struct {
    char mensajes[MAX_MENSAJES][TAM_BUFFER];
    int inicio, fin, total;
    pthread_mutex_t mutex;
    pthread_cond_t cond;
} ColaMensajes;
ColaMensajes cola = {.inicio = 0, .fin = 0, .total = 0, .mutex = PTHREAD_MUTEX_INITIALIZER, .cond = PTHREAD_COND_INITIALIZER};
 
// === Log ===
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
 
// === Estructuras para offsets y clientes ===
typedef struct {
    char nombre[50];
    int offset;
} GrupoOffset;
GrupoOffset grupos[MAX_GRUPOS];
int num_grupos = 0;
pthread_mutex_t mutex_grupos = PTHREAD_MUTEX_INITIALIZER;
 
int clientes[MAX_CLIENTES];
int total_clientes = 0;
pthread_mutex_t mutex_clientes = PTHREAD_MUTEX_INITIALIZER;
 
// === Thread pool task queue ===
typedef struct {
    int sockets[TASK_QUEUE_SIZE];
    int inicio, fin, total;
    pthread_mutex_t mutex;
    pthread_cond_t cond;
} TaskQueue;
TaskQueue task_queue = {.inicio = 0, .fin = 0, .total = 0, .mutex = PTHREAD_MUTEX_INITIALIZER, .cond = PTHREAD_COND_INITIALIZER};
 
void enqueue_task(int socket) {
    while (pthread_mutex_trylock(&task_queue.mutex) != 0) usleep((rand() % 100 + 50) * 1000);
    while (task_queue.total == TASK_QUEUE_SIZE)
        pthread_cond_wait(&task_queue.cond, &task_queue.mutex);
    task_queue.sockets[task_queue.fin] = socket;
    task_queue.fin = (task_queue.fin + 1) % TASK_QUEUE_SIZE;
    task_queue.total++;
    pthread_cond_signal(&task_queue.cond);
    pthread_mutex_unlock(&task_queue.mutex);
}
 
int dequeue_task() {
    while (pthread_mutex_trylock(&task_queue.mutex) != 0) usleep((rand() % 100 + 50) * 1000);
    while (task_queue.total == 0)
        pthread_cond_wait(&task_queue.cond, &task_queue.mutex);
    int socket = task_queue.sockets[task_queue.inicio];
    task_queue.inicio = (task_queue.inicio + 1) % TASK_QUEUE_SIZE;
    task_queue.total--;
    pthread_cond_signal(&task_queue.cond);
    pthread_mutex_unlock(&task_queue.mutex);
    return socket;
}
 
void encolar_mensaje(const char *mensaje) {
    while (pthread_mutex_trylock(&cola.mutex) != 0) usleep((rand() % 100 + 50) * 1000);
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
    while (pthread_mutex_trylock(&mutex_grupos) != 0) usleep((rand() % 100 + 50) * 1000);
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
    while (pthread_mutex_trylock(&mutex_grupos) != 0) usleep((rand() % 100 + 50) * 1000);
    for (int i = 0; i < num_grupos; i++) {
        if (strcmp(grupos[i].nombre, grupo) == 0) {
            grupos[i].offset++;
            break;
        }
    }
    pthread_mutex_unlock(&mutex_grupos);
}
 
void eliminar_cliente(int socket_cliente) {
    while (pthread_mutex_trylock(&mutex_clientes) != 0) usleep((rand() % 100 + 50) * 1000);
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
 
void *thread_worker(void *arg) {
    while (1) {
        int socket_cliente = dequeue_task();
        char buffer[TAM_BUFFER];
        int bytes_recibidos = recv(socket_cliente, buffer, TAM_BUFFER - 1, 0);
        if (bytes_recibidos <= 0) {
            close(socket_cliente);
            eliminar_cliente(socket_cliente);
            continue;
        }
        buffer[bytes_recibidos] = '\0';
        if (strcmp(buffer, "CONSUMIDOR") == 0) {
            char grupo = 'A' + (rand() % 3);
            char grupo_str[2] = {grupo, '\0'};
            send(socket_cliente, grupo_str, strlen(grupo_str), 0);
            char log_msg[100];
            snprintf(log_msg, sizeof(log_msg), "Consumidor asignado al grupo: %s", grupo_str);
            write_log(log_msg);
            while (1) {
                while (pthread_mutex_trylock(&cola.mutex) != 0) usleep((rand() % 100 + 50) * 1000);
                while (obtener_offset(grupo_str) >= cola.total) {
                    pthread_cond_wait(&cola.cond, &cola.mutex);
                }
                int offset = obtener_offset(grupo_str);
                int idx = (cola.inicio + offset) % MAX_MENSAJES;
                char mensaje[TAM_BUFFER];
                strncpy(mensaje, cola.mensajes[idx], TAM_BUFFER);
                actualizar_offset(grupo_str);
                pthread_mutex_unlock(&cola.mutex);
                if (send(socket_cliente, mensaje, strlen(mensaje), 0) < 0) break;
            }
        } else {
            encolar_mensaje(buffer);
        }
        close(socket_cliente);
        eliminar_cliente(socket_cliente);
    }
    return NULL;
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
    while (1) {
        recvfrom(socket_udp, buffer, TAM_BUFFER, 0, (struct sockaddr *)&direccion_cliente, &tam_direccion);
        char respuesta[TAM_BUFFER];
        snprintf(respuesta, sizeof(respuesta), "%s:%d", ip_broker, PUERTO_BROKER);
        sendto(socket_udp, respuesta, strlen(respuesta), 0, (struct sockaddr *)&direccion_cliente, tam_direccion);
    }
    return NULL;
}
 
int main() {
    srand(time(NULL));
    pthread_t hilo_udp, pool[NUM_THREADS];
    log_fp = fopen(LOG_FILE, "a+");
    if (!log_fp) exit(EXIT_FAILURE);
 
    int socket_udp = socket(AF_INET, SOCK_DGRAM, 0);
    struct sockaddr_in direccion_udp = {.sin_family = AF_INET, .sin_port = htons(PUERTO_DESCUBRIMIENTO), .sin_addr.s_addr = INADDR_ANY};
    bind(socket_udp, (struct sockaddr *)&direccion_udp, sizeof(direccion_udp));
    pthread_create(&hilo_udp, NULL, manejar_udp_descubrimiento, &socket_udp);
 
    int socket_tcp = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in direccion_tcp = {.sin_family = AF_INET, .sin_port = htons(PUERTO_BROKER), .sin_addr.s_addr = INADDR_ANY};
    bind(socket_tcp, (struct sockaddr *)&direccion_tcp, sizeof(direccion_tcp));
    listen(socket_tcp, 10);
 
    for (int i = 0; i < NUM_THREADS; i++)
        pthread_create(&pool[i], NULL, thread_worker, NULL);
 
    printf("Broker escuchando en el puerto TCP %d...\n", PUERTO_BROKER);
    while (1) {
        struct sockaddr_in cliente_addr;
        socklen_t cliente_len = sizeof(cliente_addr);
        int nuevo_cliente = accept(socket_tcp, (struct sockaddr *)&cliente_addr, &cliente_len);
        if (nuevo_cliente >= 0) {
            while (pthread_mutex_trylock(&mutex_clientes) != 0) usleep((rand() % 100 + 50) * 1000);
            if (total_clientes < MAX_CLIENTES) {
                clientes[total_clientes++] = nuevo_cliente;
                enqueue_task(nuevo_cliente);
                write_log("Nuevo cliente conectado");
            } else {
                close(nuevo_cliente);
            }
            pthread_mutex_unlock(&mutex_clientes);
        }
    }
    fclose(log_fp);
    return 0;
}