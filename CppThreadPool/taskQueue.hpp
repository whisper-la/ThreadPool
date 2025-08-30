#pragma once
#include <queue>
#include <pthread.h>

// 定义任务结构体
using callback=void(*)(void*);
template <typename T>
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
        this->arg=static_cast<T*>(arg);
    }
    callback function;
    T* arg;
};

template <typename T>
// 定义任务队列
class taskQueue{
    public:
        taskQueue();
        ~taskQueue();

        // 添加任务
        void addTask(task_t<T> task);
        void addTask(callback function,void* arg);
        // 获取任务
        task_t<T> getTask();
        // 获取任务数量
        inline int getTaskNum()
        {
            return m_taskQueue.size();
        }
    private:
        std::queue<task_t<T>> m_taskQueue;
        // 任务队列互斥锁
        pthread_mutex_t taskQueueMutex;
};

template <typename T>
taskQueue<T>::taskQueue()
{
    pthread_mutex_init(&taskQueueMutex,nullptr);
}

template <typename T>
taskQueue<T>::~taskQueue()
{
    pthread_mutex_destroy(&taskQueueMutex);
}

template <typename T>
void taskQueue<T>::addTask(task_t<T> task)          
{
    pthread_mutex_lock(&taskQueueMutex);
    m_taskQueue.push(task);
    pthread_mutex_unlock(&taskQueueMutex);
}

template <typename T>
void taskQueue<T>::addTask(callback function,void* arg)
{
    pthread_mutex_lock(&taskQueueMutex);
    m_taskQueue.push(task_t(function,arg));
    pthread_mutex_unlock(&taskQueueMutex);
}

template <typename T>
task_t<T> taskQueue<T>::getTask()
{
    pthread_mutex_lock(&taskQueueMutex);
    task_t<T> task=m_taskQueue.front();
    m_taskQueue.pop();
    pthread_mutex_unlock(&taskQueueMutex);
    return task;
}
