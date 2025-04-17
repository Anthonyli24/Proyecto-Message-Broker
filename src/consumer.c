#include <stdio.h>
#include <stdlib.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/sem.h>
#include <string.h>
#include <unistd.h>

#define BUFFER_SIZE 1024 // Tamaño del buffer
#define SEM_PROD 0
#define SEM_BROKER 1
#define SEM_CONSUMER 2

struct memoria_compartida { char buffer[BUFFER_SIZE]; };

int main() {
    // Genera una clave única para la memoria compartida. Usa ftok para crear una clave única basada en el directorio actual y un carácter.
    // Usa shmget para crear un segmento de memoria compartida. Usa 0666 para establecer permisos de lectura y escritura.
    key_t key = ftok(".", 'X');
    key_t semkey = ftok(".", 'S');
    int shmid = shmget(key, sizeof(struct memoria_compartida), 0666);
    int semid = semget(semkey, 3, 0666);

    struct memoria_compartida* shm = (struct memoria_compartida*) shmat(shmid, NULL, 0); // Conecta a la memoria compartida
    struct sembuf wait_consumer = {2, -1, 0}; // Esperar al broker
    struct sembuf signal_prod = {0, 1, 0}; // Avisar al productor

    // Verifica si la memoria compartida se ha creado correctamente. Si no se ha creado correctamente, imprime un mensaje de error y sale del programa
    if (shmid == -1) {
        perror("[Consumer] Error al conectar a memoria compartida");
        exit(1);
    }

    // Verifica si la memoria compartida se ha asociado correctamente. Si no se ha asociado correctamente, imprime un mensaje de error y sale del programa.
    if (shm == (void*) -1) {
        perror("[Consumer] Error al asociar memoria");
        exit(1);
    }

    // Verifica si los semáforos se han creado correctamente. Si no se han creado correctamente, imprime un mensaje de error y sale del programa
    if (semid == -1) {
        perror("[Consumer] Error al acceder a semáforos");
        exit(1);
    }

    while (1) {
        semop(semid, &wait_consumer, 1);

        printf("[Consumer] Mensaje recibido: %s\n", shm->buffer); 
        
        if (strcmp(shm->buffer, "exit") == 0) { 
            break;
        }
        
        semop(semid, &signal_prod, 1);
    }  
}