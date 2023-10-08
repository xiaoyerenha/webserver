#ifndef HTTPCONNECTION_H
#define HTTPCONNECTION_H
//#pragma once

#include<sys/epoll.h>
#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#include<unistd.h>
#include<signal.h>
#include<sys/types.h>
#include<fcntl.h>
#include<sys/socket.h>
#include<netinet/in.h>
#include<arpa/inet.h>
#include<sys/stat.h>
#include<sys/mman.h>
#include<stdarg.h>
#include<errno.h>
#include<sys/uio.h>

class http_conn
{
public:
    static int m_epollfd; //所有的socket上的事件都被注册到同一个epoll对象中
    static int m_user_count; //统计用户的数量
    static const int READ_BUFFER_SIZE = 2048; //读缓冲区大小
    static const int WRITE_BUFFER_SIZE = 1024; //写缓冲区大小
    static const int FILENAME_LEN = 200; //文件名的最大长度

    //状态
    //HTTP请求方法
    enum METHOD {GET = 0, POST, HEAD, PUT, DELETE, TRACE, OPTIONS, CONNECT};
    //主状态机，当前正在分析的
    enum CHECK_STATE {CHECK_STATE_REQUESTLINE = 0, CHECK_STATE_HEADER, CHECK_STATE_CONTENT};
    //行的读取状态
    enum LINE_STATUS {LINE_OK = 0, LINE_BAD, LINE_OPEN};
    //服务器处理HTTP请求的可能结果
    enum HTTP_CODE {NO_REQUEST, GET_REQUEST, BAD_REQUEST, NO_RESOURCE, FORBIDDEN_REQUEST, FILE_REQUEST, INTERNAL_ERROR, CLOSED_CONNECTION};

    http_conn() {};
    ~http_conn() {};

    void process(); //处理客户端请求

    void init(int sockfd, const sockaddr_in & addr); //初始化新接收的连接
    void close_conn();  //关闭连接

    bool read(); //非阻塞的读
    bool write(); //非阻塞的写

private:
    int m_sockfd; //该HTTP连接的socket
    sockaddr_in m_address; //通信的socket地址
    char m_read_buf[READ_BUFFER_SIZE]; //读缓冲区
    int m_read_idx; //标识读缓冲区读入的最后一个字节的位置

    int m_checked_index; //当前正在分析的字符在读缓冲区的位置
    int m_start_line; //当前正在解析行的起始位置

    char m_real_file[FILENAME_LEN]; // 客户请求的目标文件的完整路径
    char * m_url; //请求目标文件的文件名
    char * m_version; //协议版本
    METHOD m_method;
    char * m_host; //主机名
    int m_content_length; //请求消息的总长度
    bool m_linger; //是否保持连接
    struct stat m_file_stat; //目标文件的状态
    char * m_file_address; //客户请求的目标文件被mmap到内存中的起始位置

    CHECK_STATE m_check_state; //主状态机当前所处的状态

    char m_write_buf[WRITE_BUFFER_SIZE];
    int m_write_idx;
    struct iovec m_iv[2];
    int m_iv_count;  //被写内存块的数量

    void init(); //初始化

    HTTP_CODE process_read(); //解析HTTP请求
    HTTP_CODE parse_request_line(char * text); //解析请求首行
    HTTP_CODE parse_request_headers(char * text); //解析请求头
    HTTP_CODE parse_request_content(char * text); //解析请求内容

    LINE_STATUS parse_line(); //解析行

    char * get_line() { return m_read_buf + m_start_line; }
    HTTP_CODE do_request();
    void unmap();

    bool add_response(const char* format, ...);
    bool add_status_line(int status, const char* title);
    void add_headers(int content_len);
    bool add_content(const char* content);
    bool add_content_length(int content_len);
    bool add_content_type();
    bool add_linger();
    bool add_blank_line();
    bool process_write(HTTP_CODE ret);

    int bytes_to_send; 
    int bytes_have_send;
};

#endif