#ifndef LST_TIMER_H
#define LST_TIMER_H
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdlib.h> 
class timer_node;
struct client_data{
    sockaddr_in addr;
    int sockfd;
    timer_node* timer;
};

class timer_node{
public:
    time_t expire;
    timer_node* prev;
    timer_node* next;
    client_data* user_data;
    void (* cb_func)(client_data*);
public:
    timer_node():prev(NULL),next(NULL){}
    ~timer_node();
};

class sortimer_lst{
public:
    sortimer_lst();
    ~sortimer_lst();

    void add_timer(timer_node *item);
    void adjust_timer(timer_node *item);
    void del_timer(timer_node *item);
    void tick();
private:
    void add_timer(timer_node *timer, timer_node *lst_head);
    timer_node *head;
    timer_node *tail;
};

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