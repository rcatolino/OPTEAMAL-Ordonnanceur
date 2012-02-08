//Module providing several ipcs to use with the gthread API
#define _GNU_SOURCE
#include "gthread.h"
#include "hw.h"
#include "ipcs.h"
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <sys/epoll.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <signal.h>
#include <mqueue.h>
#include <unistd.h>

struct event {
  struct event * ev_next;
  struct epoll_event ev; 
  struct thread * waiting;
};
static struct event * ev_first=NULL;
static int epolld=0;

void ioEvent(int signum){
  DEBUG("io event!\n");
}
void check_events(){
  struct epoll_event evs;
  int ret;
  ret=epoll_wait(epolld,&evs,1,0);
  if (ret==-1){
    perror("epoll_wait ");
    return;
  }
  if (ret==0){
    //no event have happened
    return;
  }
  DEBUG("An event has occured : %d, data : %p\n",evs.events,evs.data.ptr);
  DEBUG("first event : %p\n",ev_first);
  DEBUG("thread pointer : %p\n",((struct event*)(evs.data.ptr))->waiting);
  //DEBUG("thread state : %d\n",((struct event*)(evs.data.ptr))->waiting->etat);
}
int events_init(){
	struct sigaction sa;
  sigemptyset(&sa.sa_mask);
	sa.sa_handler = ioEvent;
	sa.sa_flags = 0 ;
	sigaction(SIGIO, &sa, (struct sigaction *)0);

 
  epolld = epoll_create1(0);
  if (epolld==-1){
    DEBUG("epoll instance creation failed\n");
    return 1;
  }
  return 0;
}

int register_event(int fd, uint32_t events){
  struct event * new_ev = malloc(sizeof(struct event));
  struct event * i;
  int ret=0;
  new_ev->ev.events=events | EPOLLET | EPOLLONESHOT ;
  new_ev->ev.data.ptr=new_ev;
  DEBUG("registering pointer : %p\n",new_ev);
  new_ev->waiting = current_thread;
  //chain the event in first position:
  new_ev->ev_next=ev_first;
  ev_first=new_ev;
  //Is this file descriptor registered already?
  for (i=ev_first->ev_next; i!=NULL; i=i->ev_next){
    if (i->ev.data.fd==fd && (i->ev.events & events)==0){
      new_ev->ev.events|=events; // Add the new event to watch
      ret=epoll_ctl(epolld,EPOLL_CTL_MOD,fd,&(new_ev->ev)); //And register it.
      //In the case where the new event is already watched, there is nothing
      //more to do with epoll.
      return ret;
    }
  }
  if (i==NULL){
    //This file descriptor is not watched yet, register it.
    ret=epoll_ctl(epolld,EPOLL_CTL_ADD,fd,&(new_ev->ev));
  }
  return ret;
}

mqd_t gmq_open(const char * name, int oflag, mode_t mode, \
    struct mq_attr * attr){
  mqd_t ret;
  oflag|=O_NONBLOCK;
  ret = mq_open(name,oflag,mode,attr);
  if (ret==-1) return ret;
  if (fcntl(ret,F_SETFL,O_ASYNC | O_NONBLOCK)==-1) perror("fcntl SETFL");
  if (fcntl(ret,F_SETOWN,getpid())==-1) perror("fcntl SETOWN ");
  if (fcntl(ret,F_SETSIG,0)==-1) perror("fcntl SETSIG");
  return ret;
}

int gmq_send(mqd_t mqdes, const char * msg_ptr, size_t msg_len, \
    unsigned int msg_prio) {
  int ret=0;
  ret = mq_send(mqdes, msg_ptr, msg_len, msg_prio);
  if(ret==0) return 0;
  if (errno==EAGAIN){
    DEBUG("mq_send would block, register it\n");
    //register new event
    return register_event(mqdes,EPOLLOUT);
  }
  return ret;
}
ssize_t gmq_receive(mqd_t mqdes, char * msg_ptr, size_t msg_len,\
   unsigned int * msg_prio){
  int ret=0;

  ret = mq_receive(mqdes, msg_ptr, msg_len, msg_prio);
  if(ret==0) return 0; //no need to wait
  DEBUG("mq_receive ret = %d\n",ret);
  if (errno==EAGAIN){
    DEBUG("mq_receive would block, register it\n");
    //register new event
    return register_event(mqdes,EPOLLIN);
  }
  return ret;
}
