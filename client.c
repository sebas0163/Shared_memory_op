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
#include <gtk/gtk.h>

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
double time_ =0;
sem_t *sem_free;
sem_t *sem_filled;
sem_t *sem_i_client_mutex;
sem_t *sem_n_process;

// Global pointer to the GTK text buffer
GtkTextBuffer *buffer;

// Global pointer to text buffer
char *text_buffer;
long filesize;

typedef struct {
    char *filename;
    int mode;
    int period;
} ExecuteModeArgs;

/**
 * Count how many chars still in the buffer
*/
void getBuffChar(){
    for (int i =0; i < data_shm_size; i++){
        if (data_shm[i] > 0 && data_shm[i] != 0x2a ){   // 0x0 and 0x2a are used as null chars 
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
 * Get current process stadistics 
*/
void getstadistics(){
    getrusage(RUSAGE_SELF, &ru);
    sem_wait(sem_n_process);
    control_shm[9]+=ru.ru_stime.tv_usec;
    control_shm[8] += ru.ru_utime.tv_usec;
    control_shm[3] += time_;
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
    // Close semaphores and unlink them
void cleanup() {
    close_semaphore(SEM_FREE_SPACE, &sem_free);
    close_semaphore(SEM_FILLED_SPACE, &sem_filled);
    close_semaphore(SEM_I_CLIENT_MUTEX, &sem_i_client_mutex);
    close_semaphore(SEM_N_PROCESS,&sem_n_process);

    free(text_buffer);
}
/**
 * Handles the termination of the process
*/
void handle_end(int sig) {
    getstadistics();
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

/**
 * Open sempahores created in creator process
*/
void setup_semaphores() {
    sem_free = sem_open(SEM_FREE_SPACE, 0);
    sem_filled = sem_open(SEM_FILLED_SPACE, 0);
    sem_i_client_mutex = sem_open(SEM_I_CLIENT_MUTEX, 0);
    sem_n_process =sem_open(SEM_N_PROCESS,0);

    if (sem_free == SEM_FAILED || sem_filled == SEM_FAILED || sem_i_client_mutex == SEM_FAILED || sem_n_process == SEM_FAILED) {
        perror("Failed to open semaphore");
        exit(EXIT_FAILURE);
    }
}

/**
 * Returns the index global variable that indicates what character to read
*/
int get_index(){
    struct timespec inicio, fin;
    clock_gettime(CLOCK_MONOTONIC, &inicio);
    sem_wait(sem_i_client_mutex);
    clock_gettime(CLOCK_MONOTONIC, &fin);
    time_+= ((fin.tv_sec - inicio.tv_sec)*1000+(fin.tv_nsec-inicio.tv_nsec)/1000000);
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

/**
 * Set global text buffer with the content of original file
 * @param file_ptr: Pointer to file pointer
*/
void set_text_buffer(FILE **file_ptr){
    // Determine the file size
    fseek(*file_ptr, 0, SEEK_END);
    filesize = ftell(*file_ptr);
    rewind(*file_ptr);

    // Allocate memory for the file content
    text_buffer = malloc(filesize + 1);
    if (text_buffer == NULL) {
        fclose(*file_ptr);
        fprintf(stderr, "Memory allocation failed\n");
        exit(EXIT_FAILURE);
    }

    // Read the file into buffer
    size_t bytesRead = fread(text_buffer, 1, filesize, *file_ptr);
    if (bytesRead != filesize) {
        free(text_buffer);
        fclose(*file_ptr);
        fprintf(stderr, "Error reading file\n");
        exit(EXIT_FAILURE);
    }
    text_buffer[bytesRead] = '\0'; // Null-terminate the string
}

void execute_mode(const char *filename, int mode, int period) {
    FILE *file = fopen(filename, "r+");
    if (!file) {
        perror("Failed to open file");
        exit(EXIT_FAILURE);
    }

    set_text_buffer(&file); // set global buffer with original file content

    char ch;
    int index = 0; 
    int eof = 0;
    char blank_ch = 32;     //blank space
    char null_ch = 0x2a;    // asterisk character

    if (mode == 0) printf("Press Enter to write next character...\n");

    while (eof != 1) {
        if (mode == 0) while (getchar() != '\n');  // Wait for Enter key (if manual mode)
        if (mode == 1) {
            usleep(period * 1000); //sleep in microseconds (if automatic mode)
        }
        struct timespec inicio, fin;
        clock_gettime(CLOCK_MONOTONIC, &inicio);
        sem_wait(sem_free);
        clock_gettime(CLOCK_MONOTONIC, &fin);
        time_+= ((fin.tv_sec - inicio.tv_sec)*1000+(fin.tv_nsec-inicio.tv_nsec)/1000000);
        int sem_value;
        sem_getvalue(sem_free, &sem_value);

        // get current value and update value of index (global)
        index = get_index();

        // Position the file stream to the current index
        fseek(file, index, SEEK_SET);
        fread(&ch, 1, 1, file);

        if (feof(file)) eof = 1;    //reached end of file
        else{

            fseek(file, index, SEEK_SET);
            fputc(blank_ch, file);     //overwrite position with blank space

            text_buffer[index] = null_ch; // update 

            // write timestamp to shared memory
            read_timestamp(&index, &ch);

            // Write the character to shared memory
            index = index % data_shm_size;  // Circular buffer
            data_shm[index] = ch;

            sem_post(sem_filled);
            sem_getvalue(sem_filled, &sem_value);
        }
    }

    fclose(file);
}

// Thread function to execute mode
void* execute_mode_thread(void *arg) {
    ExecuteModeArgs *args = (ExecuteModeArgs *)arg;
    execute_mode(args->filename, args->mode, args->period);
    return NULL;
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

/**
 * Function to update the text view content.
 * @param new_text: The text to be set in the text view.
 */
void edit_text(const char *new_text) {
    GtkTextIter start, end;

    // Get the start and end iterators of the text buffer
    gtk_text_buffer_get_start_iter(buffer, &start);
    gtk_text_buffer_get_end_iter(buffer, &end);

    // Replace the current content with the new text
    gtk_text_buffer_delete(buffer, &start, &end);
    gtk_text_buffer_insert(buffer, &start, new_text, -1);
}

/**
 * Callback function to update the text.
 * @param user_data: User data provided when the callback is called (unused).
 * @return gboolean: Whether to continue the timer, return FALSE to stop.
 */
static gboolean update_client_content(gpointer user_data) {
    char window_text[filesize+50];

    // Format the window text
    snprintf(window_text, sizeof(window_text), "File contents:\n\n%s", text_buffer);

    // Update the text view
    edit_text(window_text);

    return G_SOURCE_CONTINUE; // Continue calling this function
}

/**
 * Function to set up and show the GTK application window and its components.
 * @param app: The GTK application instance.
 * @param user_data: User data provided when the callback is called (unused).
 */
static void activate_client_win(GtkApplication* app, gpointer user_data) {
    GtkWidget *window;
    GtkWidget *text_view;
    GtkWidget *scrolled_window;

    // Create a new window with the specified title
    window = gtk_application_window_new(app);
    gtk_window_set_title(GTK_WINDOW(window), "Client");
    gtk_window_maximize(GTK_WINDOW(window));    //maximize window

    // Create a new text view, set it to non-editable, and get its buffer
    text_view = gtk_text_view_new();
    buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(text_view));
    gtk_text_view_set_editable(GTK_TEXT_VIEW(text_view), FALSE);
    gtk_text_view_set_cursor_visible(GTK_TEXT_VIEW(text_view), FALSE);
    gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(text_view), GTK_WRAP_WORD_CHAR);  // Ensure wrapping at character level

     // Create a scrolled window
    scrolled_window = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolled_window),
                                   GTK_POLICY_NEVER,  // Never create a horizontal scrollbar
                                   GTK_POLICY_AUTOMATIC); // Automatically create a vertical scrollbar when needed

    // Add the text view to the scrolled window
    gtk_container_add(GTK_CONTAINER(scrolled_window), text_view);

    // Add the scrolled window to the window
    gtk_container_add(GTK_CONTAINER(window), scrolled_window);

    // Display everything
    gtk_widget_show_all(window);

    // Set up a timer to call update_client_content every second
    g_timeout_add_seconds(1, (GSourceFunc)update_client_content, NULL);
}

int main(int argc, char *argv[]) {
    GtkApplication *app;
    int status, mode, period;
    pthread_t mode_thread;
    ExecuteModeArgs args;

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

    control_shm[N_PROCESSES] ++; //register new process to processes counter

    setup_semaphores();

    //set thread args
    args.filename = argv[1];
    args.mode = mode;
    args.period = period;

    // Run execute_mode in a separate thread
    pthread_create(&mode_thread, NULL, execute_mode_thread, &args);

    // Create a new GTK application instance
    char app_id[100] = "org.gtk.client";
    sprintf(app_id + strlen(app_id), "%ld", control_shm[N_PROCESSES]); //set unique ID
    app = gtk_application_new(app_id, G_APPLICATION_FLAGS_NONE);
    // Connect the 'activate' signal, which sets up the window and its contents
    g_signal_connect(app, "activate", G_CALLBACK(activate_client_win), NULL);
    // Run the application, which calls the 'activate' function
    status = g_application_run(G_APPLICATION(app), 0, NULL);
    // Clean up the application instance after the application quits
    g_object_unref(app);

    // Wait for the execute_mode thread to finish
    pthread_join(mode_thread, NULL);

    handle_end(1);
    return EXIT_SUCCESS;
}

