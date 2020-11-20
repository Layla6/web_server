#include<iostream>
#include "thread_pool.h"
#include "exp_task.h"
#include "locker.h"
#include <unistd.h>
#include "./log/log.h"
#include "./sql_pool/sql_pool.h"
#include "web_server.h"
#include "./config/config.h"
//#include "http_task/http_conn.h"
using namespace std;
void test_thread_pool(){
    threadpool<task> *m_threadpool=new threadpool<task>(1,NULL,8,1000);
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
    m_log->init((char*)"./serverLog",0,64,10,10);     
    //为什么要强制转换：
    //"./serverLog"是一个不变常量，在c++中叫做string literal，type是const char *，
    //而p则是一个char指针。如果强行赋值会发生什么呢？没错，就是将右边的常量强制类型转换成一个指针，结果就是我们在修改一个const常量。
    //编译运行的结果会因编译器和操作系统共同决定，有的编译器会通过，有的会抛异常，就算过了也可能因为操作系统的敏感性而被杀掉。
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
void test_sqlPool(sql_pool* m_connPool){
    //***********NOTED*******************
    //函数MYSQL* sql_pool::getConnection()中：
    //问题1：是否应该阻塞，还是返回空（请求函数需要额外判断返回的连接是否为空）
    //问题2：为了避免阻塞或者为空（请求不到），主线程必须永久持有一个sql连接
    MYSQL *mysql=NULL;
    connectionRAII mysqlcon(&mysql,m_connPool);
    if(mysql_query(mysql,"select username,passwd from user")){      //return 0 means success!
        LOG_ERROR("mysql select error:%s\n",mysql_error(mysql));
    }
    MYSQL_RES* result=mysql_store_result(mysql);
    int num_fields=mysql_num_fields(result);
    MYSQL_FIELD* fields=mysql_fetch_field(result);
    while(MYSQL_ROW row=mysql_fetch_row(result)){
        cout<<row[0]<<"    "<<row[1]<<endl;
    }
    MYSQL*  my_db1=NULL;
    connectionRAII mysqlcon1(&my_db1,m_connPool);
    MYSQL*  my_db2=NULL;
    connectionRAII mysqlcon2(&my_db2,m_connPool);
    MYSQL*  my_db3=NULL;
    connectionRAII mysqlcon3(&my_db3,m_connPool);
    cout<<(my_db3)<<endl;
    //MYSQL*  my_db4=NULL;
    //connectionRAII mysqlcon4(&my_db4,m_connPool);
    //cout<<my_db4<<endl;
}
int main(int argc,char* argv[]){
    //test_thread_pool();
    //test_log();

    // test sql pool  sql_connection_number is 4
    /*
    log* m_log=log::get_instance();
    m_log->init((char*)"./serverLog",0,64,10);   
    sql_pool* m_connPool=sql_pool::getInstance();
    m_connPool->init("localhost", "root", "123","webser_db", 3306, 4,0);
    test_sqlPool(m_connPool);
    */

        //需要修改的数据库信息,登录名,密码,库名
    string user = "root";
    string passwd = "123";
    string databasename = "webser_db";

    //命令行解析
    Config config;
    config.parse_arg(argc, argv);

    web_server<http_conn> server;

    //初始化
    server.init(config.PORT, user, passwd, databasename, config.LOGWrite, 
                config.OPT_LINGER, config.TRIGMode,  config.sql_num,  config.thread_num, 
                config.close_log, config.actor_model);
    //日志
    server.log_write();

    //数据库
    server.sql_connPool();

    //线程池
    server.thread_pool();

    //触发模式
    server.trig_mode();

    //监听
    server.eventListen();

    //运行
    server.eventLoop();

    return 0;
}
