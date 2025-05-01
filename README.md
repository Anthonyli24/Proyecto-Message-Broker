# Mini-Kafka - Sistema Distribuido de Broker de Mensajes

**Curso:** EIF-212 Sistemas Operativos  
**Autores:** Steven Acuña Zuñiga, Anthony Li Perera, Andres Rodriguez Castro, y Valeria Mesen Brenes  
**Fecha de entrega:** 4 de mayo de 2025

---

## 📝 Descripción del sistema

Este proyecto implementa un sistema distribuido de mensajería inspirado en Apache Kafka. El sistema está compuesto por:

- **Productores:** Procesos que envían mensajes de texto al broker de manera concurrente mediante sockets TCP.
- **Broker:** Servidor central que recibe mensajes, los registra en un archivo de log, los almacena en una cola segura, y los distribuye a los consumidores por grupo.
- **Consumidores:** Procesos que se conectan al broker, se identifican y reciben mensajes del grupo asignado. Cada grupo tiene un offset individual, y cada mensaje es procesado por un solo consumidor dentro del grupo.

Se utiliza **broadcast UDP** para el descubrimiento automático del broker en la red. Luego, se establece una conexión TCP para la comunicación entre procesos.

---

## ⚙️ Cómo compilar y ejecutar

### 🔧 Requisitos
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

---

## ⚠️ Problemas conocidos o limitaciones 




