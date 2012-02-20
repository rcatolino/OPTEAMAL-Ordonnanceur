#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <assert.h>
#include <unistd.h>
#include <signal.h>
#include <time.h>
#include <errno.h>
#include <sys/time.h>
#include "gthread.h"
#include "gmem.h"
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

void * f_idle(void *args) {
//This thread is always active, therefore when all over threads are stopped she 
//can give cpu control back.
//Notice that since this thread is always active and we dont't have any priorities
//this thread is regularly scheduled to run, even if useful work could be done instead.
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
    return NULL;
}

void wakeUpThread(int signb, siginfo_t * infos, void * ucontext){
  int i = infos->si_value.sival_int;
  //i is the sleep id of the thread to be woken up. (see gsleep)
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
  //We use a static array of five elements to stock the contexts of a sleeping
  //thread. We can then have only five threads sleeping at a given time.
  int i;
  timer_t timerId;

  //find room in sleeping array :
  irq_disable();
  for (i=0; i<MAX_THREADS; i++){
    if (sleeping[i]==NULL){
      //Set up a timer to be woken up when sleeping time is over.
      //Each sleeping thread has an id (sigev_value.sival_int) that is used
      //to determine which context to wake up on timer expiration.
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
      //Store the context
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
    thread->f = f; 
    if (f==NULL && stack_size==0){
      //This thread is the main function, and therefore already has a valid context
      thread->etat=ACTIF;
    } else if (f!=NULL && stack_size!=0){
      thread->stack = gmalloc(stack_size); 
      if (!thread->stack) return 0; 
      //thread->stack is a pointer to the beginning of the stack, and we need
      //to store a pointer the end of the stack into the base pointer, therefore :
      thread->ebp = (void *)((char*)thread->stack + stack_size - 4); 
      //Why -4 ? No idea, it seems to be working fine without it.
      //At the thread creation the stack is empty, so the stack pointer=base pointer :
      thread->esp = (void *)((char*)thread->stack + stack_size - 4); 
      //the (char*) cast is there because of the pointer arithmetics,
      //we don't want to add 4*stack_size, only stack_size
      thread->etat=INITIAL; 
    } else {
      //No entry point specified, or no stak.
      errno=EINVAL;
      return -1;
    }
    thread->args = args; 
    thread->thread_magic = CTX_MAGIC; //I still don't know why this is useful
    DEBUG("Thread initialized\n");
    return 0;
} 


void switch_to_thread(struct thread *thread) 
{ 
    TRACE("switching to thread %p\n",thread);
    assert(thread->thread_magic==CTX_MAGIC);//May be this can be used to detect
    // buffer overflows? 
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
    TRACE("thread returned from entry point\n");
    current_thread->etat=FINI;
    gfree(current_thread->stack);
    remove_current_thread();
    ordonnanceur();
    exit(EXIT_SUCCESS);//this line should not be reached
} 

int gthread_create(gthread_t * thread, int stack_size, func_t f, void *args) 
{
  int ret=0;
  struct thread *new_thread = (struct thread *)gmalloc(sizeof(struct thread));
  if (thread!=NULL){
    *thread=new_thread;
  }
  TRACE("Context created : %p \n",new_thread);
  if (!new_thread){
    errno=EAGAIN;
    return -1;
  }

  ret = init_thread(new_thread, stack_size, f, args); 
  if (ret!=0){
    gfree(new_thread);
    errno=EINVAL;
    return -1;
  }
  if ( (!current_thread) && (!first_thread))/*Si aucun contexte deja cree */ 
  {
    new_thread->next = new_thread;
    first_thread = new_thread;
    last_thread = first_thread;
    prev_thread=NULL;
    if (f==NULL && stack_size==0){
      current_thread = new_thread;
    }
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

void start_sched (void) {
	start_hw();
	irq_disable();
	setup_irq(TIMER_IRQ, ordonnanceur);
	ordonnanceur();
	TRACE("start_sched\n");
}

/* Ordonnaceur basic (round robin) */
void ordonnanceur(void)
{
	struct thread * nextCtx = NULL;
  check_events();
	irq_disable();
	TRACE("----------------------------------------------------------Ordonnancement\n");
	
	if (current_thread) // Si on a un contexte courant
  //Since the idle thread creation,
  //we always have a current context. This test seems to be useless.
	  {
			switch(current_thread->etat){
				case INITIAL:

				case ACTIF:
          TRACE("current context active\n");
					nextCtx = current_thread->next;
          TRACE("switching to next one\n");
					prev_thread=current_thread;
          //In these cases we need to schedule the next thread to run,
          //and also to update prev_thread, so that when a thread is stopped
          //it can use prev_thread as entry point in the thread linked list
          //to choose the next thread to be shceduled.
					break;
				case ATTENTE:
          TRACE("current context stopped\n");
					nextCtx = prev_thread->next;
          //In that case the prev_thread is still the last active thread. Just
          //schedule the next one to be ran.
					break;
        case ANNULE:
				case FINI:
          TRACE("current context ended\n");
					nextCtx = prev_thread->next;//TODO
          //What's to be done? free thread context?
          gfree(current_thread);
					break;
				default:
					break;
			}
	  }
    else
    {
    	nextCtx = first_thread;
    }
	if(nextCtx != current_thread){
    TRACE("switching to nextCtx : %p\n",nextCtx);
		switch_to_thread(nextCtx);
  }
	irq_enable();
}

void gsem_init(struct sem *sem, unsigned int val){
	sem->jeton = val;
	sem->waitingCtx = NULL;
	sem->lastWaitingCtx = NULL;
}

//On prend un jeton
void gsem_take(struct sem *sem){

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
void gsem_give(struct sem *sem){
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
  //chain new thread at end of list
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

int gthread_cancel(gthread_t thread){
  gthread_t prev=NULL;
  irq_disable();
  if (thread==current_thread){
    gfree(thread->stack);
    thread->etat=ANNULE;
    remove_current_thread();
    ordonnanceur();
  } else {
    if (thread->etat==ATTENTE) {
      //implement cancelation of waiting thread would prove a litle much tricky,
      //and we don't need it in ghome. (actually we don't really need cancelation...)
      irq_enable(); 
      return -1;
    }
    //find previous thread in list :
    for(prev=thread; prev->next!=thread;prev=prev->next);
    //we should use a doubly-linked-list instead
    if( thread == first_thread ) 
    {
      first_thread = thread->next;
      last_thread->next = first_thread;
    }else if(current_thread == last_thread){ 
      prev->next = first_thread;
      last_thread = prev;
    }else{ 
      prev->next = thread->next;
    }
  }
  irq_enable();
  return 0;
}
void gthread_init(gthread_t * main_thread){
  /* start timer handler */
	struct sigaction sa;
  stack_t ss;
  current_thread=NULL;

  ss.ss_sp = gmalloc(SIGSTKSZ);
  ss.ss_size = SIGSTKSZ;
  ss.ss_flags = 0;
  if (sigaltstack(&ss, NULL) == -1){
    TRACE("Unable to allocate sigalt stack");
    sa.sa_flags = SA_SIGINFO;
  }else{
    sa.sa_flags = SA_ONSTACK | SA_SIGINFO;
  }
	sigemptyset(&sa.sa_mask);
	sa.sa_sigaction = wakeUpThread;
	sigaction(SIGUSR2, &sa, (struct sigaction *)0);

  events_init();
  gthread_create(main_thread,0,NULL,NULL); //turn the main flow of execution into a gthread
  DEBUG("Main thread created\n");
  gthread_create(NULL,16384,f_idle,NULL);//Create the idle thread
	start_sched(); 
}
