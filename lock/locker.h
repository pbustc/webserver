#ifndef LOCKER_H
#define LOCKER_H

#include <exception>
#include <pthread.h>
#include <semaphore.h>

// 信号量
class sem{
public:
    // initialize the semaphore
    // return 0 on success.
    sem(){
        if(sem_init(&m_sem, 0, 0) != 0){
            throw std::exception();
        }
    }
    sem(int num){
        if(sem_init(&m_sem, 0, num) != 0){
            throw std::exception();
        }
    }
    ~sem(){
        sem_destroy(&m_sem);
    }

    // PV on sem
    // 通过sem_wait与sem_post的返回值判定是否成功调用
    bool wait(){
        return sem_wait(&m_sem) == 0;
    }
    bool post(){
        return sem_post(&m_sem) == 0;
    }
private:
    sem_t m_sem;
};

// 互斥锁
class locker{
public:
    locker(){
        if(pthread_mutex_init(&m_mutex, NULL) != 0){
            throw std::exception();
        }
    }    
    ~locker(){
        pthread_mutex_destroy(&m_mutex);
    }

    // 原子操作对其加锁或解锁
    // bool判定是否成功执行
    bool lock(){
        return pthread_mutex_lock(&m_mutex) == 0;
    }
    bool unlock(){
        return pthread_mutex_unlock(&m_mutex) == 0;
    }

    pthread_mutex_t *get(){
        return &m_mutex;
    }
private:
    pthread_mutex_t m_mutex;
};

// 条件变量
class cond{
public:
    cond(){
        if(pthread_cond_init(&m_cond, NULL) != 0){
            throw std::exception();
        }
    }
    ~cond(){
        if(pthread_cond_destroy(&m_cond)){
            throw std::exception();
        }
    }

    // 
    bool wait(pthread_mutex_t * m_mutex){
        int ret = 0;
        pthread_mutex_lock(m_mutex);
        ret = pthread_cond_wait(&m_cond, m_mutex);
        return ret == 0;
    }
    bool timewait(pthread_mutex_t * m_mutex, struct timespec t){
        int ret = 0;
        pthread_mutex_lock(m_mutex);
        ret = pthread_cond_timedwait(&m_cond, m_mutex, &t);
        pthread_mutex_unlock(m_mutex);
        return ret == 0;
    }

    bool signal(){
        return pthread_cond_signal(&m_cond);
    }
    bool broadcast(){
        return pthread_cond_broadcast(&m_cond);
    }

private:
    pthread_cond_t m_cond;
};
#endif
