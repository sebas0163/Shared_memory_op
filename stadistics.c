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
size_t control_shm_size = sizeof(long) * 12;   //initialize size of data shared memory

typedef struct TmStruct {
    char ch;
    int i;
    struct tm dtm;
} TmStruct;

/**
 * Unlink and close shared memory segments.
 * @param shm_name Name of the shared memory segment.
 * @param size Size of the shared memory.
 * @param shm_fd Pointer to the file descriptor of the shared memory.
 * @param shm_ptr Pointer to the mapped shared memory.
 */
void unlink_shared_mem(const char *shm_name, size_t size, int *shm_fd, void **shm_ptr) {
    if (*shm_ptr) {
        munmap(*shm_ptr, size);
        *shm_ptr = NULL;
    }

    if (*shm_fd != -1) {
        close(*shm_fd);
        *shm_fd = -1;
    }

    shm_unlink(shm_name);
}
/**
Close the shared memory 
*/
void cleanup(){
    unlink_shared_mem(SHM_CONTROL, control_shm_size, &control_shm_fd, (void **)&control_shm);
}
//iniciar memoria
/**
 * Set up shared memory
 *
 * @param shm_name Name of the shared memory segment to open
 * @param size Size of the shared memory to map
 * @param shm_fd Pointer to store the file descriptor of the shared memory
 * @param shm_ptr Pointer to store the address of the mapped shared memory
 */
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
/**
 This function takes the process information from the shared memory
*/
void getstadistics(){
    control_shm[7]=(control_shm[5]*sizeof(char)+ control_shm[5]*sizeof(TmStruct)+ 12*sizeof(long));
    printf("Tiempo bloqueado del Cliente %ld ns\n" , control_shm[3]);
    printf("Tiempo bloqueado del Reconstructor %ld ns\n", control_shm[4]);
    printf("Caracteres transferidos %ld\n", control_shm[5]);
    printf("Caracteres en buffer %ld\n", control_shm[6]);
    printf("Memoria usada %ld bytes\n", control_shm[7]);
    printf("Tiempo en modo usuario del cliente %ld ns \n", control_shm[8]);
    printf("Tiempo en modo kernel del cliente %ld ns \n", control_shm[9]);
    printf("Tiempo en modo usuario del reconstructor %ld ns \n", control_shm[10]);
    printf("Tiempo en modo kernel del reconstructor %ld ns \n", control_shm[11]);
}
int main(){
    setup_shared_memory(SHM_CONTROL, control_shm_size, &control_shm_fd, (void **)&control_shm);
    getstadistics();
    cleanup();
}