#pragma once
#include <queue>
#include <pthread.h>
#include <unistd.h>
#include <iostream>
#include <string.h>
#include "taskQueue.hpp"


template <typename T>
// 定义线程池类
class threadPool{
    public:
        threadPool(int minThreadNum,int maxThreadNum);
        ~threadPool();
        // 添加任务
        void addTask(task_t<T> task);
        void addTask(callback function,void* arg);
        int getBusyThreadNum(); // 获取忙线程数量
        int getLiveThreadNum(); // 获取存活线程数量
    private:
        // 线程函数
        static void* threadFunc(void* arg);
        // 管理线程函数
        static void* managerFunc(void* arg);
        void threadExit(); // 线程退出
    private:
        taskQueue<T> *m_taskQueue; // 任务队列  
        pthread_t* threadArray; // 线程池数组
        pthread_t managerThread; // 管理线程

        int liveThreadNum;  // 存活线程数量
        int busyThreadNum; // 忙线程数量
        int minThreadNum; // 最小线程数量
        int maxThreadNum; // 最大线程数量
        int exitThreadNum; // 退出线程数

        // 线程池互斥锁
        pthread_mutex_t threadPoolMutex;
        // 线程池条件变量
        pthread_cond_t notEmpty;

        bool shutdown; // 线程池是否关闭：1 关闭 0 打开
};

// 构造函数
template <typename T>
threadPool<T>::threadPool(int minThreadNum,int maxThreadNum)
{
    do
    {
        this->m_taskQueue = new taskQueue<T>;
        if(this->m_taskQueue == nullptr)
        {
            perror("threadpool m_taskQueue malloc failed......\n");
            break;
        }
        this->threadArray = new pthread_t[maxThreadNum];
        if (this->threadArray == nullptr)
        {
            perror("threadpool threadIDs malloc failed......\n");
            break;
        }
        memset(this->threadArray, 0, sizeof(pthread_t)*maxThreadNum); // 初始化线程数组
        this->minThreadNum=minThreadNum; // 最小线程数
        this->maxThreadNum=maxThreadNum; // 最大线程数
        this->liveThreadNum=minThreadNum; // 初始化存活线程数
        this->busyThreadNum=0; // 初始化忙线程数
        this->exitThreadNum=0; // 初始化退出线程数

        // 初始化信号量
        if(pthread_mutex_init(&this->threadPoolMutex, NULL) != 0||
        pthread_cond_init(&this->notEmpty, NULL) != 0)
        {
            perror("threadpool mutex or cond init failed......\n");
            break;
        }

        this->shutdown=false; // 线程池是否关闭标志位

        // 创建管理者线程
        pthread_create(&this->managerThread, NULL, threadPool<T>::managerFunc, this);

        // 创建工作线程组
        for(int i=0; i < minThreadNum; i++)
        {
            pthread_create(&this->threadArray[i], NULL, threadFunc, this);
        }
        std::cout << "threadpool create success" << std::endl;
        return;
    } while (0);
    if(this->threadArray)
    {
        free(this->threadArray);
        this->threadArray=nullptr;
    }
    if (this->m_taskQueue)
    {
        free(this->m_taskQueue);
        this->m_taskQueue=nullptr;
    }
}

// 销毁线程池
/* 
    1. 先关闭线程池
    2. 阻塞回收管理者线程
    3. 唤醒消费者线程
    4. 释放堆内存
    5. 销毁信号量
*/
template <typename T>
threadPool<T>::~threadPool()
{
    // 先关闭线程池
    this->shutdown = true;
    // 阻塞回收管理者线程
    std::cout << "threadpool destroy, managerThread is " << this->managerThread << std::endl;
    pthread_join(this->managerThread, NULL);
    // 唤醒消费者线程
    for(int i=0;i<this->liveThreadNum;i++)
    {
        pthread_cond_signal(&this->notEmpty);
    }
    // 释放堆内存
    if(this->m_taskQueue)
    {
        free(this->m_taskQueue);
        this->m_taskQueue=nullptr;
    }
    if(this->threadArray)
    {
        free(this->threadArray);
        this->threadArray=nullptr;
    }
    
    // 销毁信号量
    pthread_mutex_destroy(&this->threadPoolMutex);
    pthread_cond_destroy(&this->notEmpty);
    
    std::cout << "threadpool destroy success" << std::endl;
}

template <typename T>
void threadPool<T>::addTask(task_t<T> task)
{
    if(this->shutdown)
    {
        return;
    }
    // 不需要加锁，因为任务队列已经有锁了
    // 添加任务
    this->m_taskQueue->addTask(task);
    // 唤醒消费者线程
    pthread_cond_signal(&this->notEmpty);
}


template <typename T>
void threadPool<T>::addTask(callback function,void* arg)
{
    if(this->shutdown)
    {
        return;
    }
    // 不需要加锁，因为任务队列已经有锁了
    // 添加任务
    this->m_taskQueue->addTask(function, arg);
    // 唤醒消费者线程
    pthread_cond_signal(&this->notEmpty);
}

template <typename T>
int threadPool<T>::getBusyThreadNum()
{
    int busyThreadNum = 0;
    pthread_mutex_lock(&this->threadPoolMutex);
    busyThreadNum = this->busyThreadNum;
    pthread_mutex_unlock(&this->threadPoolMutex);
    return busyThreadNum;
}

// 获取存活线程数量
template <typename T>
int threadPool<T>::getLiveThreadNum()
{
    int liveThreadNum = 0;
    pthread_mutex_lock(&this->threadPoolMutex);
    liveThreadNum = this->liveThreadNum;
    pthread_mutex_unlock(&this->threadPoolMutex);
    return liveThreadNum;
}

template <typename T>
// 线程函数
void* threadPool<T>::threadFunc(void* arg)
{
    threadPool<T>* pool = static_cast<threadPool<T>*>(arg);
    while(true)
    {
        // 加锁
        pthread_mutex_lock(&pool->threadPoolMutex);
        // 等待任务队列不为空
        while(pool->m_taskQueue->getTaskNum() == 0 && !pool->shutdown)
        {
            // 等待任务队列不为空
            pthread_cond_wait(&pool->notEmpty, &pool->threadPoolMutex);

            // 退出线程
            if(pool->exitThreadNum > 0)
            {
                pool->exitThreadNum--;
                if(pool->liveThreadNum > pool->minThreadNum)
                {
                    pool->liveThreadNum--;
                    pthread_mutex_unlock(&pool->threadPoolMutex);
                    pool->threadExit();
                }
            }
        }
        // 如果线程池关闭
        if(pool->shutdown)
        {
            pthread_mutex_unlock(&pool->threadPoolMutex);
            pool->threadExit();
        }
        // 获取任务
        task_t<T> task=pool->m_taskQueue->getTask();

        // 增加忙线程数
        pool->busyThreadNum++;
        // 解锁
        std::cout << "thread " << pthread_self() << " start work, busyThreadNum is " << pool->busyThreadNum << std::endl;
        pthread_mutex_unlock(&pool->threadPoolMutex);
        
        // 执行任务
        
        task.function(task.arg);
        // 安全地删除指针
        delete task.arg;
        task.arg = nullptr;

        // 减少忙线程数
        pthread_mutex_lock(&pool->threadPoolMutex);
        pool->busyThreadNum--;
        std::cout << "thread " << pthread_self() << " end work, busyThreadNum is " << pool->busyThreadNum << std::endl;
        pthread_mutex_unlock(&pool->threadPoolMutex);
    }
    return nullptr;
}

template <typename T>
void* threadPool<T>::managerFunc(void* arg)
{
    threadPool<T>* pool = static_cast<threadPool<T>*>(arg);
    while(!pool->shutdown)
    {
        // 线程 sleep 3s
        sleep(3);
        
        // 加锁
        pthread_mutex_lock(&pool->threadPoolMutex);
        // 获取队列大小
        int taskNum = pool->m_taskQueue->getTaskNum();
        // 获取存活线程数量
        int liveThreadNum = pool->liveThreadNum;
        // 获取忙线程数量
        int busyThreadNum = pool->busyThreadNum;
        // 解锁
        pthread_mutex_unlock(&pool->threadPoolMutex);

        const int number = 2;
        // 添加线程
        if(liveThreadNum < pool->maxThreadNum && taskNum > liveThreadNum)
        {
            // 加锁
            pthread_mutex_lock(&pool->threadPoolMutex);
            
            int count = 0;
            for(int i=0; i < pool->maxThreadNum && count < number; i++)
            {
                if(pool->threadArray[i] == 0)
                {
                    pthread_create(&pool->threadArray[i], NULL, threadFunc, pool);
                    liveThreadNum++;
                    count++;
                }
            }
            pool->liveThreadNum=liveThreadNum;
            // 解锁
            pthread_mutex_unlock(&pool->threadPoolMutex);
        }
        
        // 销毁线程
        if(liveThreadNum > pool->minThreadNum && busyThreadNum * 2 < liveThreadNum)
        {
            pthread_mutex_lock(&pool->threadPoolMutex);
            pool->exitThreadNum=number;
            pthread_mutex_unlock(&pool->threadPoolMutex);
            for(int i=0;i<number;i++)
            {
                pthread_cond_signal(&pool->notEmpty);
            }
        }
    }
    return nullptr;
}

template <typename T>
// 线程退出
void threadPool<T>::threadExit()
{
    pthread_t threadID = pthread_self();
    for(int i=0; i < this->maxThreadNum; i++)
    {
        if(this->threadArray[i] == threadID)
        {
            this->threadArray[i] = 0;
            std::cout << "thread " << threadID << " exit" << std::endl;
            break;
        }
    }
    pthread_exit(NULL);
}
