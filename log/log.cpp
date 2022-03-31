#include <string.h>
#include <time.h>
#include <sys/time.h>
#include <stdarg.h>
#include "log.h"
#include <pthread.h>
using namespace std;

Log::Log(){
    m_count = 0;
    m_is_async = false;
}

Log::~Log(){
    // 避免内存泄漏
    if(m_fp != NULL){
        fclose(m_fp);
    }
}

// 异步日志系统中需要设置阻塞队列的长度
bool Log::init(const char *file_name, int close_log, int log_buf_size, int split_lines, int max_queue_size){
    // 日志系统通过阻塞队列设置的大小判断工作方式
    // 异步 --- max_queue_size > 0
    // 同步 --- max_queue_size = 0
    if(max_queue_size > 0){
        m_is_async = true;
        // 仅异步模式下需要阻塞队列,需要创建新的线程用于将阻塞队列中的日志写入文件中
        m_log_queue = new block_queue<string>(max_queue_size);
        pthread_t tid;
        // flush_log_thread为回调函数,用于创建线程异步方式写日志
        pthread_create(&tid, NULL, flush_log_thread, NULL);
    }    

    m_close_log = close_log;    // 是否关闭日志
    m_log_buf_size = log_buf_size; // 日志缓冲区大小
    m_buf = new char[m_log_buf_size]; // 创建缓冲区
    memset(m_buf, '\0', m_log_buf_size);
    m_split_lines = split_lines;

    time_t t = time(NULL);      // 获取系统时间
    struct tm *sys_tm = localtime(&t);  // 使用t的值来填充sys_tm
    struct tm my_tm = *sys_tm;

    // strchr --- p指向字符串中搜索最后一次出现字符c位置
    // main传入的file_name: "./ServerLog"
    const char *p = strrchr(file_name, '/');
    char log_full_name[256] = {0};

    if(p == NULL){
        snprintf(log_full_name, 255, "%d_%02d_%02d_%s",my_tm.tm_year + 1900, my_tm.tm_mon + 1, my_tm.tm_mday, file_name);
    }
    else {
        strcpy(log_name, p + 1);
        strncpy(dir_name, file_name, p - file_name + 1);
        snprintf(log_full_name, 255, "%s%d_%02d_%02d_%s", dir_name, my_tm.tm_year + 1900, my_tm.tm_mon + 1, my_tm.tm_mday, file_name);
    }

    m_today = my_tm.tm_mday;

    m_fp = fopen(log_full_name, "a");

    if(m_fp == NULL){
        return false;
    }

    return true;
}

void Log::write_log(int level, const char *format, ...){
    struct timeval now = {0, 0};
    gettimeofday(&now, NULL);
    time_t t = now.tv_sec;
    struct tm *sys_tm = localtime(&t);
    struct tm my_tm = *sys_tm;

    char s[16] = {0};
    switch(level){
        case 0:
            strcpy(s, "[debug]:");
            break;
        case  1:
            strcpy(s, "[info]:");
            break;
        case 2:
            strcpy(s, "[warn]:");
            break;
        case 3:
            strcpy(s, "[error]:");
            break;
        default:
            strcpy(s, "[info]:");
            break;
    }

    // 写入一个log,对m_count++,m_split_lines最大行数进行修改
    m_mutex.lock();
    m_count++;

    if(m_today != my_tm.tm_mday || m_count % m_split_lines == 0){ 
        char new_log[256] = {0};
        fflush(m_fp);
        fclose(m_fp);
        char tail[16] = {0};
        
        snprintf(tail, 16, "%d_%02d_%02d_", my_tm.tm_year + 1900, my_tm.tm_mon + 1, my_tm.tm_mday);
        if(m_today != my_tm.tm_mday){
            snprintf(new_log, 255, "%s%s%s",dir_name, tail, log_name);
            m_today = my_tm.tm_mday;
            m_count = 0;
        }
        else{
            snprintf(new_log, 255, "%s%s%s.%lld", dir_name, tail, log_name, m_count / m_split_lines);
        }
        m_fp = fopen(new_log, "a");
    }

    m_mutex.unlock();

    // 可变参数中的宏定义
    // va_list -- 指向可变参数的第一个位置,format之后的位置
    va_list valst;
    va_start(valst, format);

    string log_str;
    m_mutex.lock();

    // 写入具体的时间内容格式
    // snprintf(char *str, size_t size, const char *format, ...)  
    // 将后面可变参数按照format的格式转换为字符串并存入str中
    // 若该字符串长度 >= size,对其进行截断并在末尾加'/0'，返回size
    // 若该字符串长度 < size,对其末尾加'/0'并返回字符串长度
    int n = snprintf(m_buf, 48, "%d-%02d-%02d %02d:%02d:%02d.%06ld %s",
                    my_tm.tm_year + 1900, my_tm.tm_mon + 1, my_tm.tm_mday,
                    my_tm.tm_hour, my_tm.tm_min, my_tm.tm_sec, now.tv_usec, s);
                    
    // 作用类似于snprintf()不同的是后面的参数为可变参数
    // 将所有需要打印的内容放到m_buf中
    int m = vsnprintf(m_buf + n, m_log_buf_size - 1, format, valst);
    m_buf[n + m] = '\n';
    m_buf[n + m + 1] = '\0';

    // c_str --> string
    // string的拷贝赋值函数
    log_str = m_buf;

    m_mutex.unlock();
    
    // 异步模式且阻塞队列未满
    if(m_is_async && !m_log_queue->full()){
        m_log_queue->push(log_str);
    }
    else{
        m_mutex.lock();
        // 将log_str写入到m_fp所指向的文件中
        fputs(log_str.c_str(), m_fp);
        m_mutex.unlock();
    }

    // 清理为va_list保留的内存空间
    va_end(valst);
}

void Log::flush(void){
    m_mutex.lock();
    // 刷新流stream的输出缓冲区
    fflush(m_fp);
    m_mutex.unlock();
}