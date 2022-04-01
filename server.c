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
#include <mysql.h>

#define BUFFER_SIZE 1024 * 1024
#define MAX_TH 30

pthread_mutex_t lock;
pthread_cond_t cond;

char *server = "localhost";
char *user = "root";
char *password = ""; /* set me first */
char *database = "dados";


void InitPilha();
int pilhaVazia();
void empilha(char ip[30], int porta, int id);
void desempilha(char *ip, int *porta, int *id);
void * worker();
void parser2(char *string);
int socket_connect(char *host, in_port_t port);
void head(char *host, int port, char *banner, char *headers);
void removeChar(char * str, char charToRemmove);

struct node {
    int id;
    int porta;
    char ip[30];
    struct node * prox;
};
struct node *pilhaptr;


int main(){
    pthread_t th[30];
    int porta, id, z,i, ti;
    char ip[30];
    MYSQL *conn;
    MYSQL_RES *res;
    MYSQL_ROW row;

    char str[] = "SELECT id, ip, porta FROM ips WHERE checado IS NULL ORDER BY rand() ASC LIMIT 3000";

    conn = mysql_init(NULL);
    if (!mysql_real_connect(conn, server, user, password, database, 0, NULL, 0)) {
        fprintf(stderr, "%s\n", mysql_error(conn));
        exit(1);
    }

    if (mysql_query(conn, str)) {
        fprintf(stderr, "%s\n", mysql_error(conn));
        exit(1);
    }

    InitPilha();

    res = mysql_use_result(conn);
    while ((row = mysql_fetch_row(res)) != NULL){
        empilha(row[1], atoi(row[2]), atoi(row[0]));
    }

    mysql_free_result(res);
        mysql_close(conn);

    pthread_mutex_init(&lock, NULL);
    pthread_cond_init(&cond, NULL);

    if(pilhaVazia() != 1){
        for(ti = 0; ti <MAX_TH; ti++){
            pthread_create(&th[ti], NULL, worker, NULL);
        }
        for(ti = 0; ti <MAX_TH; ti++){
            pthread_join(th[ti], NULL);
        }
    }

    sleep(3);
    return 0;
}


void InitPilha(){
    pilhaptr = NULL;
}


int pilhaVazia(){
    if(pilhaptr == NULL) return 1;
    else return 0;
}


void empilha(char ip[30], int porta, int id){
        struct node *p;
        p = malloc(sizeof(struct node));
        p->id = id;
    p->porta = porta;
        strcpy(p->ip, ip);
        p->prox = pilhaptr;
        pilhaptr = p;
}


void desempilha(char *ip, int *porta, int *id){
    struct node *p;
    p = pilhaptr;
    if(p==0) printf("Pilha vazia\n");
    else{
        pilhaptr = p->prox;
        strcpy(ip, p->ip);
        memcpy(porta, &p->porta, sizeof(int));
        memcpy(id, &p->id, sizeof(int));
    }
}



void * worker(){
        char banner[2000], headers[4000], sql[7000], ip[30];
        int porta, id;
    MYSQL *sc;
        sc = mysql_init(NULL);

    if (!mysql_real_connect(sc, server, user, password, database, 0, NULL, 0)) {
        fprintf(stderr, "%s\n", mysql_error(sc));
        exit(1);
    }


    while(pilhaVazia() != 1){
        pthread_mutex_lock(&lock);
            desempilha(ip, &porta, &id);
        pthread_cond_signal(&cond);
        pthread_mutex_unlock(&lock);

        head(ip, porta, banner, headers);
        sprintf(sql, "UPDATE ips SET checado = 1, server = '%s', headers = '%s' WHERE id = %d", banner, headers, id);

                if (mysql_query(sc, sql)) {
            fprintf(stderr, "%s\n", mysql_error(sc));
printf("%s\n", sql);
            exit(1);
        }
    }
    mysql_close(sc);
}



void head(char *host, int port, char *banner, char *headers){
    int fd, n, res, sock, max = 1900, max2=3999;
    char *ret, req[200], buffer2[BUFFER_SIZE];

    fd_set sockets;
    struct timeval tempo;
    tempo.tv_sec = 5;
    tempo.tv_usec = 0;

    fd = socket_connect(host, port);
    if(fd != -1){

        sprintf(req, "HEAD / HTTP/1.1\r\nAccept: */*\r\nUser-Agent: Internet Fuzzer/0.1\r\nHost: %s:%d\r\nConnection: close\r\n\r\n", host,port);
        write(fd, req, strlen(req));

        for(;;){

            FD_ZERO(&sockets);
            FD_SET(fd , &sockets);
            res = select(fd+1, &sockets, &sockets, (fd_set *)0, &tempo);
            switch(res){
                case 0:
                    strncpy(banner, "Desconhecido", 12);
                                banner[12] = '\0';
                                strncpy(headers, "0", 1);
                                headers[1]= '\0';
                    break;
                case -1:
                    strncpy(banner, "Desconhecido", 12);
                                banner[12] = '\0';
                                strncpy(headers, "0", 1);
                                headers[1] ='\0';
                    break;
                default:
                    n = read(fd, buffer2, BUFFER_SIZE - 1);
                    removeChar(buffer2, '\'');
                            if(strlen(buffer2) > max2){
                        strncpy(headers, buffer2, 3999);
                                    headers[3999]= '\0';
                                }
                            else{
                                    strncpy(headers, buffer2, strlen(buffer2));
                                    headers[strlen(buffer2)] ='\0';
                                }

                    parser2(buffer2);
                            if(strlen(buffer2) > max){
                            strncpy(banner, buffer2, 1999);
                                    banner[1999] ='\0';
                                }
                            else{
                                    strncpy(banner, buffer2, strlen(buffer2));
                                    banner[strlen(buffer2)]='\0';
                                }
                    break;
            }
        break;
        }
    shutdown(fd, SHUT_RDWR);
    close(fd);
    }
}

void parser2(char *string){
    char *primeiro, *segundo, *ret=NULL;
    int pos = 0, x = 0, y=0, tam =0;

    primeiro = strstr(string, "Server:");
    segundo = strtok(primeiro, "\n");

    if(primeiro != NULL && segundo != NULL){
        strtok(segundo, "\r");
            if(strlen(segundo) > 1999){
                strncpy(string, segundo, 1999);
                string[1999] = '\0';
        }
            else{
                strncpy(string, segundo, strlen(segundo));
            string[strlen(segundo)] = '\0';
            }
    }
    else{
        strncpy(string, "Desconhecido", 15);
    }
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


void removeChar(char * str, char charToRemmove){
    int i, j;
    int len = strlen(str);
    for(i=0; i<len; i++)
    {
        if(str[i] == charToRemmove)
        {
            for(j=i; j<len; j++)
            {
                str[j] = str[j+1];
            }
            len--;
            i--;
        }
    }
}
