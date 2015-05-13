#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <pthread.h>
#include <errno.h>
#include <termios.h>
#include <sys/time.h>
#include <sys/types.h>

#include "serial.h"

#define SERIAL_RECV_BUF_LEN                  (256)

//#define SERIAL_LOG  

static void serial_iolog(void *buf,int len,int input)
{
#ifdef SERIAL_LOG    
    struct timeval t;
    uint8_t *data = buf;
    int i;

    gettimeofday(&t,NULL);
    printf("[%4.4d.%3.3d] %s %d bytes :\n",
            (int)t.tv_sec % 100000,(int)t.tv_usec / 1000,
            input ? "Recv" : "Send",len);
    
    for (i = 0;i < len ;i++)
    {        
        if ((i & 3) == 3)
        {
            if ((i & 31) == 31 && i != len - 1)
            {
                printf("%2.2x\n",data[i]);
            }
            else
            {
                printf("%2.2x ",data[i]);
            }
        }
        else 
        {
            printf("%2.2x",data[i]);
        }
    }

    printf("\n");
#endif    
}

static void *serial_proc(void *arg)
{
    struct serial_port *serial = (struct serial_port *)arg;
    struct timeval timeout;    
    fd_set rfds;
    int maxfd,len,retval;
    char buf[SERIAL_RECV_BUF_LEN];

    while(1)
    {
        pthread_rwlock_rdlock(&serial->rwlock);
        
        if (serial->fd <= 0) 
        {
            pthread_rwlock_unlock(&serial->rwlock);
            break;
        }

        FD_ZERO(&rfds);
        FD_SET(serial->fd,&rfds);
        maxfd = serial->fd + 1;
        timeout.tv_sec = 1;
        timeout.tv_usec = 0;

        pthread_rwlock_unlock(&serial->rwlock);
        retval = select(maxfd,&rfds,NULL,NULL,&timeout);

        pthread_rwlock_rdlock(&serial->rwlock);

        if (serial->fd <= 0 || retval < 0) 
        {
            pthread_rwlock_unlock(&serial->rwlock);
            break;
        }
        
        if (retval == 0)
        {
            pthread_rwlock_unlock(&serial->rwlock);
            continue;
        }
        len = read(serial->fd,buf,SERIAL_RECV_BUF_LEN);                    
      
        pthread_rwlock_unlock(&serial->rwlock);

        serial_iolog(buf,len,1);
        serial->recv(buf,len);
    }

    return NULL;
}

struct serial_port* serial_init(const char *port,int baud,void (*recv)(void *,int))
{
    struct serial_port* serial;

    serial = calloc(sizeof(struct serial_port),1);
    if (serial == NULL)
    {
        return NULL;
    }
    
    serial->recv = recv;

    if (pthread_rwlock_init(&serial->rwlock,NULL) < 0)
    {
        goto err_rwlock;
    }

    serial->fd = open(port,O_RDWR | O_NOCTTY);
    if (serial->fd < 0) 
    {
        goto err_open;
    }
    
    if (serial_setup(serial,baud) < 0)
    {
        goto err_baud;
    }
    
    if (pthread_create(&serial->tid,NULL,serial_proc,serial) < 0)
    {
        goto err_thr;
    }

    return serial;
err_thr:    
err_baud:
    close(serial->fd);
err_open:
    pthread_rwlock_destroy(&serial->rwlock);
err_rwlock:
    free(serial);
    return NULL;
}

int serial_setup(struct serial_port *serial,int baud)
{
    const int baudrate[] = {2400,4800,9600,19200,38400,57600,115200};
    const speed_t baudspeed[] = {B2400,B4800,B9600,B19200,B38400,B57600,B115200};
    struct termios options;
    int index;

    for (index = 0;index < sizeof(baudrate) / sizeof(int);index++)
    {
        if (baudrate[index] == baud) 
        {
            break;
        }
    }
    
    if (index >= sizeof(baudrate)/sizeof(int))
    {
        return -1;
    }

    if (baud == serial->baud)
    {
        return 0;
    }

    pthread_rwlock_wrlock(&serial->rwlock);
    
    if (tcgetattr(serial->fd,&options) < 0)
    {
        pthread_rwlock_unlock(&serial->rwlock);
        return -1;
    }
    
    cfsetspeed(&options,baudspeed[index]);

    options.c_cflag &= ~(CSIZE | PARENB);
    options.c_cflag |= CS8;
    options.c_cflag |= CREAD;
    options.c_cflag |= CLOCAL;

    options.c_iflag &= ~(IGNBRK | BRKINT | PARMRK |
                         ISTRIP | INLCR | IGNCR | ICRNL | IXON);
    options.c_oflag &= ~OPOST;
    options.c_lflag &= ~(ECHO | ECHONL | ICANON | ISIG | IEXTEN);

    options.c_cc[VTIME] = (cc_t)10;
    options.c_cc[VMIN] = 0;

    if (tcsetattr(serial->fd,TCSANOW, &options) < 0)
    {
        pthread_rwlock_unlock(&serial->rwlock);
        return -1;
    }

    pthread_rwlock_unlock(&serial->rwlock);
    
    serial->baud = baud;

    return 0; 
}

int serial_send(struct serial_port *serial,void *buf,int len)
{
    int bytes,offset = 0;

    pthread_rwlock_rdlock(&serial->rwlock);
    
    if (serial->fd <= 0)
    {
        pthread_rwlock_unlock(&serial->rwlock);
        return -1;
    }

    while(offset < len)
    {
        bytes = write(serial->fd,((char*)buf) + offset,len - offset);
        if (bytes > 0) 
        {
            offset += bytes;
        }
        else 
        {
            pthread_rwlock_unlock(&serial->rwlock);
            return -1;
        }
    }
    
    pthread_rwlock_unlock(&serial->rwlock);

    serial_iolog(buf,len,0);
    return 0;
}

void serial_destroy(struct serial_port *serial)
{
    pthread_rwlock_wrlock(&serial->rwlock);
    
    close(serial->fd);
    serial->fd = -1;

    pthread_rwlock_unlock(&serial->rwlock);
    
    pthread_join(serial->tid,NULL);

    pthread_rwlock_destroy(&serial->rwlock);

    free(serial);
}


