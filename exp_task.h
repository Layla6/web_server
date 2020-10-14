#ifndef EXP_TASK_H
#define EXP_TASK_H

#include<iostream>
using namespace std;
class task
{
public:
    task();
    task(int sta);
    ~task();
    void process();
    void read_once();
    void write();
public:
    int m_state;
};
task::task(){
    m_state=0;
}
task::task(int sta){
    m_state=sta;
}

task::~task(){
}

void task::process(){
    cout<<"I am processing!"<<endl;
}
void task::read_once(){
    cout<<"I am reading!"<<endl;
}
void task::write(){
    cout<<"I am write!"<<endl;
}



#endif