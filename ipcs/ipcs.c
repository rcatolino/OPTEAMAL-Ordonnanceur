//Module providing several ipcs to use with the gthread API
#define _GNU_SOURCE
#include "gthread.h"
#include "gmem.h"
#include "hw.h"
#include "ipcs.h"
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <sys/epoll.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <signal.h>
#include <mqueue.h>
#include <sys/socket.h>
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
  DEBUG("first_listener->next : %p\n",file->first_listener);
  current_thread->next=file->first_listener;//The current thread needs to be removed!!!
  file->first_listener=current_thread; 
  //Update the events wathed by the thread :
  current_thread->events = events;
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
    if (ret==-1 && errno==ENOENT){
      DEBUG("epoll_ctl errno ENOENT :%d\n",errno);
      //WTF??? That wasn't even specified in the man page!
    }
    if (ret==-1) perror("epoll_ctl ");
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
  for (i=first_file; i!=NULL && i->fd!=fd; i=i->next);
  if (i==NULL){
    DEBUG("Watching new file : %d\n",fd);
    //This file is beeing wathed for the fisrt time, add it in linked list
    new_file=gmalloc(sizeof(struct file_watched));
    new_file->fd=fd;
    new_file->ev.data.ptr=new_file; //used when an event is trigered on this fd
    new_file->next=first_file;
    new_file->first_listener=NULL;
    first_file=new_file;
    ret = add_file_listener(new_file,events);
  } else {
    //This file is already beeing watched :
    ret = add_file_listener(i,events);
  }
  DEBUG("Event registered : %d\n",ret);
  return ret;
}

void check_events(){
  struct epoll_event evs;
  int ret;
  struct thread * woken_up;
  struct file_watched * file;
  TRACE("checking events\n");
  ret=epoll_wait(epolld,&evs,1,0);
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
  file = evs.data.ptr;
  woken_up=file->first_listener;
  DEBUG("An event has occured : %d, data : %p\n",evs.events,file);
  //Find the first thread associated with this event, note that,
  //since threads are added in first position in the linked list,
  //the first one to be found will be the last added, which mean that
  //if two or more threads are waiting for the same event on the same
  //file descriptor, the last one to be registered will be the first to
  //be woken up. This behaviour is not very nice, but it's the easier 
  //to implement right now
  DEBUG("file descriptor : %d, first thread : %p\n",\
      file->fd,woken_up);
  if ((evs.events & EPOLLIN)!=0){
    DEBUG("EPOLLIN\n");
  }
  if ((evs.events & EPOLLOUT)!=0){
    DEBUG("EPOLLOUT\n");
  }
  while (woken_up != NULL && (evs.events & woken_up->events) == 0 ){
    woken_up=woken_up->next;
  }
  if (woken_up==NULL){
    //No one seems to be registered for this event, ideally this should not happen
    DEBUG("No thread waiting for the event\n");
    return;
  }
  remove_file_listener(file);
  restart_thread(woken_up);
}

int wait_event(int fd, uint32_t events){
  int ret;
  //register new event
  stop_current_thread();
  ret = register_event(fd,events);
  if (ret==-1){
    perror("register event ");
    return -1;
  }
  ordonnanceur();
  return 0;
}
//gipcs wrappers :
// Message queues :
mqd_t gmq_open(const char * name, int oflag, mode_t mode, \
    struct mq_attr * attr){
  mqd_t ret;
  oflag|=O_NONBLOCK;
  ret = mq_open(name,oflag,mode,attr);
  return ret;
}
int gmq_send(mqd_t mqdes, const char * msg_ptr, size_t msg_len, \
    unsigned int msg_prio) {
  int ret=0;
  struct mq_attr attrs;
  ret = mq_send(mqdes, msg_ptr, msg_len, msg_prio);
  mq_getattr(mqdes,&attrs);
  if(ret==0) return 0;
  if (errno==EAGAIN){
    DEBUG("mq_send would block, delay it\n");
    wait_event(mqdes,EPOLLOUT);
    return mq_send(mqdes, msg_ptr, msg_len, msg_prio);
  }
  return ret;
}
ssize_t gmq_receive(mqd_t mqdes, char * msg_ptr, size_t msg_len,\
   unsigned int * msg_prio){
  int ret=0;
  struct mq_attr attrs;
  mq_getattr(mqdes,&attrs);
  ret = mq_receive(mqdes, msg_ptr, msg_len, msg_prio);
  if(ret==0) return 0; //no need to wait
  if (attrs.mq_curmsgs==0){
    DEBUG("mq_receive would block, delay it\n");
    wait_event(mqdes,EPOLLIN);
    return mq_receive(mqdes, msg_ptr, msg_len, msg_prio); //ask for the message again,
    //now that we know it won't block
  }
  return ret;
}
// Sockets :
int gsocket(int domain, int type, int protocol){
  type|=SOCK_NONBLOCK;
  return socket(domain,type,protocol);
}

int gaccept(int socketd, struct sockaddr * addr, socklen_t *addrlen){
  int new_socket=0;
  new_socket = accept4(socketd,addr,addrlen,SOCK_NONBLOCK);
  if(new_socket==0) return 0; //no need to wait
  if (errno==EAGAIN || errno==EWOULDBLOCK){
    DEBUG("accept would block, delay it\n");
    wait_event(socketd,EPOLLIN);
    return accept4(socketd,addr,addrlen,SOCK_NONBLOCK); //ask for the message again,
    //now that we know it won't block
  }
  return new_socket;
}

ssize_t grecv(int sockfd, void *buff, size_t len, int flags){
  int ret=0;
  ret= recv(sockfd, buff, len, flags);
  if(ret>=0) return ret; //no need to wait
  if (errno==EAGAIN || errno==EWOULDBLOCK){
    DEBUG("recv would block, delay it\n");
    wait_event(sockfd,EPOLLIN);
    DEBUG("Recv woken up\n");
    return recv(sockfd, buff, len, flags);
  }
  return ret;
}

ssize_t gsend(int sockfd, void *buff, size_t len, int flags){
  int ret=0;
  ret= send(sockfd, buff, len, flags);
  if(ret>=0) return ret; //no need to wait
  if (errno==EAGAIN || errno==EWOULDBLOCK){
    DEBUG("send would block, delay it\n");
    wait_event(sockfd,EPOLLOUT);
    return send(sockfd, buff, len, flags);
  }
  return ret;
}
