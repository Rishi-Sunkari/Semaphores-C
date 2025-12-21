#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdarg.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/sem.h>
#include <sys/types.h>
#include <errno.h>
#include <string.h>
#include <sys/wait.h>

#define SHM_KEY         0x1234
#define SEM_MUTEX_KEY   0x5678
#define SEM_WAITER_KEY  0x5679
#define SEM_CUSTOMER_KEY 0x5680
#define SEM_COOK_KEY    0x5681

#define MINUTE 100000
#define NUM_WAITERS 5
#define MAX_CUSTOMERS 200

#define TIME_INDEX        0
#define TABLES_INDEX      1
#define NEXT_WAITER_INDEX 2

#define WAITER_FR_OFFSET  198
#define WAITER_PO_OFFSET  199

#define COOK_QUEUE_BASE 1100
