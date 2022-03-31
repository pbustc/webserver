#include <mysql/mysql.h>
#include <stdio.h>
#include <string>
#include <string.h>
#include <stdlib.h>
#include <list>
#include <pthread.h>
#include <iostream>
#include "sql_connection_pool.h"

using namespace std;

connection_pool::connection_pool(){
    m_CurConn = 0;
    m_FreeConn = 0;
}

connection_pool *connection_pool::GetInstance(){
    static connection_pool connPool;
    return &connPool;
}

// 构造初始化
void connection_pool::init(string url, string User, string PassWord, string DBName, int Port, int MaxConn, int close_log){
    m_url = url;
    m_Port = Port;
    m_User = User;
    m_Password = PassWord;
    m_DatabaseName = DBName;
    m_close_log = close_log;

    // 创建MaxConn条数据库连接并以list保存
    for(int i = 0;i < MaxConn;i++){
        MYSQL *con = NULL;
        MYSQL *ret = NULL;

        // con为NULL,分配、初始化并返回新对象的地址
        ret = mysql_init(con);
        if(ret == NULL){
            LOG_ERROR("MYSQL Error: mysql_init() returns NULL");
            exit(1);
        }
        else 
            con = ret;
        
        // 与运行在主机上的MySQL数据库引擎建立连接
        ret = mysql_real_connect(con, url.c_str(), User.c_str(), PassWord.c_str(),DBName.c_str(), Port, NULL, 0);
        if(ret == NULL){
            string err_info(mysql_error(con));
            err_info = (string("MySQL Error[errno=") + std::to_string(mysql_errno(con)) + string("]:") + err_info);
            LOG_ERROR(err_info.c_str());
            exit(1);
        }
        else
            con = ret;
        connlist.push_back(con);
        ++m_FreeConn;
    }

    reserve = sem(m_FreeConn);

    m_MaxConn = m_FreeConn;
}


/* 当有请求时从数据库线程池中返回一个可用连接 */
MYSQL *connection_pool::Getconnection(){
    MYSQL *con = NULL;

    if(0 == connlist.size())
        return NULL;
    reserve.wait();

    lock.lock();
    con = connlist.front();
    connlist.pop_front();

    --m_FreeConn;
    ++m_CurConn;

    lock.unlock();
    return con;
}

/* 将数据库连接释放,放回数据库连接池 */
bool connection_pool::ReleaseConnection(MYSQL *con){
    if(NULL == con)
        return false;
    
    lock.lock();

    connlist.push_back(con);
    ++m_FreeConn;
    --m_CurConn;

    lock.unlock();

    reserve.post();

    return true;
}

/* 销毁数据库连接池 */
void connection_pool::DestoryPool(){
    lock.lock();
    if(connlist.size() > 0){
        for(auto it = connlist.begin();it != connlist.end();++it){
            MYSQL *con = *it;
            mysql_close(con);
        }
        m_CurConn = 0;
        m_FreeConn = 0;
        connlist.clear();
    }
    lock.unlock();
}

/* 当前空闲的连接数 */
int connection_pool::GetFreeConn(){
    return m_FreeConn;
}

/* 析构时销毁所有连接 */
connection_pool::~connection_pool(){
    DestoryPool();
}

/* */
connectionRAII::connectionRAII(MYSQL **SQL, connection_pool *connPool){
    // 二重指针传指针的地址
    *SQL = connPool -> Getconnection();

    conRAII = *SQL;
    poolRAII = connPool;
}

connectionRAII::~connectionRAII(){
    poolRAII->ReleaseConnection(conRAII);
}