#include<iostream>
#include "thread_pool.h"
#include "exp_task.h"
#include "locker.h"
#include <unistd.h>
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

int main(){
    test_thread_pool();
    return 0;
}
