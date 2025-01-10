#ifndef THREADPOOL_H
#define THREADPOOL_H

#include <list>
#include <cstdio>
#include <exception>
#include <pthread.h>
#include "locker.h"

// 实现线程池类，利用多线程并发处理任务

// 线程池类，将它定义为模板类是为了代码复用，模板参数T是任务类
template<typename T>
class threadpool {
public:
    /*thread_number是线程池中线程的数量，max_requests是请求队列中最多允许的、等待处理的请求的数量*/
    threadpool(int thread_number = 8, int max_requests = 10000);
    ~threadpool();
    bool append(T* request); // 用于向请求队列添加任务。

private:
    /*工作线程运行的函数，它不断从工作队列中取出任务并执行之*/
    static void* worker(void* arg);

    void run(); // 线程池内线程实际执行任务的循环体。持续循环处理任务，直到 m_stop 变为 true。
    /*
    只要 m_stop 为 false，线程就会等待信号量 m_queuestat，有信号时表示有新任务，获取锁访问队列。
    若队列为空则解锁继续等待；否则取出队首任务，解锁后执行任务的 process 方法（前提是任务指针不为空）。
    这样持续循环处理任务，直到 m_stop 变为 true。
    */

private:
    // 线程的数量，默认值为8
    int m_thread_number;  
    
    // 描述线程池的数组，大小为m_thread_number 
    // 一个动态分配的数组，用户存储线程的标识符，数组大小为m_thread_number   
    pthread_t * m_threads;

    // 请求队列中最多允许的、等待处理的请求的数量 ，默认设为10000 
    int m_max_requests; 
    
    // 请求队列
    // 一个std::list容器，用于存储等待处理的任务，任务类型是指向T类型对象的指针
    std::list< T* > m_workqueue;  

    // 保护请求队列的互斥锁，防止多个线程同时访问和修改队列，确保线程安全。
    locker m_queuelocker;   

    // 是否有任务需要处理
    // 一个信号量对象，用于指示队列中是否有任务需要处理
    sem m_queuestat;

    // 布尔变量，用于标记是否结束线程池          
    bool m_stop;                    
};

template< typename T >
threadpool< T >::threadpool(int thread_number, int max_requests) : 
        m_thread_number(thread_number), m_max_requests(max_requests), 
        m_stop(false), m_threads(NULL) {

    if((thread_number <= 0) || (max_requests <= 0) ) { // 检查传入参数是否合法
        throw std::exception();
    }

    m_threads = new pthread_t[m_thread_number]; // 分配内存用于存储线程标识符数组
    if(!m_threads) { // 如果分配失败抛出异常。
        throw std::exception();
    }

    // 创建thread_number 个线程，并将他们设置为脱离线程。
    for ( int i = 0; i < thread_number; ++i ) { // 逐个创建 thread_number 个线程
        printf( "create the %dth thread\n", i);
        if(pthread_create(m_threads + i, NULL, worker, this ) != 0) {
            /*
            int pthread_create(pthread_t *thread, const pthread_attr_t *attr, 
                    void *(*start_routine)(void *), void *arg);各参数的含义
            - thread：这是一个指向 pthread_t 类型变量的指针，pthread_t 用于标识线程。
                当 pthread_create 成功创建线程后，新线程的标识符会存储在这个指针所指向的变量中，
                后续可以凭借这个标识符来操作该线程，例如等待线程结束（使用 pthread_join）
                或者设置线程为脱离状态（使用 pthread_detach ）。
            - attr：指向 pthread_attr_t 类型结构体的指针，这个结构体用于设置线程的属性，
                像是线程栈大小、调度策略、线程优先级 等。如果传入 NULL，则表示使用默认的线程属性。
                默认属性通常能满足大多数常见的使用场景，不过在一些对性能、资源利用有特殊要求的情况下，
                就需要定制化设置属性。
            - start_routine：这是一个函数指针，指向新线程开始执行的函数。
                该函数的返回值类型是 void*，参数也是 void* 类型，也就是说新线程启动后，会从这个函数开始运行，
                并且能够接受一个任意类型的参数，这使得线程执行的任务可以灵活定制。
            - arg：作为参数传递给 start_routine 指向的函数。它可以是任何类型的数据，
                只要在 start_routine 函数内部进行相应的类型转换即可使用。
                这个参数机制方便把必要的信息传递给新线程，让它知晓要处理的任务细节。
            */
            // 如果 pthread_create 函数调用失败，返回非零值，就会进入 if 分支执行后续代码。
            // 如果线程创建失败，先释放之前已经分配的 m_threads 数组内存，避免内存泄漏。
            // 抛出一个 std::exception 类型的异常，通知调用者线程创建过程出现问题，调用者可以捕获这个异常进行相应处理。
            delete [] m_threads;
            throw std::exception();
        }
        
        if( pthread_detach( m_threads[i] ) ) {
            /*
            int pthread_detach(pthread_t thread);
            - thread 是要设置为脱离状态的线程的标识符，也就是通过 pthread_create 函数创建线程时，
                存储在 pthread_t 类型变量中的那个值。这个值唯一标识了程序中的某个线程。
            脱离状态的意义：在常规的线程模型里，当一个线程结束执行后，它的资源（例如线程栈空间）
                并不会立即被系统回收，需要其他线程调用 pthread_join 函数来等待该线程结束，并回收它的资源。
                但如果一个线程被设置为脱离状态，当它执行完毕，系统会自动回收它的所有资源，
                无需等待其他线程执行 pthread_join。
            */
            // 如果 pthread_detach 函数调用失败
            // 如果线程创建失败，先释放之前已经分配的 m_threads 数组内存，避免内存泄漏。
            delete [] m_threads;
            throw std::exception();
        }
    }
}

template< typename T >
threadpool< T >::~threadpool() {
    /*
    先释放之前已经分配的 m_threads 数组内存，避免内存泄漏。
    m_stop标记为true，结束线程池
    */
    delete [] m_threads;
    m_stop = true;
}

//主要功能是向线程池的工作队列中添加任务
template< typename T >
bool threadpool< T >::append( T* request )
{
    // 操作工作队列时一定要加锁，因为它被所有线程共享。
    // 共享队列加锁，多线程环境下，多个线程可能同时尝试访问和修改工作队列，加锁能避免数据竞争和不一致问题，确保线程安全。
    m_queuelocker.lock();
    if ( m_workqueue.size() >= m_max_requests ) {
        /*
        检查工作队列的当前大小是否已经达到或超过允许的最大请求数 m_max_requests。
        如果队列已满，继续添加任务可能导致内存溢出或其他不可预料的错误。
        */
        m_queuelocker.unlock();
        return false;
    }
    m_workqueue.push_back(request); // 如果队列未满，将传入的任务指针 request 添加到工作队列 m_workqueue 的末尾。
    m_queuelocker.unlock(); // 任务添加完成后，解锁互斥锁，以便其他线程能够操作工作队列。

    /*
    使用信号量 m_queuestat 发送一个信号。信号量在这里用于通知等待任务的线程，
    工作队列中有新的任务可供处理了，唤醒处于等待状态的线程来执行新添加的任务。
    */
   m_queuestat.post();
   
    return true; // 返回 true，表明任务已成功添加到工作队列。
}

/*
模板函数，它接收一个 void* 类型的参数 arg。
在 C++ 多线程编程中，pthread_create 创建线程时传递的参数类型是 void*，
所以这里先以通用的 void* 接收，后续再做类型转换。
*/
template< typename T >
void* threadpool< T >::worker( void* arg )
{
    /*
    将传入的 void* 类型参数 arg 强制转换为 threadpool* 类型，
    因为这个参数实际上是指向线程池对象的指针。pthread_create 函数在创建线程时，
    把 this 指针（也就是当前线程池对象自身的指针）传递给了 worker 函数，
    通过这个转换，worker 函数就能获取到线程池对象，进而访问其中的成员变量和成员方法。
    */
    threadpool* pool = ( threadpool* )arg;

    /*
    调用获取到的线程池对象 pool 的 run 方法。run 方法包含了线程池内线程实际执行任务的逻辑循环：
    不断检查任务队列是否有任务、取出任务并执行。线程创建后，就从这里开始进入正式的任务处理流程。
    */
    pool->run();

    return pool; // 返回指向线程池对象的指针
}

/*
该方法是线程池中每个工作线程实际执行任务的逻辑核心
*/
template< typename T >
void threadpool< T >::run() {

    while (!m_stop) { // 只要 m_stop 变量为 false，线程就会持续运行。

        /*
        wait 操作会让消耗m_queuestat信号量资源；当信号量的值为 0 时，调用 wait 会使当前线程进入阻塞状态 。
        前面在往任务队列添加任务时，会执行 m_queuestat.post() 来增加信号量的值，
        意味着当有新任务加入队列时，处于等待状态的线程就会被唤醒，避免线程空转消耗资源，实现高效的任务调度。
        */
        m_queuestat.wait();

        /*
        线程被唤醒后，先获取互斥锁 m_queuelocker，用于锁定任务队列 m_workqueue。
        因为多个线程共享任务队列，加锁是为了防止在访问和修改队列时出现数据竞争，确保线程安全。
        */
        m_queuelocker.lock();

        if ( m_workqueue.empty() ) { // 检查任务队列是否为空。
            /*
            如果队列确实为空，解锁互斥锁，让其他线程能够访问队列，
            然后使用 continue 回到循环开头，继续等待下一次信号量唤醒。
            */
            m_queuelocker.unlock();
            continue;
        }
        T* request = m_workqueue.front(); // 若队列不为空，取出队列头部的任务指针 request，
        m_workqueue.pop_front(); // 再将这个任务从队列中移除。
        m_queuelocker.unlock(); // 任务取出后，解锁互斥锁，允许其他线程操作任务队列。
        if ( !request ) {
            /*
            检查取出的任务指针是否为空，若为空指针，跳过本次循环，
            继续处理下一个任务，防止空指针引用导致程序崩溃。
            */
            continue;
        }

        /*
        调用任务对象的 process 方法来执行实际的任务内容。
        这里假设任务类 T 定义了 process 方法，该方法包含了针对特定任务的具体处理逻辑。
        */
        request->process();
    }

}

#endif
