#include <arpa/inet.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <fcntl.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <errno.h>
#include <unistd.h>
#include <signal.h>
#include <poll.h>
#include <netinet/tcp.h>
#include <netdb.h>
#include <pthread.h>
#include "queue.h"


#define BUFFER_SIZE 1024 * 1024
#define THREADS 20
#define MAX_SOCKETS 512
#define TIMEOUT 2
#define S_NONE       0
#define S_CONNECTING 1


struct conn_t {
    int s;
    char status;
    time_t a;
    struct sockaddr_in addr;
};
struct conn_t connlist[MAX_SOCKETS];



const int queue_s = 100000;

void init_sockets(void);
void check_sockets(void);
void fatal(char *);
void parser2(char *string);
int socket_connect(char *host, in_port_t port);
void head(char *host, int port, char *server);
void *worker(void *arg);
void grava(char arquivo[], char ip[], int porta, char server[]);



FILE *outfd;
int tot = 0;
int port = 0;




int main(int argc, char *argv[]){
    char *server;
    FILE *log;
    char *line[queue_s];
    char * li = NULL;
    size_t len = 0;
    ssize_t read;
    void *buffer[queue_s];
    queue_t queue = QUEUE_INITIALIZER(buffer);    
    int z = 0;
    pthread_t th[THREADS];
    int done = 0, i, cip = 1, bb = 0, ret, k, ns, x;
    time_t scantime;
    char ip[20], outfile[128], last[256];

    port = atoi(argv[2]);


    if (argc < 3)
    {
        printf("Usage: %s <b-block> <port> [c-block]\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    memset(&outfile, 0, sizeof(outfile));
    if (argc == 3){
        snprintf(outfile, sizeof(outfile) - 1, "scan.log", argv[1], argv[2]); }
    else if (argc >= 4)
    {
        snprintf(outfile, sizeof(outfile) - 1, "scan.log", argv[1], argv[3], argv[2]);
        bb = atoi(argv[3]);
        if ((bb < 0) || (bb > 255))
            fatal("Invalid b-range.\n");
    }
    //strcpy(argv[0],"/bin/bash");
    if (!(outfd = fopen(outfile, "a")))
    {
        perror(outfile);
        exit(EXIT_FAILURE);
    }
    printf("# scanning: ");
    fflush(stdout);

    memset(&last, 0, sizeof(last));
    init_sockets();
    scantime = time(0);

    while(!done)
    {
        for (i = 0; i < MAX_SOCKETS; i++)
        {
            if (cip == 255)
            {
                if ((bb == 255) || (argc >= 4))
                {
                    ns = 0;
                    for (k = 0; k < MAX_SOCKETS; k++)
                    {
                        if (connlist[k].status > S_NONE)
                        {
                            ns++;
                            break;
                        }
                    }

                    if (ns == 0)
                        done = 1;

                     break;
                }
                else
                {
                    cip = 0;
                    bb++;
                    for (x = 0; x < strlen(last); x++)
                        putchar('\b');
                    memset(&last, 0, sizeof(last));
                    snprintf(last, sizeof(last) - 1, "%s.%d.* (total: %d) (%.1f%% done)", argv[1], bb, tot, (bb / 255.0) * 100);
                    printf("%s", last);
                    fflush(stdout);
                }
            }

            if (connlist[i].status == S_NONE)
            {
                connlist[i].s = socket(AF_INET, SOCK_STREAM, 0);
                if (connlist[i].s == -1)
                    printf("Unable to allocate socket.\n");
                else
                {
                    ret = fcntl(connlist[i].s, F_SETFL, O_NONBLOCK);
                    if (ret == -1)
                    {
                        printf("Unable to set O_NONBLOCK\n");
                        close(connlist[i].s);
                    }
                    else
                    {
                        memset(&ip, 0, 20);
                        sprintf(ip, "%s.%d.%d", argv[1], bb, cip);
                        connlist[i].addr.sin_addr.s_addr = inet_addr(ip);

                        if (connlist[i].addr.sin_addr.s_addr == -1)
                            fatal("Invalid IP.");
                        connlist[i].addr.sin_family = AF_INET;
                        connlist[i].addr.sin_port = htons(atoi(argv[2]));
                        connlist[i].a = time(0);
                        connlist[i].status = S_CONNECTING;
                        cip++;
                    }
                }
            }
        }
        check_sockets();
    }

    printf("\n# pscan completed in %u seconds. (found %d ips)\n", (time(0) - scantime), tot);
    fclose(outfd);
    sleep(30);









    
    
    log = fopen("scan.log", "r");
    
    if(log == NULL){
    	exit(EXIT_FAILURE);
    }
    z=0;
    while((read = getline(&li, &len, log)) != -1){
    	li[strcspn(li, "\r\n")] = 0;
    	line[z] = strdup(li); 
    	queue_enqueue(&queue, line[z]);
    	z++;
    }	
    
    fclose(log);
    
    
    for(z=0;z<THREADS;z++){
    	pthread_create(&th[z], NULL, worker, &queue);
    }

    for(z=0;z<THREADS;z++){
    	pthread_join(th[z], NULL);
    }


}
       
       
       
       
       
       
       
void head(char *host, int port, char *server){
        int fd, n, res;
        int sock;
        char buffer[BUFFER_SIZE];
        char *ret;
        int max;
        fd_set sockets;

        struct timeval tempo;

        tempo.tv_sec = 5;
        tempo.tv_usec = 0;

        fd = socket_connect(host, port);
        if(fd != -1){
                write(fd, "HEAD / HTTP/1.0\r\n\r\n", strlen("HEAD / HTTP/1.0\r\n\r\n"));
                for(;;){

                        FD_ZERO(&sockets);
                        FD_SET(fd , &sockets);

                        //res = select(fd+1, &sockets, (fd_set *)0, (fd_set *)0, &tempo);
                        res = select(fd+1, &sockets, &sockets, (fd_set *)0, &tempo);
                        switch(res){
                                case 0:
                                        strncpy(server, "Desconhecido", 100);
                                        shutdown(fd, SHUT_RDWR);
                                        close(fd);
                                        break;
                                case -1:
                                        strncpy(server, "Desconhecido", 100);
                                        shutdown(fd, SHUT_RDWR);
                                        close(fd);
                                        break;
                                default:
                                        n = read(fd, buffer, BUFFER_SIZE - 1);
                                        parser2(buffer);
                                        strncpy(server, buffer, 2000);
                                        shutdown(fd, SHUT_RDWR);
                                        close(fd);
                                        break;
                        }
                        break;
                        }
                }
        }       
        
        
        
        
        
        
        
void parser2(char *string){
        char *primeiro;
        char *segundo;
        char *ret=NULL;
        int pos = 0, x = 0, y=0, tam =0;

        primeiro = strstr(string, "Server:");
        segundo = strtok(primeiro, "\n");
        if(primeiro != NULL && segundo != NULL){
                strtok(segundo, "\r");
                strncpy(string, segundo, BUFFER_SIZE);

        }
        else{
                strncpy(string, "Desconhecido", 15);
        }
}        










void *worker(void *arg){
	char *server;
	char *line;

	
	server = (char *)malloc(2000 * sizeof(char));

	while(queue_size(arg) > 0){
	    line = queue_dequeue(arg);
	    head(line, port, server);
	
	    if(strlen(line) > 6){
                        printf("http://%s:%d/|%s\n", line, port, server );
                        if(strstr(server, "Desconhecido") != NULL)
                                grava("desconhecido.txt", line, port, server);
                        else if(strstr(server, "micro_httpd") != NULL)
                                grava("micro_httpd.txt", line, port, server);
                        else if(strstr(server, "nginx") != NULL)
                                grava("nginx.txt", line, port, server);
                        else if(strstr(server, "Microsoft-IIS") != NULL)
                                grava("ms.txt", line, port, server);
                        else if(strstr(server, "lighttpd") != NULL)
                                grava("lighttpd.txt", line, port, server);
                        else if(strstr(server, "Apache") != NULL)
                                grava("Apache.txt", line, port, server);
                        else if(strstr(server, "mini_httpd") != NULL)
                                grava("mini_httpd.txt", line, port, server);
                        else
                                grava("outros.txt", line, port, server);
		}
	
	
	}
}




void grava(char arquivo[], char ip[], int porta, char server[]){
        FILE *fp;

        if((fp = fopen(arquivo, "a+")) != NULL){
                fprintf(fp, "http://%s:%d/|%s\n", ip, porta, server);
                if(fclose(fp)){
                        printf("Erro ao fechar %s\n", arquivo);
                }
        }
}





void init_sockets(void)
{
    int i;

    for (i = 0; i < MAX_SOCKETS; i++)
    {
        connlist[i].status = S_NONE;
        memset((struct sockaddr_in *)&connlist[i].addr, 0, sizeof(struct sockaddr_in));
    }
    return;
}





void check_sockets(void)
{
    int i, ret;

    for (i = 0; i < MAX_SOCKETS; i++)
    {
        if ((connlist[i].a < (time(0) - TIMEOUT)) && (connlist[i].status == S_CONNECTING))
        {
            close(connlist[i].s);
            connlist[i].status = S_NONE;
        }
        else if (connlist[i].status == S_CONNECTING)
        {
            ret = connect(connlist[i].s, (struct sockaddr *)&connlist[i].addr, sizeof(struct sockaddr_in));
            if (ret == -1)
            {
                if (errno == EISCONN)
                {
                    tot++;
                    fprintf(outfd, "%s\n", (char *)inet_ntoa(connlist[i].addr.sin_addr));
                    close(connlist[i].s);
                    connlist[i].status = S_NONE;
                }

                if ((errno != EALREADY) && (errno != EINPROGRESS))
                {
                    close(connlist[i].s);
                    connlist[i].status = S_NONE;
                }
            }
            else
            {
                tot++;
                fprintf(outfd, "%s\n",(char *)inet_ntoa(connlist[i].addr.sin_addr));
                close(connlist[i].s);
                connlist[i].status = S_NONE;
            }
        }
    }
}





void fatal(char *err)
{
    int i;
    printf("Error: %s\n", err);
    for (i = 0; i < MAX_SOCKETS; i++)
        if (connlist[i].status >= S_CONNECTING)
            close(connlist[i].s);
    fclose(outfd);
    exit(EXIT_FAILURE);
}





int socket_connect(char *host, in_port_t port){
        struct hostent *hp;
        struct sockaddr_in addr;
        struct timeval tv;
        int on = 1, sock;
        if((hp = gethostbyname(host)) == NULL){
                return -1;
        }
        else {
                bcopy(hp->h_addr, &addr.sin_addr, hp->h_length);
                addr.sin_port = htons(port);
                addr.sin_family = AF_INET;
                sock = socket(PF_INET, SOCK_STREAM, 0);
//              fcntl(sock, F_SETFL, O_NONBLOCK);
                tv.tv_sec = 5;
                tv.tv_usec = 0;
                setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, (char *)&tv, sizeof(tv));
                setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, (char *)&tv, sizeof(tv));
                if(sock == -1){
                        return -1;
                }
                if(connect(sock, (struct sockaddr *)&addr, sizeof(struct sockaddr_in)) == -1){
                        return -1;
                }
                return sock;
        }
}
