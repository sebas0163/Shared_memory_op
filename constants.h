#ifndef CONSTANTS_H
#define CONSTANTS_H

#define SHM_DATA "/data_shared_memory"
#define SHM_CONTROL "/control_shared_memory"
#define SHM_TIMESTAMPS "/timestamps_shared_memory"

#define SEM_FREE_SPACE "/sem_free_space"
#define SEM_FILLED_SPACE "/sem_filled_space"

#define SEM_I_CLIENT_MUTEX "/sem_i_client_mutex"
#define SEM_I_RECR_MUTEX "/sem_i_recr_mutex"
#define SEM_N_PROCESS "/sem_n_process"

#define I_CLIENT 0
#define I_RECREATOR 1
#define N_PROCESSES 2

#endif // CONSTANTS_H

