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
* 测试前确认已安装MySQL数据库

    ```C++
    // 建立yourdb库
    create database yourdb;

    // 创建user表
    USE yourdb;
    CREATE TABLE user(
        username char(50) NULL,
        passwd char(50) NULL
    )ENGINE=InnoDB;

    // 添加数据
    INSERT INTO user(username, passwd) VALUES('name', 'passwd');
    ```

* 修改testMain.cc中的数据库初始化信息

    ```C++
    //数据库登录名,密码,库名
    string user = "root";
    string passwd = "root";
    string databasename = "yourdb";
    ```

* build

    ```C++
    $ ./build.sh
    ```

* 启动server

    ```C++
    $ ./server
    ```

* 浏览器端

    ```C++
    $ ip:9006
    ```

个性化运行
------

```C++
./server [-p port] [-l LOGWrite] [-m TRIGMode] [-o OPT_LINGER] [-s sql_num] [-t thread_num] [-c close_log] [-a actor_model]
```

温馨提示:以上参数不是非必须，不用全部使用，根据个人情况搭配选用即可.

* -p，自定义端口号
	* 默认9006
* -l，选择日志写入方式，默认同步写入
	* 0，同步写入
	* 1，异步写入
* -m，listenfd和connfd的模式组合，默认使用LT + LT
	* 0，表示使用LT + LT
	* 1，表示使用LT + ET
    * 2，表示使用ET + LT
    * 3，表示使用ET + ET
* -o，优雅关闭连接，默认不使用
	* 0，不使用
	* 1，使用
* -s，数据库连接数量
	* 默认为8
* -t，线程数量
	* 默认为8
* -c，关闭日志，默认打开
	* 0，打开日志
	* 1，关闭日志
* -a，选择反应堆模型，默认Proactor
	* 0，Proactor模型
	* 1，Reactor模型

测试示例命令与含义

```C++
./server -p 9007 -l 1 -m 0 -o 1 -s 10 -t 10 -c 1 -a 1
```

- [x] 端口9007
- [x] 异步写入日志
- [x] 使用LT + LT组合
- [x] 使用优雅关闭连接
- [x] 数据库连接池内有10条连接
- [x] 线程池内有10条线程
- [x] 关闭日志
- [x] Reactor反应堆模型

# 模块概述
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

## HTTP
1. 解析http请求

2. 响应http请求

## 服务器
1. 主事件循环

2. 处理新的连接请求:deal_clientdta()

3. 处理定时器，当遇到关闭(client/server)或者错误请求:deal_timer()

4. 处理信号:deal_signal()

5. 处理客户端连接上接受到的数据:deal_read()

6. 处理需要发送到客户端上的数据:deal_write()

