#ifndef THREAD_POOL
#define THREAD_POOL
#include<iostream>
#include<pthread.h>
#include<list>
#include<exception>
#include "locker.h"
using namespace std;

//implement thread pool, T is the request type
template <typename T>
class threadpool{
public:
    threadpool(int actor_model,int thread_number,int max_request);
    ~threadpool();
    bool append(T* request,int state);
    bool append_p(T* request);
private:
    static void* worker(void *arg);
    void run();
private:
    int m_actor_model;      //concurrency model(0:proactor 1:reactor)
    int m_thread_num;       //the number of thread in the pool
    int m_max_requests;     //the max number of requests in the request_queue
    pthread_t *m_threads;   //thread_pool array
    list<T*> m_workqueue;   //requests queue(threads take request from queue )
    sem m_queue_state;      // is there task in the queue
    locker m_queuelocker;   //protect queue
};

template<typename T>
threadpool<T>::threadpool(int actor_model,int thread_num,int max_request):m_actor_model(actor_model),m_thread_num(thread_num),m_max_requests(max_request){
    if(thread_num<=0 || max_request<=0){
        cout<<"error in :(thread_num<=0 || max_request<=0)"<<endl;
        throw std::exception();
    }
    m_threads=new pthread_t[m_thread_num];
    if(!m_threads){
        cout<<"error in :new pthread_t"<<endl;
        throw std::exception();
    }
    for(int i=0;i<m_thread_num;++i){
        if(pthread_create(m_threads+i,NULL,worker,this)!=0){
            delete[] m_threads;
            cout<<"error in :pthread_create"<<endl;
            throw std::exception();
        }
        if(pthread_detach(m_threads[i])){
            delete[] m_threads;
            cout<<"error in :pthread_detach"<<endl;
            throw std::exception();
        }
    }    
}
template<typename T>
threadpool<T>::~threadpool(){
    delete[] m_threads;
}

template<typename T>
bool threadpool<T>::append(T *request,int state){
    m_queuelocker.lock();
    if(m_workqueue.size()>=m_max_requests){
        m_queuelocker.unlock();
        return false;
    }
    request->m_state=state;
    m_workqueue.push_back(request);
    m_queuelocker.unlock();
    m_queue_state.post();
    return true;
}
template<typename T>
bool threadpool<T>::append_p(T *request){
    m_queuelocker.lock();
    if(m_workqueue.size()>=m_max_requests){
        m_queuelocker.unlock();
        return false;
    }
    m_workqueue.push_back(request);
    m_queuelocker.unlock();
    m_queue_state.post();
    return true;
}

template<typename T>
void *threadpool<T>::worker(void *arg){
    threadpool* pool=(threadpool*)arg;
    pool->run();
    return pool;
}

template<typename T>
void threadpool<T>::run(){
    while(true){
        m_queue_state.wait();
        m_queuelocker.lock();
        if(m_workqueue.empty()){
            m_queuelocker.unlock();
            continue;
        }
        T* request=m_workqueue.front();
        m_workqueue.pop_front();
        m_queuelocker.unlock();
        request->process();
        if(m_actor_model==1){  //reactor model
            if(request->m_state==0){        //read
                request->read_once();
                request->process();
            }
            else{
                request->write();           //write
            }
        }
        else{                   //proactor model
            request->process();
        }
    }
}

#endif

/*
https://blog.csdn.net/hsd2012/article/details/51207585?utm_medium=distribute.pc_relevant.none-task-blog-BlogCommendFromMachineLearnPai2-1.channel_param&depth_1-utm_source=distribute.pc_relevant.none-task-blog-BlogCommendFromMachineLearnPai2-1.channel_param
(1)当把线程函数封装在类中，this指针会作为默认的参数被传进函数中，
从而和线程函数参数(void*)不能匹配，不能通过编译。
(2)但是当我把类中的作为pthread_create的线程函数改为static之后，
又遇到新的问题？即在该方法中访问其他非静态方法或者是变量时，没法访问，
关于这个问题又怎么解决呢？答案：将this指针作为参数传递即可。

*/