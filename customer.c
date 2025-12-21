#include "restaurant.h"

int waiter_base[] = {100, 300, 500, 700, 900};
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

void print_customer_log(int sem_mutex, int indent_level, const char* format, ...) {
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
    for (int i = 0; i < indent_level; i++) {
        strcat(indent, "\t");
    }

    char msg[256];
    va_list args;
    va_start(args, format);
    vsprintf(msg, format, args);
    va_end(args);

    printf("[%02d:%02d %s] %s%s\n", hour, minute, period, indent, msg);
    fflush(stdout);
}

void sleep_and_update(int delay_minutes, int sem_mutex, int customer_id) {
    int curr_time;
    sem_wait_op(sem_mutex, 0);
    curr_time = shm_ptr[TIME_INDEX];
    sem_signal_op(sem_mutex, 0);

    usleep(delay_minutes * MINUTE);

    sem_wait_op(sem_mutex, 0);
    if (shm_ptr[TIME_INDEX] <= curr_time + delay_minutes)
        shm_ptr[TIME_INDEX] = curr_time + delay_minutes;
    else
        fprintf(stderr, "Warning: Customer %d: time update would set time backwards\n", customer_id);
    sem_signal_op(sem_mutex, 0);
}

void cmain(int customer_id, int arrival_time, int customer_count,
    int sem_mutex, int sem_waiter, int sem_customer) {

    print_customer_log(sem_mutex, 0, "Customer %d arrives (count = %d)", customer_id, customer_count);

    if (arrival_time > 240) {
        print_customer_log(sem_mutex, 0, "Customer %d: Arrived after 3:00pm, leaving.", customer_id);
        exit(0);
    }

    sem_wait_op(sem_mutex, 0);
    if (shm_ptr[TABLES_INDEX] <= 0) {
        sem_signal_op(sem_mutex, 0);
        print_customer_log(sem_mutex, 4, "Customer %d leaves (no empty table)", customer_id);
        exit(0);
    }
    else {
        shm_ptr[TABLES_INDEX]--;
        sem_signal_op(sem_mutex, 0);
    }

    sem_wait_op(sem_mutex, 0);
    int waiter_index = shm_ptr[NEXT_WAITER_INDEX];
    shm_ptr[NEXT_WAITER_INDEX] = (shm_ptr[NEXT_WAITER_INDEX] + 1) % NUM_WAITERS;
    sem_signal_op(sem_mutex, 0);

    int base = waiter_base[waiter_index];
    sem_wait_op(sem_mutex, 0);
    int back = shm_ptr[base + 1];
    shm_ptr[base + 2 + 2 * back] = customer_id;
    shm_ptr[base + 2 + 2 * back + 1] = customer_count;
    shm_ptr[base + 1] = back + 1;
    sem_signal_op(sem_mutex, 0);

    print_customer_log(sem_mutex, 1, "Customer %d: Order placed to Waiter %c", customer_id, 'U' + waiter_index);

    sem_signal_op(sem_waiter, waiter_index);

    sem_wait_op(sem_customer, customer_id - 1);
    sem_wait_op(sem_customer, customer_id - 1);

    sem_wait_op(sem_mutex, 0);
    int current_time = shm_ptr[TIME_INDEX];
    sem_signal_op(sem_mutex, 0);
    int waiting_time = current_time - arrival_time;

    print_customer_log(sem_mutex, 2, "Customer %d gets food [Waiting time = %d]", customer_id, waiting_time);

    sleep_and_update(30, sem_mutex, customer_id);

    print_customer_log(sem_mutex, 3, "Customer %d finishes eating and leaves", customer_id);

    sem_wait_op(sem_mutex, 0);
    shm_ptr[TABLES_INDEX]++;
    sem_signal_op(sem_mutex, 0);

    exit(0);
}

int main(int argc, char* argv[]) {
    int shmid = shmget(SHM_KEY, 2048 * sizeof(int), 0666);
    shm_ptr = (int*)shmat(shmid, NULL, 0);

    int sem_mutex = semget(SEM_MUTEX_KEY, 1, 0666);
    int sem_waiter = semget(SEM_WAITER_KEY, NUM_WAITERS, 0666);
    int sem_customer = semget(SEM_CUSTOMER_KEY, MAX_CUSTOMERS, 0666);

    FILE* fp = fopen("customers.txt", "r");

    int customer_id, arrival_time, customer_count;
    int last_arrival = 0;

    while (fscanf(fp, "%d %d %d", &customer_id, &arrival_time, &customer_count) == 3) {
        if (customer_id == -1)
            break;

        if (arrival_time > last_arrival) {
            int delay = arrival_time - last_arrival;
            sleep_and_update(delay, sem_mutex, customer_id);
            last_arrival = arrival_time;
        }

        pid_t pid = fork();
        if (pid == 0) {
            cmain(customer_id, arrival_time, customer_count, sem_mutex, sem_waiter, sem_customer);
            exit(0);
        }
    }
    fclose(fp);

    while (wait(NULL) > 0);

    return 0;
}
