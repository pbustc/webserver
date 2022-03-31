>* socket网络通信的过程:
    服务端:
    1. 创建socket --- int socket(int domain, int type, int protocol)
    2. 命名socket --- int bind(int sockfd, const struct sockaddr* my_addr, socklen_t addrlen)
    3. 监听socket --- int listen(int sockfd, int backlog) 监听来自客户端的connect
    4. 接受socket --- int accpet(int sockfd, struct sockaddr *addr,socklen_t *addrlen)
    客户端需要调用connect与服务器建立连接
    >>> int connect(int sockfd, const struct sockaddr *serv_addr, socklen_t addrlen)
    关闭连接:
    int close(int fd)
    int shutdown(int sockfd, int howto)

>* LT和ET模式
    epoll对文件描述符的操作有两种模式:LT和ET
    >* 对于采用LT工作模式的文件描述符,当epoll_wait检测到其上有事件发生并将此事件通知应用程序后，应用程序可以不立即处理该事件
    >* ET模式下,当epoll_wait检测到其上有事件发生时通知给应用程序,应用程序必须立即处理该事件
>* EPOLLONESHOT事件
    ET模式下一个socket上的某个事件可能被触发多次,一个线程在读取完某个socket上的数据后开始处理这些数据,而在数据的处理过程中该socket又有新数据可读,此时另外一个线程被唤醒来读取这些新数据。
    我们期望一个socket连接在任一时刻都只被一个线程处理,可通过epoll的EPOLLONESHOT实现,对于注册了EPOLLONESHOT的事件,操作系统最多触发其上一个事件且只触发一次.