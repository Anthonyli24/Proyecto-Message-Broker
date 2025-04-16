#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <unistd.h>

#define BUFFER_SIZE 1024 // Tamaño del buffer

struct memoria_compartida {
    char buffer[BUFFER_SIZE]; // Buffer para almacenar el mensaje
    int ready; // Indica si el buffer está listo para ser leído (1) o si ya fue procesado (2)
};

int main() {
    // Genera una clave única para la memoria compartida. Usa ftok para crear una clave única basada en el directorio actual y un carácter.
    // Usa shmget para crear un segmento de memoria compartida. Usa 0666 para establecer permisos de lectura y escritura.
    key_t key = ftok(".", 'X');
    int shmid = shmget(key, sizeof(struct memoria_compartida), 0666);

    // Verifica si la memoria compartida se ha creado correctamente. Si no se ha creado correctamente, imprime un mensaje de error y sale del programa
    if (shmid == -1) {
        perror("[Producer] Error al conectar a memoria compartida");
        exit(1);
    }

    // Conecta a la memoria compartida
    struct memoria_compartida* shm = (struct memoria_compartida*) shmat(shmid, NULL, 0);

    // Verifica si la memoria compartida se ha asociado correctamente. Si no se ha asociado correctamente, imprime un mensaje de error y sale del programa.
    if (shm == (void*) -1) {
        perror("[Producer] Error al asociar memoria");
        exit(1);
    }

    while (1) { // Inicializa el estado de la memoria compartida
        if (shm->ready == 0) { // Indica que el buffer está vacío
            printf("[Producer] Ingrese mensaje: ");
            fgets(shm->buffer, BUFFER_SIZE, stdin);
            shm->buffer[strcspn(shm->buffer, "\n")] = '\0'; // Elimina el salto de línea al final del mensaje.
            shm->ready = 1; // Indica que el buffer tiene un mensaje listo para leer.
        } else {
            printf("[Producer] Esperando que el buffer esté libre...\n");
        }
        sleep(1);//Condicion de carrera por solucionar. Revision constante del buffer.
    }
    return 0;
}