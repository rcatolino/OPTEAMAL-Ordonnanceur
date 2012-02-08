
#include <mqueue.h>

mqd_t gmq_open(const char * name, int oflag, mode_t mode, \
    struct mq_attr * attr);

int gmq_send(mqd_t mqdes, const char * msg_ptr, size_t msg_len, \
    unsigned int msg_prio);

ssize_t gmq_receive(mqd_t mqdes, char * msg_ptr, size_t msg_len,\
   unsigned int * msg_prio);
