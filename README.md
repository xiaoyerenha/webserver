# webserver
- 多线程
- 线程池
- 线程池+epoll

基于Linux在应用层实现轻量级HTTP服务器功能：
1. 同步I/O模拟Proactor模式支持客户端静态资源访问；
2. 逻辑单元内部采用有限状态机的高效编程方法；
3. 使用epoll的EPOLLONESHOT事件实现一个连接在任一时刻只被一个线程处理；
4. 面向对象、模板的应用。

# 编译运行
    g++ main.cpp -o httpTest
    ./httpTest 8888

# 压力测试
    /pressure_test/webbench-1.5$ ./webbench -c 1000 -t 5 http://192.168.193.128:8888/index.html
