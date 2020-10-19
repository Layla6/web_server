#ifndef BLOCK_QUEUE_H
#define BLOCK_QUEUE_H

#include<iostream>
#include  "../locker.h"
#include<sys/time.h>
#include<exception>
using namespace std;

//block queue:loop array,thread safe
template <typename T>
class block_queue
{
private:
    
public:
    block_queue(int max_size);
    ~block_queue();
    void clear();
    bool full();
    bool empty();
    bool front(T& value);
    bool back(T& value);
    int size();
    int max_size();
    bool push(const T value);
    bool pop(T& item);
private:
    locker m_mutex;
    cond m_cond;
    T *m_array;
    int m_size;
    int m_max_size;
    int m_front;        //current idx is NULL,next is the first item
    int m_back;         //current is the last item
};
template <typename T>
block_queue<T>::block_queue(int max_size){
    if(max_size<=0){
        cout<<"error in :block_queue construc."<<endl;
        throw std::exception();
    }
    m_array=new T[max_size];
    if(!m_array){
        cout<<"error in :block_queue construc."<<endl;
        throw std::exception();
    }
    m_max_size=max_size;
    m_size=0;
    m_front=-1;
    m_back=-1;
}
template <typename T>
block_queue<T>::~block_queue(){
    m_mutex.lock();
    if(m_array)
        delete[] m_array;
    m_mutex.unlock();
}

template <typename T>
void block_queue<T>::clear(){
    m_mutex.lock();
    m_size=0;
    m_front=-1;
    m_back=-1;
    m_mutex.unlock();
}

template <typename T>
bool block_queue<T>::full(){
    m_mutex.lock();
    if(m_size>=m_max_size){
        m_mutex.unlock();
        return true;
    }
    else{
        m_mutex.unlock();
        return false;
    }     
}


template <typename T>
bool block_queue<T>::empty(){
    m_mutex.lock();
    if(m_size==0){
        m_mutex.unlock();
        return true;
    }
    else{
        m_mutex.unlock();
        return false;
    }
}

template <typename T>
bool block_queue<T>::front(T& value){
    m_mutex.lock();
    if(0==m_size){             //be careful,Don't call empty()!(be dead-lock)
        m_mutex.unlock();
        return false;
    }
    value=m_array[m_front];
    m_mutex.unlock();
    return true;
}


template <typename T>
bool block_queue<T>::back(T& value){
    m_mutex.lock();
    if(0==m_size){             //be careful,Don't call empty()!(be dead-lock)
        m_mutex.unlock();
        return false;
    }
    value=m_array[m_back];
    m_mutex.unlock();
    return true;
}

//!!!!!!!!!!
template <typename T>
int block_queue<T>::size(){
    int tmp=0;
    m_mutex.lock();
    tmp=m_size;
    m_mutex.unlock();
    return tmp;
}

template <typename T>
int block_queue<T>::max_size(){
    int tmp=0;
    m_mutex.lock();
    tmp=m_max_size;
    m_mutex.unlock();
    return tmp;
}


template <typename T>
bool block_queue<T>::push(const T value){
    m_mutex.lock();
    if(m_size>=m_max_size){
        m_cond.broadcast();
        m_mutex.unlock();
        return false;
    }
    m_back=(m_back+1)%m_max_size;
    m_array[m_back]=value;
    m_size++;
    m_cond.broadcast();
    m_mutex.unlock();
    return true;
}

//条件变量内部会先解锁mutex,然后堵塞。当被唤醒后再次争夺mutex锁上。
//因此，cond需要传入mutex（内部会解锁，唤醒后又加锁）
template <typename T>
bool block_queue<T>::pop(T& item){
    m_mutex.lock();
    while(m_size<=0){
        if(!m_cond.wait(m_mutex.get())){
            m_mutex.unlock();
            return false;
        }
    }
    //!!!!!!!
    m_front=(m_front+1)%m_max_size;
    item=m_array[m_front];
    m_size--;
    m_mutex.unlock();
    return true;
}
#endif