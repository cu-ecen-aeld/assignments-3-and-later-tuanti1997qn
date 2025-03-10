#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <netdb.h>
#include <sys/types.h>
#include <stdint.h>
#include <unistd.h>
#include <signal.h>
#include <syslog.h>
#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>



#define PORT "9000"
#define MAX_CONNECTIONS 10
#define LOG_FILE "/var/tmp/aesdsocketdata"
#define MAX_BUFF 1024

int server_fd = -1 , client_fd = -1;
void sigint_handler(int sig)
{
    if(server_fd != -1)
    {
        close(server_fd);
    }
    if(client_fd != -1)
    {
        close(client_fd);
    }

    remove(LOG_FILE);

    closelog();
    exit(0);
}

void *get_in_addr(struct sockaddr *sa)
{
    if(sa->sa_family == AF_INET)
    {
        return &(((struct sockaddr_in *)sa)->sin_addr);
    }

    return &(((struct sockaddr_in6 *)sa)->sin6_addr);
}

int main(int argc, char *argv[])
{
    struct addrinfo hints, *result;
    char buff[MAX_BUFF];
    
    
    // setup sys log
    openlog("aesdsocket", LOG_PID | LOG_PERROR, LOG_USER);
    
    // set up signal handler
    struct sigaction sa;
    sa.sa_handler = sigint_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    sigaction(SIGINT, &sa, NULL);
    sigaction(SIGTERM, &sa, NULL);

    // daemonize the server
    if (argc >= 2 && strcmp(argv[1], "-d") == 0)
    {
        syslog(LOG_INFO, "Daemonizing the server\n");
        daemon(0, 0);
    }
    
    // get address info for server
    int ret;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE; // use my IP
    ret = getaddrinfo(NULL, PORT, &hints, &result);
    if(ret != 0)
    {
        syslog(LOG_ERR, "getaddrinfo error: %s\n", gai_strerror(ret));
        return -1;
    }

    // create socket
    if((server_fd = socket(result->ai_family, result->ai_socktype, result->ai_protocol)) == -1)
    {
        syslog(LOG_ERR, "socket error code %d\n", errno);
        freeaddrinfo(result);
        return -1;
    }

    // set opt
    int optval = 1;
    if(setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval)) == -1)
    {
        syslog(LOG_ERR, "setsockopt error code %d\n", errno);
        freeaddrinfo(result);
        close(server_fd);
        return -1;
    }
 
    // bind
    if(bind(server_fd, result->ai_addr, result->ai_addrlen) == -1)
    {
        syslog(LOG_ERR, "bind error code %d\n", errno);
        freeaddrinfo(result);
        close(server_fd);
        return -1;
    }
   
    
    // listen
    if(listen(server_fd, MAX_CONNECTIONS) == -1)
    {
        syslog(LOG_ERR, "listen error code %d\n", errno);
        freeaddrinfo(result);
        close(server_fd);
        return -1;
    }
    
    struct sockaddr_storage client_addr;
    socklen_t addr_size = sizeof(client_addr);
    
    freeaddrinfo(result);

    while (1)
    {
        // accept
        client_fd = accept(server_fd, (struct sockaddr *)&client_addr, &addr_size);
        if(client_fd == -1)
        {
            syslog(LOG_ERR, "accept error code %d\n", errno);
            close(server_fd);
            return -1;
        }

        int fd = open(LOG_FILE, O_CREAT | O_WRONLY | O_APPEND, 0666);
        if(fd == -1)
        {
            syslog(LOG_ERR, "open error code %d\n", errno);
            close(client_fd);
            close(server_fd);
            return -1;
        }

        // log client data
       inet_ntop(client_addr.ss_family, get_in_addr((struct sockaddr *)&client_addr), buff, sizeof(buff));
       syslog(LOG_INFO, "Connection from %s\n", buff);
       
       memset(buff, 0, MAX_BUFF);
       // write data to file
       while((ret = recv(client_fd, buff, MAX_BUFF, 0)) > 0)
       {
            syslog(LOG_INFO, "Received %d bytes, data:%s\n", ret, buff);
            ret = write(fd, buff, ret);
            if (buff[ret - 1] == '\n') {
                break;
            }

       }
       close(fd);

       printf("test ret: %d\n",ret);

       // open file again to read
       fd = open(LOG_FILE, O_RDONLY);
       if(fd == -1) 
       {
            syslog(LOG_ERR, "open error code %d\n", errno);
            close(client_fd);
            close(server_fd);
            return -1;    
       }
       while ((ret = read(fd, buff, MAX_BUFF)) > 0)
       {
            syslog(LOG_INFO, "Read %d bytes, data:%s\n", ret, buff);
            if(send(client_fd, buff, ret, 0) == -1)
            {
                syslog(LOG_ERR, "send error code %d\n", errno);
                close(fd);
                close(client_fd);
                close(server_fd);
                return -1;
            }
            syslog(LOG_INFO, "Sent %d bytes, data:%s\n", ret, buff);
       }
       close(fd);
       close(client_fd);
    }

    return 0;
}