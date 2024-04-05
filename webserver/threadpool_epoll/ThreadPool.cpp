#include "./include/ThreadPool.h"
#include <iostream>
#include <string.h>
#include <string>
#include <unistd.h>

using namespace std;

template <typename T>
ThreadPool<T>::ThreadPool(int min, int max) {
    //实例化任务队列
    do {
        taskQ = new TaskQueue<T>;
        if(taskQ == nullptr) {
            cout << "new taskQ fail..." << endl;
            break;
        }

        //根据线程的最大上限给线程数组分配内存
        threadIDs = new pthread_t[max];
        if(threadIDs == nullptr) {
            cout << "new thread_t[] fail..." << endl;
            break;
        }
        //初始化
        memset(threadIDs, 0, sizeof(pthread_t)*max);
        minNum = min;
        maxNum = max;
        busyNum = 0;
        liveNum = min;
        exitNum = 0;
        
        //初始化互斥锁，条件变量
        if(pthread_mutex_init(&mutexPool, NULL) !=0 || 
           pthread_cond_init(&notEmpty, NULL) != 0) {
            cout << "init mutex or condition fail..." << endl;
            break;
        }

        shutdown = false;

        //创建线程
        pthread_create(&managerID, NULL, manager, this);
        for(int i=0;i<min;++i) {
            pthread_create(&threadIDs[i], NULL, worker, this);
        }
        return;
    }while(0);

    //释放资源
    if(threadIDs) delete[] threadIDs;
    if(taskQ) delete taskQ;
}

template <typename T>
ThreadPool<T>::~ThreadPool() {
    shutdown = true;
    //销毁管理者线程
    pthread_join(managerID, NULL);
    //唤醒所有消费者线程
    for(int i=0;i<liveNum;++i) {
        pthread_cond_signal(&notEmpty);
    }

    //释放内存
    if(taskQ) 
        delete taskQ;
    if(threadIDs)
        delete[] threadIDs;

    pthread_mutex_destroy(&mutexPool);
    pthread_cond_destroy(&notEmpty);
}
	
template <typename T>
void ThreadPool<T>::addTask(http_conn<T>* task) {
    if(shutdown) {
        return;
    }
    //添加任务
    //addTask()本身维护锁，不需要再加锁
    taskQ->addTask(*task);
    pthread_cond_signal(&notEmpty);
}

template <typename T>
int ThreadPool<T>::getBusyNum() {
    pthread_mutex_lock(&mutexPool);
    int busyNum = this->busyNum;
    pthread_mutex_unlock(&mutexPool);
    return busyNum;
}

template <typename T>
int ThreadPool<T>::getAliveNum() {
    pthread_mutex_lock(&mutexPool);
    int liveNum = this->liveNum;
    pthread_mutex_unlock(&mutexPool);
    return liveNum;
}

template <typename T>
void* ThreadPool<T>::worker(void* arg) {
    ThreadPool* pool = static_cast<ThreadPool*>(arg);
    
    while (true)
    {
        pthread_mutex_lock(&pool->mutexPool);
        //当前任务队列是否为空
        while (pool->taskQ->taskNumber() == 0 && !pool->shutdown)
        {
            //阻塞工作线程
            pthread_cond_wait(&pool->notEmpty, &pool->mutexPool);

            //解除阻塞后，判断是否销毁线程
            if(pool->exitNum > 0) {
                pool->exitNum--;
                if(pool->liveNum > pool->minNum) {
                    pool->liveNum--;
                    pthread_mutex_unlock(&pool->mutexPool);
                    pool->threadExit();
                }
            }
        }
        
        //判断线程池是否被关闭
        if(pool->shutdown) {
            pthread_mutex_unlock(&pool->mutexPool);
            pool->threadExit();
        }

        //从任务队列中取出一个任务
        http_conn<int> task = pool->taskQ->takeTask();
        //工作的线程+1
        pool->busyNum++;

        //解锁
        pthread_mutex_unlock(&pool->mutexPool);
        //执行任务
        cout << "thread " << to_string(pthread_self()) << " start working..." << endl;
        //task.function(task.arg);
        //delete task.arg;
        //task.arg = nullptr;
        task.process();
        task.close_conn();
        cout << "thread " << to_string(pthread_self()) << " end working..." << endl;
        pthread_mutex_lock(&pool->mutexPool);
        pool->busyNum--;
        pthread_mutex_unlock(&pool->mutexPool);
    }
    return nullptr;
}

template <typename T>
void* ThreadPool<T>::manager(void* arg) {
    ThreadPool* pool = static_cast<ThreadPool*>(arg);
    while(!pool->shutdown) {
        //每隔5s检测一次
        sleep(5);
        
        //取出线程池中的任务数和线程数量
        //取出工作的线程池数量
        pthread_mutex_lock(&pool->mutexPool);
        int queueSize = pool->taskQ->taskNumber();
        int liveNum = pool->liveNum;
        int busyNum = pool->busyNum;
        pthread_mutex_unlock(&pool->mutexPool);

        //创建线程
        //当前任务个数 > 存活的线程数 && 存活的线程数 < 最大线程个数
        if(queueSize > liveNum && liveNum < pool->maxNum) {
            //加锁
            pthread_mutex_lock(&pool->mutexPool);
            int num = 0;
            for(int i=0;i<pool->maxNum && num < NUMBER 
                && pool->liveNum < pool->maxNum;++i) {
                if(pool->threadIDs[i] == 0) {
                    pthread_create(&pool->threadIDs[i], NULL, worker, pool);
                    num++;
                    pool->liveNum++;
                }
            }
            pthread_mutex_unlock(&pool->mutexPool);
        }

        //销毁多余线程
        //忙线程*2 < 存活的线程数 && 存活的线程数 > 最小线程数
        if(busyNum * 2 < liveNum && liveNum > pool->minNum) {
            pthread_mutex_lock(&pool->mutexPool);
            pool->exitNum = NUMBER;
            pthread_mutex_unlock(&pool->mutexPool);
            for(int i=0;i<NUMBER;++i) {
                pthread_cond_signal(&pool->notEmpty);
            }
        }
    }
    return nullptr;
}

template <typename T>
void ThreadPool<T>::threadExit() {
    pthread_t tid = pthread_self();
    for(int i=0;i<maxNum;++i) {
        if(threadIDs[i] == tid) {
            threadIDs[i] = 0;
            cout << "threadExit() called, " << to_string(tid) << " exiting..." << endl;
            break;
        }
    }
    pthread_exit(NULL); // 标准C函数，用NULL
}