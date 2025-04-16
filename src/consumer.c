#include <stdio.h>
#include <stdlib.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <unistd.h>
#include <string.h>

#define BUFFER_SIZE 1024 // Tamaño del buffer

struct memoria_compartida {
    char buffer[BUFFER_SIZE]; // Buffer para almacenar el mensaje
    int ready; // Indica en que estado se encuentra el buffer. (0): Buffer Vacio, (1): Mensaje transferido al broker, (2): Mensaje transferido al consumer
};

int main() {
    // Genera una clave única para la memoria compartida. Usa ftok para crear una clave única basada en el directorio actual y un carácter.
    // Usa shmget para crear un segmento de memoria compartida. Usa 0666 para establecer permisos de lectura y escritura.
    key_t key = ftok(".", 'X');
    int shmid = shmget(key, sizeof(struct memoria_compartida), 0666);

    // Verifica si la memoria compartida se ha creado correctamente. Si no se ha creado correctamente, imprime un mensaje de error y sale del programa
    if (shmid == -1) {
        perror("[Consumer] Error al conectar a memoria compartida");
        exit(1);
    }

    // Conecta a la memoria compartida
    struct memoria_compartida* shm = (struct memoria_compartida*) shmat(shmid, NULL, 0);
    
    // Verifica si la memoria compartida se ha asociado correctamente. Si no se ha asociado correctamente, imprime un mensaje de error y sale del programa.
    if (shm == (void*) -1) {
        perror("[Consumer] Error al asociar memoria");
        exit(1);
    }

    while (1) {
        if (shm->ready == 2) { // Indica que el buffer tiene un mensaje listo para leer.
            printf("[Consumer] Mensaje recibido: %s\n", shm->buffer); 
            shm->ready = 0; // Libera el buffer para el productor
        }
        sleep(1); //Condicion de carrera por solucionar. Revision constante del buffer.
    }
    
}