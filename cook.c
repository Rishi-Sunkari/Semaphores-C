#include "restaurant.h"

int waiter_base[] = { 100, 300, 500, 700, 900 };
int* shm_ptr = NULL;

union semun {
    int val;
    struct semid_ds* buf;
    unsigned short* array;
};

void sem_wait_op(int semid, int semnum) {
    struct sembuf op;
    op.sem_num = semnum;
    op.sem_op = -1;
    op.sem_flg = 0;
    if (semop(semid, &op, 1) == -1) {
        perror("semop wait");
        exit(1);
    }
}

void sem_signal_op(int semid, int semnum) {
    struct sembuf op;
    op.sem_num = semnum;
    op.sem_op = 1;
    op.sem_flg = 0;
    if (semop(semid, &op, 1) == -1) {
        perror("semop signal");
        exit(1);
    }
}

void print_timestamped_log(int sem_mutex, const char* message) {
    int t;
    sem_wait_op(sem_mutex, 0);
    t = shm_ptr[TIME_INDEX];
    sem_signal_op(sem_mutex, 0);

    int total_minutes = t;
    int hour = 11 + (total_minutes / 60);
    int minute = total_minutes % 60;
    char period[3];
    if (hour < 12) {
        sprintf(period, "am");
    }
    else {
        if (hour > 12)
            hour -= 12;
        sprintf(period, "pm");
    }
    printf("[%02d:%02d %s]%s\n", hour, minute, period, message);
    fflush(stdout);
}

void sleep_and_update(int delay_minutes, int sem_mutex, int cook_id) {
    int curr_time;
    sem_wait_op(sem_mutex, 0);
    curr_time = shm_ptr[TIME_INDEX];
    sem_signal_op(sem_mutex, 0);

    usleep(delay_minutes * MINUTE);

    sem_wait_op(sem_mutex, 0);
    if (shm_ptr[TIME_INDEX] <= curr_time + delay_minutes) {
        shm_ptr[TIME_INDEX] = curr_time + delay_minutes;
    }
    else {
        fprintf(stderr, "Warning: Cook %c: time update would set time backwards\n", (cook_id == 0 ? 'C' : 'D'));
    }
    sem_signal_op(sem_mutex, 0);
}

int check_for_waiters(int sem_mutex) {
    for (int i = 0; i < NUM_WAITERS; i++) {
        sem_wait_op(sem_mutex, 0);
        if (shm_ptr[waiter_base[i]] < shm_ptr[waiter_base[i] + 1]) {
            sem_signal_op(sem_mutex, 0);
            return 1;
        }
        sem_signal_op(sem_mutex, 0);
    }
    return 0;
}

void cmain(int cook_id, int sem_mutex, int sem_waiter, int sem_cook) {
    char cook_char = (cook_id == 0 ? 'C' : 'D');
    char log_msg[256];

    print_timestamped_log(sem_mutex, (cook_id == 0 ? " Cook C is ready." : "\tCook D is ready."));

    while (1) {
        sem_wait_op(sem_cook, 0);

        sem_wait_op(sem_mutex, 0);
        int current_time = shm_ptr[TIME_INDEX];
        int cq_front = shm_ptr[COOK_QUEUE_BASE];
        int cq_back = shm_ptr[COOK_QUEUE_BASE + 1];

        int req_index, req_waiter, req_customer, req_count;

        if (cq_front < cq_back) {
            req_index = cq_front;
            req_waiter = shm_ptr[COOK_QUEUE_BASE + 2 + 3 * req_index];
            req_customer = shm_ptr[COOK_QUEUE_BASE + 2 + 3 * req_index + 1];
            req_count = shm_ptr[COOK_QUEUE_BASE + 2 + 3 * req_index + 2];
            shm_ptr[COOK_QUEUE_BASE] = cq_front + 1;
            int waiter_base_index = waiter_base[req_waiter];
            shm_ptr[waiter_base_index + WAITER_PO_OFFSET]++;
            sem_signal_op(sem_mutex, 0);

            if (cook_char == 'C') sprintf(log_msg, " Cook %c: Preparing order (Waiter %c, Customer %d, Count %d)",
                cook_char, 'U' + req_waiter, req_customer, req_count);
            else sprintf(log_msg, "\tCook %c: Preparing order (Waiter %c, Customer %d, Count %d)",
                cook_char, 'U' + req_waiter, req_customer, req_count);
            print_timestamped_log(sem_mutex, log_msg);

            sleep_and_update(5 * req_count, sem_mutex, cook_id);

            sem_wait_op(sem_mutex, 0);
            shm_ptr[waiter_base_index + WAITER_FR_OFFSET] = req_customer;
            shm_ptr[waiter_base_index + WAITER_PO_OFFSET] -= 1;
            sem_signal_op(sem_mutex, 0);

            sem_signal_op(sem_waiter, req_waiter);

            if (cook_char == 'C') sprintf(log_msg, " Cook %c: Prepared order (Waiter %c, Customer %d, Count %d)",
                cook_char, 'U' + req_waiter, req_customer, req_count);
            else sprintf(log_msg, "\tCook %c: Prepared order (Waiter %c, Customer %d, Count %d)",
                cook_char, 'U' + req_waiter, req_customer, req_count);
            print_timestamped_log(sem_mutex, log_msg);
        }
        else {
            sem_signal_op(sem_mutex, 0);
            continue;
        }
        // Termination condition
        current_time += 5 * req_count;
        cq_front++;
        if (current_time > 240 && cq_front >= cq_back && !check_for_waiters(sem_mutex)) {
            for (int i = 0; i < NUM_WAITERS; i++) {
                sem_signal_op(sem_waiter, i);
            }
            break;
        }
    }

    for (int i = 0; i < NUM_WAITERS; i++) {
        sem_signal_op(sem_waiter, i);
    }

    if (cook_char == 'C') sprintf(log_msg, " Cook %c: Leaving", cook_char);
    else sprintf(log_msg, "\tCook %c: Leaving", cook_char);
    print_timestamped_log(sem_mutex, log_msg);

    exit(0);
}

int main(int argc, char* argv[]) {
    int shmid;
    shmid = shmget(SHM_KEY, 2048 * sizeof(int), IPC_CREAT | 0666);
    shm_ptr = (int*)shmat(shmid, NULL, 0);

    shm_ptr[TIME_INDEX] = 0;
    shm_ptr[TABLES_INDEX] = 10;
    shm_ptr[NEXT_WAITER_INDEX] = 0;
    shm_ptr[COOK_QUEUE_BASE] = 0;
    shm_ptr[COOK_QUEUE_BASE + 1] = 0;

    int sem_mutex, sem_waiter, sem_customer, sem_cook;
    union semun arg;

    sem_mutex = semget(SEM_MUTEX_KEY, 1, IPC_CREAT | 0666);
    arg.val = 1;
    semctl(sem_mutex, 0, SETVAL, arg);

    sem_customer = semget(SEM_CUSTOMER_KEY, MAX_CUSTOMERS, IPC_CREAT | 0666);
    for (int i = 0; i < MAX_CUSTOMERS; i++) {
        arg.val = 0;
        semctl(sem_customer, i, SETVAL, arg);
    }

    sem_waiter = semget(SEM_WAITER_KEY, NUM_WAITERS, IPC_CREAT | 0666);
    for (int i = 0; i < NUM_WAITERS; i++) {
        arg.val = 0;
        semctl(sem_waiter, i, SETVAL, arg);
    }

    sem_cook = semget(SEM_COOK_KEY, 1, IPC_CREAT | 0666);
    arg.val = 0;
    semctl(sem_cook, 0, SETVAL, arg);

    pid_t pid1, pid2;
    pid1 = fork();
    if (pid1 == 0) {
        cmain(0, sem_mutex, sem_waiter, sem_cook);
        exit(0);
    }
    pid2 = fork();
    if (pid2 == 0) {
        cmain(1, sem_mutex, sem_waiter, sem_cook);
        exit(0);
    }

    waitpid(pid1, NULL, 0);
    waitpid(pid2, NULL, 0);

    shmdt(shm_ptr);

    return 0;
}
