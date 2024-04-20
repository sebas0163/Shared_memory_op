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
int data_shm_fd = -1;            //initialize file descriptor for shared memory
size_t data_shm_size = 0;   //initialize size of data shared memory

char *control_shm = NULL;      //initialize control shared memory
int control_shm_fd = -1;            //initialize file descriptor for shared memory
size_t control_shm_size = sizeof(int) * 3;   //initialize size of data shared memory

sem_t *sem_free;
sem_t *sem_filled;
sem_t *sem_i_client_mutex;

void cleanup() {
    if (data_shm) {
        munmap(data_shm, data_shm_size);
    }
    sem_close(sem_free);
    sem_close(sem_filled);
    sem_close(sem_i_client_mutex);
    shm_unlink(SHM_DATA);
}

void handle_signal(int sig) {
    cleanup();
    printf("\nTerminating program.\n");
    exit(EXIT_SUCCESS);
}

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

void setup_semaphores() {
    sem_free = sem_open(SEM_FREE_SPACE, 0);
    sem_filled = sem_open(SEM_FILLED_SPACE, 0);
    sem_i_client_mutex = sem_open(SEM_I_CLIENT_MUTEX, 0);

    if (sem_free == SEM_FAILED || sem_filled == SEM_FAILED || sem_i_client_mutex == SEM_FAILED) {
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
    int sem_value = 0;

    while ((ch = fgetc(file)) != EOF) {
        sem_wait(sem_free);
        sem_wait(sem_i_client_mutex);

        sem_value =  sem_getvalue(sem_filled, &index);
        if (sem_value == -1){
            perror("Failed to open sem_filled semaphore");
            exit(EXIT_FAILURE);
        }

        // Write the character to shared memory
        data_shm[index] = ch;
        index = (index + 1) % data_shm_size;  // Circular buffer

        sem_post(sem_i_client_mutex);
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

    data_shm_size = strtoul(argv[2], NULL, 10);
    if (data_shm_size <= 0) {
        fprintf(stderr, "Invalid memory size.\n");
        return EXIT_FAILURE;
    }

    signal(SIGINT, handle_signal);

    setup_shared_memory(SHM_DATA, data_shm_size, &data_shm_fd, (void **)&data_shm);
    setup_shared_memory(SHM_CONTROL, control_shm_size, &control_shm_fd, (void **)&control_shm);

    setup_semaphores();

    manual_mode(argv[1]);

    cleanup();
    return EXIT_SUCCESS;
}

