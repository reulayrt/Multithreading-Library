#include "semaphore.h"
#include "ec440threads.h"
#include "threads.h"
#include "Queue.h"
#include <stdio.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>
#include <signal.h>
#include <errno.h>
#include <stdbool.h>
#include <setjmp.h>
#include <stdlib.h>

#define JB_RBX 0
#define JB_RBP 1
#define JB_R12 2
#define JB_R13 3
#define JB_R14 4
#define JB_R15 5
#define JB_RSP 6
#define JB_PC  7


#define    RUNNING   0x0  // currently executing thread
#define    READY     0x1  // thread is ready to execute waiting to be scheduled
#define    BLOCKED   0x2  // waiting for I/O
#define    ZOMBIE    0x3  // Finished thread with context still save   
#define    CLEAN     0x4  // Thread has empty context

//     /*  init           init           init
//       --------       --------       --------
//       | head |  <<<  | head |  >>>  | head |
//       --------       --------       --------
//                      ^^ current node
//     */

// Using a doubly linked list for the TCB data structure to easily go back and fourth for Round Robin Scheduling
struct TCB {
    int state;  // Current thread status
    void* stack;  // Stack pointer
    jmp_buf buf;  // stores Buffer information about registers
    void* exit_state;  // Exiting state of the thread
    int fresh_thread;  // Keeps track on newly created threads to determine their status
    pthread_t waitID;  // stores the ID of the thread waiting for the new one to finish
    int wokenUp;  // How to tell if the thread was just woken up
};

// Stores extra info about semaphores
struct sem_database {
    int value;  // Amount of wake ups called to semaphor
    struct Queue* blockedQ;  // Pointer to waiting threads queue (Queue can be found in Queue.h)
    int init;  // Flag to know if the semaphore has been initialized so destroy() doesn't remove non init sema
};

struct TCB tcb[128];  // Array to store every threads information
pthread_t global_thread_running = 0;  // Value of the currently running threads ID
struct sigaction safe;  // Need for SIGALRM

// This will store the return value for the start routing of a thread
void pthread_exit_wrapper() 
{
    unsigned long int res;
    asm("movq %%rax, %0\n":"=r"(res));
    pthread_exit((void *) res);
}

void schedule_handler() {        
    // Change the state of the thread from RUNNING (hopefully) to READY
    switch(tcb[global_thread_running].state) {
        case RUNNING:
            tcb[global_thread_running].state = READY;
            break;
        case READY:
        case BLOCKED:
        case ZOMBIE:
        case CLEAN:
            break;
    }

    pthread_t current_thread = global_thread_running;

    // Round Robin scheduling method 
    while(true) {
        // There can only be 128 thread as per assignment instructions
        if(current_thread == 128 - 1) {
            current_thread = 0;
        }
        else {
            current_thread++;
        }
        
        // The first READY thread will break the loop
        if(tcb[current_thread].state == READY) {
            break;
        }
    }

    /* 
     * Used to stop infinite loop between longjmp and setjmp
     * 0 means normal return
     * else means a longjump return 
     */
    int catch22 = 0;
    
    // Save the state of any thread that was not just created and alive
    if (tcb[global_thread_running].fresh_thread == 0  && tcb[global_thread_running].state != ZOMBIE) {
        catch22 = setjmp(tcb[global_thread_running].buf);
    }
    
    // At this point everything has been done for freshly created threads so resetting the freshThread value
    if (tcb[current_thread].fresh_thread) {
        tcb[current_thread].fresh_thread = 0;
    }

    // Change the state of current selected thread to running and jump to that threads context with longjmp
    if (!catch22) {
        global_thread_running = current_thread;
        tcb[global_thread_running].state = RUNNING;
        longjmp(tcb[global_thread_running].buf, 1);
    }
}

// Initialize the first thread running main and start TCB array
void thread_init() {
    // Initializing the TCB array to empty
    int i;
    for (i = 0; i < 128; i++)
    {
        tcb[i].waitID = i;
        tcb[i].fresh_thread = 0;
        tcb[i].state = CLEAN; 
        tcb[i].wokenUp = 0;
    }

    // Creating ALARM handler
    sigemptyset(&safe.sa_mask);
    safe.sa_handler = &schedule_handler;
    safe.sa_flags = SA_NODEFER;  // SA_NODEFER will stop ALARM from being blocked, via man page
    sigaction(SIGALRM, &safe, NULL);

    int period = 50 * 1000;
    ualarm(period, period);  // first alarm goes off at 50ms then will repeated go off at 50ms intervals
}

/*
	| if (init)
	| Initialize by setting tcb array components
	| Along with creating the signal handler for the signal SIGALRM
	| create alarm that goes off every 50ms to switch running thread via schedule_handler()
	| Set status to READY
	| Save main thread context setjmp()

	- Search for next available thread
	- set the inputed thread = to the current thread so main can access it
	- save context of new thread with setjmp()
	- Saving register values to process in driver code
	- create stack
	- place address of pthread_exit to top of stack via the assemble code wrapper
	- set RSP register to top of new allocated stack
	- Schedule next READY thread
	| if (not just created & alive)
	| save context with setjmp()

	| reset newly created thread value if needed

	- Change global running thread var to newly chosen thread
	- change newly running thread state to RUNNING
	- longjmp to save state of that thread
*/
extern int pthread_create(
    pthread_t *thread, 
    const pthread_attr_t *attr, 
    void *(*start_routine) (void *), 
    void *arg)
{

    static int start = 1;  // Mark the first time pthread_create() is called for thread_init
    int main_thread = 0;  // Flag for first run through so no context is created for the main thread

    if (start) {  // check if this is the first call of pthread
        thread_init();

        tcb[0].fresh_thread = 1;
        tcb[0].state = READY;

        main_thread = setjmp(tcb[0].buf);  // Saving the main threads context
        start = 0;
    }

    // This is where the context of the new thread will be initialized
    if (!main_thread) {
        // Thread ID is indexed at 0
        pthread_t current_thread = 1;
        while(current_thread < 128 && tcb[current_thread].state != CLEAN) {  // Assigns the first available thread ID to the new thread
            current_thread++;
        }

        // The maximum threads specified in the assignment is 128
        if (current_thread == 128) {
            fprintf(stderr, "ERROR: Thread Limit Has Been Reached\n");
            exit(EXIT_FAILURE);
        }
        
        *thread = current_thread;  // Save the current thread id into *thread pointer so main can access it
        setjmp(tcb[current_thread].buf);  // Saving context of new thread into the jmp_buf data structure

        tcb[current_thread].buf[0].__jmpbuf[JB_PC] = ptr_mangle((unsigned long int)start_thunk);  // Pointing program counter RIP to start_thunk
        tcb[current_thread].buf[0].__jmpbuf[JB_R13] = (long) arg; // Saving the arguements into R13
        tcb[current_thread].buf[0].__jmpbuf[JB_R12] = (unsigned long int) start_routine;  // saving the function into register R12 (first available after src and dst regs)
        
        tcb[current_thread].stack = malloc(32767);
        void* sp_bottom = tcb[current_thread].stack + 32767;  // Point to the top of stack (for now)

        void* sp = sp_bottom - sizeof(&pthread_exit_wrapper);  // $$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$ try 8
        void (*tmp)(void*) = &pthread_exit_wrapper;  // $$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$ try unsigned long
        sp = memcpy(sp, &tmp, sizeof(tmp));

        tcb[current_thread].buf[0].__jmpbuf[JB_RSP] = ptr_mangle((unsigned long int)sp);  // Setting RPS to the top of the stack

        /*
         * Now that thread context is initialized its state can be set to ready.
         * Next we just call the scheduler
        */
        tcb[current_thread].state = READY;
        tcb[current_thread].fresh_thread = 1;  // Only to make schedule_handler bypass setjmp

        //Set the waitID to the process in case this is second use of same tid
        tcb[current_thread].waitID = current_thread;  // Just incase this is not the first use of this thread ID

        schedule_handler();
    }
    else {  // It will only be the main thread once
        main_thread = 0;
    }
    
    return 0;
}

/*
	- Set currently running thread state to dead
	- set the exit_state to value_ptr so it can wait on value_ptr
	- Handle any waiting process by setting its state to READY
	- use flag "alive" to find if any threads are still BLOCKED and 
	  still need to be scheduled
	- If all threads are finished clean up the context of all dead threads
	- exit(0)
*/
// Terminates the calling thread might have to wait for "value_ptr" thread to terminate
extern void pthread_exit(void *value_ptr) {
    tcb[global_thread_running].state = ZOMBIE;  // Set to ZOMBIE initially and change it once it has been freed
    tcb[global_thread_running].exit_state = value_ptr;  // the running thread will have to wait on value_ptr

    /*
     * Handling any waiting process before scheduling next thread
    */
    pthread_t wait = tcb[global_thread_running].waitID;
    if (wait != global_thread_running) {
        tcb[wait].state = READY;
    }

    int alive = 0;
    int i;
    for (i = 0; i < 128; i++) {  // Finding any threads waiting to finish
        switch(tcb[i].state) {
            case READY   :
            case RUNNING :
            case BLOCKED :
                alive = 1;
                break;
            case ZOMBIE:
            case CLEAN:
                break;
        }
    }

    /*
       There are more threads to be handled by the scheduler before terminating program.
       Other thread could still have jobs to complete and need to be scheduled accordingly.
    */
    if (alive) {
        schedule_handler();
    }

    // Clean all dead threads contexts
    for (i = 0; i < 128; i++) {
        if (tcb[i].state == ZOMBIE) {
            free(tcb[i].stack);
        }
    }
    exit(0);  // terminates program with 0 returned to OS to indicate successful complettion
}

// returns the id of the currently running thread
extern pthread_t pthread_self()
{
    return global_thread_running;
}


// ************************************************************* HW2 -> HW3 **********************************************


// When called thread can no longer be interrupted by any other thread (SIGALRM)
extern void lock()
{
    sigset_t set;
    sigemptyset(&set);
    sigaddset(&set, SIGALRM);
    sigprocmask(SIG_BLOCK, &set, NULL);
}

// Called after lock() and allows the thread to be interrupted again (listens to SIGALRM)
extern void unlock()
{
    sigset_t set;
    sigemptyset(&set);
    sigaddset(&set, SIGALRM);
    sigprocmask(SIG_UNBLOCK, &set, NULL);
}

/*
	| if (thread == BLOCKED)
	| Block currently running thread
	| set target thread waitID to currently running thread
	| scheduler is called

	| if (thread == DEAD)
	| Get exit state of target if value_ptr isn't NULL
	| Clean target context
	| set target state to CLEAN
*/
// Pauses current execution thread until the target "thread" terminates
extern int pthread_join(pthread_t thread, void** value_ptr) {
  
    switch(tcb[thread].state) {
        case READY   :
        case RUNNING :
        case BLOCKED :

            tcb[global_thread_running].state = BLOCKED;
            tcb[thread].waitID = global_thread_running;

            /*
             * Will remain in the BLOCKED state until the target thread finishes (or terminates)
             * Eventually it will be sent back here to continue
            */
	        schedule_handler();
            
        case ZOMBIE:

            //Get the exit state of the target if value_ptr isn't NULL
            if (value_ptr) {  // 
                *value_ptr = tcb[thread].exit_state;
            }

            /*
             * Clean target context
            */
            free(tcb[thread].stack);
            tcb[thread].state = CLEAN;
            break;

        case CLEAN:
            printf("ERROR: Unknown Result\n");
            return 3; // errno ESRCH value (no existing specified process)
            break;
    }
    return 0;
}


// ******************************************************** Semaphores *******************************************


/*
	- Allocate mem space for my semaphore database
	- Initialize init value, and set value
	- Initialize Queue data structure (LL)
	- save the semaphore in the sem_t struct
*/
// Initialize unnamed semaphore referred to by sem
extern int sem_init(sem_t *sem, int pshared, unsigned value) 
{
    // Creating mem space for semaphore database to hold extra information needed by the semaphore
    struct sem_database* sem_ptr = (struct sem_database*) malloc(sizeof(struct sem_database));

    sem_ptr->value = value;
    sem_ptr->blockedQ = createQueue();
    sem_ptr->init = 1;

    //Save that semaphore within the sem_t struct
    sem->__align = (long) sem_ptr;
    
    return 0;
}

/*
	| if (sem is not > 0)  // nothing is waiting to run
	| BLOCK currently running thread
	| newly blocked thread is entered into the blocked thread queue
	| scheduler is called to get next READY thread to take old threads place

	\ else, decrement waiting value
	
	| if (woken up)
	| reset woken up value
*/
// Decrements (locks) the semaphore pointed to by sem
extern int sem_wait(sem_t *sem)
{
    struct sem_database* sem_ptr = (struct sem_database*)(sem->__align);

    // If the value is greater than 0 then decrement process proceeds
    if (sem_ptr->value <= 0) {
        tcb[global_thread_running].state = BLOCKED;
        add2Q(sem_ptr->blockedQ, global_thread_running);
        schedule_handler();
    }
    else {  // 
        (sem_ptr->value)--;
        return 0;
    }


    if (tcb[global_thread_running].wokenUp) {  // If semaphore is woken up then exit from sem_wait with return value 0
        tcb[global_thread_running].wokenUp = 0;
        return 0;
    }

    return -1;  // Will return with -1 indicating an error occured
}

/*
	| if (there are waiting threads)
	| remove last in thread from queue
	
	| if (queue isn't empty)
	| wakeup currently waiting thread by seting wokenup to 1
	| And set state to READY

	\ else, increment semephore if there is no waiting thread
*/
// increments (unlocks) the semaphore pointed to by sem
extern int sem_post(sem_t *sem) {
    struct sem_database* sem_ptr = (struct sem_database*)(sem->__align);

    if (sem_ptr->value >= 0) {
        pthread_t wait = removeFromQ(sem_ptr->blockedQ);  // Waiting thread ID (last in) or -1 if the queue is empty
        
        // If the queue isn't empty wake up the currently waiting thread
        if (wait != -1) {  
            tcb[wait].wokenUp = 1;
            tcb[wait].state = READY;
        }
        else {  // Increment semephore if there is no waiting thread
            (sem_ptr->value)++;
        }
        return 0;
    }
    
    return -1;  // Will return with -1 indicating an error occured
}

/*
	- checks if the semaphore has been initialized
	| if so then frees the "sem" from the sem_t struct
*/
// Destroys semaphore specified at the address pointed to by sem
extern int sem_destroy(sem_t *sem) {
    struct sem_database* sem_ptr = (struct sem_database*)(sem->__align);

    // Freeing the semaphore_database for this semaphore
    if (sem_ptr->init == 1) {
        free((void *)(sem->__align));   
    }
    else {
        return -1;
    }
    
    return 0;
}