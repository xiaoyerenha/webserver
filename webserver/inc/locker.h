#ifndef LOCKER_H
#define LOCKER_H
//#pragma once

#include<pthread.h>
#include<exception>
#include"cond.h"
#include"sem.h"

//线程同步机制封装类

//互斥锁类
class locker
{
public:
    locker();
    ~locker();
    bool lock();
    bool unlock();
    pthread_mutex_t *get();

private:
    pthread_mutex_t m_mutex;
};

locker::locker()
{
    if(pthread_mutex_init(&m_mutex, NULL) != 0) {
        throw std::exception();
    }
}

locker::~locker()
{
    pthread_mutex_destroy(&m_mutex);
}

bool locker::lock()
{
    return pthread_mutex_lock(&m_mutex) == 0;
}

bool locker::unlock()
{
    return pthread_mutex_unlock(&m_mutex) == 0;
}

pthread_mutex_t *locker::get()
{
    return &m_mutex;
}


#endif