#ifndef SQL_POOL_H
#define SQL_POOL_H
#include<iostream>
#include<list>
#include<string>
#include<mysql/mysql.h>
#include "../locker.h"
#include "../log/log.h"
using namespace std;

class sql_pool{
public:
    MYSQL* getConnection();                 //get database connection
    bool releaseConnection(MYSQL* conn);    //release database connection

    int getFreeConnCount();                 //get  m_FreeConn
    void destroyPool();
    static sql_pool* getInstance();
    void init(string url, string User, string PassWord, string DataBaseName, int Port, int MaxConn,int close_log);


private:
    sql_pool();
    ~sql_pool();
    static sql_pool* connPool;

    int m_MaxConn;          // Maximum number of database connections
    int m_CurConn;          //The number of database connections that have been used
    int m_FreeConn;         //The number of database connections that have not been used yet
    locker m_mutex;
    list<MYSQL*> connList;  //sql_pool
    sem reserve;

public:
    string m_url;
    //!!!!!!!!!!!!
    int m_port;
    string m_user;
    string m_password;
    string m_dabaseName;
    int m_close_log;        

};

class connectionRAII{
public:
    connectionRAII(MYSQL** con,sql_pool* connPool);         //RAII 从现有连接池中取出一个连接
    ~connectionRAII();
private:
    MYSQL* conRAII;
    sql_pool* poolRAII;
};


#endif