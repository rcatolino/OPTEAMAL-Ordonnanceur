#include "gthread.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

struct sem *semaphore;

void f_ping(void *args) 
{ 
	int i =0;
	int j =0;
    while(1)
    { 
      //printf("A\n") ;
      //printf("B\n") ;
      //printf("C\n") ;
      i++;
      if(i>994000){
    	  i=0;
    	  printf("sem_down\n") ;
    	  sem_down(semaphore);
          if(j>3){
        	  j=0;
          }
          j++;
      }
      sleep(5);
    }
    printf("FINI\n");
} 

void f_pong(void *args) 
{ 
  while(1)
  { 
    //printf("1\n") ;
    //printf("2\n") ;
    printf("sem_up 1\n") ;
    sem_up(semaphore);
    //printf("3\n") ;
    
  } 
}

void f_poong(void *args) 
{ 
    while(1)  
    { 
      printf("sem_up 2\n") ;
      sem_up(semaphore);
      //printf("$\n") ;
      //printf("#\n") ;
      //printf("@\n") ;
    } 
}

int main ( int argc, char *argv[]){
	
	printf("START\n") ;
	printf("START\n") ;
	
	semaphore = (struct sem *)malloc(sizeof(struct sem));
	printf("Before init\n") ;
	sem_init(semaphore, 1);
	printf("After init\n") ;
	
	create_thread(16384, f_pong, NULL);
	create_thread(16384, f_ping, NULL);
	create_thread(16384, f_poong, NULL);
	start_sched(); 
	exit(EXIT_SUCCESS); 
}
