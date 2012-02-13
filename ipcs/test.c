#include <stdio.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <mqueue.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include "tcpserver.h"
#include "gthread.h"
#include "gmem.h"
#include "ipcs.h"

struct sem *semaphore;
static mqd_t mtest;
static int socketServer;
static int socketClient;

void * f_ping(void *args) 
{ 
  int i=0; 
    while(1)
    { 
      //printf("A\n") ;
      //printf("B\n") ;
      //printf("C\n") ;
      gsleep(2);
      if (i<3){
        printf("sending msg\n");
        if (gmq_send(mtest,"coucou",7,0)==-1){
          perror("mq_send ");
          return NULL;
        }
        i++;
      }
    }
    printf("FINI\n");
   return NULL;
} 

void * f_pong(void *args) 
{ 
  char buff[11];
  int ret;
  printf("f_pong\n") ;
  for(;;){
    socketClient=waitClient(socketServer);
    if (socketClient==-1) {
      perror("wait client");
      return NULL;
    }
    printf("Client connected\n");
    gsem_give(semaphore);
    while(1)
    { 
      ret=grecv(socketClient,buff,10,0);
      if (ret==-1){
        perror("grecv");
        break;
      }
      if (ret==0){
        break;
      }
      buff[ret]='\0';
      printf("%s",buff);
    } 
    printf("Client deconnected\n");
  }
  return NULL;
}

void * f_poong(void *args) 
{ 
  char msg[10];
  while (1){
    printf("Call to mq_receive\n");
    if (gmq_receive(mtest,msg,10,0)==-1){
      perror("poong mq_receive ");
      return NULL;
    }
    printf("%s\n",msg);
  }
  return NULL;
}
void * f_pouet(void * args){
  int ret;
  char buff[90];
  for(;;){
    //Wait for a client :
    gsem_take(semaphore);
    printf("Client waiting for imput...\n");
    for (ret=0;ret!=-1;){
      scanf("%80s",buff);
      buff[80]='\0';
      gsend(socketClient,buff,strlen(buff),0);
      if (ret==-1){
        perror("grecv");
        break;
      }
      gsend(socketClient,"\n",2,0);
      if (ret==-1){
        perror("grecv");
        break;
      }
    }
  }
  return NULL;
}

int main ( int argc, char *argv[]){
	
  struct mq_attr attrs = {
    .mq_maxmsg=10, //beyond 10 msgs one might need root acces
    .mq_msgsize=10,
  };
  mode_t mqMode= S_IRWXO; //Allows everything for everyone
	printf("START\n") ;
  mq_unlink("/mqtest");

  gthread_init();

	mtest=gmq_open( "/mqtest" , O_EXCL|O_RDWR|O_CREAT,\
      mqMode, &attrs);
  if (mtest==-1){
    perror("mq_open ");
    return -1;
  }
	semaphore = (struct sem *)gmalloc(sizeof(struct sem));
	gsem_init(semaphore, 0);
	
  socketServer=initServer(1337);
  printf("Server initialized\n");
  if (socketServer==-1) {
    perror("initServer");
    return 0;
  }
	gthread_create(NULL,16384, f_pong, NULL);
	gthread_create(NULL,16384, f_pouet, NULL);
	//gthread_create(16384, f_ping, NULL);
	//gthread_create(16384, f_poong, NULL);
  printf("COUCOUCOUCOU\n");
  gsleep(1500);
  mq_unlink("/mqtest");
  return 0;
}
