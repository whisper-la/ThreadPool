#pragma once
#include <queue>
#include <pthread.h>
#include <unistd.h>
#include <iostream>
#include "taskQueue.h"
#include <string>

// 定义线程池类
class threadPool{
    public:
        threadPool(int minThreadNum,int maxThreadNum);
        ~threadPool();
        // 添加任务
        void addTask(task_t task);
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
        taskQueue *m_taskQueue; // 任务队列
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

