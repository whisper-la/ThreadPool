#pragma once
#include <queue>
#include <pthread.h>

// 定义任务结构体
using callback=void(*)(void*);
struct task_t
{
    task_t()
    {
        function=nullptr;
        arg=nullptr;
    }
    task_t(callback function,void* arg)
    {
        this->function=function;
        this->arg=arg;
    }
    callback function;
    void* arg;
};

// 定义任务队列
class taskQueue{
    public:
        taskQueue();
        ~taskQueue();

        // 添加任务
        void addTask(task_t task);
        void addTask(callback function,void* arg);
        // 获取任务
        task_t getTask();
        // 获取任务数量
        inline int getTaskNum()
        {
            return m_taskQueue.size();
        }
    private:
        std::queue<task_t> m_taskQueue;
        // 任务队列互斥锁
        pthread_mutex_t taskQueueMutex;
};
