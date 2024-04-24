#include "constants.h"
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <semaphore.h>
#include <string.h>
#include <signal.h>
#include <time.h>
#include <unistd.h>
#include <sys/time.h>
#include <sys/resource.h>


long *control_shm = NULL;      //initialize control shared memory
int control_shm_fd = -1;            //initialize file descriptor for shared memory
size_t control_shm_size = sizeof(long) * 9;   //initialize size of data shared memory

//iniciar memoria
void setup_shared_memory(const char *shm_name, size_t size, int *shm_fd, void **shm_ptr) {
    *shm_fd = shm_open(shm_name, O_RDWR, 0666);
    if (*shm_fd == -1) {
        perror("Error opening shared memory");
        exit(EXIT_FAILURE);
    }

    *shm_ptr = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, *shm_fd, 0);
    if (*shm_ptr == MAP_FAILED) {
        perror("Error mapping shared memory");
        close(*shm_fd);
        *shm_fd = -1;  // Reset fd to indicate error
        exit(EXIT_FAILURE);
    }
}
//Leer los datos de la memoria 
void getstadistics(){
    printf("TIempo bloqueado del Cliente %ld\n", control_shm[3]);
    printf("TIempo bloqueado del Reconstructor %ld\n", control_shm[4]);
    printf("Caracteres transferidos %ld\n", control_shm[5]);
    printf("Caracteres en buffer %ld\n", control_shm[6]);
    printf("Memoria usada %ld\n", control_shm[7]);
    printf("Tiempo en modo usuario del cliente %ld\n", control_shm[8]);
    printf("Tiempo en modo kernel del cliente %ld\n", control_shm[9]);
    printf("Tiempo en modo usuario del reconstructor %ld\n", control_shm[10]);
    printf("Tiempo en modo kernel del reconstructor %ld\n", control_shm[11]);
}

//imprimir los datos  

//conversión a segundos

// sincronización

struct rusage ru;
void prueba(){
    int i = 5000;
    int j =0;
    while (i !=0){
        j ++;
        i --;
        printf("Numero %d\n", j);
    }
}

int main(){
    setup_shared_memory(SHM_CONTROL, control_shm_size, &control_shm_fd, (void **)&control_shm);
    getstadistics();
}