/* ------------------------------
   $Id: hw.c,v 1.2 2003/12/19 12:52:31 marquet Exp $
   ------------------------------------------------------------

   Basic hardware emulation.
   Philippe Marquet, Dec 2003
   Raphael Catolino, Jan 2012
   
*/
#include <stdio.h>
#include <sys/time.h>
#include <time.h>
#include <signal.h>
#include <assert.h>

#include "hw.h"

static irq_handler_func_t *timer_irq_handler = (irq_handler_func_t *) 0; 
static volatile int irqs_are_enable = 1;
static timer_t timerId;

static void do_timer_interrupt(int sigNb, siginfo_t * infos, void * ucontext)
{
  //Print the id of the timer that caused the signal :
  /*printf("Signal caught : %d, signal value : %d, timer overrun : %d \n",\
      infos->si_signo,infos->si_value.sival_int,infos->si_overrun);
      */
  if (timer_irq_handler && irqs_are_enable)
    timer_irq_handler();
}

void start_hw()
{
  {
    /* start timer handler */
    static struct sigaction sa;
    
    sigemptyset(&sa.sa_mask);
    sa.sa_sigaction = do_timer_interrupt;
    sa.sa_flags = SA_RESTART | SA_NODEFER | SA_SIGINFO;
    //Why SA_RESTART : 
    //Theoretically we don't need it (that's the whole point of the epoll-based blocking
    //system). However i had absolutly no envy to implement every single potentially
    //blocking system call, even though it's only a couple of lines to add for each one.
    //Instead the SA_RESTART allows me to use almost any system call without worrying 
    //about whether it will block or not. That's because if a call comes to block, the sytem 
    //will block the whole process, until the scheduler timer expire and restart another 
    //thread. The difference when we use SA_RESTART is that when the round of the thread
    //that launched the system call comes again, the call is restarted instead of failing.
    //This way we loose cpu time because the threads are actually waiting for the call to
    //return are still scheduled to run periodically, but since we have only 5 threads in
    //GHome and most of the blocking calls used are epoll-wrapped in gthread, the user will
    //probably never see any differences. By using SA_RESTART we can use a lot more
    //functionalities by sacrifying performance for some of them. Just be aware of it.
    sigaction(SIGUSR1, &sa, (struct sigaction *)0);
  }

  {
    /* set timer */ 
    struct sigevent evp = {
      .sigev_notify=SIGEV_SIGNAL,
      .sigev_signo=SIGUSR1,
      .sigev_value.sival_int=42,
    };
    struct itimerspec timerValue = {
      .it_interval.tv_sec=0,
      .it_interval.tv_nsec=TM_FREQ*1000,
      .it_value.tv_sec=0,
      .it_value.tv_nsec=TM_FREQ*1000,
    };

    timer_create(CLOCK_REALTIME,&evp,&timerId);
    timer_settime(timerId,0,&timerValue,NULL);
  }
}

void setup_irq(unsigned int irq, irq_handler_func_t handler)
{
  assert(irq = TIMER_IRQ);
  timer_irq_handler = handler;
}

void irq_disable()
{
  irqs_are_enable = 0; 
}

void irq_enable()
{
  irqs_are_enable = 1; 
}
