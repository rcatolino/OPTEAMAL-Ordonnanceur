#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <assert.h>
#include <unistd.h>
#include <signal.h>
#include <sys/time.h>
#include "gthread.h"
#include "hw.h"
  
#define TRACE(...) //printf(__VA_ARGS__)
#define RETURN_SUCCESS 0
#define RETURN_FAILURE 1
#define EXIT_SUCCESS 0 
#define CTX_MAGIC 0xDEADBEEF
 
struct thread *first_thread = NULL;
struct thread *current_thread = NULL;
struct thread *prev_thread = NULL;
struct thread *last_thread = NULL;

void f_idle(void *args)//TODO
{
    while(1)
    {
      TRACE("idle %d\n") ;
    }
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
    if (current_thread->etat == INITIAL) 
    { 
        start_current_thread(); 
    } 
}  

void start_current_thread(void) 
{ 
    current_thread->etat=ACTIF;
    irq_enable();
    current_thread->f(current_thread->args);   
    current_thread->etat=FINI; 
    free(current_thread->stack);
    remove_Current_thread();
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
	irq_disable();
	TRACE("----------------------------------------------------------Ordonnancement\n");
	
	if ( first_thread )// Si contextes dans la liste active 
	{
	if (current_thread) // Si on a un contexte courant
	  {
			switch(current_thread->etat){
				case INITIAL:

				case ACTIF:
					nextCtx = current_thread->next;
					prev_thread=current_thread;
					break;
				case ATTENTE:
					nextCtx = prev_thread->next;
					break;
				case FINI:
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
		switch_to_thread(nextCtx);
	}
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
		remove_Current_thread();
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

// On supprimme le contexte courant de la liste chainée des taches actives
void remove_Current_thread(){
	struct thread * next_of_current_thread = current_thread->next;
	// On retire thread de la liste des thread actifs
	if(current_thread == next_of_current_thread)//Si il n'y avait qu'une tache active (useless with idle)
	{
			//useless with idle
	}else{ //Si il y a d'autres taches actives
		if( current_thread == first_thread ) //Si c'est le premier à retirer de la liste des taches actives
		{
			if(current_thread == last_thread){ //Si c'est aussi le dernier aussi à retirer
				//useless with idle
			}else{
				first_thread = next_of_current_thread;
				last_thread->next = first_thread;
				prev_thread = last_thread;
			}
		}else{
			if(current_thread == last_thread){ //Si c'est le dernier à retirer et pas le premier
				prev_thread->next = first_thread;
				last_thread = prev_thread;

			}else{ //Si celui à retirer est au milieu
				prev_thread->next = next_of_current_thread;
			}
		}
	}
}










