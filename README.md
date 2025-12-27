\# Multi-Process Restaurant Simulation



A concurrent system simulation in C utilizing Linux System V Inter-Process Communication (IPC). The project simulates the workflow of a restaurant, managing synchronization between Customers, Waiters, and Cooks using Shared Memory and Semaphores.



\## 🚀 Features

\- \*\*Process-Based Simulation:\*\* Each customer, waiter, and cook runs as an independent process via `fork()`.

\- \*\*IPC Synchronization:\*\* - \*\*Shared Memory:\*\* Stores the global clock, table availability, and order queues.

&nbsp; - \*\*Semaphores:\*\* Implements Mutexes for memory safety and condition signaling for task handoffs.

\- \*\*Dynamic Scheduling:\*\* Customers arrive based on an input file (`customers.txt`) with specific arrival times and group sizes.

\- \*\*Automated Logging:\*\* Real-time timestamped logs showing the exact state of every entity in the system.



\## 🏗️ Architecture

The system is divided into four main components:

1\. \*\*Manager (Cook Process):\*\* Initializes shared memory and semaphores. Spawns and manages Cook processes.

2\. \*\*Customer Process:\*\* Handles table acquisition, ordering logic, and eating durations.

3\. \*\*Waiter Process:\*\* Acts as the middleman between the customer and the kitchen.

4\. \*\*Generator:\*\* A utility script to create randomized customer data.



\## 🛠️ Prerequisites

\- Linux/Unix Environment

\- GCC Compiler

\- Standard C Libraries (`sys/ipc.h`, `sys/shm.h`, `sys/sem.h`)



\## 📥 Installation \& Setup

1\. Clone the repository:

&nbsp;  ```bash

&nbsp;  git clone \[https://github.com/Rishi-Sunkari/Semaphores-C]

&nbsp;  cd restaurant-simulation

gcc -o cook\_server cook.c -lpthread

gcc -o waiter\_server waiter.c

gcc -o customer\_client customer.c

gcc -o gen generator.c

./gen > customers.txt


./cook\_server


./customer\_client


