#ifndef _BLOCK_QUEUE_H_
#define _BLOCK_QUEUE_H_

#include <iostream>
#include <stdlib.h>
#include  <pthread.h>
#include  <sys/time.h>
#include "../lock/locker.h"

using namespace std;

template <class T>
class block_queue{
public:
    block_queue(int max_size = 1000){
        if(max_size <= 0){
            exit(-1);
        }

        m_max_size = max_size;
        m_array = new T[max_size];
        m_size = 0;
        m_front = -1;
        m_back = -1;
    }

    ~block_queue(){
        m_mutex.lock();
        if(m_array != NULL){
            delete [] m_array;
        }
        m_mutex.unlock();
    }

    // 清空缓冲区
    void clear(){
        m_mutex.lock();
        m_size = 0;
        m_front = -1;
        m_back = -1;
        m_mutex.unlock();
    }

    // 判断队列是否满
    bool full(){
        m_mutex.lock();
        if(m_size >= m_max_size){
            m_mutex.unlock();
            return true;
        }
        m_mutex.unlock();
        return false;
    }

    // 判断队列是否为空
    bool empty(){
        m_mutex.lock();
        if(m_front == m_back){
            m_mutex.unlock();
            return true;
        }
        m_mutex.unlock();
        return false;
    }
    
    // 获取队首元素
    bool front(T &value){
        m_mutex.lock();
        if(m_size == 0){
            m_mutex.unlock();
            return false;
        }
        value = m_array[m_front];
        m_mutex.unlock();
        return true;
    }

    // 获取队尾元素
    bool back(T &value){
        m_mutex.lock();
        if(m_size == 0){
            m_mutex.unlock();
            return false;
        }
        value = m_array[m_front];
        m_mutex.unlock();
        return true;
    }

    // 获取当前元素个数
    int size(){
        // int tmp = 0;

        // m_mutex.lock();
        // tmp = m_size;

        // m_mutex.unlock();
        // return tmp;
        return m_size;
    }

    // 获取容量大小
    int max_size(){
        // int tmp = 0;

        // m_mutex.lock();
        // tmp = m_max_size;

        // m_mutex.unlock();
        // return tmp;
        return m_max_size;
    }

    // 向队列中添加元素
    bool push(const T& item){
        m_mutex.lock();
        if(m_size >= m_max_size){
            m_cond.broadcast();
            m_mutex.unlock();
            return false;
        }

        m_back = (m_back + 1) % m_max_size;
        m_array[m_back] = item;
        m_size ++;

        m_cond.broadcast();
        m_mutex.unlock();
        return true;
    }

    // 从队列中获取元素
    bool pop(T &item){
        m_mutex.lock();
        while(m_size <= 0){
            if(!m_cond.wait(m_mutex.get())){
                // wait操作失败返回错误码
                m_mutex.unlock();
                return false;
            }
        }

        m_front = (m_front + 1) % m_max_size;
        item = m_array[m_front];
        m_size --;
        m_mutex.unlock();
        return true;
    }

    // 增加超时处理的获取元素方法
    // 对wait增加时间参数
    bool pop(T &item, int ms_timeout){
        // pthread_cond_timedwait需要传入timespec格式的时间
        // 通过gettimeofday获取从格林威治时间开始的timeval
        struct timespec t = {0, 0};
        struct timeval now = {0, 0};
        gettimeofday(&now,  NULL);
        m_mutex.lock();
        if(m_size <= 0){
            t.tv_sec = now.tv_sec + ms_timeout / 1000;
            // 此处的usec感觉不太对,自己进行了一些修改
            t.tv_usec = now.tv_usec + (ms_timeout % 1000) * 1000;
            if(!m_cond.timewait(m_mutex.get(), t)){
                m_mutex.unlock();
                return false;
            }
        }

        // pthread_cond_timedwait可能由于时间到而返回
        if(m_size <= 0){
            m_mutex.unlock();
            return false;
        }

        m_front = (m_front + 1) % m_max_size;
        item = m_array[m_front];
        m_size --;
        m_mutex.unlock();
        return true;
    }
private:
    locker m_mutex;
    cond m_cond;

    T *m_array;
    int m_size;
    int m_max_size;
    int m_front;
    int m_back;
};
#endif