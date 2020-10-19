#include<iostream>
#include "log.h"
#include<memory.h>
#include<stdarg.h>
using namespace std;

log::log(){
    m_cur_line=0;
    m_is_async=false;
}
log::~log(){
    if(m_fp!=NULL)
        fclose(m_fp);
}

log* log::instance=new log();
log* log::get_instance(){
    return log::instance;
}
bool log::init(char *file_name, int close_log, int log_buf_size ,int max_lines, int max_queue_size){
    if(max_queue_size>=1){          //if max_queue_size>=1,we choose async
        m_is_async=true;
        m_log_queue=new block_queue<string>(max_queue_size);
        pthread_t tid;
        pthread_create(&tid,NULL,flush_log_thread,NULL);
    }
    m_close_log=close_log;
    m_log_buf_size=log_buf_size;
    m_buf=new char[m_log_buf_size];
    memset(m_buf,'\0',m_log_buf_size);
    m_max_line=max_lines;

    time_t t=time(NULL);
    struct tm *sys_tm=localtime(&t);
    struct tm my_tm=*sys_tm;

    // char *strchr(const char *str, int c) 在参数 str 所指向的字符串中搜索第一次出现字符 c（一个无符号字符）的位置。
    const char *p=strchr(file_name,'/');
    char log_full_name[256]={0};

    if(p==NULL){
        cout<<"111"<<endl;
        //C 库函数 int snprintf(char *str, size_t size, const char *format, ...) 设将可变参数(...)按照 format 格式化成字符串，并将字符串复制到 str 中，size 为要写入的字符的最大数目，超过 size 会被截断。
        snprintf(log_full_name,255,"/%d_%02d_%02d_%s",my_tm.tm_year+1900,my_tm.tm_mon+1,my_tm.tm_mday,file_name);
    }
    else{
        
        strcpy(log_name,p+1);
        strncpy(dir_name,file_name,p-file_name+1);
        cout<<"222"<<endl;
        cout<<dir_name<<"   "<<log_name<<endl;
        snprintf(log_full_name,255,"/%d_%02d_%02d_%s",my_tm.tm_year+1900,my_tm.tm_mon+1,my_tm.tm_mday,log_name);
        //snprintf(log_full_name,255,"%s%d_%02d_%02d_%s",dir_name,my_tm.tm_year+1900,my_tm.tm_mon+1,my_tm.tm_mday,log_name);
    }
    const char *path=NULL;
    string temp=string(file_name)+string(log_full_name);
    path=temp.c_str();
    m_file_name=file_name;
    cout<<path<<endl;
    m_today=my_tm.tm_mday;
    m_fp=fopen(path,"a");
    if(m_fp==NULL)
        return false;
    return true;
}


//Synchronous method to write log
void* log::flush_log_thread(void *){
    log::get_instance()->async_write_log();
}
void log::async_write_log(){
    string single_log;
    //locker que_mutex;
    while(m_log_queue->pop(single_log)){
        m_mutex.lock();
        //que_mutex.lock();
        fputs(single_log.c_str(),m_fp);
        //que_mutex.unlock();
        m_mutex.unlock();
    }
}

void log::write_log(int level,const char *format,...){
    struct timeval now = {0, 0};
    gettimeofday(&now, NULL);
    time_t t = now.tv_sec;
    struct tm *sys_tm = localtime(&t);
    struct tm my_tm = *sys_tm;
    char s[16] = {0};
    switch (level){
    case 0:
        strcpy(s,"[debug]:");
        break;
    case 1:
        strcpy(s,"[info]:");
        break;
    case 2:
        strcpy(s,"[warn]:");
        break;
    case 3:
        strcpy(s,"[error]:");
        break;
    default:
        strcpy(s,"[info]:");
        break;
    }

    m_mutex.lock();
    m_cur_line++;
    if(m_today!=my_tm.tm_mday||m_cur_line%m_max_line==0){   //everydaylog
        char new_log[256]={0};
        fflush(m_fp);
        fclose(m_fp);
        char tail[16]={0};
        snprintf(tail, 16, "%d_%02d_%02d_", my_tm.tm_year + 1900, my_tm.tm_mon + 1, my_tm.tm_mday);
        if(m_today!=my_tm.tm_mday){
            snprintf(new_log, 255, "/%s%s", tail, log_name);
            m_today=my_tm.tm_mday;
            m_cur_line=0;
        }
        else{
            snprintf(new_log, 255, "/%s%s.%lld", tail, log_name, m_cur_line / m_max_line);
        }
        const char *path=NULL;
        string temp=string(m_file_name)+string(new_log);
        path=temp.c_str();
        m_fp=fopen(path,"a");
    }
    m_mutex.unlock();
    va_list valst;
    va_start(valst,format);
    string log_str;

    m_mutex.lock();
    int n = snprintf(m_buf, 48, "%d-%02d-%02d %02d:%02d:%02d.%06ld %s ",
                    my_tm.tm_year + 1900, my_tm.tm_mon + 1, my_tm.tm_mday,
                    my_tm.tm_hour, my_tm.tm_min, my_tm.tm_sec, now.tv_usec, s);
    //int vsnprintf(char *s, size_t n, const char *format, va_list arg)
    //功能：将格式化的可变参数列表写入大小的缓冲区，如果在printf上使用格式，则使用相同的文本组成字符串，如果使用arg标识的变量则将参数列表中的元素作为字符串存储在char型指针s中。

    int m=vsnprintf(m_buf+n,m_log_buf_size-1,format,valst);
    m_buf[n+m]='\n';
    m_buf[n+m+1]='\0';
    log_str=m_buf;
    m_mutex.unlock();
    

    if(m_is_async&&!m_log_queue->full()){//asynchronization
        m_log_queue->push(log_str);
    }
    else{                               //synchronization
        m_mutex.lock();
        fputs(log_str.c_str(),m_fp);
        m_mutex.unlock();
    }
    va_end(valst);
}

void log::flush(void){
    m_mutex.lock();
    fflush(m_fp);         
    m_mutex.unlock();
}

