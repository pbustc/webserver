#include "webserver.h"

WebServer::WebServer(){
    users = new http_conn[MAX_FD];

    // root文件夹路径
    char server_path[200];
    getcwd(server_path, 200);
    char root[6] = "/root";
    m_root = (char *)malloc(strlen(server_path) + strlen(root) + 1);
    strcpy(m_root, server_path);
    strcat(m_root, root);

    // 定时器
    users_timer = new client_data[MAX_FD];
}

WebServer::~WebServer(){
    close(m_epollfd);
    close(m_listenfd);
    close(m_pipefd[1]);
    close(m_pipefd[0]);
    delete [] users;
    delete [] users_timer;
    delete m_pool;
}

void WebServer::init(int port, string user, string passWord, string databaseName,int log_write, 
int opt_linger, int trigmode, int sql_num,int thread_num, int close_log, int actor_mode){
    m_port = port;
    m_user = user;
    m_password = passWord;
    m_databaseName = databaseName;
    m_sql_num = sql_num;
    m_thread_num = thread_num;
    m_log_write = log_write;
    m_OPT_LINGER = opt_linger;
    m_TRIGMode = trigmode;
    m_close_log = close_log;
    m_actormodel = actor_mode;
}

void WebServer::trig_mode(){
    // LT + LT
    if(0 == m_TRIGMode){
        m_LISTENTrigmode = 0;
        m_CONNTrigmode = 0;
    }
    // LT + ET
    else if(1 == m_TRIGMode){
        m_LISTENTrigmode = 0;
        m_CONNTrigmode = 1;
    }
    // ET + LT
    else if(2 == m_TRIGMode){
        m_LISTENTrigmode = 1;
        m_CONNTrigmode = 0;
    }
    else if(3 == m_TRIGMode){
        m_LISTENTrigmode = 1;
        m_CONNTrigmode = 1;
    }
}

/* 对日志系统进行初始化 */
void WebServer::log_write(){
    // m_close_log --- 是否关闭日志
    // m_log_write --- 是否以异步模式工作
    if(0 == m_close_log){
        if(1 == m_log_write)
            Log::get_instance()->init("./WebServerLog", m_close_log, 2000, 800000, 800);
        else    
            Log::get_instance()->init("./WebServerLog", m_close_log, 2000, 800000, 0);
    }
}

/* 对数据库线程池的初始化 */
void WebServer::sql_pool(){
    m_connPool = connection_pool::GetInstance();
    m_connPool->init("localhost", m_user, m_password, m_databaseName, 3306, m_sql_num, m_close_log);
    users->initmysql_result(m_connPool);
}

void WebServer::thread_pool(){
    m_pool = new threadpool<http_conn>(m_actormodel, m_connPool, m_thread_num);
}

void WebServer::eventListen(){
    // int socket(int domain, int type, int protocol)
    // 创建一个socket,返回其fd
    // 此处选择PF_INET协议族 -- TCP/IP
    // SOCK_STREAM -- 流服务
    m_listenfd = socket(PF_INET, SOCK_STREAM, 0);
    assert(m_listenfd >= 0);

    // 设置socket文件描述符属性
    // int setsockopt(int sockfd, int level, int option_name, const void *option_value,
    //                    socklen_t option_len)
    // SO_LINGER --- 若有数据待发送,则延迟关闭
    if(0 == m_OPT_LINGER){
        struct linger tmp = {0, 1};
        setsockopt(m_listenfd, SOL_SOCKET, SO_LINGER, &tmp, sizeof(tmp));
    }
    else if(1 == m_OPT_LINGER){
        struct linger tmp = {1, 1};
        setsockopt(m_listenfd, SOL_SOCKET, SO_LINGER, &tmp, sizeof(tmp));
    }
    
    int ret = 0;
    // sockaddr_in -- TCP/IP协议族专用的IP地址
    struct sockaddr_in address;
    bzero(&address, sizeof(address));
    address.sin_family = AF_INET;
    // INADDR_ANY -- 0.0.0.0 -- Address to accept any incoming messages
    // 网络字节序 -- 大端字节序
    // htonl,htons实现从主机字节序到网络字节序的转换
    address.sin_addr.s_addr = htonl(INADDR_ANY);
    address.sin_port = htons(m_port);

    int flag = 1;
    // SO_REUSEADDR -- 重用本地地址
    // 允许服务器bind一个地址，即使这个地址当前已经存在已建立的连接
    setsockopt(m_listenfd, SOL_SOCKET, SO_REUSEADDR, &flag, sizeof(flag));
    // 所有专用socket地址在实际使用时需转换为通用socket地址sockaddr,所有socket接口使用的参数都是sockaddr
    ret = bind(m_listenfd, (struct sockaddr *)&address, sizeof(address));
    assert(ret >= 0);

    // 创建一个监听队列以存放待处理的客户连接
    // backlog = 5 -- 所有处于半连接状态(SYN_RCVD)和完全连接状态(ESTABLISHED)的socket上限
    ret = listen(m_listenfd, 5);
    assert(ret >= 0);

    // 设置utils中的 m_timeslot
    utils.init(TIMESLOT);

    // epoll把关心的文件描述符上的事件放在内核事件表中,无需像select和poll每次调用重复传入文件描述符集或事件集
    // epoll创建内核事件表
    epoll_event events[MAX_EVENT_NUMBER];
    // 提示内核事件表有多大
    m_epollfd = epoll_create(5);
    assert(m_epollfd != -1);

    // 向m_epollfd指向的内核事件表中添加m_listenfd的文件描述符
    // 其中EPOLLONESHOT = false -- 对于某个socket,可能同时两个线程对其进行读和写,使用ONESHOT保证在任意时刻只被一个线程处理
    // m_LISTENTrigmode -- 事件是否以ET模式工作
    utils.addfd(m_epollfd, m_listenfd, false, m_LISTENTrigmode);
    http_conn::m_epollfd = m_epollfd;

    // 创建一对管道套接字
    ret = socketpair(PF_UNIX, SOCK_STREAM, 0, m_pipefd);
    assert(ret != -1);
    // 设置管道写端为非阻塞,尽可能地减少信号处理函数的执行时间
    utils.setnonblocking(m_pipefd[1]);
    // 设置管道读端为ET非阻塞事件并加入内核事件表
    utils.addfd(m_epollfd, m_pipefd[0], false, 0);

    // 设置信号处理函数
    utils.addsig(SIGPIPE, SIG_IGN); // SIG_IGN -- 忽略信号
    // 收到SIGALRM/SIGTERM信号时向管道写入管道的数据
    utils.addsig(SIGALRM, utils.sig_handler, false);
    utils.addsig(SIGTERM, utils.sig_handler, false);
    
    // 它可以在进程中设置一个定时器，当定时器指定的时间到时，它向进程发送SIGALRM信号
    alarm(TIMESLOT);

    // 对Utils进行相应的设置,管道用于进程间通信,epoll文件描述符用于对内核事件表进行操作
    Utils::u_pipefd = m_pipefd;
    Utils::u_epollfd = m_epollfd;
}

/* 根据客户端发出的信息建立相应的http连接和timer */
void WebServer::timer(int connfd, struct  sockaddr_in client_address){
    // 建立http连接并将connfd添加到epoll内核事件表
    users[connfd].init(connfd, client_address, m_root, m_CONNTrigmode, m_close_log, m_user, m_password,m_databaseName);

    // 设置定时器
    users_timer[connfd].address = client_address;
    users_timer[connfd].sockfd = connfd;
    util_timer *timer = new util_timer;
    timer->user_data = &users_timer[connfd];
    timer->cb_func = cb_func;
    time_t cur = time(NULL);
    timer->expire = cur + 3 * TIMESLOT;
    users_timer[connfd].timer = timer;
    utils.m_timer_lst.add_timer(timer);

}

/* 若有数据传输则将该连接的超时时间进行调整 */
void WebServer::adjust_timer(util_timer *timer){
    time_t cur = time(NULL);
    timer->expire = cur + 3 * TIMESLOT;
    utils.m_timer_lst.adjust_timer(timer);

    LOG_INFO("%s", "adjust timer once");
}

/* 断开连接,移除定时器,将事件从epoll内核事件表移除*/
void WebServer::deal_timer(util_timer *timer, int sockfd){
    timer->cb_func(&users_timer[sockfd]);
    if(timer){
        utils.m_timer_lst.del_timer(timer);
    }

    LOG_INFO("close fd %d", users_timer[sockfd].sockfd);
}

/* 处理来自客户端的数据 */
bool WebServer::dealclientdata(){
    struct sockaddr_in client_address;
    socklen_t client_addrlength = sizeof(client_address);

    // Level Trigger
    // LT模式通知应用程序后可不进行处理
    if(0 == m_LISTENTrigmode){
        int connfd = accept(m_listenfd, (struct sockaddr *)&client_address, &client_addrlength);
        if(connfd < 0){
            LOG_ERROR("%s:errno is:%d", "accept error", errno);
            return false;
        }
        // 当http_conn的连接超载
        if(http_conn::m_user_count >= MAX_FD){
            utils.show_error(connfd, "Internal server busy");
            LOG_ERROR("%s", "Internal server busy");
            return false;
        }
        timer(connfd, client_address);
    }

    // Edge Trigger
    // ET模式下仅触发一次,应用程序应当立即处理,所以应使用while(1)循环
    else {
        while(1){
            int connfd = accept(m_listenfd, (struct sockaddr *)&client_address, &client_addrlength);
             if(connfd < 0){
                LOG_ERROR("%s:errno is:%d", "accept error", errno);
                break;
            }
            // 当http_conn的连接超载
            if(http_conn::m_user_count >= MAX_FD){
                utils.show_error(connfd, "Internal server busy");
                LOG_ERROR("%s", "Internal server busy");
                break;
            }
            timer(connfd, client_address);
        }
        return false;
    }
    return true;
}

/* 处理来自管道的信号信息 */
bool WebServer::dealwithsignal(bool &timeout, bool &stop_server){
    int ret = 0;
    int sig;
    char signals[1024];

    // 接收来自m_pipefd[0]的数据,signals为缓冲区
    // recv返回实际读取数据的长度
    ret = recv(m_pipefd[0], signals, sizeof(signals), 0);
    if(ret == -1){
        return false;
    }
    else if(ret == 0){
        return false;
    }
    else{
        for(int i = 0;i < ret;++i){
            switch (signals[i])
            {
            case SIGALRM:
            {
                timeout = true;
                break;
            }
            case SIGTERM:
            {
                stop_server = true;
                break;
            }
            }
        }
    }
    return true;
}

/* 处理来自客户端需要读取的数据 */
void WebServer::dealwithread(int sockfd){
    util_timer *timer = users_timer[sockfd].timer;

    // Reactor
    if(1 == m_actormodel){
        if(timer){
            adjust_timer(timer);
        }

        // 若检测到读事件则将该事件放入请求队列
        m_pool->append(users + sockfd, 0);

        while(1){
            if(1 == users[sockfd].improv){
                if(1 == users[sockfd].timer_flag){
                    deal_timer(timer, sockfd);
                    users[sockfd].timer_flag = 0;
                }
                users[sockfd].improv = 0;
                break;
            }
        }
    }

    // Proactor
    else{
        if(users[sockfd].read_once()){
            LOG_INFO("deal with the client(%s)", inet_ntoa(users[sockfd].get_address()->sin_addr));

            // 若监测到读事件则将该事件放入请求队列
            m_pool -> append_p(users + sockfd);

            if(timer){
                adjust_timer(timer);
            }
        }
        else{
            deal_timer(timer, sockfd);
        }
    }
}

void WebServer::dealwithwrite(int sockfd){

}

void WebServer::eventLoop(){
    bool timeout = false;
    bool stop_server = false;

    while(!stop_server){
        // 在一段超时时间内等待一组文件描述符上的事件
        // 此处timeout为-1 -- 永远阻塞等待
        // 如果检测到事件就将所有就绪的事件从内核事件表中复制到events数组中
        int number = epoll_wait(m_epollfd, events, MAX_EVENT_NUMBER, -1);
        if(number < 0 && errno != EINTR){
            // epoll_wait 失败
            LOG_ERROR("%s", "epoll failure");
            break;
        }
        
        for(int i = 0;i < number;++i){
            int sockfd = events[i].data.fd;

            // 程序初始化时只将m_listenfd和m_pipefd[0]加入epoll内核事件表
            if(sockfd == m_listenfd){
                bool flag = dealclientdata();
                // 未能成功建立起与client的连接
                if(false == flag){
                    continue;
                }
            }

            // EPOLLRDHUP --- TCP连接被对方关闭或对方关闭了写操作
            // EPOLLHUP --- 挂起
            // EPOLLERR --- 错误
            // 当发生关闭、挂起或错误时
            else if(events[i].events & (EPOLLRDHUP | EPOLLHUP | EPOLLERR)){
                util_timer *timer = users_timer[sockfd].timer;
                deal_timer(timer, sockfd);
            }

            // 由管道读端触发epoll且数据可读时处理数据
            // 管道制定的协议只可能发送SIGALRM和SIGTREM信号
            else if((sockfd == m_pipefd[0]) && (events[i].events & EPOLLIN)){
                bool flag = dealwithsignal(timeout, stop_server);
            }

            // 处理来自客户端发出的信息
            else if(events[i].events & EPOLLIN){
                dealwithread(sockfd);
            }

            // 处理需要向客户端发出的信息
            else if(events[i].events & EPOLLOUT){
                dealwithwrite(sockfd);
            }
        }

        if(timeout){
            utils.timer_handler();

            LOG_INFO("%s", "timer tick");

            timeout = false;
        }
    }


}

