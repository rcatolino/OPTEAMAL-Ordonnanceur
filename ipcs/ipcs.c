//Module providing several ipcs to use with the gthread API
#include "gthread.h"
#include "hw.h"
#include "ipcs.h"
#include <stdlib.h>
#include <errno.h>
#include <sys/epoll.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <mqueue.h>

struct event {
  struct event * ev_next;
  struct epoll_event ev; 
  struct thread * waiting;
};
static struct event * ev_first=NULL;
static int epolld=0;

int events_init(){
  epolld = epoll_create1(0);
  if (epolld){
    TRACE("epoll instance creation failed\n");
    return 1;
  }
  return 0;
}

int register_event(int fd, uint32_t events){
  struct event * new_ev = malloc(sizeof(struct event));
  struct event * i;
  int ret=0;
  new_ev->ev.events=events | EPOLLET;
  new_ev->ev.data.fd=fd;
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
  oflag|=O_NONBLOCK;
  return mq_open(name,oflag,mode,attr);
}

int gmq_send(mqd_t mqdes, const char * msg_ptr, size_t msg_len, \
    unsigned int msg_prio) {
  int ret=0;
  ret = mq_send(mqdes, msg_ptr, msg_len, msg_prio);
  if(ret==0) return 0;
  if (errno!=EAGAIN){
    //register new event
    return register_event(mqdes,EPOLLIN);
  }
  return 0;
}

