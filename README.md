# Multithreading Library for Linux

This project implements a user-mode multithreading library for Linux, which provides a basic thread system and a locking mechanism. The library offers a small subset of the pthread API, focusing on essential functionalities like thread creation, termination, and manipulation. It also includes a round-robin scheduling mechanism, basic locking to ensure thread safety, and semaphore support for coordinating interactions between multiple threads.

## Functions

### Thread Management

- `thread_init()`: Initializes the first thread and changes the SIGALARM handler to `schedule_handler()`. This enables the use of `ualarm()` to call the scheduler at 50ms intervals.

- `schedule_handler()`: Implements round-robin scheduling for up to 128 threads. It switches threads after a state of READY, saves the current running environment for the thread with `setjmp()`, and checks if the thread has exited. The next thread has its state changed to RUNNING and longjmp()'s to the saved environment.

- `pthread_create()`: Calls `thread_init()` on first call. Creates a new thread for each call of the function. Initializes the stack pointer, sets the top of the stack to the `pthread_exit` address, and initializes registers.

- `pthread_self()`: Returns the thread ID of the currently running thread.

- `pthread_exit()`: Called after a thread finishes all its execution instructions. Sets the thread's status to FREE, frees the thread's stack, and calls `schedule_handler()` if there are still threads. Otherwise, it exits with `exit(0)`.

- `pthread_join(pthread_t thread, void **value_ptr)`: Postpones the execution of the calling thread until the target thread terminates, unless the target thread has already terminated. On return from a successful `pthread_join` call with a non-NULL `value_ptr` argument, the value passed to `pthread_exit` by the terminating thread is made available in the location referenced by `value_ptr`.

### Locking Mechanism

- `lock()`: Prevents a thread from being interrupted by any other thread. The thread can no longer be interrupted once it calls this function.

- `unlock()`: Resumes the normal status of a locked thread, allowing it to be scheduled whenever an alarm signal is received. A thread should call `unlock()` only after it has previously performed a `lock()` operation.

### Semaphore Support

- `sem_init(sem_t *sem, int pshared, unsigned value)`: Initializes an unnamed semaphore referred to by `sem`. The `pshared` argument always equals 0, meaning the semaphore is shared between threads of the process. Attempting to initialize an already initialized semaphore results in undefined behavior.

- `sem_wait(sem_t *sem)`: Decrements (locks) the semaphore pointed to by `sem`. If the semaphore's value is greater than zero, the decrement proceeds, and the function returns immediately. If the semaphore has a value of zero, the call blocks until it becomes possible to perform the decrement.

- `sem_post(sem_t *sem)`: Increments (unlocks) the semaphore pointed to by `sem`. If the semaphore's
