#include <stdio.h>
#include <stdbool.h>
#include <sys/ioctl.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <strings.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <errno.h>
#include <assert.h>
#include <openssl/md5.h>

#include "ff_config.h"
#include "ff_api.h"
#include "ff_epoll.h"

#define MAX_EVENTS 512
#define BOX 10379
#define BROKER "90272"
#define MAX_BYTES 2700

struct epoll_event ev;
struct epoll_event events[MAX_EVENTS];

int epfd;
int sockfd;

int seq;

/* STRUCTS */
#pragma pack (1)
typedef struct MESSAGE_HEADER
{
    short TransactionCode;
    int32_t LogTime;
    char AlphaChar[2];
    int32_t TraderId;
    short ErrorCode;
    int64_t Timestamp;
    int64_t Timestamp1;
    int64_t Timestamp2;
    short MessageLength;

    // MESSAGE_HEADER(short transCode):TransactionCode(htons(transCode)){}
    // MESSAGE_HEADER(){}
}MESSAGE_HEADER;

typedef struct 
{
    MESSAGE_HEADER MESSAGE_HEADER_ ;
    short BoxID;
    char BrokerID[5];
    char Filler[1];
}MS_GR_REQUEST;

typedef struct {
    MESSAGE_HEADER MESSAGE_HEADER_;
    short BoxID;
    char BrokerID[5];
    char Filler[1];
    char IPAddress[16];
    int32_t Port;
    char SessionKey[8];   
}MS_GR_RESPONSE;


typedef struct
{
    short length;
    int32_t SequenceNo;
    unsigned char md5checksum[16];
    char pkt[MAX_BYTES];    

} packet;

packet_init(packet * _packet, MS_GR_REQUEST pkt_)
{   
    int32_t pkt_length = sizeof(pkt_);
    memcpy(_packet->pkt, &pkt_, pkt_length);

    _packet->length = htons(22 + pkt_length);

    _packet->SequenceNo = htons(++seq);

    const unsigned char* pkt_bytes = (const unsigned char *) (_packet->pkt);
    MD5(pkt_bytes, pkt_length, _packet->md5checksum);  
}

char smalltext[] = 
    "oπo connected"
    "\r\n";

char largetext[] =
    "blah blah blah"
    "\r\n";

bool connected = false;

int loop(void * arg)
{
    /* Wait for events to happen */

    int nevents = ff_epoll_wait(epfd, events, MAX_EVENTS, 0);
    int i;
    
    for (i = 0; i < nevents; ++i)
    {
        
        if (events[i].events & EPOLLERR)
        {
            /* Simply close socket */
            ff_epoll_ctl(epfd, EPOLL_CTL_DEL, events[i].data.fd, NULL);
            ff_close(events[i].data.fd);

            printf("[helloworld] ff_close\n");
        }
        else if (events[i].events & EPOLLIN)
        {   
            printf("[helloworld] ff_epollin event\n");
            
            if(!connected) {
                
                connected = true;
                
                MS_GR_REQUEST gr_req = {.MESSAGE_HEADER_ = {.TransactionCode = 2400}};
                gr_req.BoxID = htons(BOX);
                memcpy(gr_req.BrokerID, BROKER, sizeof(gr_req.BrokerID));
                gr_req.MESSAGE_HEADER_.MessageLength = htons(sizeof(gr_req));

                packet send_envelope;
                packet_init(&send_envelope, gr_req);
                short size = ntohs(send_envelope.length);
                
                ff_write(events[i].data.fd, &send_envelope, size);
                continue;
            }

            packet recv_envelope;
            size_t readlen = ff_read(events[i].data.fd, &recv_envelope, MAX_BYTES);
            short size = ntohs(recv_envelope.length);
            
            if (readlen > 0)
            {   
                for (int i =0; i<size; ++i)
                {
                    printf("%c ", recv_envelope.pkt[i]);
                }
                printf("\n");
                // printf("[helloworld] ff_read: %s\n", recv_envelope.pkt);
                // ff_write(events[i].data.fd, largetext, sizeof(largetext) - 1);
            }
            else
            {
                // if readlen == 0 then close the connection.
                ff_epoll_ctl(epfd, EPOLL_CTL_DEL, events[i].data.fd, NULL);
                ff_close(events[i].data.fd);
            }
        }
        else
        {
            printf("unknown event: %8.8X\n", events[i].events);
        }
    }
}

int main(int argc, char *argv[])
{   
    int ret = 0;
    ff_init(argc, argv);

    sockfd = ff_socket(AF_INET, SOCK_STREAM, 0);
    printf("sockfd:%d\n", sockfd);
    if (sockfd < 0)
    {
        printf("ff_socket failed\n");
        exit(1);
    }

    int on = 1;
    ff_ioctl(sockfd, FIONBIO, &on); // set socket to 0:blocking 1:nonblocking
    
    /* connect_local () */

    int optval = 1;
    const char *inst = "10.244.176.92";

    struct sockaddr_in local_interface;
    inet_aton(inst, &(local_interface.sin_addr));
    local_interface.sin_family = AF_INET;

    ff_setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, (const void *)&optval, (socklen_t) sizeof(optval));
    ret = ff_bind(sockfd, (const struct linux_sockaddr*) &local_interface, (socklen_t)sizeof(local_interface));
    if (ret < 0)
    {
        printf("ff_bind failed sockfd:%d, errno:%d, %s\n", sockfd, errno, strerror(errno));
    }
    
    /* connect_server () */

    const char *ip_addr_server = "172.19.245.107";
    uint16_t ip_addr_port = 10263;

    struct sockaddr_in serv_addr;
    bzero(&serv_addr, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(ip_addr_port);
    // my_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    inet_pton(AF_INET, ip_addr_server, &serv_addr.sin_addr.s_addr);

    assert((epfd = ff_epoll_create(0)) > 0);
    ev.data.fd = sockfd;
    ev.events = EPOLLIN | EPOLLET;
    ff_epoll_ctl(epfd, EPOLL_CTL_ADD, sockfd, &ev);

    ret = ff_connect(sockfd, (struct linux_sockaddr *)&serv_addr, sizeof(serv_addr));
    if (ret < 0 && errno != EINPROGRESS)
    {
        printf("ff_connect failed sockfd:%d, errno:%d, %s\n", sockfd, errno, strerror(errno));
    }
   
    seq = 0;
    ff_run(loop, NULL);
    return 0;
}