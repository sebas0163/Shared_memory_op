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

char *data_shm = NULL;      //initialize data shared memory
int data_shm_fd = -1;            //initialize file descriptor for shared memory
size_t data_shm_size = 0;   //initialize size of data shared memory

//define struct to store info about timestamp
typedef struct TmStruct {
    char ch;
    int i;
    struct tm dtm;
} TmStruct;

TmStruct *tm_shm = NULL;      //initialize timestamps shared memory
int tm_shm_fd = -1;            //initialize file descriptor for shared memory
size_t tm_shm_size = 0;   //initialize size of timestamps shared memory

long *control_shm = NULL;      //initialize control shared memory
int control_shm_fd = -1;            //initialize file descriptor for shared memory
size_t control_shm_size = sizeof(long) * 9;   //initialize size of data shared memory
struct rusage ru;          // Estructura con los datos del proceso

sem_t *sem_free;
sem_t *sem_filled;
sem_t *sem_i_client_mutex;

/**
 * Close and unlink a semaphore.
 * @param sem_name Name of the semaphore to unlink.
 * @param sem_ptr Pointer to the semaphore pointer.
 */
void close_semaphore(const char *sem_name, sem_t **sem_ptr) {
    if (*sem_ptr != SEM_FAILED) {
        sem_close(*sem_ptr);  // Close the semaphore
        sem_unlink(sem_name); // Unlink the semaphore from the system
        *sem_ptr = SEM_FAILED; // Set the semaphore pointer to SEM_FAILED to indicate it's closed
    }
}
void getstadistics(){
    getrusage(RUSAGE_SELF, &ru);
    control_shm[8] += ru.ru_utime.tv_usec;
    control_shm[9]+=ru.ru_stime.tv_usec;
    printf("Valor modo usuario %ld\n", control_shm[8]);
    printf("Valor modo kernel %ld\n", control_shm[3]);
}
void checkProcess(){
    control_shm[2] --;
    if(control_shm[2]==0){
        system("./stadistics");
    }
}

/**
 * Clean up resources including shared memory and semaphores.
 */
    // Close semaphores and unlink them
void cleanup() {
    close_semaphore(SEM_FREE_SPACE, &sem_free);
    close_semaphore(SEM_FILLED_SPACE, &sem_filled);
    close_semaphore(SEM_I_CLIENT_MUTEX, &sem_i_client_mutex);
}
void handle_end(int sig) {
    getstadistics();
    cleanup();
    checkProcess();
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

/**
 * Returns the index global variable that indicates what character to read
*/
int get_index(){
    clock_t inicio, fin;
    inicio = clock();
    sem_wait(sem_i_client_mutex);
    fin = clock();
    control_shm[3]+= (long)(fin-inicio)/CLOCKS_PER_SEC;
    int index = control_shm[I_CLIENT];  //read global variable
    control_shm[I_CLIENT]++;    //update global variable
    sem_post(sem_i_client_mutex);
    return index;
}

/**
 * Writes timestamp to shared memory
*/
int read_timestamp(int *index, char *ch){
    time_t now = time(NULL);
    struct tm *now_tm = localtime(&now);

    int i = (*index) % data_shm_size;  // get position in shared memory

    // Define timestamp struct
    TmStruct timestamp;
    timestamp.ch = *ch;
    timestamp.i = *index;
    timestamp.dtm = *now_tm;

    tm_shm[i] = timestamp;

    // Buffer to hold the datetime string
    char datetime_buf[25];
    // Format datetime: e.g., "2023-04-15 12:01:58"
    strftime(datetime_buf, sizeof(datetime_buf), "%Y-%m-%d %H:%M:%S", &tm_shm[i].dtm);

    printf("index = %i\tvalue = %c\tdatetime = %s\n", tm_shm[i].i, tm_shm[i].ch, datetime_buf);
}

void execute_mode(const char *filename, int mode, int period) {
    FILE *file = fopen(filename, "r+");
    if (!file) {
        perror("Failed to open file");
        exit(EXIT_FAILURE);
    }

    char ch;
    int index = 0; 
    int eof = 0;

    if (mode == 0) printf("Press Enter to write next character...\n");

    while (eof != 1) {
        if (mode == 0) while (getchar() != '\n');  // Wait for Enter key (if manual mode)
        if (mode == 1) {
            usleep(period * 1000); //sleep in microseconds (if automatic mode)
        }
        sem_wait(sem_free);
        int sem_value;
        sem_getvalue(sem_free, &sem_value);
        printf("sem_free: %d\n", sem_value);

        // get current value and update value of index (global)
        index = get_index();

        // Position the file stream to the current index
        fseek(file, index, SEEK_SET);
        fread(&ch, 1, 1, file);

        char blank = 32;    //blank space
        fseek(file, index, SEEK_SET);
        fputc(blank, file);     //overwrite position with blank space

        if (feof(file)) eof = 1;    //reached end of file
        else{
            // write timestamp to shared memory
            read_timestamp(&index, &ch);

            // Write the character to shared memory
            index = index % data_shm_size;  // Circular buffer
            data_shm[index] = ch;

            sem_post(sem_filled);
            sem_getvalue(sem_filled, &sem_value);
            printf("sem_filled: %d\n", sem_value);
        }
    }

    fclose(file);
}

/**
 * Reads command-line arguments and returns mode
 * @param argv: CLI arguments array
 * @return int: -1 if an error ocurred, else: returns mode code
*/
int get_mode(char *argv[]){
    char *endptr;
    int mode = strtol(argv[3], &endptr, 10); // set mode
    // Check for errors: No digits were found or the number is out of range
    if (endptr == argv[3] || *endptr != '\0') {
        fprintf(stderr, "Invalid mode number: %s\n", argv[1]);
        return -1;
    } else if (!(mode == 0 || mode == 1)){  // only two modes are allowed
        fprintf(stderr, "Invalid mode: %s\n", argv[1]);
        return -1;
    }
    return mode;
}

/**
 * Reads command-line arguments and returns automatic period (in ms)
 * @param argv: CLI arguments array
 * @return int: -1 if an error ocurred, else: returns period
*/
int get_period(char *argv[]){
    char *endptr;
    int period = strtol(argv[4], &endptr, 10); // set mode
    // Check for errors: No digits were found or the number is out of range
    if (endptr == argv[4] || *endptr != '\0') {
        fprintf(stderr, "Invalid period number: %s\n", argv[1]);
        return -1;
    } else if (period <= 0){  // only two modes are allowed
        fprintf(stderr, "Invalid period: %s\n", argv[1]);
        return -1;
    }
    return period;
}

int main(int argc, char *argv[]) {
    int mode;
    int period;
    if (argc != 5) {
        fprintf(stderr, "Usage: %s <filename> <memory size in bytes> <mode> <period>\n", argv[0]);
        return EXIT_FAILURE;
    }

    // Read third argument to set shared memory size
    data_shm_size = strtoul(argv[2], NULL, 10);
    if (data_shm_size <= 0) {
        fprintf(stderr, "Invalid memory size.\n");
        return EXIT_FAILURE;
    }

    // set mode (user or automatic)
    mode = get_mode(argv);
    if (mode == -1) return EXIT_FAILURE;

    //set period for automatic mode
    period = get_period(argv);
    if (period == -1) return EXIT_FAILURE;
    

    // Handle process termination
    signal(SIGINT, handle_end);

    // Read data shared memory
    setup_shared_memory(SHM_DATA, data_shm_size, &data_shm_fd, (void **)&data_shm);

    // Set size for timestamps shared memory
    tm_shm_size = sizeof(TmStruct) * data_shm_size;

    // Read timestamps shared memory
    setup_shared_memory(SHM_TIMESTAMPS, tm_shm_size, &tm_shm_fd, (void **)&tm_shm);

    // Read control shared memory
    setup_shared_memory(SHM_CONTROL, control_shm_size, &control_shm_fd, (void **)&control_shm);
    control_shm[2] ++;

    setup_semaphores();

    execute_mode(argv[1], mode, period);
    handle_end(1);
    return EXIT_SUCCESS;
}

