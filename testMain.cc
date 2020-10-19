#include<iostream>
#include "thread_pool.h"
#include "exp_task.h"
#include "locker.h"
#include <unistd.h>
#include "./log/log.h"
using namespace std;
void test_thread_pool(){
    threadpool<task> *m_threadpool=new threadpool<task>(1,8,1000);
    int max_user=4;
    task *users=new task[max_user];       //默认0read,1wirte
    users[1].m_state=1;
    for(int i=0;i<max_user;++i){
        cout<<"current request:  "<<i<<endl;
        m_threadpool->append_p(users+i);
    }
    sleep(10);
    delete m_threadpool;
}
void test_log(){
    log* m_log=log::get_instance();
    //test synchronization
    //m_log->init("./serverLog",0,64,10);    
          
    //test asynchronization
    m_log->init("./serverLog",0,64,10,10);     
    int i=40;
    while(i--){
        if(i%4==0)
            LOG_DEBUG("%s","in the debug!");
        if(i%4==1)
            LOG_INFO("%s","in the info!");
        if(i%4==2)
            LOG_WARN("%s","in the warn!");
        if(i%4==3)
            LOG_ERROR("%s","in the error!");
    }
}

int main(){
    //test_thread_pool();
    test_log();
    return 0;
}
