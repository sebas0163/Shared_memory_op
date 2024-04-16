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

char *shared_memory = NULL;
int shm_fd = -1;
size_t shared_mem_size = 0;

void cleanup() {
    if (shared_memory) {
        munmap(shared_memory, shared_mem_size);
        shm_unlink(SHM_NAME);
    }
    sem_unlink(SEM_FREE_SPACE);
    sem_unlink(SEM_FILLED_SPACE);
    sem_unlink(SEM_MUTEX);
    if (shm_fd != -1) {
        close(shm_fd);
    }
}

void handle_signal(int sig) {
    cleanup();
    printf("\nTerminating program.\n");
    exit(EXIT_SUCCESS);
}

void initialize_shared_memory(size_t size) {
    shm_fd = shm_open(SHM_NAME, O_CREAT | O_RDWR, 0666);
    if (shm_fd == -1) {
        perror("Error opening shared memory");
        exit(EXIT_FAILURE);
    }

    if (ftruncate(shm_fd, size) == -1) {
        perror("Error setting size of shared memory");
        close(shm_fd);
        exit(EXIT_FAILURE);
    }

    shared_memory = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);
    if (shared_memory == MAP_FAILED) {
        perror("Error mapping shared memory");
        close(shm_fd);
        exit(EXIT_FAILURE);
    }

    memset(shared_memory, 0, size);
}

void initialize_semaphores() {
    sem_t *sem_free = sem_open(SEM_FREE_SPACE, O_CREAT, 0666, shared_mem_size);
    sem_t *sem_filled = sem_open(SEM_FILLED_SPACE, O_CREAT, 0666, 0);
    sem_t *sem_mutex = sem_open(SEM_MUTEX, O_CREAT, 0666, 1);

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
    for (size_t i = 0; i < shared_mem_size; i++) {
        printf("%02x ", ((unsigned char*)shared_memory)[i]);
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

    shared_mem_size = strtoul(argv[1], NULL, 10);
    if (shared_mem_size <= 0) {
        fprintf(stderr, "Invalid memory size.\n");
        return EXIT_FAILURE;
    }

    signal(SIGINT, handle_signal);

    initialize_shared_memory(shared_mem_size);
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

