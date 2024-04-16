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
size_t shared_mem_size = 0;
sem_t *sem_free;
sem_t *sem_filled;
sem_t *sem_mutex;

void cleanup() {
    if (shared_memory) {
        munmap(shared_memory, shared_mem_size);
    }
    sem_close(sem_free);
    sem_close(sem_filled);
    sem_close(sem_mutex);
    shm_unlink(SHM_NAME);
}

void handle_signal(int sig) {
    cleanup();
    printf("\nTerminating program.\n");
    exit(EXIT_SUCCESS);
}

void setup_shared_memory(size_t size) {
    int fd = shm_open(SHM_NAME, O_RDWR, 0666);
    if (fd == -1) {
        perror("Error opening shared memory");
        exit(EXIT_FAILURE);
    }

    shared_memory = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (shared_memory == MAP_FAILED) {
        perror("Error mapping shared memory");
        close(fd);
        exit(EXIT_FAILURE);
    }

    close(fd);
}

void setup_semaphores() {
    sem_free = sem_open(SEM_FREE_SPACE, 0);
    sem_filled = sem_open(SEM_FILLED_SPACE, 0);
    sem_mutex = sem_open(SEM_MUTEX, 0);

    if (sem_free == SEM_FAILED || sem_filled == SEM_FAILED || sem_mutex == SEM_FAILED) {
        perror("Failed to open semaphore");
        exit(EXIT_FAILURE);
    }
}

void manual_mode(const char *filename) {
    FILE *file = fopen(filename, "r");
    if (!file) {
        perror("Failed to open file");
        exit(EXIT_FAILURE);
    }

    char ch;
    int index = 0;
    while ((ch = fgetc(file)) != EOF) {
        sem_wait(sem_free);
        sem_wait(sem_mutex);

        // Write the character to shared memory
        shared_memory[index] = ch;
        index = (index + 1) % shared_mem_size;  // Circular buffer

        sem_post(sem_mutex);
        sem_post(sem_filled);

        printf("Press Enter to write next character...\n");
        while (getchar() != '\n');  // Wait for Enter key
    }

    fclose(file);
}

int main(int argc, char *argv[]) {
    if (argc != 3) {
        fprintf(stderr, "Usage: %s <filename> <memory size in bytes>\n", argv[0]);
        return EXIT_FAILURE;
    }

    shared_mem_size = strtoul(argv[2], NULL, 10);
    if (shared_mem_size <= 0) {
        fprintf(stderr, "Invalid memory size.\n");
        return EXIT_FAILURE;
    }

    signal(SIGINT, handle_signal);

    setup_shared_memory(shared_mem_size);
    setup_semaphores();

    manual_mode(argv[1]);

    cleanup();
    return EXIT_SUCCESS;
}

