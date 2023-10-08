# webserver
基于Linux在应用层实现简单的HTTP服务器功能：  
  1、同步I/O模拟Proactor模式支持客户端静态资源访问；  
  2、逻辑单元内部采用有限状态机的高效编程方法；  
  3、使用epoll的EPOLLONESHPOT事件实现一个连接在任一时刻只被一个线程处理。  


# 编译运行
g++ *.cpp -I ../inc -lpthread   
./a.out 10000

# 压力测试
/webbench-1.5$ ./webbench -c 1000 -t 5 http://192.168.193.128:10000/index.html
