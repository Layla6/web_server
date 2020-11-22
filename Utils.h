#ifndef UTILS_H
#define UTILS_H
#include "./timer/lst_timer.h"
#include "./http_task/http_conn.h"

class utils{
public:
    utils(){}
    ~utils(){}
    //对文件描述符设置非阻塞
    void init(int timeSlot);

    int setnonblocking(int fd);     
    //将内核事件表注册读事件，ET模式，选择开启EPOLLONESHOT
    void addfd(int epollfd,int fd,bool one_shot,int TrigMode);
    
    //信号处理函数
    static void sig_handler(int sig);

    //设置信号函数
    void addsig(int sig, void(handler)(int), bool restart = true);

    //定时处理任务，重新定时以不断触发SIGALRM信号
    void timer_handler();

    void show_error(int connfd, const char *info);
public:
    static int* u_pipefd;
    sortimer_lst m_timer_lst;
    static int u_epollfd;
    int m_TIMESLOT;
};

void cb_func(client_data *user_data);

#endif