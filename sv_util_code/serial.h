#ifndef  __SERIAL_INCL__
#define  __SERIAL_INCL__
#include <stdint.h>
#include <pthread.h>

struct serial_port
{
    int fd;
    int baud;
    void (*recv)(void *,int);
    pthread_t tid;
    pthread_rwlock_t rwlock;
};

struct serial_port* serial_init(const char *port,int baud,void (*recv)(void *,int));
int serial_setup(struct serial_port *serial,int baud);
int serial_send(struct serial_port *serial,void *buf,int len);
void serial_destroy(struct serial_port *serial);

#endif //__SERIAL_INCL__
