/* ------------------------------
   $Id: hw.h,v 1.1 2003/12/18 17:15:06 marquet Exp $
   ------------------------------------------------------------

   Basic hardware simulation
   Philippe Marquet, Dec 2003
   
*/

#ifndef _HW_H_
#define _HW_H_

typedef void (irq_handler_func_t)(void); 

#define TM_FREQ 9000 //8ms
#define TIMER_IRQ	2
#define TRACE(...) //printf(__VA_ARGS__)
#define DEBUG(...) //printf(__VA_ARGS__)

void setup_irq(unsigned int irq, irq_handler_func_t handler);
void start_hw();

void irq_disable();
void irq_enable(); 

#endif
