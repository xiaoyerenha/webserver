#ifndef SEM_H
#define SEM_H
//#pragma once

#include<semaphore.h>
#include<exception>

//信号量类
class sem
{
public:
    sem();
    ~sem();
    sem(int num);
    bool wait();
    bool post();

private:
    sem_t m_sem;
};

sem::sem()
{
    if(sem_init(&m_sem, 0, 0) != 0)
    {
        throw std::exception();
    }
}

sem::~sem()
{
    sem_destroy(&m_sem);
}

sem::sem(int num)
{
    if(sem_init(&m_sem, 0, num) != 0)
    {
        throw std::exception();
    }
}

bool sem::wait()
{
    return sem_wait(&m_sem) == 0;
}

bool sem::post()
{
    return sem_post(&m_sem) == 0;
}

#endif