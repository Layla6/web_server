#ifndef WEB_SERVER_H
#define WEB_SERVER_H
#include<iostream>
#include<sys/epoll.h>
#include "thread_pool.h"
#include "./sql_pool/sql_pool.h"
#include "exp_task.h"
#include "./timer/lst_timer.h"
using namespace std;

const int MAX_EVENT_NUMBER=10000;
const int MAX_FD=65536;

class web_server
{
public:
    web_server();
    ~web_server();
    void init(int port,string user,string password,string databasename
            ,int log_write,int opt_linger,int trigmode,int sql_num
            ,int thread_num,int close_log,int actor_model);
    void thread_pool();
    void sql_connPool();
    void log_write();
    void trig_mode();
    
    void eventListen();
    void eventLoop();
    
    void timer(int connfd,struct sockaddr_in client_address);
    void adjust_timer(timer_node* timer);
    void deal_time(timer_node* timer,int sockfd);

    bool deal_clientdta();
    bool deal_signal(bool& timeout,bool& stop_server);
    void deal_read(int sockfd);
    void deal_write(int sockfd);
public:
    int m_port;
    char *m_root;
    int m_log_write;
    int m_close_log;
    int m_actor_model;
    int m_pipefd[2];
    int m_epollfd;
    task* users;

    sql_pool* m_connPool;
    string m_user;
    string m_password;
    string m_databaseName;
    int m_sql_num;

    threadpool<task> *m_threadPool;
    int m_thread_num;

    epoll_event events[MAX_EVENT_NUMBER];

    int m_listenfd;
    int m_OPT_LINGER;
    int m_TRIGMode;
    int m_LISTENTriMode;
    int m_CONNTriMode;

    

};

#endif