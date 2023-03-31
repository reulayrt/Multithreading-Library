// #include "ec440threads.h"
// #include <stdio.h>
// #include <string.h>
// #include <pthread.h>
// #include <unistd.h>
// #include <signal.h>
// #include <stdbool.h>
// #include <setjmp.h>
// #include <stdlib.h>

// #define JB_RBX 0
// #define JB_RBP 1
// #define JB_R12 2
// #define JB_R13 3
// #define JB_R14 4
// #define JB_R15 5
// #define JB_RSP 6
// #define JB_PC 7

// #define FREE     0x0 
// #define RUNNING  0x1
// #define READY    0x2

// struct TCB *head;
// head = NULL;

// struct TCB *global_thread_running;
// global_thread_running = NULL;

// unsigned int *threadID = 0; // The first thread (main) will have an id of 0
// int numOfThreads = 0; // Keep track of how many thread are created and closed

// // Using a doubly linked list for the TCB data structure to easily go back and fourth for Round Robin Scheduling
// struct TCB
// {
//     int state;  // Current thread status
//     pthread_t id;  // Stores current thread id
//     char* stack;  // Stack pointer
//     jmp_buf buf;  // stores Buffer information about registers

//     struct TCB *next;
//     struct TCB *prev;
// };

// // Initialize the first thread running main and start linked list
// static void thread_init() {
//     head = (struct TCB *) malloc (sizeof(struct TCB)); // assigning memory for head

//     // defining the head of the link list on the first thread created
//     head->id = threadID;  // This will be the id of the main() thread
//     head->stack = NULL;  // Don't need stack information for thead 0
//     setjmp(head->buf);  // Update registers on current location
//     head->state = RUNNING;


//     head->next = head;
//     head->prev = head;

//     /*  init           init           init
//       --------       --------       --------
//       | head |  <<<  | head |  >>>  | head |
//       --------       --------       --------
//                      ^^ current node
//     */
//     global_thread_running = head;

//     /*
//         After identifying the main() thread we can start counting for new 
//         threads that we create
//     */
//     threadID += 1;
//     numOfThreads += 1;

//     // Creating ALARM handler
//     struct sigaction safe;
//     sigemptyset(&safe,sa_mask);
//     safe.sa_flags = SA_NODEFER; // SA_NODEFER will stop ALARM from being blocked, via man page
//     safe.sa_handler = schedule_handler;
//     sigaction(SIGALRM, &safe, NULL);

//     int period = 50 * 1000;
//     ualarm(period, period); // first alarm goes off at 50ms then will repeated go off at 50ms intervals

// }

// static void schedule_handler(int signal) {  // $$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$ try real location if not working $$$$$$$$$$$$$$$$$$$ also try without input 

//     // setjmp will return 0 on first call (then can return whatever specified in the longjmp that points to it)
//     if(!setjmp(global_thread_running->buf)) {


//         if (global_thread_running->state != FREE) {
//             global_thread_running->state = READY;
//         }

//         int count = 0;
//         // There can only be 128 thread as per assignment instructions
//         while(count < 128) {  // Round Robin scheduling method
//             count += 1;
//             global_thread_running = global_thread_running->next;
//             if (global_thread_running->state == READY) {
//                 break;
//             }
//         }

//         global_thread_running->state = RUNNING;

//         /*
//             Jumping to restore the next thread and setting the value for setjmp to return 1
//         */
//         longjmp(global_thread_running->buf, 1);
//     }
// }

// int pthread_create (
//     pthread_t thread,
//     const pthread_attr_t *attr,
//     void *(*start_routine) (void*),
//     void *arg ) 
// {
//     static bool start = true;
//     if (start) {  // check if this is the first call of pthread
//         start = false;
//         *thread = threadID;  // Changes whatever the current thead id is to my defined threadID for easy tracking
//         thread_init();
//     }

//     // Create new thread
//     struct TCB *tcb = (struct TCB *) malloc (sizeof(struct TCB));

//     tcb->id = threadID;
//     tcb->stack = malloc(32767);  // 32,767 is the specified stack size in assignment
//     tcb->state = READY;

//     *thread = threadID;  // Changes whatever the current thead id is to my defined threadID for easy tracking
//     threadID += 1;

//     /* 
//      * Creating new node in the TCB doubly linked list for the new thread I just initialized.

//      * Since "head" is globaly defined as NULL there will always be a head at the front of the 
//      * linked list that gets initialized in the thread_init() function and a head at the end
//      * of the linked list that maintains the global definition of NULL. This ultimately means
//      * that the final head in the linked list is just a NULL node signifying the end of the list.
//     */
//     struct TCB *update = head->prev;
//     update->next = tcb;
//     tcb->prev = update;
//     tcp->next = head;
//     head->prev = tcb;

//     numOfThreads += 1;

//     void* sp = malloc(32767);
//     tcb->stack = sp;
//     sp += 32767 - 8;  // $$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$ base = base + THREAD_STACK_SIZE - 8;  // If it were a 32-bit machine it would be -4
//     *(unsinged long int*) sp = (unsigned long int) &pthread_exit;  // stack goes from the top down

//     setjmp(tcb->buf);  // Saving new registers into jmpbuf

//     // Saving registers to jmpbuf
//     tcb->buf[0].__jmpbuf[JB_PC] = ptr_mangle((unsigned long int) start_thunk);  // $$$$$$$$$$$$$$$$$$$ comment these
//     tcb->buf[0].__jmpbuf[JB_RSP] = ptr_mangle((unsigned long int) sp);
//     tcb->buf[0].__jmpbuf[JB_R13] = (unsigned long int) arg;
//     tcb->buf[0].__jmpbuf[JB_R12] = (unsigned long int) start_routine;

//     return 0; // Returns 0 on completion
// }

// // returns the id of the currently running thread
// pthread_t pthread_self() {
//     return global_thread_running->id;
// }


// void pthread_exit() {
//     global_thread_running->state = FREE;
//     numOfThreads--;

//     free(global_thread_running->stack);

//     if (numOfThreads < 1) {  // $$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$ !numOfThreads
//         exit(0);  // terminates program with 0 returned to OS to indicate successful complettion
//     }
//     /*
//        There are more threads to be handled by the scheduler before terminating program.
//        Other thread could still have jobs to complete and need to be scheduled accordingly.
//     */
//     else {
//         schedule_handler(1);
//     }

//     // $$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$ you can test this by commenting it out
//      __builtin_unreachable();  // Tells compilers this location is unreachable to suppress a -Werror warning
// }


// ***************************************************************





#ifndef QUEUE
#define QUEUE

#include <pthread.h>
#include <stdlib.h> 
#include <signal.h> 
#include <stdio.h>

// FIRST IN LAST OUT QUEUE

// Node for Linked List to store entry
struct Queue_Thread { 
    pthread_t key; 
    struct Queue_Thread* next; 
}; 
 
/*
 * Using a linked list data structure to hold blocked thread queue 
*/
struct Queue { 
    struct Queue_Thread *head;
    struct Queue_Thread *back; 
}; 

// Builds new node for Linked List
struct Queue_Thread* addNode(pthread_t kee) { 

    // worker is a temp var to define new Linked List node
    struct Queue_Thread* worker = (struct Queue_Thread*) malloc (sizeof(struct Queue_Thread)); 
    worker->key = kee; 
    worker->next = NULL; 
    return worker; 
} 

// initializes an empty "queue" (Linked List)
struct Queue* createQueue() { 
    struct Queue* line = (struct Queue*) malloc (sizeof(struct Queue)); 
    line->head = line->back = NULL; 
    return line; 
} 

// Creates a key for node of the blocked thread 
void add2Q(struct Queue* line, pthread_t key)  { 

    struct Queue_Thread* new_node = addNode(key);  // Initializing new node
  

    if (line->back == NULL) {  // If there is nothing already in the queue then the new node is the head and back of Linked List
        line->head = line->back = new_node; 
        return; 
    } 
  
    // Making a ring structure
    line->back->next = new_node; 
    line->back = new_node; 
}

// Deletes a key from the blocked thread queue (Linked List) 
pthread_t removeFromQ(struct Queue* line) { 

    if (line->head == NULL) {  // Check if queue is empty (if so return NULL)
        return (pthread_t) -1; 
    }
  
    struct Queue_Thread* worker = line->head;  // Old head
  
    line->head = line->head->next;  // defining new head 
  
    // If head becomes NULL, then change rear also as NULL 
    if (line->head == NULL) {  // If the head is NULL then the back will also be NULL
        line->back = NULL; 
    }
  
    return worker->key;
} 



#endif