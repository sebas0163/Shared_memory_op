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
#include <gtk/gtk.h>

char *data_shm = NULL;      //initialize data shared memory
int data_shm_fd = -1;            //initialize file descriptor for shared memory
size_t data_shm_size = 0;   //initialize size of data shared memory

char *control_shm = NULL;      //initialize control shared memory
int control_shm_fd = -1;            //initialize file descriptor for shared memory
size_t control_shm_size = sizeof(int) * 3;   //initialize size of data shared memory

char *tm_shm = NULL;      //initialize timestamps shared memory
int tm_shm_fd = -1;            //initialize file descriptor for shared memory
size_t tm_shm_size = 0;   //initialize size of timestamps shared memory

// Global pointer to the text buffer
GtkTextBuffer *buffer;

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
    if (data_shm_fd != -1) {
        close(data_shm_fd);
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

/**
 * Initialize semaphores
 *  - sem_free: how many spaces available in shared memory
 *  - sem_filled: how many spaces have been written in shared memory
 *  - sem_i_client_mutex: mutual exclusion to update global variable cient index
*/
void initialize_semaphores() {
    sem_t *sem_free = sem_open(SEM_FREE_SPACE, O_CREAT, 0666, data_shm_size);
    sem_t *sem_filled = sem_open(SEM_FILLED_SPACE, O_CREAT, 0666, 0);
    sem_t *sem_i_client_mutex = sem_open(SEM_I_CLIENT_MUTEX, O_CREAT, 0666, 1);

    if (sem_free == SEM_FAILED || sem_filled == SEM_FAILED || sem_i_client_mutex == SEM_FAILED) {
        perror("Failed to open semaphore");
        cleanup();
        exit(EXIT_FAILURE);
    }

    sem_close(sem_free);
    sem_close(sem_filled);
    sem_close(sem_i_client_mutex);
}

void display_memory_contents() {
    printf("\nShared Memory Contents:\n");
    for (size_t i = 0; i < data_shm_size; i++) {
        printf("%c", ((unsigned char*)data_shm)[i]);
        if ((i + 1) % 64 == 0)
            printf("\n");
    }
    printf("\n");
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
 * Callback function to update the timer text.
 * @param user_data: User data provided when the callback is called (unused).
 * @return gboolean: Whether to continue the timer, return FALSE to stop.
 */
static gboolean update_text_content(gpointer user_data) {
    char window_text[data_shm_size+50];

    // Format the window text
    snprintf(window_text, sizeof(window_text), "Shared Memory Contents:\n\n%s", data_shm);

    // Update the text view
    edit_text(window_text);

    return G_SOURCE_CONTINUE; // Continue calling this function
}

/**
 * Function to set up and show the GTK application window and its components.
 * @param app: The GTK application instance.
 * @param user_data: User data provided when the callback is called (unused).
 */
static void activate(GtkApplication* app, gpointer user_data) {
    GtkWidget *window;
    GtkWidget *text_view;

    // Create a new window with the specified title and default size
    window = gtk_application_window_new(app);
    gtk_window_set_title(GTK_WINDOW(window), "Creator");
    // gtk_window_set_default_size(GTK_WINDOW(window), 200, 200);
    gtk_window_maximize(GTK_WINDOW(window));

    // Create a new text view, set it to non-editable, and get its buffer
    text_view = gtk_text_view_new();
    buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(text_view));
    gtk_text_view_set_editable(GTK_TEXT_VIEW(text_view), FALSE);
    gtk_text_view_set_cursor_visible(GTK_TEXT_VIEW(text_view), FALSE);

    // Add the text view to the window and display everything
    gtk_container_add(GTK_CONTAINER(window), text_view);
    gtk_widget_show_all(window);

    // Set up a timer to call update_text_content every second
    g_timeout_add_seconds(1, (GSourceFunc)update_text_content, NULL);
}

int main(int argc, char **argv) {

    GtkApplication *app;
    int status;
    
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <memory size in bytes>\n", argv[0]);
        return EXIT_FAILURE;
    }

    // Read second argument to set shared memory size
    data_shm_size = strtoul(argv[1], NULL, 10);
    if (data_shm_size <= 0) {
        fprintf(stderr, "Invalid memory size.\n");
        return EXIT_FAILURE;
    }

    // set size for timestamps shared memory
    tm_shm_size = (sizeof(int) + sizeof(char) + sizeof(struct tm)) * data_shm_size;

    // Handle process termination
    signal(SIGINT, handle_signal);

    // Initialize the shared memory for data
    initialize_shared_memory(SHM_DATA, data_shm_size, &data_shm_fd, (void **)&data_shm);

    // Initialize the shared memory for timestamps
    initialize_shared_memory(SHM_TIMESTAMPS, tm_shm_size, &tm_shm_fd, (void **)&tm_shm);

    // Initialize the shared memory for control
    initialize_shared_memory(SHM_CONTROL, control_shm_size, &control_shm_fd, (void **)&control_shm);

    // Initialize general semaphores
    initialize_semaphores();
    
    // Create a new GTK application instance
    app = gtk_application_new("org.gtk.example", G_APPLICATION_FLAGS_NONE);
    // Connect the 'activate' signal, which sets up the window and its contents
    g_signal_connect(app, "activate", G_CALLBACK(activate), NULL);
    // Run the application, which calls the 'activate' function
    status = g_application_run(G_APPLICATION(app), 0, NULL);
    // Clean up the application instance after the application quits
    g_object_unref(app);

    cleanup(); // Clean resources on exit
    return status;
}

