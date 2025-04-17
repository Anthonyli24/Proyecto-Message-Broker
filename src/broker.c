#include <stdio.h>
#include <stdlib.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/sem.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#define BUFFER_SIZE 1024 // Tamaño del buffer

struct memoria_compartida { char buffer[BUFFER_SIZE]; };

int main() {
    // Genera una clave única para la memoria compartida. Usa ftok para crear una clave única basada en el directorio actual y un carácter
    // Usa shmget para crear un segmento de memoria compartida. Usa 0666 para establecer permisos de lectura y escritura
    key_t key = ftok(".", 'X');
    key_t semkey = ftok(".", 'S');
    int shmid = shmget(key, sizeof(struct memoria_compartida), 0666 | IPC_CREAT);
    int semid = semget(semkey, 3, 0666 | IPC_CREAT);
   
     struct memoria_compartida* shm = (struct memoria_compartida*) shmat(shmid, NULL, 0); // Conecta a la memoria compartida
     struct sembuf wait_broker = {1, -1, 0}; // Espera al productor
     struct sembuf signal_consumer = {2, 1, 0}; // Avisar al consumidor

    semctl(semid, 0, SETVAL, 1); // Productor empieza activo
    semctl(semid, 1, SETVAL, 0); // Broker espera
    semctl(semid, 2, SETVAL, 0); // Consumidor espera

    // Verifica si la memoria compartida se ha creado correctamente. Si no se ha creado correctamente, imprime un mensaje de error y sale del programa
    if (shmid == -1) {
        perror("[Broker] Error al crear memoria compartida");
        exit(1);
    }

    // Verifica si la memoria compartida se ha asociado correctamente. Si no se ha asociado correctamente, imprime un mensaje de error y sale del programa.
    if (shm == (void*) -1) {
        perror("[Broker] Error al asociar memoria");
        exit(1);
    } 

    // Verifica si los semáforos se han creado correctamente. Si no se han creado correctamente, imprime un mensaje de error y sale del programa
    if (semid == -1) {
        perror("[Broker] Error al crear semáforos");
        exit(1);
    }
    
    while (1) {
        semop(semid, &wait_broker, 1);

        time_t now = time(NULL);
        printf("[Broker] Mensaje recibido: %s\n", shm->buffer);

        FILE *log = fopen("broker.log", "a");
        if (log) {
            fprintf(log, "[%s] Transferido: %s\n", ctime(&now), shm->buffer);
            fclose(log);
        }

        if (strcmp(shm->buffer, "exit") == 0) {
            break;
        }

        semop(semid, &signal_consumer, 1);
    }
    return 0;
}