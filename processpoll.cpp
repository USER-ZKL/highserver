#ifndef PROCESSPOOL
#define PROCESSPOOL
#include<unistd.h>
//subprocess
class process{
public:
	process():m_pid(-1){}
public:
	pid_t m_pid;	//subprocess pid
	int m_pipefd[2];//pipe communicate between main process and sub process
}

template< typename T>
class processpool{
private:
	processpool(int listenfd, int process_number = 8);//private constructor in order to crate processpool only through other function
public:
	static processpool<T>* create(int listenfd, int process_number = 8){	//one mode (static) make create processpool only one
		if(!m_instance){	//if m_instance is null then new a processpool
			m_instance = new processpool<T>(listenfd,process_number);
		}
		return m_instance;
	}
	~processpool(){
		delete []m_sub_process;
	}
	//start processpool
	void run();
private:
	void setup_sig_pipe();
	void run_parent();
	void run_child();
private:
	static const int MAX_PROCESS_NUMBER = 16;
	//the max number of client each subprocess can process
	static const int USER_PER_PROCESS = 65536;
	//the max number of event epoll can process
	static const int MAX_EVENT_NUMBER = 10000;
	//the number  of process in processpool
	int m_process_number;
	//the idex of subprocess in processpool start of 0
	int m_idx;
	int m_listenfd;
	//subprocess decide whether stop or not through m_stop
	int m_stop;
	//the subscribe of all subprocess
	process* m_sub_process;
	//the static example of processpool
	static processpool<T>* m_instance;
}
template<typename T>
processpool<T>* processpool<T>::m_instance = NULL;

static int sig_pipefd[2];	//the pipe to process signal,to unified event source

//set noblock
static int setnoblocking(int fd){
	int old_optition = fcntl(fd,F_GETFL);
	int new_optition = old_optition | O_NONBLOCK;
	fcntl(fd,F_SETFL,new_optition);
	return old_optition;
}

//add fd event to epollfd
static void addfd(int epollfd,int fd){
	epoll_event event;
	event.data.fd = fd;
	event.events = EPOLLIN | EPOLLET;
	epoll_ctl(epollfd,EPOLL_CTL_ADD,fd,&event);
	setnoblocking(fd);
}

//remove fd events from epollfd
static void removefd(int epollfd,int fd){
	epoll_ctl(epollfd, EPOLL_CTL_DEL, fd, 0);
	close(fd);
}

static void sig_handler(int sig){
	int save_errno = errno;
	int msg = sig;
	send(sig_pipe[1], (char*)&msg, 1, 0);
	errno = save_errno;
}

static void addsig(int sig, void(handler)(int), bool restart = true){
	struct sigaction sa;
	memset(&sa,'\0',sizeof(sa));
	sa.sa_handler = handler;
	if(restart){
		sa.sa_flags |= SA_RESTART;
	}
	sigfillset(&sa.sa_mask);
	assert(sigaction(sig,&sa,NULL)!=-1)'
}

//the constructor of processpool
template<typename T>
processpool<T>::processpool(int listenfd, int process_number)
				:m_listenfd(listenfd),m_process_number(process_number),m_idx(-1),m_stop(false){
				
	assert((process_number > 0) && (process_number <= MAX_PROCESS_NUMBER));
	m_sub_process = new process[process_number];
	assert(m_sub_process);	
	
	//create process_number process and create pipe batween main process
	for(int i = 0; i < process_number; i++){
		int ret = socketpair(PF_UNIX, SOCK_STREAM, 0, m_sub_process[i].m_pipefd);
		assert(ret = 0);

		m_sub_process[i].m_pid = fork();
		assert(m_sub_process[i].m_pid >= 0);
		if(m_sub_process[i].m_pid > 0){
			close(m_sub_process[i].pipefd[1]);	//close write end in main process
			continue;
		}else{
			close(m_sub_process[i].pipefd[0]);	//close read end in subprocess
			m_idx = i;
			break;
		}
	
	}
}

// unified events sources
template<typename T>
void processpool<T>::setup_sig_pipe(){
	//create epoll events lists and signal pipe
	m_epolled = epoll_create(5)
	assert(m_epolled!= -1)

	int ret = socketpair(PF_UNIX, SOCK_STREAM, 0, sig_pipefd);
	assert(ret != -1);

	setnoblocking(sig_pipefd[1]);
	addfd(m_epollfd, sig_pipefd[0]);
	
	//set handle fun of signal 
	addsig(SIGCHLD, sig_handler);
	addsig(SIGTERM, sig_handler);
	addsig(SIGINT, sig_handler);
	addsig(SIGPIPE, SIG_IGN);
}

template<typename T>
void processpool<T>::run(){
	if(m_idx  != -1){
		run_child();
		return;
	}
	run_parent();
}

template<typename T>
void processpool<T>::run_child(){
	setup_sig_pipe();
	//subprocess get pipe communicate between main process through its m_idx	
	int pipefd = m_sub_process[m_idx].m_pipefd[1];
	//subprocess need to listen pipefd, main process can call subprocess to accept a new lianjie through this pipe
	addfd(m_epollfd, pipefd);

	epoll_event events[MAX_EVENT_NUMBER];
	T* users = new T[USER_PER_PROCESS];
	assert(users);
	int number = 0;
	int ret = -1;
	while(!m_stop){
		number = epoll_wait(m_epollfd, events, MAX_EVENT_NUMBER, -1);
		if((number < 0) && (errno != EINTR)){
			printf("epoll failure\n");
		}
	
	
	}
}
#endif
