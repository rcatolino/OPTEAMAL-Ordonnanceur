#ifndef _GTHREAD_H_ 
#define _GTHREAD_H_ 

#include <stdint.h>
enum threadstate {FINI, ACTIF, ATTENTE, INITIAL, ANNULE}; 
typedef void * (func_t)(void *); 
typedef struct thread * gthread_t;

typedef struct sem {
    int jeton;
    struct thread *waitingCtx;
    struct thread *lastWaitingCtx;
} gsem_t;

struct thread { 
    void *esp; 
    void *ebp; 
    void *args; 
    void *stack; 
    enum threadstate etat; 
    func_t *f; 
    unsigned int thread_magic; 
    struct thread *next;
    uint32_t events;
};

struct thread *current_thread;

unsigned int gsleep(unsigned int seconds);
int init_thread(struct thread *thread, int stack_size, func_t f, void *args); 
void start_sched (void);
void switch_to_thread(struct thread *thread); 
void remove_current_thread();
void start_current_thread(void); 
void restart_thread(struct thread * restarting);
void stop_current_thread();
void remove_Current_thread();
void yield(void);
void ordonnanceur(void);

void gthread_init(gthread_t * main_thread);
//If the main_thread parameter is not NULL it is used to store main thread id,
//thus allowing to cancel it.
int gthread_create(gthread_t * thread, int stack_size, func_t f, void* args); 
int gthread_cancel(gthread_t thread);
//gthread_cancel allows only to cancel a active thread (including the calling one),
//it will return -1 if the thread was waiting, and won't return at all if the thread
//was the calling one.
void gsem_init(struct sem *sem, unsigned int val);
void gsem_take(struct sem *sem);
void gsem_give(struct sem *sem);

//ipcs : 
int events_init();
void check_events();

#endif 

