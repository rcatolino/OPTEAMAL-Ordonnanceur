#include <stdio.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <mqueue.h>
#include <stdlib.h>
#include <unistd.h>
#include "gthread.h"
#include "ipcs.h"

struct sem *semaphore;
static mqd_t mtest;

void f_ping(void *args) 
{ 
  gsleep(2);
  printf("sending msg\n");
  if (gmq_send(mtest,"lol",4,0)==-1){
    perror("mq_send ");
    return ;
  }
    while(1)
    { 
      //printf("A\n") ;
      //printf("B\n") ;
      //printf("C\n") ;
      printf("Going to sleep\n");
      gsleep(1);
      printf("Waking up, sem_down\n");
      sem_down(semaphore);
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
    gsleep(2);
    sem_up(semaphore);
    //printf("3\n") ;
    
  } 
}

void f_poong(void *args) 
{ 
  char msg[10];
  while (1){
    printf("Call to mq_receive \n");
    if (gmq_receive(mtest,msg,10,0)==-1){
      perror("poong mq_receive ");
      return ;
    }
    gsleep(2);
  }
}


int main ( int argc, char *argv[]){
	
  struct mq_attr attrs = {
    .mq_maxmsg=10, //beyond 10 msgs one might need root acces
    .mq_msgsize=10,
  };
  mode_t mqMode= S_IRWXO; //Allows everything for everyone
	printf("START\n") ;
	
  gthread_init();
  mq_unlink("/mqtest");
	mtest=gmq_open( "/mqtest" , O_EXCL|O_RDWR|O_CREAT,\
      mqMode, &attrs);
  if (mtest==-1){
    perror("mq_open ");
    return -1;
  }
  
  
	semaphore = (struct sem *)malloc(sizeof(struct sem));
	printf("Before init\n") ;
	sem_init(semaphore, 1);
	printf("After init\n") ;
	
	create_thread(16384, f_pong, NULL);
	create_thread(16384, f_ping, NULL);
	create_thread(16384, f_poong, NULL);
	start_sched(); 
  
  mq_unlink("/mqtest");
	exit(EXIT_SUCCESS); 
}
