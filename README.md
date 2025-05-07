# Mini-Kafka - Sistema Distribuido de Broker de Mensajes

**Curso:** EIF-212 Sistemas Operativos  
**Autores:** Steven Acuña Zuñiga, Anthony Li Perera, Andres Rodriguez Castro, y Valeria Mesen Brenes  

---

## 📌 Descripción del sistema

Este proyecto implementa un sistema distribuido de mensajería inspirado en Apache Kafka. El sistema está compuesto por:

- **Productores:** Procesos que envían mensajes de texto al broker de manera concurrente mediante sockets TCP.
- **Broker:** Servidor central que recibe mensajes, los registra en un archivo de log, los almacena en una cola segura, y los distribuye a los consumidores por grupo.
- **Consumidores:** Procesos que se conectan al broker, se identifican y reciben mensajes del grupo asignado. Cada grupo tiene un offset individual, y cada mensaje es procesado por un solo consumidor dentro del grupo.

Se utiliza **broadcast UDP** para el descubrimiento automático del broker en la red. Luego, se establece una conexión TCP para la comunicación entre procesos.

---

## ⚙️ Cómo compilar y ejecutar

### 📚 Requisitos
- GCC (compilador de C)
- Linux o entorno compatible con POSIX (pthread, sockets)
- `make`

### 🔨 Compilación
1. Dirígete al directorio `src`.
2. Ejecuta el comando `make` para compilar el proyecto.

### ▶️ Ejecución
- **Ejecutar el broker**: Ejecuta el archivo `broker` utilizando el comando `./broker`.
- **Ejecutar uno o varios productores**: Ejecuta el archivo `producer` utilizando el comando `./producer`.
- **Ejecutar uno o varios consumidores**: Ejecuta el archivo `consumer` utilizando el comando `./consumer`.

**Nota**: Todos los procesos deben ejecutarse en ordenadores que se encuentren en la misma red local para el correcto descubrimiento vía UDP broadcast.

---

## 🛡️ Explicación de la estrategia utilizada para evitar interbloqueos 
Para prevenir interbloqueos (deadlocks) entre hilos al acceder a recursos compartidos como la cola de mensajes, la lista de clientes y los offsets de grupo, se implementó una estrategia basada en:

**Intento no bloqueante con reintento (`pthread_mutex_trylock` + `usleep`)**

En lugar de usar `pthread_mutex_lock` (que bloquea indefinidamente), se usa `trylock` para intentar adquirir el mutex.  
Si no está disponible, el hilo espera un tiempo aleatorio y vuelve a intentar, evitando así la condición de espera circular.

Este enfoque simple y efectivo permite mantener el sistema libre de deadlocks sin necesidad de establecer un orden fijo para la adquisición de locks.
---

## ⚠️ Problemas conocidos o limitaciones 

- **Asignación aleatoria de grupos a consumidores:**  
  Actualmente, los consumidores se asignan aleatoriamente a uno de los tres grupos (`A`, `B`, `C`), lo que puede causar un **desequilibrio de carga** si muchos consumidores caen en el mismo grupo.  
  Se podría mejorar balanceando de forma dinámica según el número de consumidores activos por grupo.

- **La cola de mensajes es finita (`MAX_MENSAJES = 1000`) y puede llenarse:**  
  Si los consumidores no procesan los mensajes lo suficientemente rápido, el broker puede dejar de aceptar nuevos mensajes una vez que la cola esté llena, ya que no hay un mecanismo de control de flujo o backpressure.

- **Sin manejo explícito de reconexión o persistencia de offset:**  
  Si un consumidor se desconecta, su estado (offset) se pierde. No hay persistencia de los offsets en disco, por lo que el consumidor no puede continuar desde el último mensaje recibido en una reconexión.
