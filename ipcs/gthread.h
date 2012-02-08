#ifndef _GTHREAD_H_ 
#define _GTHREAD_H_ 

enum threadstate {FINI, ACTIF, ATTENTE, INITIAL}; 
typedef void (func_t)(void *); 

struct sem {
    int jeton;
    struct thread *waitingCtx;
    struct thread *lastWaitingCtx;
};

struct thread { 
    void *esp; 
    void *ebp; 
    void *args; 
    void *stack; 
    enum threadstate etat; 
    func_t *f; 
    unsigned int thread_magic; 
    struct thread *next;
};

struct thread *current_thread;

unsigned int gsleep(unsigned int seconds);
void gthread_init();
void start_sched (void);
void switch_to_thread(struct thread *thread); 
void remove_current_thread();
void start_current_thread(void); 
void restart_thread(struct thread * restarting);
void stop_current_thread();
void yield(void);
void ordonnanceur(void);

int init_thread(struct thread *thread, int stack_size, func_t f, void *args); 
int create_thread(int stack_size, func_t f, void* args); 

void sem_init(struct sem *sem, unsigned int val);
void sem_up(struct sem *sem);
void sem_down(struct sem *sem);

void remove_Current_thread();
//ipcs : 
int events_init();
void check_events();


#endif 

