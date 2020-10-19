#ifndef WEB_SERVER_H
#define WEB_SERVER_H
#include<iostream>
#include<sys/epoll.h>
#include "thread_pool.h"
#include "exp_task.h"
using namespace std;

const int MAX_EVENT_NUMBER=10000;
const int MAX_FD=65536;

class web_server
{
public:
    web_server();
    ~web_server();
    void init(int port,string user,string password,string databasename
    ,int log_write,int opt_linger,int trigmode,int sql_num,int thread_num,int close_log,int actor_model);
    void thread_pool();
    void sql_pool();
    void log_write();
    void trig_mode();
    void eventListen();
    void eventLoop();
public:
    int m_port;
    char *m_root;
    int m_log_write;
    int m_close_log;
    int m_actor_model;
    int m_pipefd[2];
    int m_epollfd;
    task* users;

    threadpool<task> *m_pool;
    int m_thread_num;

    epoll_event events[MAX_EVENT_NUMBER];

    int m_listenfd;
    int m_OPT_LINGER;
    int m_TRIGMode;
    int m_LISTENTriMode;
    int m_CONNTriMode;

};

web_server::web_server()
{
}

web_server::~web_server()
{
}


#endif