#include "lst_timer.h"
#include "../http/http_conn.h"

sort_timer_lst::sort_timer_lst(){
    head = NULL;
    tail = NULL;
}

/* 链表被销毁时删除所有定时器 */
sort_timer_lst::~sort_timer_lst(){
    util_timer *tmp = head;
    while(tmp){
        head = tmp -> next;
        delete tmp;
        tmp = head;
    }
}

/* 将目标定时器timer添加到链表中 */
void sort_timer_lst::add_timer(util_timer *timer){
    if(!timer) return;
    if(!head){
        head = tail = timer;
        return;
    }
    // 保证链表的升序特性,按照定时器的超时时间进行排序
    if(timer->expire < head->expire){
        timer -> next = head;
        head -> prev = timer;
        head = timer;
        return;
    }
    add_timer(timer, head);
}

/* 改变目标定时器的超时时间后对链表进行调整  --- 此处仅针对将timer的超时时间增加 */
void sort_timer_lst::adjust_timer(util_timer *timer){
    if(!timer) return;
    util_timer *tmp = timer -> next;

    // 若timer已经处于链表尾部或其超时时间仍然小于下一个定时器
    if(!tmp || timer -> expire < tmp -> expire){
        return;
    }

    // timer是链表的头节点
    if(timer == head){
        head = head -> next;
        head -> prev = NULL;
        timer -> next = NULL;
        add_timer(timer, head);
    }

    // timer不是链表的头节点
    else{
        timer -> next -> prev = timer -> prev;
        timer -> prev -> next = timer -> next;
        add_timer(timer, timer->next);
    }
}

/* 将目标定时器从链表中删除 */
void sort_timer_lst::del_timer(util_timer *timer){
    if(!timer) return;

    // 链表中仅有timer一个定时器
    if((timer == head) && (timer == tail)){
        delete timer;
        head = NULL;
        tail = NULL;
        return;
    }

    // 链表中有至少有两个定时器且timer是链表的头节点
    if(timer == head){
        head = head -> next;
        head -> prev = NULL;
        delete timer;
        return;
    }

    // 链表中至少有两个定时器且timer是链表的尾结点
    if(timer == tail){
        tail = tail -> prev;
        tail -> next = NULL;
        delete timer;
        return;
    }

    // 链表至少有三个定时器且timer位于链表的中间位置
    timer -> prev -> next = timer -> next;
    timer -> next -> prev = timer -> prev;
    delete timer;
}

/* SIGALRM信号每次被触发就在其信号处理函数中执行一次tick函数以处理链表上到期的任务 */
void sort_timer_lst::tick(){
    if(!head) return;
    printf("timer tick\n");
    time_t cur = time(NULL);
    util_timer *tmp = head;
    // 从头到尾处理链表中每个定时器,直到遇到一个尚未到期的定时器
    while(tmp){
        // 每个定时器都使用绝对时间作为超时值
        if(cur < tmp -> expire){
            // 链表升序排列
            break;
        }
        // 调用定时器的回调函数以执行定时任务
        tmp -> cb_func(tmp -> user_data);

        // 执行完定时器中的定时任务后便将其删除并重置头节点
        head = tmp -> next;
        if(head){
            head -> prev = NULL;
        }
        delete tmp;
        tmp = head;
    }
}

/* 将目标定时器timer添加到lst_head之后的部分链表中 */
void sort_timer_lst::add_timer(util_timer *timer, util_timer *lst_head){
    util_timer *prev = lst_head;
    util_timer *tmp = prev -> next;
    // 遍历lst_head节点之后的部分链表,找到第一个超时时间大于目标定时器的节点
    while(tmp){
        if(timer -> expire < tmp -> expire){
            prev -> next = timer;
            timer -> next = tmp;
            tmp -> prev = timer;
            timer -> prev = prev;
            break;
        }
        prev = tmp;
        tmp = tmp -> next;
    }

    // 遍历完lst_head结点之后的链表未发现超时时间大于目标定时器的节点
    // 将目标定时器插入链表尾部
    if(!tmp){
        prev -> next = timer;
        timer -> prev = prev;
        timer -> next = NULL;
        tail = timer;
    }
}


void Utils::init(int timeslot){
    m_TIMESLOT = timeslot;
}

/* 对文件描述符设置非阻塞 */
// 文件设置成非阻塞，即便发现内容为空，也不会阻塞等待而是会直接返回
// 需要将管道写端设为非阻塞,读端设为ET非阻塞
int Utils::setnonblocking(int fd){
    int old_option = fcntl(fd, F_GETFL);
    int new_option = old_option | O_NONBLOCK;
    fcntl(fd, F_SETFL, new_option);
    return old_option;
} 

void Utils::addfd(int epollfd, int fd, bool one_shot, int TRIGMode){
    epoll_event event;
    event.data.fd = fd;

    if(1 == TRIGMode)
        event.events = EPOLLIN || EPOLLET | EPOLLRDHUP;
    else   
        event.events = EPOLLIN | EPOLLRDHUP;
    if(one_shot)
        event.events |= EPOLLONESHOT;
    epoll_ctl(epollfd, EPOLL_CTL_ADD, fd, &event);
    setnonblocking(fd);
}


/* 自定义的信号处理函数 */
// 信号处理函数仅通过管道发送信号值,不处理信号对应的逻辑,缩短异步执行时间
void Utils::sig_handler(int sig){
    // 为保证函数的可重入性,保留原有的errno
    int save_errno = errno;
    int msg = sig;

    // 将信号值从管道写端写入
    // ssize_t send(int sockfd, const void *buf, size_t len, int flags)
    // 向sockfd指向的文件写入
    send(u_pipefd[1], (char *)&msg, 1, 0);
    errno = save_errno;
}

/* 设置信号函数 */
// handler使用的参数是Utils::sig_handler
void Utils::addsig(int sig, void(handler)(int), bool restart){
    struct sigaction sa;
    memset(&sa, '\0', sizeof(sa));
    // 设置sigaction的sa_handler --- sa_handler是一个指向信号处理函数的指针
    sa.sa_handler = handler;
    if(restart)
        // sa_flags用于指定信号处理的行为
        // SA_RESTART --- 使被信号打断的系统调用自动重新发起
        sa.sa_flags |= SA_RESTART;
    // sig_set是一个信号集,sigset_t实际是一个长整型数组,其中每个位表示一个信号
    // sigfillset(sigset_t &t)在信号集中设置所有信号,不设置掩码
    sigfillset(&sa.sa_mask); 
    // 设置sig的信号处理函数等
    assert(sigaction(sig, &sa, NULL) != -1);
}


// 定时处理任务,重新定时以不断触发SIGALRM信号
void Utils::timer_handler(){
    m_timer_lst.tick();
    alarm(m_TIMESLOT);
}
void Utils::show_error(int connfd, const char *info){
    send(connfd, info, strlen(info), 0);
    close(connfd);
}

int *Utils::u_pipefd = 0;
int Utils::u_epollfd = 0;

class Utils;

void cb_func(client_data *user_data){
    epoll_ctl(Utils::u_epollfd, EPOLL_CTL_DEL, user_data->sockfd, 0);
    assert(user_data);
    close(user_data->sockfd);
    http_conn::m_user_count --;
}

