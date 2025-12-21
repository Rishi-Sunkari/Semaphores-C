#include "restaurant.h"

int waiter_base[] = { 100, 300, 500, 700, 900 };
int* shm_ptr = NULL;

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

void print_waiter_log(int sem_mutex, int waiter_index, const char* message) {
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
    char indent[64] = "";
    for (int i = 0; i < waiter_index; i++) {
        strcat(indent, "\t");
    }
    printf("[%02d:%02d %s] %s%s\n", hour, minute, period, indent, message);
    fflush(stdout);
}

void sleep_and_update(int delay_minutes, int sem_mutex, int waiter_index) {
    int curr_time;
    sem_wait_op(sem_mutex, 0);
    curr_time = shm_ptr[TIME_INDEX];
    sem_signal_op(sem_mutex, 0);

    usleep(delay_minutes * MINUTE);

    sem_wait_op(sem_mutex, 0);
    if (shm_ptr[TIME_INDEX] <= curr_time + delay_minutes)
        shm_ptr[TIME_INDEX] = curr_time + delay_minutes;
    else
        fprintf(stderr, "Warning: Waiter %c: time update would set time backwards\n", 'U' + waiter_index);
    sem_signal_op(sem_mutex, 0);
}

void wmain(int waiter_id, int sem_mutex, int sem_waiter, int sem_customer, int sem_cook) {
    int base = waiter_base[waiter_id];

    sem_wait_op(sem_mutex, 0);
    shm_ptr[base] = 0;
    shm_ptr[base + 1] = 0;
    shm_ptr[base + WAITER_FR_OFFSET] = 0;
    shm_ptr[base + WAITER_PO_OFFSET] = 0;
    sem_signal_op(sem_mutex, 0);

    char message[100];
    sprintf(message, "Waiter %c is ready", 'U' + waiter_id);
    print_waiter_log(sem_mutex, waiter_id, message);

    while (1) {
        sem_wait_op(sem_waiter, waiter_id);

        sem_wait_op(sem_mutex, 0);
        int current_time = shm_ptr[TIME_INDEX];
        int front = shm_ptr[base];
        int back = shm_ptr[base + 1];
        int fr = shm_ptr[base + WAITER_FR_OFFSET];
        int pending = shm_ptr[base + WAITER_PO_OFFSET];

        if (current_time > 240 && front >= back && fr == 0 && pending == 0) {
            sem_signal_op(sem_mutex, 0);
            break;
        }
        if (fr != 0) {
            int customer_ready = fr;
            shm_ptr[base + WAITER_FR_OFFSET] = 0;
            sem_signal_op(sem_mutex, 0);

            char log_msg[256];
            sprintf(log_msg, "Waiter %c: Serving food to Customer %d", 'U' + waiter_id, customer_ready);
            print_waiter_log(sem_mutex, waiter_id, log_msg);
            sem_signal_op(sem_customer, customer_ready - 1);
            continue;
        }
        if (front < back) {
            int customer_id_order = shm_ptr[base + 2 + 2 * front];
            int customer_count_order = shm_ptr[base + 2 + 2 * front + 1];
            shm_ptr[base] = front + 1;
            sem_signal_op(sem_mutex, 0);

            char log_msg[256];
            sprintf(log_msg, "Waiter %c: Placing order for Customer %d (count = %d)",
                'U' + waiter_id, customer_id_order, customer_count_order);
            print_waiter_log(sem_mutex, waiter_id, log_msg);

            sleep_and_update(1, sem_mutex, waiter_id);

            sem_signal_op(sem_customer, customer_id_order - 1);
            sem_wait_op(sem_mutex, 0);
            int cq_back = shm_ptr[COOK_QUEUE_BASE + 1];
            shm_ptr[COOK_QUEUE_BASE + 2 + 3 * cq_back] = waiter_id;
            shm_ptr[COOK_QUEUE_BASE + 2 + 3 * cq_back + 1] = customer_id_order;
            shm_ptr[COOK_QUEUE_BASE + 2 + 3 * cq_back + 2] = customer_count_order;
            shm_ptr[COOK_QUEUE_BASE + 1] = cq_back + 1;
            sem_signal_op(sem_mutex, 0);

            sem_signal_op(sem_cook, 0);
            continue;
        }
        sem_signal_op(sem_mutex, 0);
    }

    char term_msg[256];
    sprintf(term_msg, "Waiter %c leaving (no more customer to serve)", 'U' + waiter_id);
    print_waiter_log(sem_mutex, waiter_id, term_msg);
    exit(0);
}

int main(int argc, char* argv[]) {
    int shmid = shmget(SHM_KEY, 2048 * sizeof(int), 0666);
    shm_ptr = (int*)shmat(shmid, NULL, 0);

    int sem_mutex = semget(SEM_MUTEX_KEY, 1, 0666);
    int sem_waiter = semget(SEM_WAITER_KEY, NUM_WAITERS, 0666);
    int sem_customer = semget(SEM_CUSTOMER_KEY, MAX_CUSTOMERS, 0666);
    int sem_cook = semget(SEM_COOK_KEY, 1, 0666);

    pid_t pids[NUM_WAITERS];
    for (int i = 0; i < NUM_WAITERS; i++) {
        pid_t pid = fork();
        if (pid == 0) {
            wmain(i, sem_mutex, sem_waiter, sem_customer, sem_cook);
            exit(0);
        }
        else {
            pids[i] = pid;
        }
    }
    for (int i = 0; i < NUM_WAITERS; i++) {
        waitpid(pids[i], NULL, 0);
    }

    shmdt(shm_ptr);
    return 0;
}
