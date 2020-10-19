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
## 线程池
1. 工作队列m_workqueue有std::list实现 （思考：可以利用多链表加入任务优先级）
2. webserver将任务插入到工作队列m_workqueue中，线程池从中取
3. 注意事项：
    if(pthread_create(m_threads+i,NULL,worker,this)!=0)
    * 当把线程函数封装在类中，this指针会作为默认的参数被传进函数中，从而和线程函数参数(void*)不能匹配，不能通过编译。
    * 线程函数改为static之后，为了访问其他非静态方法或者是变量时，则应该将this指针作为参数传递。
## 日志系统（同步/异步）
websever调用write_log写日志
1. 同步日志
    1. write_log生成日志信息log_str，将日志信息log_str直接写道日志文件中
2. 异步日志
    1. 实现线程安全的阻塞队列（block_queue.h）
    2. write_log生成日志信息log_str，将日志信息push到阻塞队列
    3. 创建一个日志线程，将阻塞队列中的日志信息写入日志文件
3. 按日期天以及日志最大行max_line分割日志文件
