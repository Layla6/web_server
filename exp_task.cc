#include "exp_task.h"
#include "sql_pool/sql_pool.h"
using namespace std;

task::task(){
    m_state=0;
}
task::task(int sta){
    m_state=sta;
}
void task::initmysql_result(sql_pool *connpool){
    
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