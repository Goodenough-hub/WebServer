#ifndef LOCKER_H
#define LOCKER_H

#include <exception>
#include <pthread.h>
#include <semaphore.h>

// 线程同步机制封装类

// 互斥锁类
class locker {
public:
    /*
    构造函数用于初始化互斥锁。在对象创建时，调用 pthread_mutex_init 函数
    来初始化类中的 m_mutex 成员变量，也就是创建一个 pthread 互斥锁。
    pthread_mutex_init 的第二个参数如果传入 NULL，表示使用默认属性初始化互斥锁。
    要是初始化失败，函数返回非零值，此时构造函数会抛出一个 std::exception 异常，
    防止创建出一个带有未初始化互斥锁的对象。
    */
    locker() {
        if(pthread_mutex_init(&m_mutex, NULL) != 0) {
            throw std::exception();
        }
    }

    /*
    析构函数负责清理资源。当 locker 对象生命周期结束时，调用 pthread_mutex_destroy 函数
    来销毁之前初始化的互斥锁，释放相关的系统资源，确保没有内存泄漏等资源管理问题。
     */
    ~locker() {
        pthread_mutex_destroy(&m_mutex);
    }

    /*
    该方法尝试获取互斥锁。它调用 pthread_mutex_lock 函数对 m_mutex 加锁，
    如果加锁成功，pthread_mutex_lock 返回 0，函数返回 true；若加锁失败（例如已经被其他线程锁定 ），
    则返回非零值，函数返回 false。
    */
    bool lock() {
        return pthread_mutex_lock(&m_mutex) == 0;
    }

    /*
    用于释放已经获取到的互斥锁。调用 pthread_mutex_unlock 函数解锁 m_mutex，
    解锁成功时函数返回 0，此方法也就返回 true；解锁失败则返回非零值，方法返回 false。
    */
    bool unlock() {
        return pthread_mutex_unlock(&m_mutex) == 0;
    }

    /*
    这个方法返回互斥锁对象的指针。
    在某些更复杂的场景，比如需要将这个互斥锁传递给其他函数或类，该方法就提供了一种便捷的获取途径。
    */
    pthread_mutex_t *get()
    {
        return &m_mutex;
    }

private:
    pthread_mutex_t m_mutex;
};


// 条件变量类-线程同步
class cond {
public:
    /*
    构造函数用于初始化条件变量。在对象创建时，
    调用pthread_cond_init函数来初始化类中的m_cond成员变量，也就是创建一个 POSIX 条件变量。
    pthread_cond_init的第二个参数如果传入NULL，表示使用默认属性初始化条件变量。要是初始化失败，
    函数返回非零值，此时构造函数会抛出一个std::exception异常，防止创建出一个带有未初始化条件变量的对象。
    */
    cond(){
        if (pthread_cond_init(&m_cond, NULL) != 0) {
            throw std::exception();
        }
    }

    /*
    析构函数负责清理资源。当cond对象生命周期结束时，调用pthread_cond_destroy函数来
    销毁之前初始化的条件变量，释放相关的系统资源，确保没有内存泄漏等资源管理问题。
    */
    ~cond() {
        pthread_cond_destroy(&m_cond);
    }

    /*
    该方法用于让线程等待条件变量被通知。线程调用pthread_cond_wait函数时，
    会自动释放传入的互斥锁m_mutex，然后进入阻塞状态，等待其他线程调用signal或者broadcast方法唤醒它。
    被唤醒后，线程会重新获取互斥锁，函数返回值为 0 表示操作成功，此时方法返回true，否则返回false 。
    */
    bool wait(pthread_mutex_t *m_mutex) {
        int ret = 0;

        ret = pthread_cond_wait(&m_cond, m_mutex); // 等待，调用了该函数，线程会阻塞。
        // 线程调用等待函数时，不仅阻塞，还会主动释放关联的互斥锁，出让资源给其他线程去改变条件，被唤醒后再尝试夺回互斥锁，以此保障条件判断与执行的连贯性。
        // pthread_cond_wait 会执行的两个关键操作
        // - 释放互斥锁m_mutex，让其他线程能够获取该互斥锁
        // - 进入阻塞状态，线程在释放锁之后，会进入阻塞等待状态，
        //      直到其他线程调用了与该条件变量相关的 pthread_cond_signal 或者 pthread_cond_broadcast 函数来唤醒它。
        //      一旦线程被唤醒，pthread_cond_wait并不会立刻让线程恢复执行，而是会尝试重新获取之前释放的互斥锁mutex 。
        //      只有成功重新获取到互斥锁后，线程才可以继续执行后续代码，这一步保证了线程恢复执行时，共享资源依旧处于受保护状态，防止出现数据竞争等同步问题。

        return ret == 0;
    }

    /*
    让线程在等待条件变量时设置一个超时时间
    */
    bool timewait(pthread_mutex_t *m_mutex, struct timespec t) { // t用来指定线程等待的最长时间
        int ret = 0;
        ret = pthread_cond_timedwait(&m_cond, m_mutex, &t); // 等待多长时间，调用了这个函数，线程会阻塞，直到指定的时间结束。
        // 一旦线程被唤醒，无论唤醒是因为条件达成还是超时到期，它都会尝试重新获取之前释放的互斥锁 m_mutex，
        // 只有成功获取到锁，线程才能继续执行后续代码，确保共享资源访问的安全性。
        return ret == 0;
    }

    // 用于唤醒一个正在等待特定条件变量的线程
    bool signal() {
        return pthread_cond_signal(&m_cond) == 0;
    }

    // 唤醒所有正在等待指定条件变量的线程 
    bool broadcast() {
        return pthread_cond_broadcast(&m_cond) == 0;
    }

private:
    pthread_cond_t m_cond;
};


// 信号量类
class sem {
public:

    /*
    该构造函数用于初始化一个信号量，初始值设为 0。
    sem_init 是 POSIX 信号量的初始化函数，第一个参数 &m_sem 指向要初始化的信号量对象；
    第二个参数 0 表示这个信号量是线程间共享的（如果设为非零值，则用于进程间共享 ）；
    第三个参数 0 设定了信号量的初始值。
    若初始化失败，sem_init 返回非零值，构造函数会抛出 std::exception 异常，
    防止创建出一个带有未初始化信号量的对象。
    */
    sem() {
        if( sem_init( &m_sem, 0, 0 ) != 0 ) {
            throw std::exception();
        }
    }

    /* 
    这个构造函数接受一个整数参数 num，用于将信号量初始化为指定的值。
    同样调用 sem_init 函数，按参数配置初始化信号量，失败时抛出异常。
    这为用户提供了灵活性，可按需设定信号量初始资源数量。
    */
    sem(int num) {
        if( sem_init( &m_sem, 0, num ) != 0 ) {
            throw std::exception();
        }
    }

    /*
    析构函数负责清理资源。当 sem 对象生命周期结束时，调用 sem_destroy 函数来
    销毁之前初始化的信号量，释放相关的系统资源，避免内存泄漏等资源管理问题。
    */
    ~sem() {
        sem_destroy( &m_sem );
    }

    // 等待信号量
    /*
    wait 方法用于让线程等待信号量。线程调用 sem_wait 函数时，
    如果信号量的值大于 0，则信号量的值减 1，线程继续执行；
    若信号量的值为 0，线程会进入阻塞状态，直到信号量的值变为大于 0，
    线程获取到信号量，值减 1 后继续执行。函数返回值为 0 表示操作成功，
    所以此方法返回 true；若返回非零值，表示出现错误，方法返回 false 。
    */
    bool wait() {
        return sem_wait( &m_sem ) == 0;
    }

    // 增加信号量
    /*
    post 方法用于增加信号量的值。线程调用 sem_post 函数，会使信号量的值加 1，
    如果有线程正在等待这个信号量（即信号量的值为 0 时 ），则唤醒其中一个等待线程。
    函数返回值为 0 表示操作成功，该方法返回 true；若返回非零值，表示出现错误，方法返回 false 。
    */
    bool post() {
        return sem_post( &m_sem ) == 0;
    }
private:
    sem_t m_sem; // 信号量对象
};

#endif