#include "http_conn.h"

#include <iostream>
using namespace std;
int setnonblocking(int fd){
    int old_option = fcntl(fd, F_GETFL);
    int new_option = old_option | O_NONBLOCK;
    fcntl(fd, F_SETFL, new_option);
    return old_option;
}

void addfd(int epollfd, int fd, bool one_shot, int TRIGMode){
    epoll_event event;
    event.data.fd = fd;

    if (1 == TRIGMode)
        event.events = EPOLLIN | EPOLLET | EPOLLRDHUP;
    else
        event.events = EPOLLIN | EPOLLRDHUP;

    if (one_shot)
        event.events |= EPOLLONESHOT;
    epoll_ctl(epollfd, EPOLL_CTL_ADD, fd, &event);
    setnonblocking(fd);
}

void removefd(int epollfd, int fd){
    epoll_ctl(epollfd, EPOLL_CTL_DEL, fd, 0);
    close(fd);
}
void modfd(int epollfd, int fd, int ev, int TRIGMode){
    epoll_event event;
    event.data.fd = fd;

    if (1 == TRIGMode)
        event.events = ev | EPOLLET | EPOLLONESHOT | EPOLLRDHUP;
    else
        event.events = ev | EPOLLONESHOT | EPOLLRDHUP;

    epoll_ctl(epollfd, EPOLL_CTL_MOD, fd, &event);
}

//定义http响应的一些状态信息
const char *ok_200_title = "OK";
const char *error_400_title = "Bad Request";
const char *error_400_form = "Your request has bad syntax or is inherently impossible to staisfy.\n";
const char *error_403_title = "Forbidden";
const char *error_403_form = "You do not have permission to get file form this server.\n";
const char *error_404_title = "Not Found";
const char *error_404_form = "The requested file was not found on this server.\n";
const char *error_500_title = "Internal Error";
const char *error_500_form = "There was an unusual problem serving the request file.\n";

//!!!!!!!!!!!!!!!!!!
//put variable to web_server class
locker m_lock;
map<string,string> users;

//initialize stctic variable
int http_conn::m_epollfd=-1;
int http_conn::m_user_count=0;



void http_conn::initmysql_result(sql_pool* connPool){
    MYSQL* mysql=NULL;
    connectionRAII mysqlcon(&mysql,connPool);
    if (mysql_query(mysql, "SELECT username,passwd FROM user")){
        LOG_ERROR("SELECT error:%s\n", mysql_error(mysql));
    }
    MYSQL_RES* result=mysql_store_result(mysql);
    while(MYSQL_ROW row=mysql_fetch_row(result)){
        string temp1(row[0]);
        string temp2(row[1]);
        users[temp1]=temp2;
    }
}

void http_conn::init()
{
    mysql = NULL;
    bytes_to_send = 0;
    bytes_have_send = 0;
    m_check_state = CHECK_STATE_REQUESTLINE;
    m_linger = false;
    m_method = GET;
    m_url = 0;
    m_version = 0;
    m_content_length = 0;
    m_host = 0;
    m_start_line = 0;
    m_checked_idx = 0;
    m_read_idx = 0;
    m_write_idx = 0;
    cgi = 0;
    m_state = 0;

    RW_done=0;
    RW_error=0;

    memset(m_read_buf, '\0', READ_BUFFER_SIZE);
    memset(m_write_buf, '\0', WRITE_BUFFER_SIZE);
    memset(m_real_file, '\0', FILENAME_LEN);
}

void http_conn::init(int sockfd, const sockaddr_in &addr, char *root, int TRIGMode,int close_log, string user, string passwd, string sqlname){
    m_sockfd=sockfd;
    m_address=addr;
    doc_root=root;
    m_TrigMode=TRIGMode;
    m_close_log=close_log;

    addfd(m_epollfd,m_sockfd,true,m_TrigMode);
    m_user_count++;
    strcpy(sql_user,user.c_str());
    strcpy(sql_passwd,passwd.c_str());
    strcpy(sql_name,sqlname.c_str());
}

void http_conn::close_conn(bool real_close){
    if(real_close&(m_sockfd!=-1)){
        LOG_INFO("close %d\n",m_sockfd);
        removefd(m_epollfd,m_sockfd);
        m_user_count--;
        m_sockfd=-1;
    }
}

void http_conn::unmap(){
    if (m_file_adderss){
        munmap(m_file_adderss, m_file_stat.st_size);
        m_file_adderss = 0;
    }
}

//循环读取客户数据，直到无数据可读或对方关闭连接
//非阻塞ET工作模式下，需要一次性将数据读完
bool http_conn::read_once(){


    if(m_read_idx>=READ_BUFFER_SIZE)
        return false;
    
    int bytes_read=0;
    //LT读取数据
    if(0==m_TrigMode){
        bytes_read=recv(m_sockfd,m_read_buf+m_read_idx,READ_BUFFER_SIZE-m_read_idx,0);
        if(bytes_read<=0){
            return false;
        }

        
        m_read_idx+=bytes_read;
        return true;
    }
    else{
    //ET读取数据
        while(true){
            bytes_read=recv(m_sockfd,m_read_buf+m_read_idx,READ_BUFFER_SIZE-m_read_idx,0);
            if(bytes_read==-1){
                if(errno==EAGAIN||errno==EWOULDBLOCK)
                    break;
                return false;
            }
            else if(bytes_read==0){
                return false;
            }
            m_read_idx+=bytes_read;
        }
        return true;
    }
}
//① writev以顺序iov[0]、iov[1]至iov[iovcnt-1]从各缓冲区中聚集输出数据到fd
//② readv则将从fd读入的数据按同样的顺序散布到各缓冲区中，readv总是先填满一个缓冲区，然后再填下一个
//EAGAIN(try again,nonblock,ex,write:the buffer/fd is full.read:the buffer is empty,please try again(later)) https://www.cnblogs.com/pigerhan/archive/2013/02/27/2935403.html
//https://www.cnblogs.com/pigerhan/archive/2013/02/27/2935403.html
//https://mp.weixin.qq.com/s?__biz=MzAxNzU2MzcwMw==&mid=2649274431&idx=1&sn=2dd28c92f5d9704a57c001a3d2630b69&chksm=83ffb167b48838715810b27b8f8b9a576023ee5c08a8e5d91df5baf396732de51268d1bf2a4e&token=1686112912&lang=zh_CN#rd
bool http_conn::write(){
    int temp=0;
    //如果需要发送的数据大小为0，则等待新数据到来
    if(bytes_to_send==0){
        modfd(m_epollfd,m_sockfd,EPOLLIN,m_TrigMode);
        init();
        return true;
    }

    while(1){
        temp=writev(m_sockfd,m_iv,m_iv_count);
        if(temp<0){
            if(errno==EAGAIN){
                modfd(m_epollfd,m_sockfd,EPOLLOUT,m_TrigMode);
                return true;
            }
            unmap();
            return false;
        }
        //更新已经发送的数据和需要发送的数据 字节数
        bytes_have_send+=temp;
        bytes_to_send-=temp;
        if(bytes_have_send>=m_iv[0].iov_len){
            //发送当前文件
            m_iv[0].iov_len=0;
            //???为什么减去m_write_idx
            //because byte_to_send=m_write_idx+m_file_stat.st.size(see func:process_write())
            m_iv[1].iov_base=m_file_adderss+(bytes_have_send-m_write_idx);
            m_iv[1].iov_len=bytes_to_send;
        }
        else{
            //发送当前url响应报文
            //????m_file_adderss and m_write_buf
            m_iv[0].iov_base=m_write_buf+bytes_have_send;
            m_iv[0].iov_len=m_iv[0].iov_len-bytes_have_send;
        }

        if(bytes_to_send<=0){
            unmap();
            modfd(m_epollfd,m_sockfd,EPOLLIN,m_TrigMode);
            //优雅关闭
            if(m_linger){
                init();
                return true;
            }
            else{
                return false;
            }
            
        }
        //注意这里没有返回，如果数据没有写完，则将一直处于while(1)循环中
    }
}

//C/C++大文件/数据网络传输方法总结 https://blog.csdn.net/jmppok/article/details/18360595
//https://mp.weixin.qq.com/s?__biz=MzAxNzU2MzcwMw==&mid=2649274431&idx=1&sn=2dd28c92f5d9704a57c001a3d2630b69&chksm=83ffb167b48838715810b27b8f8b9a576023ee5c08a8e5d91df5baf396732de51268d1bf2a4e&token=1686112912&lang=zh_CN#rd
bool http_conn::write_errBigFile(){
    int temp=0;
    //如果需要发送的数据大小为0，则等待新数据到来
    int new_add=0;
    if(bytes_to_send==0){
        modfd(m_epollfd,m_sockfd,EPOLLIN,m_TrigMode);
        init();
        return true;
    }

    while(1){
        //https://blog.csdn.net/zhangge3663/article/details/84584335
        temp=writev(m_sockfd,m_iv,m_iv_count);
        if(temp>=0){
            bytes_have_send+=temp;
            //new_add=bytes_have_send-m_write_idx;
            bytes_to_send-=temp;
        }
        else{
            if(errno==EAGAIN){
                if(bytes_have_send>=m_iv[0].iov_len){
                    m_iv[0].iov_len=0;
                    //m_iv[1].iov_base=m_file_adderss+new_add;
                    m_iv[1].iov_base=m_file_adderss+bytes_have_send-m_write_idx;
                    //m_iv[1].iov_len=bytes_to_send;
                    m_iv[1].iov_len=bytes_to_send;
                }
                else{
                    m_iv[0].iov_base=m_write_buf+bytes_have_send;
                    m_iv[0].iov_len=m_iv[0].iov_len-bytes_have_send;
                }
                modfd(m_epollfd,m_sockfd,EPOLLOUT,m_TrigMode);
                return true;
            }
            unmap();
            return false;
        }

        //bytes_to_send-=temp;
        
        if(bytes_to_send<=0){
            unmap();
            modfd(m_epollfd,m_sockfd,EPOLLIN,m_TrigMode);
            //优雅关闭
            if(m_linger){
                init();
                return true;
            }
            else{
                return false;
            }
            
        }
        //注意这里没有返回，如果数据没有写完，则将一直处于while(1)循环中
    }
}

//从状态机，用于分析出一行内容
//返回值为行的读取状态，有LINE_OK,LINE_BAD,LINE_OPEN
http_conn::LINE_STATUS http_conn::parse_line(){
    char temp;
    for(;m_checked_idx<m_read_idx;m_checked_idx++){
        temp=m_read_buf[m_checked_idx];
        if(temp=='\r'){
            if(m_checked_idx+1==m_read_idx)
                return LINE_OPEN;
            else if(m_read_buf[m_checked_idx+1]=='\n'){
                m_read_buf[m_checked_idx++]='\0';
                m_read_buf[m_checked_idx++]='\0';
                return LINE_OK;
            }
            return LINE_BAD;
        }
        else if(temp=='\n'){
            if(m_checked_idx>1 && m_read_buf[m_checked_idx-1]=='\r'){
                m_read_buf[m_checked_idx++]='\0';
                m_read_buf[m_checked_idx++]='\0';
                return LINE_OK;
            }
            return LINE_BAD;
        }
    }
    return LINE_OPEN;
}
//strpbrk检索str1中第一个匹配str2中字符的 字符
//char str[] = "I welcome any ideas from readers， of course.";
//char *rc=strpbrk(str,"come");//elocome.....  e是第一个匹配come中的单词
//示例代码的运行结果为“elcome any ideas from readers，of course.”
http_conn::HTTP_CODE http_conn::parse_request_line(char *text){
    m_url=strpbrk(text," \t");  //**************** why note: " \t" instead of "\t"  
    //m_url=strpbrk(text," ");  //reason: space is also complete the same function.
    if(!m_url)
        return BAD_REQUEST;
    
    *m_url++='\0';
    char* method=text;
    //忽略大小写,返回值 若参数s1和s2字符串相等则返回0。s1大于s2则返回大于0 的值，s1 小于s2 则返回小于0的值
    if(strcasecmp(method,"GET")==0){
        m_method=GET;
    }
    else if(strcasecmp(method,"POST")==0){
        m_method=POST;
        cgi=1;
    }
    else{
        return BAD_REQUEST;
    }
    //strspn检索str1中第一个不在str2中出现的字符下表
    m_url+=strspn(m_url," \t");  //过滤tab符，找到m_url的开头
    //m_url+=strspn(m_url," "); 
    m_version=strpbrk(m_url," \t"); //找到m_version（开头有tab）
    if(!m_version)
        return BAD_REQUEST;
    *m_version++='\0';
    //cout<<"m_url:***************"<<m_url<<"   "<<endl;
    m_version+=strspn(m_version," \t");//过滤tab符，找到m_version的开头
    if(strcasecmp(m_version,"HTTP/1.1")!=0)
        return BAD_REQUEST;
    if(strncasecmp(m_url,"http://",7)==0){
        m_url+=7;
        m_url=strchr(m_url,'/');
    }
    if(strncasecmp(m_url,"https://",8)==0){
        m_url+=7;
        m_url=strchr(m_url,'/');
    }
    if(!m_url || m_url[0]!='/')
        return BAD_REQUEST;
    
    if (strlen(m_url) == 1)
        strcat(m_url, "judge.html");
    m_check_state = CHECK_STATE_HEADER;
    return NO_REQUEST;
}

http_conn::HTTP_CODE http_conn::parse_headers(char *text){
    if(text[0]=='\0'){
        if(m_content_length!=0){
            m_check_state=CHECK_STATE_CONTENT;
            return NO_REQUEST;
        }
        return GET_REQUEST;
    }
    else if(strncasecmp(text,"Connection:",11)==0){
        text+=11;
        text+=strspn(text," \t");
        if(strcasecmp(text,"keep-alive")==0){
            m_linger=true;
        }
    }
    else if(strncasecmp(text,"Content-length:",15)==0){
        text+=15;
        text+=strspn(text," \t");
        m_content_length=atol(text);
    }
    else if(strncasecmp(text,"Host:",5)==0){
        text+=5;
        text+=strspn(text," \t");
        m_host=text;
    }
    else{
        LOG_INFO("oop! unknow header: %s",text);
    }
    return NO_REQUEST;
}

//判断是否完全读入
http_conn::HTTP_CODE http_conn::parse_content(char *text){
    //？？？？？？？？？？？m_start_line
    //已解决：parse_headers时 修改m_check_state=CHECK_STATE_CONTENT && line_status==LINE_OK  因此while只走了第一个判断，因此不会调用parse_line()，也就不会修改m_check_idx
    
    //判断buffer中是否读取了消息体
    if(m_read_idx>=(m_content_length+m_checked_idx)){
        text[m_content_length]='\0';
        //POST请求中最后为输入的用户名和密码
        m_string=text;
        return GET_REQUEST;
    }
    return NO_REQUEST;
}
http_conn::HTTP_CODE http_conn::do_request(){
    strcpy(m_real_file,doc_root);
    int len=strlen(doc_root);
    LOG_INFO("************** m_url %s  ************",m_url);
    //strrchr 函数在字符串 s 中是从后到前（或者称为从右向左）查找字符 c，找到字符 c 第一次出现的位置就返回，返回值指向这个位置。
    const char* p=strrchr(m_url,'/');

    //处理CGI
    if(cgi==1&&(*(p+1)=='2'||*(p+1)=='3')){
        char flag=m_url[1];
        char* m_url_real=(char*)malloc(sizeof(char)*200);
        strcpy(m_url_real,"/");
        //??????????????????
        strcat(m_url_real,m_url+2);
        strncpy(m_real_file + len, m_url_real, FILENAME_LEN - len - 1);
        free(m_url_real);

        //提取用户和密码
        //user=123&password=123
        char name[100],password[100];
        int i;
        for(i=5;m_string[i]!='&';++i)
            name[i-5]=m_string[i];
        name[i-5]='\0';
        
        int j=0;
        //i=i+10   &password=
        for(i=i+10;m_string[i]!='\0';++i,++j)
            password[j]=m_string[i];
        password[j]='\0';
        cout<<name<<"  "<<password<<endl;
        for(auto it=users.begin();it!=users.end();it++){
            cout<<it->first<<"  "<<it->second<<endl;
        }
        
        if(*(p+1)=='3'){
            char* sql_insert=(char*)malloc(sizeof(char)*200);
            strcpy(sql_insert,"INSERT INTO user(username,passwd) VALUES(");
            strcat(sql_insert,"'");
            strcat(sql_insert,name);
            strcat(sql_insert,"','");
            strcat(sql_insert,password);
            strcat(sql_insert,"')");
            cout<<sql_insert<<endl;
            if(users.find(name)==users.end()){
                m_lock.lock();
                int res=mysql_query(mysql,sql_insert);
                users.insert(pair<string,string>(name,password));
                m_lock.unlock();
                if (!res)
                    strcpy(m_url, "/log.html");
                else
                    strcpy(m_url, "/registerError.html");
            }
            else{
                strcpy(m_url, "/registerError.html");
            }
        }
        else if (*(p + 1) == '2'){
            if (users.find(name) != users.end() && users[name] == password)
                strcpy(m_url, "/welcome.html");
            else
                strcpy(m_url, "/logError.html");
        }
    }

    if(*(p+1)=='0'){
        char *m_url_real = (char *)malloc(sizeof(char) * 200);
        strcpy(m_url_real, "/register.html");
        strncpy(m_real_file + len, m_url_real, strlen(m_url_real));
        free(m_url_real);
    }
    else if (*(p + 1) == '1'){
        char *m_url_real = (char *)malloc(sizeof(char) * 200);
        strcpy(m_url_real, "/log.html");
        strncpy(m_real_file + len, m_url_real, strlen(m_url_real));
        free(m_url_real);
    }
    else if (*(p + 1) == '5'){
        char *m_url_real = (char *)malloc(sizeof(char) * 200);
        strcpy(m_url_real, "/picture.html");
        strncpy(m_real_file + len, m_url_real, strlen(m_url_real));
        free(m_url_real);
    }
    else if (*(p + 1) == '6'){
        char *m_url_real = (char *)malloc(sizeof(char) * 200);
        strcpy(m_url_real, "/video.html");
        strncpy(m_real_file + len, m_url_real, strlen(m_url_real));
        free(m_url_real);
    }
    else if (*(p + 1) == '7'){
        char *m_url_real = (char *)malloc(sizeof(char) * 200);
        strcpy(m_url_real, "/fans.html");
        strncpy(m_real_file + len, m_url_real, strlen(m_url_real));
        free(m_url_real);
    }
    else
        //否则发送url实际请求的文件
        strncpy(m_real_file + len, m_url, FILENAME_LEN - len - 1);
        
    if(stat(m_real_file,&m_file_stat)<0)
        return NO_REQUEST;
    if(!(m_file_stat.st_mode&S_IROTH))
        return FORBIDDEN_REQUEST;
    if(S_ISDIR(m_file_stat.st_mode))
        return BAD_REQUEST;
    
    int fd=open(m_real_file,O_RDONLY);
    m_file_adderss=(char*)mmap(0,m_file_stat.st_size,PROT_READ, MAP_PRIVATE, fd, 0);
    close(fd);
    ////表示请求文件存在，且可以访问
    return FILE_REQUEST;
}
http_conn::HTTP_CODE http_conn::process_read(){
    LINE_STATUS line_status=LINE_OK;
    HTTP_CODE ret=NO_REQUEST;
    char* text=0;
    ////已解决：parse_headers时 修改m_check_state=CHECK_STATE_CONTENT && line_status==LINE_OK  因此while只走了第一个判断，因此不会调用parse_line()，也就不会修改m_check_idx
    while((m_check_state == CHECK_STATE_CONTENT && line_status == LINE_OK) || ((line_status = parse_line()) == LINE_OK)){
        text=get_line();
        m_start_line=m_checked_idx;
        LOG_INFO("%s",text);
        switch(m_check_state){
            case CHECK_STATE_REQUESTLINE:{
                ret=parse_request_line(text);
                LOG_INFO("******************** m_url %s  m_version %s   %d",m_url,m_version,ret);
                if(ret==BAD_REQUEST)
                    return BAD_REQUEST;
                break;
            }
            case CHECK_STATE_HEADER:{
                ret=parse_headers(text);
                LOG_INFO("******************** m_url %s  m_version %s   %d",m_url,m_version,ret);
                if (ret == BAD_REQUEST){
                    return BAD_REQUEST;
                }
                else if (ret == GET_REQUEST){
                    return do_request();
                }
                break;
            }
            case CHECK_STATE_CONTENT:{
                ret = parse_content(text);
                LOG_INFO("******************** m_url %s  m_version %s   %d",m_url,m_version,ret);
                if (ret == GET_REQUEST)
                    return do_request();
                line_status = LINE_OPEN;
                break;
            }
            default:
                return INTERNAL_ERROR;
        }
    }
    return NO_REQUEST;
}
bool http_conn::add_response(const char *format, ...){
    if(m_write_idx>=WRITE_BUFFER_SIZE)
        return false;
    va_list arg_list;
    va_start(arg_list,format);
    int len=vsnprintf(m_write_buf+m_write_idx,WRITE_BUFFER_SIZE-m_write_idx-1,format,arg_list);
    if(len>=(WRITE_BUFFER_SIZE-1-m_write_idx)){
        va_end(arg_list);
        return false;
    }
    m_write_idx+=len;
    va_end(arg_list);
    LOG_INFO("request: \n %s",m_write_buf);
    return true;
}
bool http_conn::add_status_line(int status, const char *title){
    return add_response("%s %d %s\r\n","HTTP/1.1",status,title);
}

bool http_conn::add_headers(int content_len){
    return add_content_length(content_len) && add_linger() && add_blank_line();
}
bool http_conn::add_content_length(int content_len){
    return add_response("Content-Length:%d\r\n", content_len);
}
bool http_conn::add_linger(){
    return add_response("Connection:%s\r\n", (m_linger == true) ? "keep-alive" : "close");
}
bool http_conn::add_blank_line(){
    return add_response("%s", "\r\n");
}

bool http_conn::add_content(const char *content){
    return add_response("%s",content);
}
bool http_conn::process_write(HTTP_CODE ret){
    switch (ret)
    {
    case INTERNAL_ERROR:{
        add_status_line(500,error_500_title);
        add_headers(strlen(error_500_form));
        if(!add_content(error_500_form))
            return false;
        break;
    }
    case BAD_REQUEST:{
        add_status_line(404,error_404_title);
        add_headers(strlen(error_404_form));
        if(!add_content(error_404_form))
            return false;
        break;
    }
    case FORBIDDEN_REQUEST:{
        add_status_line(404,error_403_title);
        add_headers(strlen(error_403_form));
        if(!add_content(error_403_form))
            return false;
        break;
    }    
    //updata m_iv[0] and m_iv[1]
    case FILE_REQUEST:{
        add_status_line(200,ok_200_title);
        if(m_file_stat.st_size!=0){
            add_headers(m_file_stat.st_size);
            m_iv[0].iov_base=m_write_buf;
            m_iv[0].iov_len=m_write_idx;
            m_iv[1].iov_base=m_file_adderss;
            m_iv[1].iov_len=m_file_stat.st_size;
            m_iv_count=2;
            bytes_to_send=m_write_idx+m_file_stat.st_size;
            return true;
        }
        else{
            const char *ok_string = "<html><body></body></html>";
            add_headers(strlen(ok_string));
            if (!add_content(ok_string))
                return false;
        }
    }   
    default:
        return false;
    }
    //INTERNAL_ERROR  BAD_REQUEST  FORBIDDEN_REQUEST
    m_iv[0].iov_base=m_write_buf;
    m_iv[0].iov_len=m_write_idx;
    m_iv_count=1;
    bytes_to_send=m_write_idx;
    return true;
}

//此时 web_server.deal_read/write（proactor）或者thread_pool(reactor).run已经将需要读/写的数据准备好了
//工作线程run在数据准备后,直接调用http_conn::process(),处理客户任务
void http_conn::process(){
    cout<<"m_read_buf: "<<m_read_buf<<endl;
    HTTP_CODE read_ret=process_read();
    LOG_INFO("********** %d *************",read_ret);
    if(read_ret==NO_REQUEST){
        //当未读取完整的数据时(ex,1.process_read中parse_line返回LINE_OPEN)，2.LINE_OPEN)
        modfd(m_epollfd,m_sockfd,EPOLLIN,m_TrigMode);
        return ;
    }
    
    bool write_ret=process_write(read_ret);
    if(!write_ret){
        close_conn();
    }
    modfd(m_epollfd,m_sockfd,EPOLLOUT,m_TrigMode);
}




