#include<iostream>
#include<arpa/inet.h>
#include<unistd.h>
#include<cstring>
#define BUF_SIZE 100
#define MAX_CLNT 256

using namespace std;

int create_sock(int _port);
void wait_connect(int _servSock);
void * handle_clnt(void * arg);
void send_msg(char * msg, int len);
void error_handling(string msg);

int clnt_cnt = 0;
int clnt_socks[MAX_CLNT];
pthread_mutex_t mutx;

int main(int argc, char* argv[]) {

    int serv_sock, clnt_sock;
    if(argc != 2) {
        cout << "Usage : " << argv[0] << " <port>" << endl;
        exit(1);
    }

    pthread_mutex_init(&mutx, nullptr);

    serv_sock = create_sock(atoi(argv[1]));

    wait_connect(serv_sock);

    close(serv_sock);

    return 0;
}

int create_sock(int _port) {
    int serv_sock = socket(PF_INET, SOCK_STREAM, 0);
    struct sockaddr_in serv_adr;

    memset(&serv_adr, 0, sizeof(serv_adr));
    serv_adr.sin_family = AF_INET;
    serv_adr.sin_addr.s_addr = htonl(INADDR_ANY);
    serv_adr.sin_port = htons(_port);

    if(bind(serv_sock, (struct sockaddr*) &serv_adr, sizeof(serv_adr)) == -1) {
        error_handling("bind() error");
        exit(1);
    }
        
    if(listen(serv_sock, 5) == -1) {
        error_handling("listen() error");
        exit(1);
    }
    cout << "bind and listen success, wait client connect ... " << endl;
    return serv_sock;   
}

void wait_connect(int _servSock) {
    struct sockaddr_in clnt_adr;
    socklen_t clnt_adr_sz;
    pthread_t t_id;

    while(1) {
        clnt_adr_sz = sizeof(clnt_adr);
        int clnt_sock = accept(_servSock, (struct sockaddr*)&clnt_adr, &clnt_adr_sz);
        if(clnt_sock == -1) {
            continue;
        }

        pthread_mutex_lock(&mutx);
        clnt_socks[clnt_cnt++] = clnt_sock;
        pthread_mutex_unlock(&mutx);

        pthread_create(&t_id, nullptr, handle_clnt, (void*)&clnt_sock);
        pthread_detach(t_id);
        cout << "Connected client IP : " << inet_ntoa(clnt_adr.sin_addr) << endl;
    }
}

void * handle_clnt(void * arg) {
    int clnt_sock = *((int*)arg);
    int str_len = 0, i;
    char msg[BUF_SIZE];

    while((str_len=read(clnt_sock, msg, sizeof(msg))) != 0)
        send_msg(msg, str_len);

    pthread_mutex_lock(&mutx);
    for(i=0;i<clnt_cnt;++i) {
        if(clnt_sock == clnt_socks[i]) {
            while(i++<clnt_cnt-1)
                clnt_socks[i] = clnt_socks[i+1];
            break;
        }
    }

    clnt_cnt--;
    pthread_mutex_unlock(&mutx);
    close(clnt_sock);
    return nullptr;
}

void send_msg(char * msg, int len) {
    int i;
    pthread_mutex_lock(&mutx);
    for(i=0;i<clnt_cnt;++i)
        write(clnt_socks[i], msg, len);
    pthread_mutex_unlock(&mutx);
}

void error_handling(string msg) {
    cout << msg << endl;
}