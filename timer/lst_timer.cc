#include "lst_timer.h"
#include <time.h>
#include <fcntl.h>
#include <sys/epoll.h>
#include <string.h>
#include <errno.h>
#include <signal.h>
#include <assert.h>
#include <unistd.h>
#include "./http_task/http_conn.h"
//ascend
sortimer_lst::sortimer_lst(){
    head=NULL;
    tail=NULL;
}
sortimer_lst::~sortimer_lst(){
    timer_node* tmp=head;
    while(tmp){
        head=tmp->next;
        delete tmp;
        tmp=head;
    }
}
void sortimer_lst::add_timer(timer_node* item){
    if(item==NULL)
        return;
    if(head==NULL){
        head=tail=item;
        return ;
    }
    if(item->expire<head->expire){
        item->next=head;
        head->prev=item;
        head=item;
        return;
    }
    add_timer(item,head);
}

void sortimer_lst::adjust_timer(timer_node* item){
    if (!item){
        return;
    }
    timer_node *tmp = item->next;
    if (!tmp || (item->expire < tmp->expire)){
        return;
    }
    if (item == head){
        head = head->next;
        head->prev = NULL;
        item->next = NULL;
        add_timer(item, head);
    }
    else{
        item->prev->next = item->next;
        item->next->prev = item->prev;
        add_timer(item, item->next);
    }
}

void sortimer_lst::del_timer(timer_node* item){
    if (!item)
        return;
    if ((item == head) && (item == tail)){
        delete item;
        head = NULL;
        tail = NULL;
        return;
    }
    if (item == head){
        head = head->next;
        head->prev = NULL;
        delete item;
        return;
    }
    if (item == tail){
        tail = tail->prev;
        tail->next = NULL;
        delete item;
        return;
    }
    item->prev->next = item->next;
    item->next->prev = item->prev;
    delete item;
}
void sortimer_lst::add_timer(timer_node *timer, timer_node *lst_head){
    timer_node *prev = lst_head;
    timer_node *tmp = prev->next;
    while (tmp){
        if (timer->expire < tmp->expire){
            prev->next = timer;
            timer->next = tmp;
            tmp->prev = timer;
            timer->prev = prev;
            break;
        }
        prev = tmp;
        tmp = tmp->next;
    }
    if (!tmp){
        prev->next = timer;
        timer->prev = prev;
        timer->next = NULL;
        tail = timer;
    }
}
void sortimer_lst::tick(){
    if(head==NULL)
        return ;
    time_t cur=time(NULL);
    timer_node* tmp=head;
    while(tmp){
        //没有超时
        if(cur<tmp->expire)
            break;
        tmp->cb_func(tmp->user_data);
        head=tmp->next;
        if(head)
            head->prev=NULL;
        delete tmp;
        tmp=head;
    }

}


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

void utils::addsig(int sig, void(handler)(int), bool restart = true){
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