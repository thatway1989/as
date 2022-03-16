 #include <stdio.h>
 #include <sys/socket.h>
 #include <sys/types.h>
 #include <string.h>
 #include <netinet/in.h>
 #include <stdlib.h>
 #include <errno.h>
 #include <unistd.h>
 #include <arpa/inet.h>
 #include "doip.h"
 
 #define MAXLINE 1024
 int main(int argc,char **argv)
 {
    int listenfd,connfd;
    struct sockaddr_in sockaddr;
    unsigned char buff[MAXLINE];
    int n;

    memset(&sockaddr,0,sizeof(sockaddr));

    sockaddr.sin_family = AF_INET;
    sockaddr.sin_addr.s_addr = htonl(INADDR_ANY);
    sockaddr.sin_port = htons(10004);

    listenfd = socket(AF_INET,SOCK_STREAM,0);

    bind(listenfd,(struct sockaddr *) &sockaddr,sizeof(sockaddr));

    listen(listenfd,1024);


    printf("Please wait for the client information\n");

    for(;;)
    {
        if((connfd = accept(listenfd,(struct sockaddr*)NULL,NULL))==-1)
        {
            printf("accpet socket error: %s errno :%d\n",strerror(errno),errno);
            continue;
        }
        n = recv(connfd,buff,sizeof(buff),0);
        printf("receive n = %d\n",n);
        buff[n] = '\0';
        printf("recv msg from client:%s",buff);
        for(int i = 0;i < 20;i++ )
        {
            printf("%x ",buff[i]);
        }
        DoIP_RxTcp_HandleCopyRxDataCopyBody(buff);
        close(connfd);
    }
    close(listenfd);
 }