
#define _GNU_SOURCE 1
#include<stdio.h>
#include<netinet/in.h>
#include<stdlib.h>
#include<string.h>
#include<arpa/inet.h>
#include<assert.h>
#include<sys/socket.h>
#include<unistd.h>
#include<sys/epoll.h>
#include<fcntl.h>
#include<stdbool.h>
#include<sys/uio.h>
#include<poll.h>
#define MAX_EVENT_NUMBER 1024
#define BUFFER_SIZE 64
int main(int argc, char *argv[]){
	if(argc <= 2){
		printf("ipaddress port\n");
		return 0;
	}

	const char *ip = argv[1];
	int port = atoi(argv[2]);

	struct sockaddr_in server_address;
	bzero(&server_address,sizeof(server_address));
	server_address.sin_family = AF_INET;
	inet_pton(AF_INET, ip, &server_address.sin_addr);
	server_address.sin_port = htons(port);
	
	int sockfd = socket(PF_INET, SOCK_STREAM, 0);
	assert(sockfd >= 0);
	
	if(connect(sockfd, (struct sockaddr*)&server_address, sizeof(server_address)) < 0){
		printf("connection failed\n");
		close(sockfd);
		return 1;
	}

	struct pollfd fds[2];
	fds[0].fd = 0;
	fds[0].events = POLLIN;
	fds[0].revents = 0;
	fds[1].fd = sockfd;
	fds[1].events = POLLIN | POLLRDHUP;
	fds[1].revents = 0;
	char read_buf[BUFFER_SIZE];
	char write_buf[BUFFER_SIZE];
	char total[BUFFER_SIZE + 10];
	char name[10];
	printf("please enter your name:");
	scanf("%s",name);
	
	int pipefd[2];
	int ret = pipe(pipefd);
	assert(ret != -1);

	while(1){
		ret = poll(fds, 2, -1);
		if(ret < 0){
			printf("poll failed\n");
			break;
		}

		if(fds[1].revents & POLLRDHUP){
			printf("server close connect\n");
			break;
		}else if(fds[1].revents & POLLIN){
			memset(read_buf, '\0', BUFFER_SIZE);
			recv(fds[1].fd, read_buf, BUFFER_SIZE-1, 0);
			printf("%s",read_buf);
		}

		if(fds[0].revents & POLLIN){
			//ret = splice(0,NULL, pipefd[1], NULL, 32768, SPLICE_F_MORE | SPLICE_F_MOVE);
			//ret = splice(pipefd[0], NULL, sockfd, NULL, 32768, SPLICE_F_MORE | SPLICE_F_MOVE);
				
				
			memset(total,'\0', sizeof(total));
			memset(write_buf,'\0', sizeof(write_buf));
			read(fds[0].fd,write_buf,BUFFER_SIZE-1);
			strcat(total,name);
			strcat(total,": ");
			strcat(total,write_buf);
			send(sockfd, total, strlen(total), 0);
		}
	}

	close(sockfd);
	return 0;
}
