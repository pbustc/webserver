#ifndef _LOG_H_
#define _LOG_H_

#include <stdio.h>
#include <iostream>
#include <string>
#include <stdarg.h>
#include <pthread.h>

#include "block_queue.h"

class Log
{
public:
    // C++11以后,编译器可保证静态局部变量的线程安全
    // 单例模式
    // 1.懒汉模式 --- 不用的时候不去初始化，所以在第一次被使用时才进行初始化
    // 2.饿汉模式 --- 在程序运行时立即初始化
    
    static Log *get_instance(){
        static Log instance;
        return &instance;
    }

    static void *flush_log_thread(void *args){
        Log::get_instance()->async_write_log();
    }

    // 
    bool init(const char *file_name, int close_log, int log_buf_size = 8192, int split_lines = 5000000,int max_queue_size = 0);

    void write_log(int level, const char *format, ...);

    void flush(void);

private:
    // 单例模式
    Log();
    virtual ~Log();
    void *async_write_log(){
        string single_log;
        // 从阻塞队列中取出一个日志string,写入文件
        // pop封装在block_queue中,无数据时返回false
        while(m_log_queue->pop(single_log)){
            m_mutex.lock();
            fputs(single_log.c_str(), m_fp);
            m_mutex.unlock();
        }
    }

private:
    char dir_name[128]; // 路径名
    char log_name[128]; // log文件名
    int m_split_lines;  // 日志最大行数
    int m_log_buf_size; // 日志缓冲区大小
    long long m_count;  // 日志行数记录
    int m_today;        // 记录当前日期
    FILE *m_fp;         // 打开log的文件指针
    char *m_buf;        // 日志缓冲字符串
    block_queue<string> *m_log_queue;   // 阻塞队列
    bool m_is_async;                    // 同步标志位
    locker m_mutex;                     // 互斥锁
    int m_close_log;                    // 关闭日志

};

#define LOG_DEBUG(format, ...) if(0 == m_close_log){\
    Log::get_instance()->write_log(0, format, ##__VA_ARGS__);\
    Log::get_instance()->flush();}

#define LOG_INFO(format, ...) if(0 == m_close_log){\
    Log::get_instance()->write_log(1, format, ##__VA_ARGS__);\
    Log::get_instance()->flush();}

#define LOG_WARN(format, ...) if(0 == m_close_log){\
    Log::get_instance()->write_log(2, format, ##__VA_ARGS__);\
    Log::get_instance()->flush();}

#define LOG_ERROR(format, ...) if(0 == m_close_log){\
    Log::get_instance()->write_log(3, format, ##__VA_ARGS__);\
    Log::get_instance()->flush();}    


#endif
