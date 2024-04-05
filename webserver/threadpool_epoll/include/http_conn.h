//#pragma once
#ifndef HTTPCONNECTION_H
#define HTTPCONNECTION_H

#include<arpa/inet.h>
#include<sys/stat.h>
#include<stdarg.h>
#include<fcntl.h>
#include<sys/mman.h>
#include<sys/uio.h>
#include<sys/epoll.h>
#include<errno.h>
#include<string.h>


template <typename T>
class http_conn {
public:
    //所有socket上的事件都被注册到同一个epoll对象中
    static int m_epollfd; 
    static const int READ_BUFFER_SIZE =2048;
    static const int WRITE_BUFFER_SIZE = 1024;
    static const int FILENAME_LEN = 200;
    static int m_usr_count;

    //状态
    //HTTP请求方法
    enum METHOD {GET = 0, POST, HEAD, PUT, DELETE, TRACE, OPTIONS, CONNECT};
    //主状态机，当前正在分析的
    enum CHECK_STATE {CHECK_STATE_REQUESTLINE = 0, CHECK_STATE_HEADER, CHECK_STATE_CONTENT};
    //行的读取状态
    enum LINE_STATUS {LINE_OK = 0, LINE_BAD, LINE_OPEN};
    //服务器处理HTTP请求的可能结果
    enum HTTP_CODE {NO_REQUEST, GET_REQUEST, BAD_REQUEST, NO_RESOURCE, FORBIDDEN_REQUEST, FILE_REQUEST, INTERNAL_ERROR, CLOSED_CONNECTION};
    
    //http_conn<T>() {};
    //http_conn<T>(int sfd, const sockaddr_in &addr);
    ~http_conn<T>() {};

    //处理客户端请求
    void process();
    //void init(int sockfd, const sockaddr_in &addr);
    void close_conn();

    void init();
    void init(int sockfd, const sockaddr_in & addr);
    bool read(); //非阻塞读
    bool write(); //非阻塞写
    void unmap();


private:
    int m_sockfd; //该HTTP连接的socket
    sockaddr_in m_addr; //通信的socket地址
    char m_read_buf[READ_BUFFER_SIZE]; //读缓冲区
    int m_read_idx; //标识读缓冲区读入的最后一个字节的位置
    int m_checked_idx; //当前正在分析的字符在读缓冲区的位置
    int m_start_line; //当前正在解析行的起始位置

    struct stat m_file_stat; //目标文件的状态
    struct iovec m_iv[2];
    int m_iv_count; //被写内存块的数量
    char m_write_buf[WRITE_BUFFER_SIZE];
    int m_write_idx;
    int bytes_to_send;

    char m_real_file[FILENAME_LEN]; //客户请求的目标文件的完整路径
    bool m_linger; //是否保持连接
    char *m_file_address; //客户请求的目标文件被mmap到内存中的起始位置
    CHECK_STATE m_check_state; //主状态机当前所处的状态

    char *m_url;
    char *m_version;
    METHOD m_method;
    char *m_host;
    int m_content_length; //请求消息的总长度
    
    HTTP_CODE process_read(); //解析HTTP请求
    HTTP_CODE parse_request_line(char *text); //解析请求首行
    HTTP_CODE parse_request_headers(char *text); //解析请求头
    HTTP_CODE parse_request_content(char *text); //解析请求内容
    char *get_line() {return m_read_buf + m_start_line;}
    HTTP_CODE do_request();

    LINE_STATUS parse_line();

    bool process_write(HTTP_CODE ret);
    bool add_status_line(int status, const char *title);
    void add_headers(int content_len);
    bool add_content(const char *content);
    bool add_response(const char* format, ...);
    bool add_content_length(int content_len);
    bool add_content_type();
    bool add_linger();
    bool add_blank_line();
};

#endif