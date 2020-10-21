#include<iostream>
#include<mysql/mysql.h>
#include "sql_pool.h"
using namespace std;

sql_pool::sql_pool(){
    m_CurConn=0;
    m_FreeConn=0;
}

sql_pool* sql_pool::connPool=new sql_pool();

sql_pool* sql_pool::getInstance(){
    return connPool;
}
void sql_pool::init(string url, string User, string PassWord, string DataBaseName, int Port, int MaxConn,int close_log){
    m_url=url;
    m_user=User;
    m_password=PassWord;
    m_dabaseName=DataBaseName;
    m_MaxConn=MaxConn;
    m_port=Port;
    m_close_log=close_log;

    for(int i=0;i<m_MaxConn;++i){
        MYSQL* con=NULL;
        con=mysql_init(con);
        if(con==NULL){
            LOG_ERROR("Mysql initialize error!");
            exit(1);
        }
        con=mysql_real_connect(con,m_url.c_str(),m_user.c_str(),m_password.c_str(),m_dabaseName.c_str(),m_port,NULL,0);
        if(con==NULL){
            LOG_ERROR("Mysql connect error!");
            exit(1);
        }
        connList.push_back(con);
        ++m_FreeConn;
    }
    reserve=sem(m_FreeConn);
    m_MaxConn=m_FreeConn;

}

MYSQL* sql_pool::getConnection(){
    MYSQL* con=NULL;
    //***********NOTED*******************
    //问题1：是否应该阻塞，还是返回空（请求函数需要额外判断返回的连接是否为空）
    //问题2：为了避免阻塞或者为空（请求不到），主线程必须永久持有一个sql连接
    //if(connList.size()==0)
    //    return NULL;
    reserve.wait();
    m_mutex.lock();
    con=connList.front();
    connList.pop_front();

    --m_FreeConn;
    ++m_CurConn;
    cout<<"sql_pool free_num: "<<m_FreeConn<<"   "<<"sql_pool used_num: "<<m_CurConn<<endl;
    m_mutex.unlock();
    return con;
}

bool sql_pool::releaseConnection(MYSQL* con){
    if(con==NULL)
        return false;
    m_mutex.lock();
    connList.push_back(con);
    ++m_FreeConn;
    --m_CurConn;
    cout<<m_FreeConn<<"   "<<m_CurConn<<endl;
    m_mutex.unlock();
    reserve.post();
    return true;
}

void sql_pool::destroyPool(){
    m_mutex.lock();
    if(connList.size()>0){
        list<MYSQL*>::iterator it;
        for(;it!=connList.end();it++){
            MYSQL* tmp=*it;
            mysql_close(tmp);
        }
        m_CurConn=0;
        m_FreeConn=0;
        connList.clear();
    }
    m_mutex.unlock();
}

int sql_pool::getFreeConnCount(){
    return m_FreeConn;
}
connectionRAII::connectionRAII(MYSQL**sql,sql_pool* connPool){
    *sql=connPool->getConnection();
    conRAII=*sql;
    poolRAII=connPool;
}
connectionRAII::~connectionRAII(){
    poolRAII->releaseConnection(conRAII);
}

