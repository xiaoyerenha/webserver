#include "./include/ThreadPool.h"
#include "ThreadPool.cpp" //模板类声明和定义分开写要包含源文件
#include "./include/http_conn.h"
#include "http_conn.cpp"
#include<unistd.h>
#include<arpa/inet.h>
#include<iostream>

using namespace std;
/*
void taskFunc(void* arg) {
    int num = *(int*)arg;
    cout << "thread " << to_string(pthread_self()) << " is working, number = " << to_string(num) << endl;
    sleep(1);
}
*/

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

    while (true)
    {
        usr_addr_sz = sizeof(usr_addr);
        int usr_sock = accept(_servSock, (struct sockaddr*)&usr_addr, &usr_addr_sz);
        if(usr_sock == -1) {
            continue;
        }

        //users[usr_sock].init(usr_sock, usr_addr);
        pool->addTask(http_conn<int>(usr_sock, usr_addr));

    }
    
}

int main(int argc, char* argv[]) {
    /*//测试线程池
    //创建线程池
    ThreadPool<int> pool(3, 10); //线程数[3,10]
    for(int i=0;i<100;++i) {
        int* num = new int(i+100);
        pool.addTask(Task<int>(taskFunc, num));
    }

    sleep(20);
    */

    if(argc != 2) {
        cout << "Usage : " << argv[0] << " <port>" << endl;
        exit(-1);
    }

    int serv_sock = create_sock(atoi(argv[1]));

    wait_connect(serv_sock);

    return 0;
}