#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <assert.h>
#include <unistd.h>
#include <signal.h>
#include <time.h>
#include <sys/time.h>
#include "gthread.h"
#include "hw.h"
  
#define RETURN_SUCCESS 0
#define RETURN_FAILURE 1
#define EXIT_SUCCESS 0 
#define CTX_MAGIC 0xDEADBEEF
#define MAX_THREADS 5
 
struct thread *first_thread = NULL;
struct thread *prev_thread = NULL;
struct thread *last_thread = NULL;
static struct thread * sleeping[MAX_THREADS] = {NULL};

void f_idle(void *args)
{
  struct timespec time = {
    .tv_sec=0,
    .tv_nsec=TM_FREQ*1000,
  };
    while(1)
    {
      TRACE("idle \n") ;
      nanosleep(&time,NULL);
      TRACE("waking up\n");
    }
}

void wakeUpThread(int signb, siginfo_t * infos, void * ucontext){
  int i = infos->si_value.sival_int;
  irq_disable();
  if (sleeping[i]==NULL){
    irq_enable();
    return;
    //Big hack, just forget you saw this block. Please.
  }
  TRACE("This is a wake up call!, sleep id : %d\n",i);
  TRACE("current_thread : %p, last_thread : %p, sleeping thread : %p\n"\
      ,current_thread,last_thread,sleeping[i]);
  sleeping[i]->etat = ACTIF;
  last_thread->next = sleeping[i];
  last_thread = last_thread->next;
  last_thread->next = first_thread;			
  sleeping[i]=NULL;

	irq_enable();
}
unsigned int gsleep(unsigned int seconds){
  //find room in sleeping :
  int i;
  timer_t timerId;

  irq_disable();
  for (i=0; i<MAX_THREADS; i++){
    if (sleeping[i]==NULL){
      /* set timer */ 
      //struct itimerval value;
      struct sigevent evp = {
        .sigev_notify=SIGEV_SIGNAL,
        .sigev_signo=SIGUSR2,
        .sigev_value.sival_int=i,
      };
      struct itimerspec timerValue = {
        .it_interval.tv_sec=0,
        .it_interval.tv_nsec=0,
        .it_value.tv_sec=seconds,
        .it_value.tv_nsec=0,
      };
      TRACE("Going to sleep with id : %d!\n",i);
      timer_create(CLOCK_REALTIME,&evp,&timerId);
      timer_settime(timerId,0,&timerValue,NULL);

      //Put the thread in sleeping mode :
      remove_current_thread();
      current_thread->etat = ATTENTE;
      sleeping[i]=current_thread;
      ordonnanceur();
      return 0;
    }
  }
  irq_enable();
  return -1;
}
/* Initialisation du contexte d'execution associee a f*/
int init_thread(struct thread *thread, int stack_size, func_t f, void *args) 
{ 	
    thread->stack = malloc(stack_size); 
    if (!thread->stack) return 0; 
    thread->esp = (void *)((unsigned char*)thread->stack + stack_size - 4    ); 
    thread->ebp = (void *)((unsigned char*)thread->stack + stack_size - 4    ); 
    thread->f = f; 
    thread->args = args; 
    thread->etat=INITIAL; 
    thread->thread_magic = CTX_MAGIC;
    return 0;
} 


void switch_to_thread(struct thread *thread) 
{ 
    assert(thread->thread_magic==CTX_MAGIC); 
    assert(thread->etat!=FINI); 
    if (current_thread) /* Si il y a un contexte courant */
        asm("movl %%esp, %0" "\n" "movl %%ebp, %1" 
            :"=r"(current_thread->esp), 
            "=r"(current_thread->ebp)  
        );
    current_thread=thread; 
    asm("movl %0, %%esp" "\n" "movl %1, %%ebp" 
        : 
        :"r"(current_thread->esp), 
         "r"(current_thread->ebp) 
    ); 
    TRACE("switching done\n");
    if (current_thread->etat == INITIAL) 
    { 
        TRACE("starting new thread\n");
        start_current_thread(); 
    } 
}  

void start_current_thread(void) 
{ 
    current_thread->etat=ACTIF;
    irq_enable();
    TRACE("calling thread entry point\n");
    current_thread->f(current_thread->args);   
    TRACE("thread returned from main function\n");
    current_thread->etat=FINI; 
    free(current_thread->stack);
    remove_current_thread();
    ordonnanceur();
    exit(EXIT_SUCCESS); 
} 

int create_thread(int stack_size, func_t f, void *args) 
{
    struct thread *new_thread = (struct thread *)malloc(sizeof(struct thread));
    if (! new_thread) return 0;

    init_thread(new_thread, stack_size, f, args); 

    if ( (!current_thread) && (!first_thread))/*Si aucun contexte deja cree */ 
    { 
		new_thread->next = new_thread;
		first_thread = new_thread;
		last_thread = first_thread;
		prev_thread=NULL;
    } 
    else 
    {
        new_thread->next = first_thread; 
        last_thread->next = new_thread;
        last_thread = new_thread;
    }

    return 0;
} 

void yield(void) 
{ 
	ordonnanceur();
	/*
    if (current_thread) // Si on a un contexte courant
        switch_to_thread(current_thread->next); 
    else 
        switch_to_thread(first_thread);
    */
} 

void start_sched (void)
{
	start_hw();
	irq_disable();
	setup_irq(TIMER_IRQ, ordonnanceur);
	ordonnanceur();
	TRACE("start_sched\n");
}

/* Ordonnaceur basic (on prend betement les fonctions a la suite */
void ordonnanceur(void)
{
	struct thread * nextCtx = NULL;
  check_events();
	irq_disable();
	TRACE("----------------------------------------------------------Ordonnancement\n");
	
	if (current_thread) // Si on a un contexte courant
	  {
			switch(current_thread->etat){
				case INITIAL:

				case ACTIF:
          TRACE("current context active\n");
					nextCtx = current_thread->next;
          TRACE("switching to next one\n");
					prev_thread=current_thread;
					break;
				case ATTENTE:
          TRACE("current context stopped\n");
					nextCtx = prev_thread->next;
					break;
				case FINI:
          TRACE("current context ended\n");
					nextCtx = prev_thread->next;//TODO
					break;
				default:
					break;
			}
	  }
    else
    {
    	nextCtx = first_thread;
    }
	if(nextCtx != current_thread)
    TRACE("switching to nextCtx : %p\n",nextCtx);
		switch_to_thread(nextCtx);
	irq_enable();
}

void sem_init(struct sem *sem, unsigned int val){
	sem->jeton = val;
	sem->waitingCtx = NULL;
	sem->lastWaitingCtx = NULL;
}

//On prend un jeton
void sem_up(struct sem *sem){

	irq_disable();
	
	if(sem->jeton > 0){//Si un jeton est disponible
		sem->jeton--;
	}else{ //Mise en attente de la tache
		remove_current_thread();
		current_thread->etat = ATTENTE;

		// On insert thread dans la liste d'attente sur le semaphore
		if(!sem->lastWaitingCtx) // Si aucun contexte en attente
		{
			sem->waitingCtx = current_thread;
			sem->waitingCtx->next = NULL;
			sem->lastWaitingCtx = sem->waitingCtx;
		}else{
			//On rajoute le thread en fin de liste d'attente
			sem->lastWaitingCtx->next = current_thread;
			sem->lastWaitingCtx = current_thread;
			sem->lastWaitingCtx->next = NULL;
		}
		ordonnanceur();
	}
}

// Donner un jeton
void sem_down(struct sem *sem){
	irq_disable();
	sem->jeton++;
	
	while(sem->jeton > 0 && sem->waitingCtx) //une tache peut prendre un jeton
	{
		sem->jeton--;
		sem->waitingCtx->etat = ACTIF;
		
		// mise a jour de la liste des thread en attente sur sem
		if( sem->waitingCtx->next == NULL)
			sem->lastWaitingCtx = NULL;
				
		// on insert la tache a la fin des taches actives
		last_thread->next = sem->waitingCtx;
		sem->waitingCtx = sem->waitingCtx->next;
		last_thread = last_thread->next;
		last_thread->next = first_thread;			
	}
	irq_enable();
}
void restart_thread(struct thread * restarting){

  irq_disable();
  restarting->etat = ACTIF;
  last_thread->next = restarting;
  last_thread = last_thread->next;
  last_thread->next = first_thread;			
  irq_enable();

}
void stop_current_thread(){
  irq_disable();
  remove_current_thread();
  current_thread->etat = ATTENTE;
  irq_enable();
}
// On supprimme le contexte courant de la liste chainée des taches actives
void remove_current_thread(){
	struct thread * next_of_current_thread = current_thread->next;
	// On retire thread de la liste des thread actifs
  if( current_thread == first_thread ) //Si c'est le premier à retirer de la liste des taches actives
  {
    first_thread = next_of_current_thread;
    last_thread->next = first_thread;
    prev_thread = last_thread;
  }else if(current_thread == last_thread){ //Si c'est le dernier à retirer et pas le premier
    prev_thread->next = first_thread;
    last_thread = prev_thread;
  }else{ //Si celui à retirer est au milieu
    prev_thread->next = next_of_current_thread;
  }
}

void gthread_init(){
  //This function is only used for the sleep implementation
  /* start timer handler */
	struct sigaction sa;
  stack_t ss;
  current_thread=NULL;

  ss.ss_sp = malloc(SIGSTKSZ);
  ss.ss_size = SIGSTKSZ;
  ss.ss_flags = 0;
  if (sigaltstack(&ss, NULL) == -1){
    TRACE("Unable to allocate sigalt stack");
  }
	sigemptyset(&sa.sa_mask);
	sa.sa_sigaction = wakeUpThread;
	sa.sa_flags = SA_ONSTACK | SA_SIGINFO;
	sigaction(SIGUSR2, &sa, (struct sigaction *)0);

  events_init();
  create_thread(20384,f_idle,NULL);
}
