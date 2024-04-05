#include "./include/ThreadPool.h"
#include "ThreadPool.cpp" //模板类声明和定义分开写要包含源文件
#include "./include/http_conn.h"
#include "http_conn.cpp"
#include<unistd.h>
#include<arpa/inet.h>
#include<sys/epoll.h>//
#include<iostream>

#define MAX_EVENT_NUMBER 10000 //监听的最大事件数
#define MAX_FD 65535 //最大的文件描述符数

using namespace std;

http_conn<int> *users = new http_conn<int>[MAX_FD];

void handleFunc(void* arg) {
    cout << "handleFunc" << endl;
    cout << *(int*)arg << endl;
    cout << "arg after" << endl;
    
}

void error_handling(string msg) {
    cout << msg << endl;
}

int create_sock(int _port) {
    int serv_sock = socket(PF_INET, SOCK_STREAM, 0);
    
    //设置端口复用
    int reuse = 1;
    setsockopt(serv_sock, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));

    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(_port);

    if(bind(serv_sock, (struct sockaddr*)&addr, sizeof(addr)) == -1) {
        error_handling("bind() error");
        exit(-1);
    }

    if(listen(serv_sock, 5) == -1) {
        error_handling("listen() error");
        exit(-1);
    }
    return serv_sock;
}

void wait_connect(int _servSock) {
    struct sockaddr_in usr_addr;
    socklen_t usr_addr_sz;
    //创建线程池
    ThreadPool<int> *pool = nullptr;
    try {
        pool = new ThreadPool<int>(3, 10);
    } catch(...) {
        exit(-1);
    }

    //创建epoll对象，事件数组，添加
    epoll_event events[MAX_EVENT_NUMBER];
    int epollfd = epoll_create(5);
    addfd(epollfd, _servSock, false);
    http_conn<int>::m_epollfd = epollfd;

    while (true)
    {
        int num = epoll_wait(epollfd, events, MAX_EVENT_NUMBER, -1);
        if((num < 0) && (errno != EINTR)) {
            cout << "epoll failure" << endl;
            break;
        }

        //遍历事件数组
        for(int i=0;i<num;++i) {
            int sockfd = events[i].data.fd;
            if(sockfd == _servSock) {
                usr_addr_sz = sizeof(usr_addr);
                int usr_sock = accept(_servSock, (struct sockaddr*)&usr_addr, &usr_addr_sz);

                if(usr_sock == -1) {
                    close(usr_sock);
                    continue;
                }

                //新用户，初始化，放入数组
                users[usr_sock].init(usr_sock, usr_addr);
            }
            else if(events[i].events & (EPOLLRDHUP | EPOLLERR)) {
                //用户异常断开事件
                users[sockfd].close_conn();
            }
            else if(events[i].events & EPOLLIN) {
                if(users[sockfd].read()) {
                    //一次性读完所有数据
                    pool->addTask(users + sockfd);
                }
                else users[sockfd].close_conn();
            }
            else if(events[i].events & EPOLLOUT) {
                if(!users[sockfd].write()) {
                    users[sockfd].close_conn();
                }
            }
        }
    }

    close(epollfd);
    close(_servSock);
    delete[] users;
    
}

int main(int argc, char* argv[]) {

    if(argc != 2) {
        cout << "Usage : " << argv[0] << " <port>" << endl;
        exit(-1);
    }

    int serv_sock = create_sock(atoi(argv[1]));

    wait_connect(serv_sock);

    return 0;
}