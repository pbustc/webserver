同步/异步日志系统
===============
同步/异步日志系统主要涉及了两个模块，一个是日志模块，一个是阻塞队列模块,其中加入阻塞队列模块主要是解决异步写入日志做准备.
> * 自定义阻塞队列
> * 单例模式创建日志
> * 同步日志
> * 异步日志
> * 实现按天、超行分类

阻塞队列block_queue
======================
通过循环数组实现
- void clear() -- 清空队列

- bool full() -- 判断队列是否满,通过m_size >= m_max_size判断

- bool empty() -- 判断队列是否为空,通过 0 == m_size判断

- bool front(T &value) -- 若队列不为空返回队首元素

- bool back(T &value) -- 若队列不为空返回队尾元素

- int size() -- 返回当前队列中元素个数 m_size

- int max_size() -- 返回当前队列中的容量 m_max_size
 
- bool push(const T &item) -- 当队列未满时向队列添加元素item,并唤醒等待该条件变量的线程

- bool pop(T &item) -- 若队列为空时将该线程阻塞,等待被唤醒,之后取出队首元素

- bool pop(T &item, int ms_timeout) -- 若队列为空时等待至多ms_timeout的时间,之后取出队首元素或返回false

日志系统
=======
通过饱汉单例模式实现
饿汉模式可能存在的问题 -- 非静态对象（函数外的static对象）在不同编译单元中的初始化顺序是未定义的。如果在初始化完成之前调用 getInstance() 方法会返回一个未定义的实例

- static Log *get_instance() -- C++11后编译器对静态局部变量线程安全进行保证,返回唯一实例

- void *async_write_log() --- 从阻塞队列中取出记录并输出到文件中

- static void *flush_log_thread(void *args) --- 设置为静态变量对async_write_log进行调用

- bool init(const char *file_name, int close_log, int log_buf_size = 8192, int split_lines = 5000000, int max_queue_size = 0) -- 对日志系统的工作模式、日志文件名等进行初始化设置

- void write_log(int level, const char *format, ...) --- 生成一个string类型的记录并输出或保存到阻塞队列中

- void flush(void) --- 刷新文件流的输出缓冲区,将实际的数据写入文件中

