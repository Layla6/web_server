#ifndef LST_TIMER_H
#define LST_TIMER_H
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdlib.h> 
#include <time.h>
#include <fcntl.h>
#include <sys/epoll.h>
#include <string.h>
#include <errno.h>
#include <signal.h>
#include <assert.h>
#include <unistd.h>
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
    ~timer_node(){}
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

#endif