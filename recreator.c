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
size_t control_shm_size = sizeof(long) * 12;   //initialize size of data shared memory

struct rusage ru;          // Estructura con los datos del proceso

sem_t *sem_free;
sem_t *sem_filled;
sem_t *sem_i_recr_mutex;
sem_t *sem_n_process;
/**
This function count how many chars still in the buffer
*/
void getBuffChar(){
    for (int i =0; i <data_shm_size; i++){
        if (data_shm[i] !=0){
            control_shm[6] ++;
        }
    }
}
/**
This function check if this is the only process in execution, if this is true, open the stadistics process
*/
void checkProcess(){
    control_shm[2] --;
    if(control_shm[2]==0){
        getBuffChar();
        system("./stadistics > stats.txt");
    }
}
/**
This module ask for this process stadistics 
*/
void getstadistics(){
    getrusage(RUSAGE_SELF, &ru);
    sem_wait(sem_n_process);
    control_shm[10]+=ru.ru_stime.tv_usec;
    control_shm[11] += ru.ru_utime.tv_usec;
    checkProcess();
    sem_post(sem_n_process);
}

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

/**
 * Clean up resources including shared memory and semaphores.
 */
void cleanup() {
    // Close semaphores and unlink them
    close_semaphore(SEM_FREE_SPACE, &sem_free);
    close_semaphore(SEM_FILLED_SPACE, &sem_filled);
    close_semaphore(SEM_I_RECR_MUTEX, &sem_i_recr_mutex);
    close_semaphore(SEM_n_PROCESS, &sem_n_process);
}


/**
 * Clean up when terminating process with Ctrl-c
*/
void handle_end(int sig) {
    getstadistics();
    cleanup();
    printf("\nTerminating process.\n");
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
    sem_i_recr_mutex = sem_open(SEM_I_CLIENT_MUTEX, 0);
    sem_n_process =sem_open(SEM_n_PROCESS,0);

    if (sem_free == SEM_FAILED || sem_filled == SEM_FAILED || sem_i_recr_mutex == SEM_FAILED || sem_n_process == SEM_FAILED) {
        perror("Failed to open semaphore");
        exit(EXIT_FAILURE);
    }
}

/**
 * Returns the index global variable that indicates the position of the char in file
*/
int get_index(){
    struct timespec inicio, fin;
    clock_gettime(CLOCK_MONOTONIC, &inicio);
    sem_wait(sem_i_recr_mutex);
    clock_gettime(CLOCK_MONOTONIC, &fin);
    control_shm[4]+= ((fin.tv_sec - inicio.tv_sec)+(fin.tv_nsec-inicio.tv_nsec))/100; //here we set the blocked time
    int index = control_shm[I_RECREATOR];  //read global variable
    control_shm[I_RECREATOR]++;    //update global variable
    sem_post(sem_i_recr_mutex);
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

    if (mode == 0) printf("Press Enter to recreate next character...\n");

    while (eof != 1) {
        if (mode == 0) while (getchar() != '\n');  // Wait for Enter key (if manual mode)
        if (mode == 1) {
            usleep(period * 1000); //sleep in microseconds (if automatic mode)
        }
        int sem_value;
        sem_getvalue(sem_filled, &sem_value);
        printf("sem_filled: %d\n", sem_value);
        sem_wait(sem_filled);

        // get current value and update value of index (global)
        index = get_index();

        // Position the file stream to the current index
        fseek(file, index, SEEK_SET);
        fread(&ch, 1, 1, file);

        if (feof(file)) eof = 1;    //reached end of file
        else{

            // Write the character to shared memory
            control_shm[5] ++; //here we increment the counter of chars transferred
            int i_shm = index % data_shm_size;  // Circular buffer
            ch = data_shm[i_shm];   //get current value
            data_shm[i_shm] = 0;    // set to null

            fseek(file, index, SEEK_SET);
            fputc(ch, file);

            // read timestamp from shared memory
            //read_timestamp(&index, &ch);

            sem_post(sem_free);
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
    handle_end(1); // this always handle the end of the process 
    
    return EXIT_SUCCESS;
}

