#include <stdio.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <stdlib.h>
#include <netinet/in.h>
#include <errno.h>
#include <string.h>
#include <arpa/inet.h>
#include <unistd.h>
#include "doip.h"

#define MAX_LEN 32

int main(int argc,char **argv)
{
    int socketfd;
    struct sockaddr_in sockaddr;
    unsigned char sendline[MAX_LEN] = {0};
    unsigned char data[2][16] = {
		{0xBE,0xEF,0xDA,0x00,0x00,0x00},
		{0xBE,0xEF,0xFE,0xED,0x11,0x01}
	};
    int send_num,send_len,id;

    char *servInetAddr = "172.18.0.200";
	int port = 13400;
	int tcp_flag = 0;

    if(argc != 5){
		printf("DoIP:argc is %d, use default config!\n", argc);
    }else {
		servInetAddr = argv[1];
    	port = atoi(argv[2]);
		if(strcmp(argv[3],"tcp") == 0){
			tcp_flag = 1;
		}
    	id = atoi(argv[4]);
		printf("DoIP:port=%d, id=%d\n", port, id);
    }

    memset(&sockaddr,0,sizeof(sockaddr));
    sockaddr.sin_port = htons(port);
    sockaddr.sin_family = AF_INET;

	if(tcp_flag == 1){
		printf("DoIP:send packet to %s %d tcp\n",servInetAddr, port);
		socketfd = socket(AF_INET,SOCK_STREAM,0);
		inet_pton(AF_INET,servInetAddr,&sockaddr.sin_addr);

		if((connect(socketfd,(struct sockaddr*)&sockaddr,sizeof(sockaddr))) < 0 )
		{
			printf("connect error %s errno: %d\n",strerror(errno),errno);
			exit(0);
		}
	} else {
		printf("DoIP:send packet to %s %d udp\n",servInetAddr, port);
		socketfd = socket(AF_INET,SOCK_DGRAM,0);
		sockaddr.sin_addr.s_addr = inet_addr(servInetAddr);
	}

	if(id == 0){ //acvive route
		send_len = 7;
		DoIP_Util_FillGenHdr(DOIP_HDR_TYPE_ROUT_ACTIV_REQ, send_len,sendline);
	} else { //diag req
		send_len = 6;
		DoIP_Util_FillGenHdr(DOIP_HDR_TYPE_DIAG_REQ, send_len,sendline);
	}

    for(int i = 0;i < send_len;i++ )
		sendline[i+8] = data[id][i];
    
    printf("send begin\n");
	send_len += 8;
    for(int i = 0;i < send_len;i++ )
		printf("%02x ",sendline[i]);

	if(tcp_flag == 1){
		send_num = send(socketfd,sendline,send_len,0);
	} else {
		send_num = sendto(socketfd, sendline, send_len,0,(struct sockaddr *)&sockaddr, sizeof(sockaddr));
	}

    if(send_num < 0) {
      printf("send mes error: %s errno : %d",strerror(errno),errno);
      exit(0);
    }
    printf("\nsend end, send_num = %d\n",send_num);
    close(socketfd);
    printf("exit!!!\n\n");
    exit(0);
}
