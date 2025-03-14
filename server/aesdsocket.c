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
#include <sys/queue.h>
#include <pthread.h>



#define PORT "9000"
#define MAX_CONNECTIONS 10
#define LOG_FILE "/var/tmp/aesdsocketdata"
#define MAX_BUFF 1024

int server_fd = -1 , client_fd = -1;
int program_terminated = 0;
pthread_mutex_t file_lock = PTHREAD_MUTEX_INITIALIZER;

// link list
struct thread_node{
    LIST_ENTRY(thread_node) entries;
    pthread_t thread_id;
    int client_fd;
};

LIST_HEAD(thread_list, thread_node) thread_list_head;

void sigint_handler(int sig)
{
    syslog(LOG_INFO, "Caught signal %d\n", sig);
    program_terminated = 1;
    // if(server_fd != -1)
    // {
    //     close(server_fd);
    // }
    // if(client_fd != -1)
    // {
    //     close(client_fd);
    // }

    // remove(LOG_FILE);

    // closelog();
    // exit(0);
}

void *serve_client(void *arg)
{
    struct thread_node *this_thead_data = (struct thread_node *)arg;
    char buff[MAX_BUFF]; 
    int ret;
    int client_fd = this_thead_data->client_fd;

    memset(buff, 0, MAX_BUFF);


    pthread_mutex_lock(&file_lock);
    int fd = open(LOG_FILE, O_CREAT | O_WRONLY | O_APPEND, 0666);
    if(fd == -1)
    {
        syslog(LOG_ERR, "open error code %d\n", errno);
        close(client_fd);
        return NULL;
    }

    // log client data
    
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
        return NULL;    
    }
    while ((ret = read(fd, buff, MAX_BUFF)) > 0)
    {
        syslog(LOG_INFO, "Read %d bytes, data:%s\n", ret, buff);
        if(send(client_fd, buff, ret, 0) == -1)
        {
            syslog(LOG_ERR, "send error code %d\n", errno);
            close(fd);
            close(client_fd);
            return NULL;
        }
        syslog(LOG_INFO, "Sent %d bytes, data:%s\n", ret, buff);
    }
    close(fd);
    pthread_mutex_unlock(&file_lock);
    
    // free resources
    close(client_fd);
    LIST_REMOVE(this_thead_data, entries);
    free(this_thead_data);

    return NULL;
}

void *get_in_addr(struct sockaddr *sa)
{
    if(sa->sa_family == AF_INET)
    {
        return &(((struct sockaddr_in *)sa)->sin_addr);
    }

    return &(((struct sockaddr_in6 *)sa)->sin6_addr);
}

void *timestamp_thread(void *arg)
{
    while (!program_terminated)
    {
        // print timestamp every 10 seconds, RFC 2822 compliant
        sleep(10);
        time_t rawtime = time(NULL);
        struct tm *timeinfo = localtime(&rawtime);
        if (!timeinfo)
        {
            syslog(LOG_ERR, "localtime error code %d\n", errno);
            return NULL;
        }
        char buffer[80];
        strftime(buffer, 80, "timestamp: %a, %d %b %Y %T %z\n", timeinfo);
        // write to LOG_FILE with mutex lock
        pthread_mutex_lock(&file_lock);
        int fd = open(LOG_FILE, O_CREAT | O_WRONLY | O_APPEND, 0666);
        if(fd == -1)
        {
            syslog(LOG_ERR, "open error code %d\n", errno);
            return NULL;
        }
        int ret = write(fd, buffer, strlen(buffer));
        if(ret == -1)
        {
            syslog(LOG_ERR, "write error code %d\n", errno);
            close(fd);
            return NULL;
        }
        close(fd);
        pthread_mutex_unlock(&file_lock); 
    }
    return NULL;
}

int main(int argc, char *argv[])
{
    struct addrinfo hints, *result;
    char buff[MAX_BUFF];
    
    // initialize the list
    LIST_INIT(&thread_list_head);

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
    int ret;
    if (argc >= 2 && strcmp(argv[1], "-d") == 0)
    {
        syslog(LOG_INFO, "Daemonizing the server\n");
        ret = daemon(0, 0);
        if(ret == -1)
        {
            syslog(LOG_ERR, "daemon error code %d\n", errno);
            return -1;
        }
    }
    
    // get address info for server
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

    pthread_t timestamp_thread_id;
    if(pthread_create(&timestamp_thread_id, NULL, timestamp_thread, NULL) != 0)
    {
        syslog(LOG_ERR, "pthread_create error code %d\n", errno);
        close(server_fd);
        return -1;
    }

    while (!program_terminated)
    {
        // accept connection
        client_fd = accept(server_fd, (struct sockaddr *)&client_addr, &addr_size);
        if(client_fd == -1)
        {
            syslog(LOG_ERR, "accept error code %d\n", errno);
            // close(server_fd);
            continue;
        }

        struct thread_node *new_thread_node = (struct thread_node *)malloc(sizeof(struct thread_node));

        inet_ntop(client_addr.ss_family, get_in_addr((struct sockaddr *)&client_addr), buff, sizeof(buff));
        syslog(LOG_INFO, "Connection from %s\n", buff);
        if ( !new_thread_node )
        {
            syslog(LOG_ERR, "malloc error code %d\n", errno);
            close(client_fd);
            continue;
        }
        
        new_thread_node->client_fd = client_fd;
        LIST_INSERT_HEAD(&thread_list_head, new_thread_node, entries);
        ret = pthread_create(&new_thread_node->thread_id, NULL, serve_client, (void *)new_thread_node);
        if(ret != 0)
        {
            syslog(LOG_ERR, "pthread_create error code %d\n", errno);
            close(client_fd);
            LIST_REMOVE(new_thread_node, entries);
            free(new_thread_node);
            continue;
        }
    }



    // clean up
    if(server_fd != -1)
    {
        close(server_fd);
    }
    if(client_fd != -1)
    {
        close(client_fd);
    }

    remove(LOG_FILE);
    
    while (!LIST_EMPTY(&thread_list_head))
    {
        struct thread_node *node = LIST_FIRST(&thread_list_head);
        pthread_cancel(node->thread_id);
        pthread_join(node->thread_id, NULL);
        LIST_REMOVE(node, entries);
        free(node);
    }

    pthread_cancel(timestamp_thread_id);
    pthread_join(timestamp_thread_id, NULL);
    
    closelog();
    return 0;
}