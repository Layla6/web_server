# web_server
# 介绍
Linux下http服务器开发，学习网络编程以及系统编程。
* 单元测试各个模块（testMain.cc）
* 线程池(thread_pool.h)
* 实现日志系统（同步与异步）(./log/*)
# 环境
* os:ubuntu18.04
* database:mysql
* complier:g++7.5.0
# 使用方法
# 实现技巧
# 概述
## 日志系统（同步/异步）
websever调用write_log写日志
1. 同步日志
    1. write_log生成日志信息log_str，将日志信息log_str直接写道日志文件中
2. 异步日志
    1. 实现线程安全的阻塞队列（block_queue.h）
    2. write_log生成日志信息log_str，将日志信息push到阻塞队列
    3. 创建一个日志线程，将阻塞队列中的日志信息写入日志文件
3. 按日期天以及日志最大行max_line分割日志文件
## 数据库连接池
1. 连接池conn_pool由std::list实现(list<MYSQL*>),
2. 单例模式实现sql_pool
3. 利用RAII机制封装数据库连接请求（从conn_pool取出的mysql连接），防止申请了数据库连接却忘记释放。
4. 注意事项：函数MYSQL* sql_pool::getConnection()中：
    * 问题1：是否应该阻塞，还是返回空（请求函数需要额外判断返回的连接是否为空）
    * 问题2：为了避免阻塞或者为空（请求不到），主线程必须永久持有一个sql连接
    * 当前方案采用信号量阻塞
## 线程池
1. 工作队列m_workqueue由std::list实现 （思考：可以利用多链表加入任务优先级）
2. webserver将任务插入到工作队列m_workqueue中，线程池从中取
3. 单例模式实现thread_pool
4. 注意事项：
    if(pthread_create(m_threads+i,NULL,worker,this)!=0)
    * 当把线程函数封装在类中，this指针会作为默认的参数被传进函数中，从而和线程函数参数(void*)不能匹配，不能通过编译。
    * 线程函数改为static之后，为了访问其他非静态方法或者是变量时，则应该将this指针作为参数传递。
## 定时器
1. 定时器处理非活动连接
    * 由于非活跃连接占用了连接资源，严重影响服务器的性能，通过实现一个服务器定时器，处理这种非活跃连接，释放连接资源。
    * 利用alarm函数周期性地触发SIGALRM信号,该信号的信号处理函数利用管道通知主循环执行定时器链表上的定时任务.
    * 若超时，则调用超时处理函数cb_func，该函数从epoolfd中删除该文件描述符fd，并且关闭该文件描述符。
2. 基于升序的双向链表实现每个文件描述符上的（统一事件源）的定时任务

