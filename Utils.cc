#include "Utils.h"

void utils::init(int timeslot){
    m_TIMESLOT=timeslot;
}
//文件描述符标志、文件状态标志
//https://blog.csdn.net/kyang_823/article/details/79496362?utm_medium=distribute.pc_relevant_t0.none-task-blog-BlogCommendFromMachineLearnPai2-1.channel_param&depth_1-utm_source=distribute.pc_relevant_t0.none-task-blog-BlogCommendFromMachineLearnPai2-1.channel_param
//ONESHOT的作用就是在任意时刻,一个socket只能由一个线程处理
//https://blog.csdn.net/le119126/article/details/46364399?utm_medium=distribute.pc_relevant.none-task-blog-BlogCommendFromMachineLearnPai2-1.channel_param&depth_1-utm_source=distribute.pc_relevant.none-task-blog-BlogCommendFromMachineLearnPai2-1.channel_param
int utils::setnonblocking(int fd){
    int old_option=fcntl(fd,F_GETFL);
    int new_option=old_option|O_NONBLOCK;
    fcntl(fd,new_option);
    return old_option;
}
//FIFO 遇到EPOLLHUP 问题  EPOLLHUP： 表示对应的文件描述符被挂断；
//https://blog.csdn.net/sunweiliang/article/details/80593139
void utils::addfd(int epollfd,int fd,bool one_shot,int TrigMode){
        epoll_event event; 
        event.data.fd=fd;
        if(1==TrigMode)
            event.events=EPOLLIN|EPOLLET|EPOLLHUP;
        else
            event.events=EPOLLIN|EPOLLET;
        if(one_shot)
            event.events |= EPOLLONESHOT;
        epoll_ctl(epollfd,EPOLL_CTL_ADD,fd,&event);
        setnonblocking(fd);
}

void utils::sig_handler(int sig){
    int save_errno=errno;                   //为保证函数的可重入性，保留原来的errno
    int msg=sig;
    send(u_pipefd[1],(char*)&msg,1,0);
    errno=save_errno;
}

void utils::addsig(int sig, void(handler)(int), bool restart){
    struct sigaction sa;
    memset(&sa,'\0',sizeof(sa));
    sa.sa_handler=handler;
    if(restart)
        sa.sa_flags |= SA_RESTART;
    sigfillset(&sa.sa_mask);
    assert(sigaction(sig, &sa, NULL) != -1);
}
//alarm()函数的主要功能是设置信号传送闹钟，即用来设置信号SIGALRM在经过参数seconds秒数后发送给目前的进程。
//如果未设置信号SIGALARM的处理函数，那么alarm()默认处理终止进程。
void utils::timer_handler(){
    m_timer_lst.tick();
    alarm(m_TIMESLOT);
}

void utils::show_error(int connfd, const char *info){
    //?????????? why connfd
    send(connfd, info, strlen(info), 0);
    close(connfd);
}
int *utils::u_pipefd = 0;
int utils::u_epollfd = 0;

void cb_func(client_data *user_data){
    epoll_ctl(utils::u_epollfd,EPOLL_CTL_DEL,user_data->sockfd,0);
    assert(user_data);
    close(user_data->sockfd);
    http_conn::m_user_count--;
}