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

#include "ff_config.h"
#include "ff_api.h"
#include "ff_epoll.h"

#define MAX_EVENTS 512

struct epoll_event ev;
struct epoll_event events[MAX_EVENTS];

int epfd;
int sockfd;

char html[] =
    "HTTP/1.1 200 OK\r\n"
    "Server: F-Stack\r\n"
    "Date: Sat, 25 Feb 2017 09:26:33 GMT\r\n"
    "Content-Type: text/html\r\n"
    "Content-Length: 438\r\n"
    "Last-Modified: Tue, 21 Feb 2017 09:44:03 GMT\r\n"
    "Connection: keep-alive\r\n"
    "Accept-Ranges: bytes\r\n"
    "\r\n"
    "<!DOCTYPE html>\r\n"
    "<html>\r\n"
    "<head>\r\n"
    "<title>Welcome to F-Stack!</title>\r\n"
    "<style>\r\n"
    "    body {  \r\n"
    "        width: 35em;\r\n"
    "        margin: 0 auto; \r\n"
    "        font-family: Tahoma, Verdana, Arial, sans-serif;\r\n"
    "    }\r\n"
    "</style>\r\n"
    "</head>\r\n"
    "<body>\r\n"
    "<h1>Welcome to F-Stack!</h1>\r\n"
    "\r\n"
    "<p>For online documentation and support please refer to\r\n"
    "<a href=\"http://F-Stack.org/\">F-Stack.org</a>.<br/>\r\n"
    "\r\n"
    "<p><em>Thank you for using F-Stack.</em></p>\r\n"
    "</body>\r\n"
    "</html>";

char smalltext[] = 
    "oπo connected"
    "\r\n";

char largetext[] =
    "blah blah blah"
    "\r\n";

bool connected = false;

int loop(void *arg)
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
                ff_write(events[i].data.fd, smalltext, sizeof(smalltext) - 1);
                continue;
            }

            char buf[256];
            size_t readlen = ff_read(events[i].data.fd, buf, sizeof(buf));
            if (readlen > 0)
            {
                printf("[helloworld] ff_read: %s\n", buf);
                ff_write(events[i].data.fd, largetext, sizeof(largetext) - 1);
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

    /* From Deepak's code*/
    // int optval = 1;
    // ff_setsockopt(sockfd, SO_BROADCAST, SO_DEBUG, (const void *)&optval, (socklen_t) sizeof(optval));

    const char *ip_addr_server = "10.244.176.81";
    uint16_t ip_addr_port = 10263;

    struct sockaddr_in my_addr;
    bzero(&my_addr, sizeof(my_addr));
    my_addr.sin_family = AF_INET;
    my_addr.sin_port = htons(ip_addr_port);
    // my_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    inet_pton(AF_INET, ip_addr_server, &my_addr.sin_addr.s_addr);

    int ret;

    assert((epfd = ff_epoll_create(0)) > 0);
    ev.data.fd = sockfd;
    ev.events = EPOLLIN | EPOLLET;
    ff_epoll_ctl(epfd, EPOLL_CTL_ADD, sockfd, &ev);

    ret = ff_connect(sockfd, (struct linux_sockaddr *)&my_addr, sizeof(my_addr));
    if (ret < 0 && errno != EINPROGRESS)
    {
        printf("ff_connect failed sockfd:%d, errno:%d, %s\n", sockfd, errno, strerror(errno));
    }
   
    ff_run(loop, NULL);
    return 0;
}