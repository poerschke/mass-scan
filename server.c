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
#include <mysql.h>

#define BUFFER_SIZE 1024 * 1024
#define THREADS 20
#define TIMEOUT 2

const int  queue_s = 100000;




void parser2(char *string);
int socket_connect(char *host, in_port_t port);
int split(char *str);
void head(char *host, int port, char *server);
void *worker(void *arg);
void grava(char arquivo[], char ip[], int porta, char server[]);



char *server = "localhost";
char *user = "root";
char *password = ""; /* set me first */
char *database = "dados";




int main(int argc, char *argv[]){


        pthread_t th[THREADS];
        MYSQL *conn;
        MYSQL_RES *res;
        MYSQL_ROW row;
        int z,i;
        char str[500];
        char ip[30];
        char lista[6000][30];
        void *buffer[queue_s];
        queue_t queue = QUEUE_INITIALIZER(buffer);




        conn = mysql_init(NULL);

        /* Connect to database */
        if (!mysql_real_connect(conn, server, user, password,
                                      database, 0, NULL, 0)) {
                fprintf(stderr, "%s\n", mysql_error(conn));
                exit(1);
        }




       sprintf(str, "SELECT ip, porta FROM ips WHERE checado IS NULL ORDER BY id ASC LIMIT 5000");

        if (mysql_query(conn, str)) {
                fprintf(stderr, "%s\n", mysql_error(conn));
                exit(1);
        }


        res = mysql_use_result(conn);
        i=0;
        while ((row = mysql_fetch_row(res)) != NULL){
                sprintf(lista[i],"%s:%s", row[0], row[1]);
                queue_enqueue(&queue, lista[i]);
                i++;
        }

        mysql_free_result(res);
        mysql_close(conn);



        if(queue_size(&queue) > 0 ){
            for(z=0;z<THREADS;z++){
                pthread_create(&th[z], NULL, worker, &queue);
            }
            for(z=0;z<THREADS;z++){
                pthread_join(th[z], NULL);
            }
        }
        else{
                printf("Sem ips no banco de dados\n");

        }

}







void head(char *host, int port, char *server){
        int fd, n, res;
        int sock;
        char buffer2[BUFFER_SIZE];
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
                                        n = read(fd, buffer2, BUFFER_SIZE - 1);
                                        parser2(buffer2);
                                        strncpy(server, buffer2, 2000);
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
        int port;
        char sql[1000];
        MYSQL *conn;
        MYSQL_RES *res;
        MYSQL_ROW row;
        conn = mysql_init(NULL);

      if (!mysql_real_connect(conn, server, user, password,
                                      database, 0, NULL, 0)) {
                fprintf(stderr, "%s\n", mysql_error(conn));
                exit(1);
        }



        server = (char *)malloc(2000 * sizeof(char));
//      line = (char *)malloc(40 * sizeof(char));

        while(queue_size(arg) > 0){
            line = queue_dequeue(arg);
            port = split(line);
            head(line, port, server);

            if(strlen(line) > 6){

                        sprintf(sql, "UPDATE ips SET checado = 1, server = '%s' WHERE ip = '%s' AND porta = %d", server, line, port);
                        if (mysql_query(conn, sql)) {
                                fprintf(stderr, "%s\n", mysql_error(conn));
                                exit(1);
                        }

                        printf("http://%s:%d/|%s\n", line, port, server );
                }


        }
        mysql_close(conn);
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







int split(char *str){
        char *token;
        char *search = ":";
        str = strtok(str, search);
        return atoi(strtok(NULL, search));
}

