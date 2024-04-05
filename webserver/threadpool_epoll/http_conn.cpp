#include "./include/http_conn.h"
#include <iostream>
#include<unistd.h>

using namespace std;

template<typename T>
int http_conn<T>::m_epollfd = -1;
template<typename T>
int http_conn<T>::m_usr_count = 0;

//定义响应的状态信息
const char* ok_200_title = "OK";
const char* error_400_title = "Bad Request";
const char* error_400_form = "Your request has bad syntax or is inherently impossible to satisfy.\n";
const char* error_403_title = "Forbidden";
const char* error_403_form = "You do not have permission to get file from this server.\n";
const char* error_404_title = "Not Found";
const char* error_404_form = "The requested file was not found on this server.\n";
const char* error_500_title = "Internal Error";
const char* error_500_form = "There was an unusual problem serving the requested file.\n";

//网站的根目录
const char* doc_root = "/home/ma/code/MyLinuxWebServer/webserver_new/threadpool/resources";

void setnonblocking(int fd) {
    int old_flag = fcntl(fd, F_GETFL);
    int new_flag = old_flag | O_NONBLOCK;
    fcntl(fd, F_SETFL, new_flag);
}

//添加文件描述符到epoll
void addfd(int epollfd, int fd, bool one_shot) {
    epoll_event event;
    event.data.fd = fd;
    event.events = EPOLLIN | EPOLLRDHUP;

    if(one_shot) {
        event.events |= EPOLLONESHOT;
    }

    epoll_ctl(epollfd, EPOLL_CTL_ADD, fd, &event);

    //设置文件描述符非阻塞
    setnonblocking(fd);
}

//修改，重置socket上EPOLLONESHOT事件，以确保下一次可读时，EPOLLIN事件能被触发
void modfd(int epollfd, int fd, int ev) {
    epoll_event event;
    event.data.fd = fd;
    event.events = ev | EPOLLONESHOT | EPOLLRDHUP;
    epoll_ctl(epollfd, EPOLL_CTL_MOD, fd, &event);
}

//从epoll中删除文件描述符
void removefd(int epollfd, int fd) {
    epoll_ctl(epollfd, EPOLL_CTL_DEL, fd, 0);
    close(fd);
}

template <typename T>
bool http_conn<T>::read() {
    if(m_read_idx >= READ_BUFFER_SIZE) {
        return false;
    }

    //读取到字节
    int bytes_read = 0;
    while(true) {
        bytes_read = recv(m_sockfd, m_read_buf+m_read_idx, READ_BUFFER_SIZE-m_read_idx, 0);
        if(bytes_read == -1) {
            if(errno == EAGAIN || errno == EWOULDBLOCK) {
                //没有数据
                break; 
            }
            return false;
        }
        else if(bytes_read == 0) {
            //对方关闭连接
            return false;
        }
        m_read_idx += bytes_read;
    }
    cout << "一次性读完所有数据" << m_read_buf << endl;
    return true;
}

template <typename T>
bool http_conn<T>::write() {
    cout << "一次性写完所有数据" << endl;

    int temp = 0;
    int bytes_have_send = 0;
    //将要发送的字节
    int bytes_to_send = m_write_idx;

    //将要发送的字节为0，本次响应结束
    if(bytes_to_send == 0) {
        modfd(m_epollfd, m_sockfd, EPOLLIN);
        init();
        return true;
    }

    while(1) {
        //分散写
        temp = writev(m_sockfd, m_iv, m_iv_count);
        if(temp <= -1) {
            //写缓存没有空间，等待下一轮EPOLLOUT事件
            if(errno == EAGAIN) {
                modfd(m_epollfd, m_sockfd, EPOLLOUT);
                return true;
            }
            //对内存映射区执行munmap操作
            unmap();
            return false;
        }

        bytes_to_send -= temp;
        bytes_have_send += temp;
        if(m_iv[0].iov_len <= bytes_have_send) {
            m_iv[0].iov_len = 0;
            m_iv[1].iov_base = m_file_address + (bytes_have_send - m_write_idx);
            m_iv[1].iov_len = bytes_to_send;
        }
        else {
            m_iv[0].iov_base = m_write_buf + bytes_have_send;
            m_iv[0].iov_len = m_iv[0].iov_len - temp;
        }

        if(bytes_to_send <= 0) {
            //没有数据发送
            unmap();
            modfd(m_epollfd, m_sockfd, EPOLLIN);

            if(m_linger) {
                init();
                return true;
            }
            else return false;
        }
    }
}

template <typename T>
void http_conn<T>::unmap() {
    if(m_file_address) {
        munmap(m_file_address, m_file_stat.st_size);
        m_file_address = 0;
    }
}

template <typename T>
void http_conn<T>::init(int sockfd, const sockaddr_in & addr) {
    m_sockfd = sockfd;
    m_addr = addr;

    //端口复用
    int reuse = 1;
    setsockopt(m_sockfd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));

    //添加到epoll对象中
    addfd(m_epollfd, sockfd, true);
    m_usr_count++;

    init();

}

template <typename T>
void http_conn<T>::init() {
    
    bytes_to_send = 0;
    m_check_state = CHECK_STATE_REQUESTLINE;
    m_checked_idx = 0;
    m_start_line = 0;
    m_read_idx = 0;
    m_method = GET;
    m_url = 0;
    m_version = 0;
    m_content_length = 0;
    m_host = 0;
    m_linger = false;
    m_write_idx = 0;
    bzero(m_read_buf, READ_BUFFER_SIZE);
    bzero(m_write_buf, WRITE_BUFFER_SIZE);
    bzero(m_real_file, FILENAME_LEN);
}

template<typename T>
void http_conn<T>::process() {
    cout << "parse request, create response" << endl;
    
    //接收数据
    int bytes_read = 0;
    if((bytes_read = recv(m_sockfd, m_read_buf + m_read_idx, READ_BUFFER_SIZE - m_read_idx, 0))!=0) {
        m_read_idx += bytes_read;
    }
    cout << "m_read_idx: " << m_read_idx << endl;


    //解析HTTP请求
    http_conn<T>::HTTP_CODE read_ret = process_read();
    cout << read_ret << endl;
    if(read_ret == NO_REQUEST) {
        return;
    }
    
    cout << "read_ret: " << read_ret << endl;
    //生成响应
    bool write_ret = process_write(read_ret);

    int bytes_have_send = 0;
    bytes_to_send = m_write_idx;
    int temp;

    cout << "bytes_to_send: " << bytes_to_send << endl;

    if((temp = writev(m_sockfd, m_iv, m_iv_count))!=-1) {
        cout << "write_buff: " << m_iv[0].iov_base << endl;
        cout << "file_address: " << m_iv[1].iov_base << endl;
        /*
        bytes_to_send -= temp;
        bytes_have_send += temp;
        if(m_iv[0].iov_len <= bytes_have_send) {
            m_iv[0].iov_len = 0;
            m_iv[1].iov_base = m_file_address + (bytes_have_send - m_write_idx);
            m_iv[1].iov_len = bytes_to_send;
        }
        else {
            m_iv[0].iov_base = m_write_buf + bytes_have_send;
            m_iv[0].iov_len = m_iv[0].iov_len - temp;
        }
        */
    }

    if(!write_ret) {
        //close_conn();
        if(m_sockfd != -1) {
            m_sockfd = -1;
            m_usr_count--;
        }
    }
    //close(m_sockfd);
}

template<typename T>
void http_conn<T>::close_conn() {
    if(m_sockfd != -1) {
        removefd(m_epollfd, m_sockfd);
        m_sockfd = -1;
        m_usr_count--;
    }
}

template <typename T>
typename http_conn<T>::HTTP_CODE http_conn<T>::process_read() {
    LINE_STATUS line_status = LINE_OK;
    http_conn<T>::HTTP_CODE ret = NO_REQUEST;

    char *text = 0;

    while(((m_check_state == CHECK_STATE_CONTENT) && (line_status == LINE_OK)) || (line_status = parse_line()) == LINE_OK) {
        
        text = get_line(); //获取一行数据

        m_start_line = m_checked_idx;
        cout << "got 1 http line: " << text << endl;

        cout << "m_check_state: " << m_check_state << endl;
        switch (m_check_state)
        {
        case CHECK_STATE_REQUESTLINE:
            ret = parse_request_line(text);
            if(ret == BAD_REQUEST) {
                return BAD_REQUEST;
            }
            break;

        case CHECK_STATE_HEADER:
            ret = parse_request_headers(text);
            if(ret == BAD_REQUEST) {
                return BAD_REQUEST;
            } 
            else if(ret == GET_REQUEST) {
                return do_request();
            }

        case CHECK_STATE_CONTENT:
            ret = parse_request_content(text);
            if(ret == GET_REQUEST) {
                return do_request();
            }
            line_status = LINE_OPEN;
            break;
        
        default:
            return INTERNAL_ERROR;
        }
    }
    return NO_REQUEST;
}

template <typename T>
typename http_conn<T>::HTTP_CODE http_conn<T>::parse_request_line(char *text) {
    //获得请求方法，目标URL，HTTP版本
    //GET /index.html HTTP/1.1
    m_url = strpbrk(text, " \t");

    *m_url++ = '\0';

    cout << "parse_request_line..." << endl;

    char *method = text;
    if(strcasecmp(method, "GET") == 0) {
        m_method = GET;
    }
    else {
        return BAD_REQUEST;
    }

    m_version = strpbrk(m_url, " \t");
    if(!m_version) {
        return BAD_REQUEST;
    }
    *m_version++ = '\0';

    if(strcasecmp(m_version, "HTTP/1.1") != 0) {
        return BAD_REQUEST;
    }

    if(strncasecmp(m_url, "http://", 7) == 0) {
        m_url += 7;
        m_url = strchr(m_url, '/');
    }

    if(!m_url || m_url[0] != '/') {
        return BAD_REQUEST;
    }

    m_check_state = CHECK_STATE_HEADER;
    return NO_REQUEST;
}
template <typename T>
typename http_conn<T>::HTTP_CODE http_conn<T>::parse_request_headers(char *text) {
    cout << "parse_request_headers..." << endl;
    //遇到空行，表示解析完毕
    if(text[0] == '\0') {
        //由消息体，状态机转移到CHECK_STATE_CONTENT状态
        if(m_content_length != 0) {
            m_check_state = CHECK_STATE_CONTENT;
            return NO_REQUEST;
        }
        return GET_REQUEST;
    }
    else if(strncasecmp(text, "Connection:", 11) == 0) {
        text += 11;
        text += strspn(text, " \t");
        if(strcasecmp(text, "keep-alive") == 0) {
            m_linger = true;
        }
    }
    else if(strncasecmp(text, "Content-Length:", 15) == 0) {
        text += 15;
        text += strspn(text, " \t");
        m_content_length = atol(text);
    }
    else if(strncasecmp(text, "Host:", 5) == 0) {
        text += 5;
        text += strspn(text, " \t");
        m_host = text;
    }
    else {
        cout << "oop! unknow header " << text << endl;
    }
    return NO_REQUEST;
}
template <typename T>
typename http_conn<T>::HTTP_CODE http_conn<T>::parse_request_content(char *text) {
    cout << "parse_request_content..." << endl;
    //仅判断是否被完整的读入
    if(m_read_idx >= (m_content_length + m_checked_idx)) {
        text[m_content_length] = '\0';
        return GET_REQUEST;
    }
    return NO_REQUEST;
}
/*
    当得到一个完整、正确的HTTP请求时，分析目标文件的属性：
        如果目标文件存在、对所有用户可读，且不是目录，则使用mmap将其映射到内存地址 m_file_address处，
        并告诉调用者获取文件成功。
*/
template <typename T>
typename http_conn<T>::HTTP_CODE http_conn<T>::do_request() {
    cout << "do_request..." << endl;
    strcpy(m_real_file, doc_root);
    int len = strlen(doc_root);
    strncpy(m_real_file + len, m_url, FILENAME_LEN - len - 1);

    if(stat(m_real_file, &m_file_stat) < 0) {
        return NO_REQUEST;
    }

    //判断访问权限 S_IROTH:其他用户读权限
    if(!(m_file_stat.st_mode & S_IROTH)) {
        return FORBIDDEN_REQUEST;
    }

    //判断是否是目录
    if(S_ISDIR(m_file_stat.st_mode)) {
        return BAD_REQUEST;
    }

    //以只读的方式打开文件
    int fd = open(m_real_file, O_RDONLY);
    //创建内存映射
    m_file_address = (char *)mmap(0, m_file_stat.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
    close(fd);
    return FILE_REQUEST;
}
template <typename T>
typename http_conn<T>::LINE_STATUS http_conn<T>::parse_line() {
    //解析一行，\r\n
    char temp;

    for(;m_checked_idx < m_read_idx;++m_checked_idx) {
        temp = m_read_buf[m_checked_idx];
        if(temp == '\r') {
            if((m_checked_idx + 1) == m_read_idx) {
                return LINE_OPEN;
            }
            else if(m_read_buf[m_checked_idx + 1] == '\n') {
                m_read_buf[m_checked_idx++] = '\0';
                m_read_buf[m_checked_idx++] = '\0';
                return LINE_OK;
            }
            return LINE_BAD;
        }
        else if(temp == '\n') {
            if((m_checked_idx > 1) && (m_read_buf[m_checked_idx - 1] == '\r')) {
                m_read_buf[m_checked_idx - 1] = '\0';
                m_read_buf[m_checked_idx++] = '\0';
                return LINE_OK;
            }
            return LINE_BAD;
        }
    }
    return LINE_OPEN;
}


template <typename T>
bool http_conn<T>::process_write(http_conn<T>::HTTP_CODE ret) {
    cout << "process_write..." << endl;
    cout << "ret: " << ret << endl;
    switch(ret) {
        case INTERNAL_ERROR:
            add_status_line(500, error_500_title);
            add_headers(strlen(error_500_form));
            if(!add_content(error_500_form)) {
                return false;
            }
            break;
        case BAD_REQUEST:
            add_status_line(400, error_400_title);
            add_headers(strlen(error_400_form));
            if(!add_content(error_400_form)) {
                return false;
            }
            break;
        case NO_RESOURCE:
            add_status_line(404, error_404_title);
            add_headers(strlen(error_404_form));
            if(!add_content(error_404_form)) {
                return false;
            }
            break;
        case FORBIDDEN_REQUEST:
            add_status_line(403, error_403_title);
            add_headers(strlen(error_403_form));
            if(!add_content(error_403_form)) {
                return false;
            }
            break;
        case FILE_REQUEST:
            add_status_line(200, ok_200_title);
            add_headers(m_file_stat.st_size);
            m_iv[0].iov_base = m_write_buf;
            m_iv[0].iov_len = m_write_idx;
            m_iv[1].iov_base = m_file_address;
            m_iv[1].iov_len = m_file_stat.st_size;
            m_iv_count = 2;

            bytes_to_send = m_write_idx + m_file_stat.st_size;

            return true;
        default:
            return false;
    }
    m_iv[0].iov_base = m_write_buf;
    m_iv[0].iov_len = m_write_idx;
    m_iv_count = 1;
    bytes_to_send = m_write_idx;
    return true;
}

template <typename T>
bool http_conn<T>::add_status_line(int status, const char *title) {
    return add_response("%s %d %s\r\n", "HTTP/1.1", status, title);
}
template <typename T>
void http_conn<T>::add_headers(int content_len) {
    add_content_length(content_len);
    add_content_type();
    add_linger();
    add_blank_line();
}
template <typename T>
bool http_conn<T>::add_content(const char *content) {
    return add_response("%s", content);
}
template <typename T>
bool http_conn<T>::add_response(const char* format, ...) {
    if(m_write_idx >= WRITE_BUFFER_SIZE) {
        return false;
    }
    va_list arg_list;
    va_start(arg_list, format);
    int len = vsnprintf(m_write_buf + m_write_idx, WRITE_BUFFER_SIZE - 1 - m_write_idx, format, arg_list);
    if(len >= (WRITE_BUFFER_SIZE - 1 - m_write_idx)) {
        return false;
    }
    m_write_idx += len;
    va_end(arg_list);
    return true;
}
template <typename T>
bool http_conn<T>::add_content_length(int content_len) {
    return add_response("Content-Length: %d\r\n", content_len);
}
template <typename T>
bool http_conn<T>::add_content_type() {
    return add_response("Content-Type: %s\r\n", "text/html");
}
template <typename T>
bool http_conn<T>::add_linger() {
    return add_response("Connection: %s\r\n", (m_linger == true) ? "keep-alive" : "close");
}
template <typename T>
bool http_conn<T>::add_blank_line() {
    return add_response("%s", "\r\n");
}

