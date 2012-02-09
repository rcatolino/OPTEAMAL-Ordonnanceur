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

struct file_watched {
  int fd;
  struct epoll_event ev;
  struct thread * first_listener;
  struct file_watched * next;
};
static struct file_watched * first_file=NULL;
static int epolld=0;

void ioEvent(int signum){
  DEBUG("io event!\n");
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
int remove_file_listener(struct file_watched * file){
  //Remove first thread from listeners :
  struct epoll_event ev;
  file->first_listener=file->first_listener->next; //The first thread need to be stoped!!
  if (file->first_listener==NULL){
    return epoll_ctl(epolld,EPOLL_CTL_DEL,file->fd,&ev);
  }
  return 0;
}
int add_file_listener(struct file_watched * file, uint32_t events){
  int ret;
  //add the current thread to the threads pending for this event :
  current_thread->next=file->first_listener;//The current thread needs to be removed!!!
  file->first_listener=current_thread; 
  //Update the events watched on this file : 
  DEBUG("Adding file listener\n");
  if (file->first_listener->next==NULL){
    //This file descriptor is not registered with epoll yet :
    file->ev.events=events | EPOLLET; // Add the new event to watch
    ret=epoll_ctl(epolld,EPOLL_CTL_ADD,file->fd,&(file->ev)); //And register it.
    DEBUG("epoll_ctl ret : %d, fd : %d\n",ret,file->fd);
    return ret;
  }
  if ((file->ev.events & events)==0){
    //this event is not beeing watched yet :
    file->ev.events|=events; // Update the events to watch
    ret=epoll_ctl(epolld,EPOLL_CTL_MOD,file->fd,&(file->ev));
    return ret;
  } else {
    //In the case where the new event is already watched, there is nothing
    //more to do with epoll.
    file->ev.events|=events; 
    return 0;
  }
}
int register_event(int fd, uint32_t events){

  struct file_watched * i;
  int ret;
  struct file_watched * new_file;
  //Is the file descriptor in the file list already?
  for (i=first_file; i!=NULL && i->fd==fd; i=i->next);
  if (i==NULL){
    DEBUG("Watching new file : %d\n",fd);
    //This file is beeing wathed for the fisrt time, add it in linked list
    new_file=malloc(sizeof(struct file_watched));
    new_file->fd=fd;
    new_file->ev.data.ptr=new_file; //used when an event is trigered on this fd
    new_file->next=first_file;
    first_file=new_file;
    ret = add_file_listener(new_file,events);
  } else {
    //This file is already beeing watched :
    ret = add_file_listener(i,events);
  }
  /*
  struct event * new_ev = malloc(sizeof(struct event));
  struct event * i;
  int ret=0;
  new_ev->ev.events=events | EPOLLET;
  new_ev->ev.data.fd=fd;
  DEBUG("registering pointer : %p\n",new_ev);
  new_ev->waiting = current_thread;
  //chain the event in first position:
  new_ev->ev_next=ev_first;
  ev_first=new_ev;
  //Is this file descriptor registered already?
  for (i=ev_first->ev_next; i!=NULL; i=i->ev_next){
    if (i->ev.data.fd==fd && (i->ev.events & events)==0){
      DEBUG("file descriptor already watched, for another event\n");
      new_ev->ev.events|=events; // Add the new event to watch
      ret=epoll_ctl(epolld,EPOLL_CTL_MOD,fd,&(new_ev->ev)); //And register it.
      return ret;
    } else if (i->ev.data.fd==fd){
      DEBUG("file descriptor watched already for this event\n");
      //In the case where the new event is already watched, there is nothing
      //more to do with epoll.
      return ret;
    }
  }
  if (i==NULL){
    DEBUG("file descriptor not watched yet\n");
    //This file descriptor is not watched yet, register it.
    ret=epoll_ctl(epolld,EPOLL_CTL_ADD,fd,&(new_ev->ev));
  }
  */
  DEBUG("Event registered\n");
  return ret;
}

void check_events(){
  struct epoll_event evs[1];
  int ret;
  struct thread * woken_up;
  struct file_watched * file;
  TRACE("checking events\n");
  ret=epoll_wait(epolld,evs,1,0);
  if (ret==-1){
    perror("epoll_wait ");
    return;
  }
  if (ret==0){
    //no event have happened
    TRACE("No events\n");
    return;
  }
  DEBUG("new event!\n");
  file = evs[0].data.ptr;
  DEBUG("An event has occured : %d, data : %p\n",evs[0].events,file);
  //Find the first thread associated with this event, note that,
  //since threads are added in first position in the linked list,
  //the first one to be found will be the last added, which mean that
  //if two or more threads are waiting for the same event on the same
  //file descriptor the last one to be registered will be the first to
  //be woken up. This behaviour is not very nice, but it's the easier 
  //to implement right now
  DEBUG("file descriptor : %d, first thread : %p\n",\
      file->fd,file->first_listener);
  woken_up=file->first_listener;
  remove_file_listener(file);
  restart_thread(woken_up);
}

mqd_t gmq_open(const char * name, int oflag, mode_t mode, \
    struct mq_attr * attr){
  mqd_t ret;
  oflag|=O_NONBLOCK;
  ret = mq_open(name,oflag,mode,attr);
  if (ret==-1) return ret;
  /*
  if (fcntl(ret,F_SETFL,O_ASYNC | O_NONBLOCK)==-1) perror("fcntl SETFL");
  if (fcntl(ret,F_SETOWN,getpid())==-1) perror("fcntl SETOWN ");
  if (fcntl(ret,F_SETSIG,0)==-1) perror("fcntl SETSIG");
  */ //That doesn't seem to be working...
  return ret;
}

int gmq_send(mqd_t mqdes, const char * msg_ptr, size_t msg_len, \
    unsigned int msg_prio) {
  int ret=0;
  ret = mq_send(mqdes, msg_ptr, msg_len, msg_prio);
  if(ret==0) return 0;
  if (errno==EAGAIN){
    DEBUG("mq_send would block, register it\n");
    //remove current thread from active threads :
    stop_current_thread();
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
    stop_current_thread();
    ret = register_event(mqdes,EPOLLIN);
    if (ret==-1) perror("register event ");
    ordonnanceur();
    return ret;
  }
  return ret;
}
