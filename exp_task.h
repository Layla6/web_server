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
    void initmysql_result(sql_pool *connpool);
public:
    int m_state;
};




#endif