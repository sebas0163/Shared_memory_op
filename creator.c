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

char *data_shm = NULL;      //initialize data shared memory
int shm_fd = -1;            //initializa file descriptor for shared memory
size_t data_shm_size = 0;   //initialize size of data shared memory

/**
 * Unmap shared memory files and unlink semaphores
*/
void cleanup() {
    if (data_shm) {
        munmap(data_shm, data_shm_size);
        shm_unlink(SHM_DATA);
    }
    sem_unlink(SEM_FREE_SPACE);
    sem_unlink(SEM_FILLED_SPACE);
    sem_unlink(SEM_I_CLIENT_MUTEX);
    if (shm_fd != -1) {
        close(shm_fd);
    }
}

/**
 * Terminate process with Ctrl+C
 * 
 * @param sig
*/
void handle_signal(int sig) {
    cleanup();
    printf("\nTerminating process.\n");
    exit(EXIT_SUCCESS);
}

/**
 * Initialize shared memory segment
 *
 * @param shm_name Name of the shared memory segment to open
 * @param size The size of the shared memory segment
 * @param shm_fd Pointer to the file descriptor for the shared memory
 * @param shm_ptr Pointer to the shared memory location
 */
void initialize_shared_memory(const char *shm_name, size_t size, int *shm_fd, void **shm_ptr) {
    *shm_fd = shm_open(shm_name, O_CREAT | O_RDWR, 0666);
    if (*shm_fd == -1) {
        perror("Error opening shared memory");
        exit(EXIT_FAILURE);
    }

    if (ftruncate(*shm_fd, size) == -1) {
        perror("Error setting size of shared memory");
        close(*shm_fd);
        exit(EXIT_FAILURE);
    }

    *shm_ptr = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, *shm_fd, 0);
    if (*shm_ptr == MAP_FAILED) {
        perror("Error mapping shared memory");
        close(*shm_fd);
        exit(EXIT_FAILURE);
    }

    memset(*shm_ptr, 0, size);
}

void initialize_semaphores() {
    sem_t *sem_free = sem_open(SEM_FREE_SPACE, O_CREAT, 0666, data_shm_size);
    sem_t *sem_filled = sem_open(SEM_FILLED_SPACE, O_CREAT, 0666, 0);
    sem_t *sem_mutex = sem_open(SEM_I_CLIENT_MUTEX, O_CREAT, 0666, 1);

    if (sem_free == SEM_FAILED || sem_filled == SEM_FAILED || sem_mutex == SEM_FAILED) {
        perror("Failed to open semaphore");
        cleanup();
        exit(EXIT_FAILURE);
    }

    sem_close(sem_free);
    sem_close(sem_filled);
    sem_close(sem_mutex);
}

void display_memory_contents() {
    printf("\nShared Memory Contents:\n");
    for (size_t i = 0; i < data_shm_size; i++) {
        printf("%02x ", ((unsigned char*)data_shm)[i]);
        if ((i + 1) % 32 == 0)
            printf("\n");
    }
    printf("\n");
}


int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <memory size in bytes>\n", argv[0]);
        return EXIT_FAILURE;
    }

    data_shm_size = strtoul(argv[1], NULL, 10);
    if (data_shm_size <= 0) {
        fprintf(stderr, "Invalid memory size.\n");
        return EXIT_FAILURE;
    }

    signal(SIGINT, handle_signal);

    // Initialize the shared memory for data
    initialize_shared_memory(SHM_DATA, data_shm_size, &shm_fd, (void **)&data_shm);
    initialize_semaphores();
    printf("Shared memory and synchronization primitives initialized.\n");

    while (1) {
	    system("clear");
        display_memory_contents();
        sleep(2); // Refresh every 2 seconds
    }

    cleanup(); // Clean resources on exit
    return EXIT_SUCCESS;
}

