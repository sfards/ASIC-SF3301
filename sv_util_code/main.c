#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <unistd.h>
#include <string.h>
#include <sys/time.h>
#include "serial.h"

//#define PIGPIO_RESET

void gpio_reset(void);

static void show_usage()
{
    printf("chiptest for SF3301\n");
    printf("usage:\n");
    printf("  for test:\n");
    printf("    chiptest <serial port path> <btc/ltc> <chipid> <freq> [count]\n");
    printf("  for debug:\n"); 
    printf("    chiptest <serial port path> <write data> [wait time]\n");
    printf("  for detect:\n"); 
    printf("    chiptest <serial port path> <det> [wait time]\n");
}

static int char2hex(char c)
{
    if (c >= '0' && c <= '9')
    {
        return (c - '0');
    }
    else if (c >= 'a' && c <= 'f')
    {
        return (c - 'a' + 10);
    }
    else if (c >= 'A' && c <= 'F')
    {
        return (c - 'A' + 10);
    }

    return -1;
}

static int hexstr2bin(char *cmd,uint8_t *buf,int len)
{
    int i,j;
    int h,l;
    if (len * 2 < strlen(cmd))
    {
        return -1;
    }

    for (i = 0,j = 0;i < strlen(cmd);i++)
    {
        if (cmd[i] == ' ')
        {
            continue;
        }

        h = char2hex(cmd[i]);
        l = char2hex(cmd[i + 1]);
        if (h < 0 || l < 0)
        {
            return -1;
        }

        buf[j++] = (h << 4) | l;
        i++;
    }
    return j;
}

static void send_cmd(struct serial_port *serial,char * cmd,uint8_t *buf,int len)
{
    int size = hexstr2bin(cmd,buf,len);
    if (size > 0)
    {
         serial_send(serial,buf,size);
    }
    usleep(2000);
}

static uint32_t calc_freq(int freq)
{
    uint32_t val = 0x0000600d;
    uint32_t pll_od,pll_f;
    int fact = freq / 25;;
    if (fact < 1)
    {
        fact = 1;
    }
    if (fact > 40) 
    {
        fact = 40;
    }
    
    pll_od = fact < 8 ? 8 : 1; 
    pll_f = pll_od * fact; 

    val |= (pll_f << 17) | (pll_od << 24);
        
    return ((val << 24) | (val >> 24) | ((val << 8) & 0xff0000) | ((val >> 8) & 0xff00));
}

static uint32_t btc_nonce[4] = 
{0x77e446d1,0xa7d458f7,0x6b908a24,0x9c1aa145};

static int btc_nonce_count = 0;
static int btc_nonce_error = 0;
static int btc_chipid = 0;

static void btc_test_cb(void *buf,int len)
{
    static uint8_t recv_buf[1024];
    static int recv_buf_offset = 0;
    int i,k;

    if (recv_buf_offset + len > 1024)
    {
        len -= 1024 - recv_buf_offset;
    }
    memcpy(recv_buf + recv_buf_offset,buf,len);

    recv_buf_offset += len;
    
    i = 0;
    while(i <= recv_buf_offset - 8)
    {
        if (recv_buf[i] == 0x55)
        {                
            if (recv_buf[i + 1] == btc_chipid 
                && recv_buf[i + 2] == 0xff 
                && recv_buf[i + 3] == 0x00)
            {
                uint32_t nonce = recv_buf[i + 7] | recv_buf[i + 6] << 8 
                                  | recv_buf[i + 5] << 16 | recv_buf[i + 4] << 24;
                for (k = 0;k < 4;k++)
                {
                    if (nonce == btc_nonce[k])
                    {
                        break;
                    }
                }         
                if (k >= 4)
                {
                    btc_nonce_error++;
                }
                btc_nonce_count++;
            }
            i += 7;
        }
        i++;
    }        
    if (i > 0)
    {
        memmove(recv_buf,recv_buf + i,recv_buf_offset - i);
        recv_buf_offset -= i;
    }
}

static void btc_test(char *path,int baud,int chipid,int freq,int count)
{
    char *cmd_auto_cfg = "55fef07f010000c0";
    char *cmd_clear = "55ff f002 0000 0000 0000 0000 0000 0000 0000 0000 0000 0000";
    char *cmd_disable = "55ff f01e 0000 0000";
    char *cmd_enable = "55ff f01e 0300 0000";
    char cmd_set_freq[17] = "";
    char *cmd_init_seq[] = 
    {
        "55ff f054 0200 5a8a","55ff f052 6895 a712",
        "55ff f055 8233 7f2a","55ff f058 1911 1420",
        "55ff f05a 0000 0020","55ff f05c 0210 0000",
        "55ff f060 1906 1420","55ff f064 2410 9640",
        "55ff f066 ffaa 0312","55ff f06a 0122 0030",
        "55ff ef1f 180c 7e17","55ff ef01 ffff ffff",
        "55ff ef00 0000 0000","55ff f020 1f1b 81b4",
        "55ff f002 0100 0000","55ff f003 0100 0000",
        "55ff f004 0100 0000","55ff f005 0100 0000",
        "55ff f006 0100 0000",
        "55ff f002 0300 0000","55ff f003 0300 0000",
        "55ff f004 0300 0000","55ff f005 0300 0000",
        "55ff f006 0300 0000","55ff f002 0700 0000",
        "55ff f003 0700 0000","55ff f004 0700 0000",
        "55ff f005 ffff ff07","55ff f006 ffff ff07",
        "55ff f002 ffff ff0f","55ff f003 ffff ff0f",
        "55ff f004 ffff ff0f","55ff f005 ffff ff0f",
        "55ff f006 ffff ff0f","55ff f002 ffff ff1f",
        "55ff f003 ffff ff1f","55ff f004 ffff ff1f",
        "55ff f005 ffff ff1f","55ff f006 ffff ff1f",
        "55ff f002 ffff ff3f",
        "55ff f003 ffff ff3f","55ff f004 ffff ff3f",
        "55ff f005 ffff ff3f","55ff f006 ffff ff3f",
        "55ff f002 ffff ff7f","55ff f003 ffff ff7f",
        "55ff f004 ffff ff7f","55ff f005 ffff ff7f",
        "55ff f006 ffff ff7f","55ff f002 ffff ffff",
        "55ff f003 ffff ffff","55ff f004 ffff ffff",
        "55ff f005 ffff ffff","55ff f006 ffff ffff",
    };
   
    char *cmd_task_fmt[] =
    { 
        "55%2.2x ef21 00e0 ff1f 1b71 4871 7e49 8ce1"
        "5868 5055 77ba 7c3d c696 a917 c26b adde"
        "ed36 3e61 6f5e cd7d df70 61f2 553b 7eaf"
        "1817 17f0",
        "55%2.2x ef41 00e0 ff1f a092 4de2"
        "edca 349f 87ab 15d1 58cc 6363 e3c0 ac09"
        "0ac6 7120 3cb7 2390 3fa6 9689 cc0c ccc0"
        "553b 7ec0 1817 17f0",
        "55%2.2x ef61 00e0 ff1f c7cf 6e20 0dd9 7643 1899 7d16 b005 b0ee"
        "437a 35d0 9192 4818 42ab a3a3 5b7a 9a72"
        "6bdd d834 553b 7ec0 1817 17f0",
        "55%2.2x ef01 00e0 ff1f f7fd b588 65ae 81a0 ca67 3add"
        "3ba4 7ed9 6b6c 1e93 6033 e089 95c8 8a71"
        "8899 65a4 6ed1 11a5 553b 7ec0 1817 17f4",
    };

    char cmd_task_buf[320];
    struct serial_port *serial;
    uint8_t buf[1024];
    const int buf_len = 1024;
    int i,prev = 0;
    serial = serial_init(path,baud,btc_test_cb);
    sprintf(cmd_set_freq,"55fff000%8.8x",calc_freq(freq));
    if (serial == NULL)
    {
        printf("Failed open serial port\n");
        return;
    }
    btc_chipid = chipid;

    send_cmd(serial,cmd_auto_cfg,buf,buf_len);
    sleep(2);
    send_cmd(serial,cmd_clear,buf,buf_len);
    send_cmd(serial,cmd_disable,buf,buf_len);
    send_cmd(serial,cmd_enable,buf,buf_len);
    send_cmd(serial,cmd_set_freq,buf,buf_len);
    for (i = 0;i < 34;i++)
    { 
        send_cmd(serial,cmd_init_seq[i],buf,buf_len);
    }

    for (i = 0;i < 4;i++)
    {
        sprintf(cmd_task_buf,cmd_task_fmt[i],chipid);
        send_cmd(serial,cmd_task_buf,buf,buf_len);
    }

    while(count > btc_nonce_count)
    {
        sleep(1);
        if (btc_nonce_count > prev)
        {
            prev = btc_nonce_count;
            printf("btc nonce count = %d error = %d (%3.2f%%)\n",
                   btc_nonce_count,btc_nonce_error,
                   (btc_nonce_count - btc_nonce_error) * 100.0 / btc_nonce_count);
        }
    }
    send_cmd(serial,cmd_clear,buf,buf_len);
    send_cmd(serial,cmd_disable,buf,buf_len);
    serial_destroy(serial);
}

extern char *ltc_task;
extern uint32_t ltc_nonce[140];
static int ltc_nonce_count = 0;
static int ltc_nonce_error = 0;
static int ltc_chipid = 0;

static void ltc_test_cb(void *buf,int len)
{
    static uint8_t recv_buf[128];
    static int recv_buf_offset = 0;
    int i,k;

    if (recv_buf_offset + len > 128)
    {
        len -= 128 - recv_buf_offset;
    }
    memcpy(recv_buf + recv_buf_offset,buf,len);

    recv_buf_offset += len;
    
    i = 0;
    while(i <= recv_buf_offset - 8)
    {
        if (recv_buf[i] == 0x55 && ltc_nonce_count < 140)
        {                
            if (recv_buf[i + 1] == ltc_chipid 
                && recv_buf[i + 2] == 0xff 
                && recv_buf[i + 3] == 0x00)
            {
                uint32_t nonce = recv_buf[i + 7] | recv_buf[i + 6] << 8 
                                 | recv_buf[i + 5] << 16 | recv_buf[i + 4] << 24;
                for (k = 0;k < 140;k++)
                {
                    if (nonce == ltc_nonce[k])
                    {
                        break;
                    }
                }         
                if (k >= 140)
                {
                    ltc_nonce_error++;
                }
                ltc_nonce_count++;
            }
            i += 7;
        }
        i++;
    }        
    if (i > 0)
    {
        memmove(recv_buf,recv_buf + i,recv_buf_offset - i);
        recv_buf_offset -= i;
    }
}

static void ltc_test(char *path,int baud,int chipid,int freq,int count)
{
    char *cmd_auto_cfg = "55fef07f010800c0";
    char *cmd_en_pll = "55fff002ffffff7f";
    char *cmd_cmp_mode = "55ffbf300a000080";
    char *cmd_init_nonce = "55ffbf0000000000";
    char *cmd_task_fmt = "55%2.2xbf01%s";
    char cmd_set_freq[17] = "";

    char cmd_task_buf[320];
    struct serial_port *serial;
    uint8_t buf[1024];
    const int buf_len = 1024;
    int prev = 0;
    serial = serial_init(path,baud,ltc_test_cb);
    sprintf(cmd_task_buf,cmd_task_fmt,chipid,ltc_task);
    sprintf(cmd_set_freq,"55fff000%8.8x",calc_freq(freq));
    if (serial == NULL)
    {
        printf("Failed open serial port\n");
        return;
    }
    ltc_chipid = chipid; 

    send_cmd(serial,cmd_auto_cfg,buf,buf_len);
    sleep(2);
    send_cmd(serial,cmd_set_freq,buf,buf_len);
    send_cmd(serial,cmd_en_pll,buf,buf_len);
    send_cmd(serial,cmd_cmp_mode,buf,buf_len);
    send_cmd(serial,cmd_init_nonce,buf,buf_len);
   
    send_cmd(serial,cmd_task_buf,buf,buf_len);
    while(count > ltc_nonce_count)
    {
        sleep(1);
        if (ltc_nonce_count > prev)
        {
            prev = ltc_nonce_count;
            printf("ltc nonce count = %d error = %d (%3.2f%%)\n",
                   ltc_nonce_count,ltc_nonce_error,
                   (ltc_nonce_count - ltc_nonce_error) * 100.0 / ltc_nonce_count);
        }
    }
 
    serial_destroy(serial);
}

static void reg_debug_cb(void *buf,int len)
{
    static int count = 0;
    uint8_t *p = buf;
    
    while(len--)
    {
        printf("%2.2x",*p++);
        if (((count++) & 7) == 7)
        {
            printf("\n");
        }
    }
}

static void reg_debug(char *path,int baud,char *data,int waittime)
{
    struct serial_port *serial;
    uint8_t buf[1024];
    const int buf_len = 1024;
    serial = serial_init(path,baud,reg_debug_cb);
    if (serial == NULL)
    {
        printf("Failed open serial port\n");
        return;
    }

    send_cmd(serial,data,buf,buf_len);
    if (waittime)
    {
        sleep(waittime);
    }
    else 
    {
        usleep(20000);
    }

    serial_destroy(serial);
}

static void chip_detect_cb(void *buf,int len)
{
    static uint8_t recv_buf[128];
    static int recv_buf_offset = 0;
    int i;

    if (recv_buf_offset + len > 128)
    {
        len -= 128 - recv_buf_offset;
    }
    memcpy(recv_buf + recv_buf_offset,buf,len);

    recv_buf_offset += len;
    
    i = 0;
    while(i <= recv_buf_offset - 8)
    {
        if (recv_buf[i] == 0x55)
        {                
            if (recv_buf[i + 2] == 0x00 && recv_buf[i + 3] == 0x75)
            {
                printf("detected chip id = %d, device id = %2.2x%2.2x%2.2x%2.2x\n",
                        recv_buf[i + 1],recv_buf[i + 7],recv_buf[i + 6],
                        recv_buf[i + 5],recv_buf[i + 4]);
            }
            i += 7;
        }
        i++;
    }        
    if (i > 0)
    {
        memmove(recv_buf,recv_buf + i,recv_buf_offset - i);
        recv_buf_offset -= i;
    }
}

static void chip_detect(char *path,int baud,int waittime)
{
    char *cmd_auto_cfg = "55fef07f010000c0";
    char *cmd_deviceid = "55fff0f500000000";
    struct serial_port *serial;
    uint8_t buf[1024];
    const int buf_len = 1024;
#ifdef PIGPIO_RESET
    gpio_reset();
#endif
    serial = serial_init(path,baud,chip_detect_cb);
    if (serial == NULL)
    {
        printf("Failed open serial port\n");
        return;
    }

    send_cmd(serial,cmd_auto_cfg,buf,buf_len);
    sleep(3);
    send_cmd(serial,cmd_deviceid,buf,buf_len);
    
    if (waittime)
    {
        sleep(waittime);
    }
    else 
    {
        usleep(20000);
    }
    serial_destroy(serial);
}

int main(int argc,char **argv)
{
    int baud = 115200;
    char *path = argv[1];

    if (argc < 3)
    {
        show_usage();
        exit(0);
    }
    
    if (strcmp(argv[2],"btc") == 0 || strcmp(argv[2],"ltc") == 0)
    {
        int chipid,freq,count;
        if (argc < 5)
        {
            show_usage();
            exit(0);
        }
        chipid = atoi(argv[3]);
        freq = atoi(argv[4]);
        count = argc > 5 ? atoi(argv[5]) : 1;
        if (strcmp(argv[2],"btc") == 0)
        {
            btc_test(path,baud,chipid,freq,count);
        }
        else
        {
            ltc_test(path,baud,chipid,freq,count);
        }
    }    
    else 
    {
        int waittime = argc > 3 ? atoi(argv[3]) : 0;         
        if (strcmp(argv[2],"det") == 0)
        {
           chip_detect(path,baud,waittime);
        }
        else
        {
            reg_debug(path,baud,argv[2],waittime);
        }
    }
 
    return 0;
}

