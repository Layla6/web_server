#ifndef LOG_H
#define LOG_H

#include<iostream>
#include<stdio.h>
#include "block_queue.h"
using namespace std;

class log
{

public:
    int m_close_log;

    log();
    ~log();
    static log* get_instance();
    //??? void *
    static void *flush_log_thread(void *);
    bool init(char *file_name, int close_log, int log_buf_size = 8192, int max_lines = 5000000, int max_queue_size = 0);  //max_queue_size=0 sync
    void write_log(int level,const char *format,...);
    void flush(void);
    //??? void *
    void async_write_log();

private:
    static log* instance;
    char *m_file_name;
    char dir_name[128];         //log path
    char log_name[128];         //log file name
    long long m_max_line;       //max line of log file  
    int m_log_buf_size;         //log buffer size
    long long m_cur_line;             //current line of log file
    int m_today;                //classify by day
    
    

    FILE* m_fp;                 //打开log的文件指针
    char *m_buf;                //log buffer
    block_queue<string>* m_log_queue;
    bool m_is_async;
    locker m_mutex;
    

};

#define LOG_DEBUG(format,...) if(log::get_instance()->m_close_log==0){log::get_instance()->write_log(0,format,##__VA_ARGS__);log::get_instance()->flush();}
#define LOG_INFO(format,...) if(log::get_instance()->m_close_log==0){log::get_instance()->write_log(1,format,##__VA_ARGS__);log::get_instance()->flush();}
#define LOG_WARN(format,...) if(log::get_instance()->m_close_log==0){log::get_instance()->write_log(2,format,##__VA_ARGS__);log::get_instance()->flush();}
#define LOG_ERROR(format,...) if(log::get_instance()->m_close_log==0){log::get_instance()->write_log(3,format,##__VA_ARGS__);log::get_instance()->flush();}

#endif