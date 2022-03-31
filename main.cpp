#include "config.h"
#include <string>
using namespace std;
int main(int argc, char *argv[]){
    // 数据库信息
    string user = "root";
    string passwd = "1";
    string databasename = "testdb";

    //命令行的解析
    Config config;
    config.parse_arg(argc, argv);

    WebServer server;

    // 初始化
    server.init(config.PORT, user, passwd, databasename, config.LOGWrite,
                config.OPT_LINGER, config.TRIGMode, config.sql_num, config.thread_num,
                config.close_log, config.actor_model);

    // 日志
    server.log_write();

    // 数据库
    server.sql_pool();

    // 线程池
    server.thread_pool();

    // 触发模式
    server.trig_mode();

    // 监听
    server.eventListen();

    // 
    server.eventLoop();

    return 0;
}