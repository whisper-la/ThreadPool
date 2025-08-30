#include "taskQueue.h"


taskQueue::taskQueue()
{
    pthread_mutex_init(&taskQueueMutex,nullptr);
}

taskQueue::~taskQueue()
{
    pthread_mutex_destroy(&taskQueueMutex);
}

void taskQueue::addTask(task_t task)
{
    pthread_mutex_lock(&taskQueueMutex);
    m_taskQueue.push(task);
    pthread_mutex_unlock(&taskQueueMutex);
}

void taskQueue::addTask(callback function,void* arg)
{
    pthread_mutex_lock(&taskQueueMutex);
    m_taskQueue.push(task_t(function,arg));
    pthread_mutex_unlock(&taskQueueMutex);
}

task_t taskQueue::getTask()
{
    pthread_mutex_lock(&taskQueueMutex);
    task_t task=m_taskQueue.front();
    m_taskQueue.pop();
    pthread_mutex_unlock(&taskQueueMutex);
    return task;
}