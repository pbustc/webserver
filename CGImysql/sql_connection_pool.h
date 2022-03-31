#ifndef _CONNECTION_POOL_
#define _CONNECTION_POOL_

#include <stdio.h>
#include <list>
#include <mysql/mysql.h>
#include <error.h>
#include <string.h>
#include <iostream>
#include <string>
#include "../lock/locker.h"
#include "../log/log.h"

using namespace std;

class connection_pool{
public:
    MYSQL *Getconnection();                 // 获取数据库连接

    bool ReleaseConnection(MYSQL *conn);    // 释放一个数据库连接

    int GetFreeConn();                      // 获取连接

    void DestoryPool();                     // 销毁所有连接

    // 单例模式
    static connection_pool *GetInstance();

    void init(string url, string user, string passwd, string DataBaseName, int Port, int Maxconn, int close_log);

private:
    connection_pool();
    ~connection_pool();

    int m_MaxConn;  // 最大连接数
    int m_CurConn;  // 当前已使用的连接数
    int m_FreeConn; // 当前空闲的连接数

    locker lock;
    list<MYSQL *> connlist; // 连接池
    sem reserve;

public:
    string m_url;
    string m_Port;
    string m_User;
    string m_Password;
    string m_DatabaseName;
    int m_close_log;
};

class connectionRAII{
public:
    connectionRAII(MYSQL **conn, connection_pool *connPool);
    ~connectionRAII();

private:
    MYSQL *conRAII;
    connection_pool *poolRAII;
};

#endif