#ifndef _GTHREAD_H_ 
#define _GTHREAD_H_ 

enum threadtate {FINI, ACTIF, ATTENTE, INITIAL}; 
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
    enum threadtate etat; 
    func_t *f; 
    unsigned int thread_magic; 
    struct thread *next;
};

unsigned int gsleep(unsigned int seconds);
void gthread_init();
void start_sched (void);
void switch_to_thread(struct thread *thread); 
void start_current_thread(void); 
void yield(void);
void ordonnanceur(void);

int init_thread(struct thread *thread, int stack_size, func_t f, void *args); 
int create_thread(int stack_size, func_t f, void* args); 

void sem_init(struct sem *sem, unsigned int val);
void sem_up(struct sem *sem);
void sem_down(struct sem *sem);

void remove_Current_thread();

#endif 

