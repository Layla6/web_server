#ifndef WEB_SERVER_H
#define WEB_SERVER_H
/*
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <iostream>
#include <sys/epoll.h>
#include <cassert>
*/
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <cassert>
#include <sys/epoll.h>
#include "thread_pool.h"
#include "./http_task/http_conn.h"
#include "Utils.h"
using namespace std;
//*********** undefined reference 错误的解决方法
//g++不支持模板类的分离编译，因此模板的实现最好都些在.h文件中，否则将出现undefined reference to XXXX 的错误。
// https://tieba.baidu.com/p/2629409433
const int MAX_EVENT_NUMBER=10000;
const int MAX_FD=65536;
const int TIMESLOT = 5;   
template<typename Task>
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
    void deal_timer(timer_node* timer,int sockfd);

    bool deal_clientdta();
    bool deal_signal(bool& timeout,bool& stop_server);
    void deal_read(int sockfd);
    void deal_write(int sockfd);
public:
    int m_port;
    char *m_root;//
    int m_log_write;
    int m_close_log;
    int m_actor_model;
    int m_pipefd[2];//
    int m_epollfd;//
    Task* users;

    sql_pool* m_connPool;//
    string m_user;
    string m_password;
    string m_databaseName;
    int m_sql_num;

    threadpool<Task> *m_threadPool;//
    int m_thread_num;

    epoll_event events[MAX_EVENT_NUMBER];//

    int m_listenfd;
    int m_OPT_LINGER;
    int m_TRIGMode;
    int m_LISTENTriMode;
    int m_CONNTriMode;

    client_data* users_timer;
    utils tools;
    
};

template<typename Task>
web_server<Task>::web_server(){
    //work class
    users=new Task[MAX_FD];
    //root
    char server_path[200];
    getcwd(server_path,200);
    char root[6]="/root";
    m_root=(char*)malloc(strlen(server_path)+strlen(root)+1);
    strcpy(m_root,server_path);
    strcat(m_root,root);

    //timer
    users_timer=new client_data[MAX_FD];
}
template<typename Task>
web_server<Task>::~web_server(){
    close(m_epollfd);
    close(m_listenfd);
    close(m_pipefd[0]);
    close(m_pipefd[1]);
    delete[] users;
    delete[] users_timer;
    //delete m_connPool; //不需要delete  因为用RAII包装类
    delete m_threadPool;
}
template<typename Task>
void web_server<Task>::init(int port,string user,string password,string databasename,int log_write
    ,int opt_linger,int trigmode,int sql_num,int thread_num,int close_log,int actor_model){
    m_port=port;
    m_user=user;
    m_password=password;
    m_databaseName=databasename;
    m_log_write=log_write;
    m_OPT_LINGER=opt_linger;
    m_TRIGMode=trigmode;
    m_sql_num=sql_num;
    m_thread_num=thread_num;
    m_close_log=close_log;
    m_actor_model=actor_model;
}
template<typename Task>
void web_server<Task>::trig_mode(){
    //LT + LT
    if (0 == m_TRIGMode)
    {
        m_LISTENTriMode = 0;
        m_CONNTriMode = 0;
    }
    //LT + ET
    else if (1 == m_TRIGMode)
    {
        m_LISTENTriMode = 0;
        m_CONNTriMode = 1;
    }
    //ET + LT
    else if (2 == m_TRIGMode)
    {
        m_LISTENTriMode = 1;
        m_CONNTriMode = 0;
    }
    //ET + ET
    else if (3 == m_TRIGMode)
    {
        m_LISTENTriMode = 1;
        m_CONNTriMode = 1;
    }
}

template<typename Task>
void web_server<Task>::log_write(){
    if(0==m_close_log){
        if(1==m_log_write)
            log::get_instance()->init((char*)"./serverLog", m_close_log, 2000, 800000, 800);
        else
            log::get_instance()->init((char*)"./serverLog", m_close_log, 2000, 800000, 0);        
    }
}

template<typename Task>
void web_server<Task>::sql_connPool(){
    m_connPool=sql_pool::getInstance();
    m_connPool->init("localhost", m_user, m_password, m_databaseName, 3306, m_sql_num, m_close_log);
    users->initmysql_result(m_connPool);
}

template<typename Task>
void web_server<Task>::thread_pool(){
    int max_req=10000;
    m_threadPool=new threadpool<Task>(m_actor_model,m_connPool,m_thread_num,max_req);
}

template<typename Task>
void web_server<Task>::eventListen(){
    m_listenfd=socket(PF_INET,SOCK_STREAM,0);
    assert(m_listenfd>=0);
    if(0==m_OPT_LINGER){
        struct linger tmp={0,1};
        setsockopt(m_listenfd,SOL_SOCKET,SO_LINGER,&tmp,sizeof(tmp));
    }
    else if(1==m_OPT_LINGER){
        struct linger tmp={1,1};
        setsockopt(m_listenfd,SOL_SOCKET,SO_LINGER,&tmp,sizeof(tmp));
    }

    int ret=0;
    struct sockaddr_in address;
    //bzero和memset https://blog.csdn.net/weixin_42235488/article/details/80589583?utm_medium=distribute.pc_relevant.none-task-blog-BlogCommendFromMachineLearnPai2-1.channel_param&depth_1-utm_source=distribute.pc_relevant.none-task-blog-BlogCommendFromMachineLearnPai2-1.channel_param
    bzero(&address,sizeof(address));
    //memser(&address,0,sizeof(address));
    address.sin_family=AF_INET;
    //INADDR_ANY  https://blog.csdn.net/jeffasd/article/details/51461568?utm_medium=distribute.pc_relevant.none-task-blog-BlogCommendFromMachineLearnPai2-1.channel_param&depth_1-utm_source=distribute.pc_relevant.none-task-blog-BlogCommendFromMachineLearnPai2-1.channel_param
    address.sin_addr.s_addr=htonl(INADDR_ANY);
    address.sin_port=htons(m_port);

    //setsockopt中参数之SO_REUSEADDR的意义 https://blog.csdn.net/chenlycly/article/details/52191441?utm_medium=distribute.pc_relevant.none-task-blog-BlogCommendFromMachineLearnPai2-2.channel_param&depth_1-utm_source=distribute.pc_relevant.none-task-blog-BlogCommendFromMachineLearnPai2-2.channel_param
    int flag=1;
    setsockopt(m_listenfd,SOL_SOCKET,SO_REUSEADDR,&flag,sizeof(flag));
    ret=bind(m_listenfd,(struct sockaddr*)&address,sizeof(address));
    assert(ret>=0);
    ret=listen(m_listenfd,5);
    assert(ret>=0);

    tools.init(TIMESLOT);

    //epoll
    epoll_event events[MAX_EVENT_NUMBER];
    m_epollfd=epoll_create(5);
    assert(m_epollfd!=-1);    
    tools.addfd(m_epollfd,m_listenfd,false,m_LISTENTriMode);
    http_conn::m_epollfd=m_epollfd;
    ret=socketpair(PF_UNIX,SOCK_STREAM,0,m_pipefd);
    assert(ret!=-1);
    //？？？？？？？？？？？？
    tools.setnonblocking(m_pipefd[1]);
    tools.addfd(m_epollfd,m_pipefd[0],false,0);
    //SIGPIPE信号处理 https://blog.csdn.net/u010982765/article/details/79038062
    //https://blog.csdn.net/kuniqiw/article/details/95235197
    tools.addsig(SIGPIPE,SIG_IGN);
    tools.addsig(SIGALRM,tools.sig_handler,false);
    tools.addsig(SIGTERM,tools.sig_handler,false);
    alarm(TIMESLOT);
    utils::u_pipefd=m_pipefd;
    utils::u_epollfd=m_epollfd;
}

template<typename Task>
void web_server<Task>::timer(int connfd,struct sockaddr_in client_address){
    //初始化用户连接
    users[connfd].init(connfd,client_address,m_root,m_CONNTriMode,m_close_log,m_user,m_password,m_databaseName);
    
    //创建定时器，并建立正确的连接
    users_timer[connfd].addr=client_address;
    users_timer[connfd].sockfd=connfd;

    timer_node* timer=new timer_node();
    timer->user_data=&users_timer[connfd];
    timer->cb_func=cb_func;
    time_t cur=time(NULL);
    timer->expire=cur+3*TIMESLOT;
    users_timer[connfd].timer=timer;
    tools.m_timer_lst.add_timer(timer);
}

template<typename Task>
bool web_server<Task>::deal_clientdta(){
    struct sockaddr_in client_address;
    socklen_t client_addr_len=sizeof(client_address);
    if(0==m_LISTENTriMode){
        int connfd=accept(m_listenfd,(struct sockaddr*)&client_address,&client_addr_len);
        if(connfd<0){
            LOG_ERROR("%s:errno is:%d","accept error!",errno);
            return false;
        }
        if(http_conn::m_user_count>=MAX_FD){
            tools.show_error(connfd,"server busy!");
            LOG_ERROR("%s","server busy!");
            return false;
        }
        timer(connfd,client_address);
    }
    else{
        while(1){
            int connfd=accept(m_listenfd,(struct sockaddr*)&client_address,&client_addr_len);
            if(connfd<0){
                LOG_ERROR("%s:errno is:%d","accept error!",errno);
                break;
            }
            if(http_conn::m_user_count>=MAX_FD){
                tools.show_error(connfd,"server busy!");
                LOG_ERROR("%s","server busy!");
                break;
            }
            timer(connfd,client_address);
        }
        //?????
        return false;
    }
    return true;
}

template<typename Task>
bool web_server<Task>::deal_signal(bool& timeout,bool& stop_server){
    int ret=0;
    int sig;
    char signals[1024];
    ret=recv(m_pipefd[0],signals,sizeof(signals),0);
    if(ret==-1){
        return false;
    }
    else if(ret==0){
        return false;
    }
    else{
        for (int i = 0; i < ret; i++){
            switch (signals[i])
            {
            case SIGALRM:
                timeout=true;
                break;
            case SIGTERM:
                stop_server=true;
                break;
            default:
                break;
            }
        }
    }
    return true;
}
template<typename Task>
void web_server<Task>::adjust_timer(timer_node* timer){
    time_t cur=time(NULL);
    timer->expire=cur+3*TIMESLOT;
    tools.m_timer_lst.adjust_timer(timer);
    LOG_INFO("%s","adjust timer once!");
}

template<typename Task>
void web_server<Task>::deal_timer(timer_node* timer,int sockfd){
    timer->cb_func(&users_timer[sockfd]);
    if(timer)
        tools.m_timer_lst.del_timer(timer);
    LOG_INFO("close fd %d", users_timer[sockfd].sockfd);        
}

template<typename Task>
void web_server<Task>::deal_read(int sockfd){
    timer_node* timer=users_timer[sockfd].timer;
    if(1==m_actor_model){
        //reactor 
        if(timer)
            adjust_timer(timer);
        
        m_threadPool->append(users+sockfd,0);
        //reactor 等待事件写完成（thread_pool.run）,完成后会将RW_done置1
        //若写事件失败会将RW_done和RW_error置1
        while(true){
            if(1==users[sockfd].RW_done){
                if(1==users[sockfd].RW_error){
                    deal_timer(timer,sockfd);
                    users[sockfd].RW_error=0;
                }
                users[sockfd].RW_done=0;
                break;
            }
        }
    }
    else{
        //proactor
        if(users[sockfd].read_once()){
            LOG_INFO("deal with the client(%s)", inet_ntoa(users[sockfd].get_address()->sin_addr));
            if(timer)
                adjust_timer(timer);    
            m_threadPool->append_p(users+sockfd);
        }
        else{
            deal_timer(timer,sockfd);
        }
        
    }
}
template<typename Task>
void web_server<Task>::deal_write(int sockfd){
    timer_node* timer=users_timer[sockfd].timer;
    if(1==m_actor_model){
        if(timer)
            adjust_timer(timer);
        m_threadPool->append(users+sockfd,1);
        while(true){
            //主线程轮询读写是否完成
            if(1==users[sockfd].RW_done){
                if(1==users[sockfd].RW_error){
                    deal_timer(timer,sockfd);
                    users[sockfd].RW_error=0;
                }
                users[sockfd].RW_done=0;
                break;
            }
        }
    }
    else{
        if(users[sockfd].write()){
            LOG_INFO("send data to client(%s)",inet_ntoa(users[sockfd].get_address()->sin_addr));
            if(timer)
                adjust_timer(timer);
        }
        else{
            deal_timer(timer,sockfd);
        }
    }
}

template<typename Task>
void web_server<Task>::eventLoop(){
    bool timeout=false;
    bool stop_server=false;

    while(!stop_server){
        int count=epoll_wait(m_epollfd,events,MAX_EVENT_NUMBER,-1);
        if(count<0 && errno!=EINTR){
            LOG_ERROR("%s","epoll wait failure!");
            break;
        }

        for(int i=0;i<count;++i){
            int sockfd=events[i].data.fd;

            //处理新收到的客户连接
            if(sockfd==m_listenfd){
                bool flag=deal_clientdta();
                //????????
                if(false==flag)
                    continue;
            }
            else if(events[i].events & (EPOLLRDHUP | EPOLLHUP | EPOLLERR)){
                //对应的文件描述符被挂断/服务端连接关闭,或表示对应的文件描述符发生错误
                //对端正常关闭（程序里close()，shell下kill或ctr+c），触发EPOLLIN和EPOLLRDHUP               
                timer_node *timer=users_timer[sockfd].timer;
                deal_timer(timer,sockfd);
            }
            else if((sockfd==m_pipefd[0])&&events[i].events&EPOLLIN){
                //处理信号
                //对端数据写入，pipe[0]读端，可读
                bool flag=deal_signal(timeout,stop_server);
                if(false==flag)
                    LOG_ERROR("%s","dealsignal failure!");
            }
            else if(events[i].events&EPOLLIN){
                //处理客户端连接上接受到的数据
                deal_read(sockfd);
            }
            else if(events[i].events&EPOLLOUT){
                deal_write(sockfd);
            }
        }
        if(timeout){
            tools.timer_handler();
            LOG_INFO("%s", "timer tick");
            timeout = false;
        }

    }
}


#endif