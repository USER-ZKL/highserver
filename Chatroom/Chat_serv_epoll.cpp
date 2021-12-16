#include<stdio.h>
#include<unistd.h>
#include<stdlib.h>
#include<netinet/in.h>
#include<arpa/inet.h>
#include<sys/socket.h>
#include<string.h>
#include<assert.h>
#include<poll.h>
#include<errno.h>
#include<fcntl.h>
#include<sys/epoll.h>
#define FD_LIMIT 65535
#define BUFFER_SIZE 1024
#define USER_LIMIT 5
#define MAX_EVENT_NUMBER 1024
struct client_data{
	struct sockaddr_in address;
	char *write_buf;
	char buf[BUFFER_SIZE];
};
int setnoblocking(int fd){
	int old_option = fcntl(fd, F_GETFL);
	int new_option = old_option | O_NONBLOCK;
	fcntl(fd, F_SETFL, new_option);
	return old_option;

}

void addfd(int epollfd, int fd, bool enable_et){
	epoll_event event;
	event.data.fd = fd;
	event.events = EPOLLIN | EPOLLOUT | EPOLLRDHUP | EPOLLERR;
	if(enable_et){
		event.events |= EPOLLET;
	}
	epoll_ctl(epollfd, EPOLL_CTL_ADD, fd, &event);
	setnoblocking(fd);
}
void lt(int number, struct epoll_event *events, int epollfd, int listenfd,int user_counter,struct client_data *users){
		int ret = 0;
		for(int i = 0; i < number; i++){
			int sockfd = events[i].data.fd;
			if(sockfd == listenfd){
				struct sockaddr_in client_address;
				socklen_t client_addrlength = sizeof(client_address);
				int connfd = accept(listenfd, (struct sockaddr*)&client_address, &client_addrlength);

				if(connfd < 0){
					printf("errno is %d\n",errno);
					continue;
				}

				if(user_counter >= USER_LIMIT){
					const char *info = "too many users\n";
					printf("%s\n",info);
					send(connfd, info, strlen(info), 0);
					continue;
				}

				user_counter++;
				users[connfd].address = client_address;
				addfd(epollfd, connfd, false);
				printf("comes a new users, now have %d users\n",user_counter);
			
			}
			else if(events[i].events & EPOLLERR){
				printf("get an error from %d\n",events[i].data.fd);
				char errors[100];
				memset(errors, '\0', 100);
				socklen_t length = sizeof(errors);
				if(getsockopt(events[i].data.fd, SOL_SOCKET, SO_ERROR, &errors, &length) < 0){
					printf("get sockopt error\n");
				}
				continue;
			}
			else if(events[i].events & EPOLLRDHUP){
				users[sockfd] = users[events[user_counter].data.fd];
				close(sockfd);
				epoll_ctl(epollfd, EPOLL_CTL_DEL, sockfd, NULL);	
				user_counter--;
				printf("a client left");
			
			}
			else if(events[i].events & POLLIN){
					memset(users[sockfd].buf,'\0',BUFFER_SIZE);
					ret = recv(sockfd, users[sockfd].buf, BUFFER_SIZE-1, 0);
					printf("get %d bytes of client data %s from %d\n",ret,users[sockfd].buf, sockfd);
					if(ret < 0){
						if(errno != EAGAIN){
							close(sockfd);
							users[sockfd] = users[events[user_counter].data.fd];
							epoll_ctl(epollfd, EPOLL_CTL_DEL, sockfd, NULL);
							user_counter--;
						}
					}
					else if(ret == 0){
					}
					else{
						for(int j = 0; j < number; ++j){
							int jfd = events[j].data.fd;
							if((jfd == sockfd ) || (jfd == listenfd))
								continue;
							//strcat(users[jfd].write_buf,users[sockfd].buf);
							users[jfd].write_buf = users[sockfd].buf;
						}
					}
	
				}
			else if(events[i].events & POLLOUT){
				if(!users[sockfd].write_buf){
					continue;
				}
				ret = send(sockfd, users[sockfd].write_buf, strlen(users[sockfd].write_buf), 0);
				users[sockfd].write_buf = NULL;
			
			}
		}




}
int main(int argc, char *argv[]){
	if(argc <= 2){
		printf("ipaddress, port");
	}

	
	const char *ip = argv[1];
	int port = atoi(argv[2]);

	struct sockaddr_in address;
	bzero(&address, sizeof(address));
	address.sin_family = AF_INET;
	inet_pton(AF_INET, ip, &address.sin_addr);
	address.sin_port = htons(port);
	
	int listenfd = socket(PF_INET, SOCK_STREAM, 0);
	assert(listenfd >= 0);
	int ret = bind(listenfd, (struct sockaddr*)&address, sizeof(address));
	assert(ret != -1);

	ret = listen(listenfd,5);
	assert(ret != -1);

	struct client_data *users = new client_data[FD_LIMIT];
	int user_counter = 0;
	
	int epollfd = epoll_create(USER_LIMIT + 1);
	assert(epollfd != -1);
	struct epoll_event event;
	event.data.fd = listenfd;
	event.events = EPOLLIN | EPOLLERR | EPOLLET;
	setnoblocking(listenfd);
	epoll_ctl(epollfd, EPOLL_CTL_ADD, listenfd, &event );
	struct epoll_event events[MAX_EVENT_NUMBER];
	
	while(1){
		int number = epoll_wait(epollfd,events,MAX_EVENT_NUMBER,-1);
		if(number < 0){
			perror("epoll_wait");
			printf("poll failure\n");
			break;
		}
		lt(number, events, epollfd, listenfd,user_counter,users);
	}
	delete[]users;
	close(listenfd);
	return 0;
}
