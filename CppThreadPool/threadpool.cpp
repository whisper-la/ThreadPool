#include "threadpool.h"
#include "string.h"

using namespace std;


// 构造函数
threadPool::threadPool(int minThreadNum,int maxThreadNum)
{
    do
    {
        this->m_taskQueue = new taskQueue;
        if(this->m_taskQueue == nullptr)
        {
            perror("threadpool m_taskQueue malloc failed......\n");
            break;
        }
        this->threadArray= new pthread_t[maxThreadNum];
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
        pthread_create(&this->managerThread, NULL, managerFunc, this); // @todo

        // 创建工作线程组
        for(int i=0;i<minThreadNum;i++)
        {
            pthread_create(&this->threadArray[i], NULL, threadFunc, this);
        }
        cout<<"threadpool create success"<<endl;
        return ;
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
threadPool::~threadPool()
{
    // 先关闭线程池
    this->shutdown=true;
    // 阻塞回收管理者线程
    cout<<"threadpool destroy, managerThread is "<<this->managerThread<<endl;
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
    cout<<"threadpool destroy success"<<endl;
}

void threadPool::addTask(task_t task)
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

void threadPool::addTask(callback function,void* arg)
{
    if(this->shutdown)
    {
        return;
    }
    // 不需要加锁，因为任务队列已经有锁了
    // 添加任务
    this->m_taskQueue->addTask(function,arg);
    // 唤醒消费者线程
    pthread_cond_signal(&this->notEmpty);
}


int threadPool::getBusyThreadNum()
{
    int busyThreadNum=0;
    pthread_mutex_lock(&this->threadPoolMutex);
    busyThreadNum=this->busyThreadNum;
    pthread_mutex_unlock(&this->threadPoolMutex);
    return busyThreadNum;
}

// 获取存活线程数量
int threadPool::getLiveThreadNum()
{
    int liveThreadNum=0;
    pthread_mutex_lock(&this->threadPoolMutex);
    liveThreadNum=this->liveThreadNum;
    pthread_mutex_unlock(&this->threadPoolMutex);
    return liveThreadNum;
}

// 线程函数
void* threadPool::threadFunc(void* arg)
{
    threadPool* pool=static_cast<threadPool*>(arg);
    while(true)
    {
        // 加锁
        pthread_mutex_lock(&pool->threadPoolMutex);
        // 等待任务队列不为空
        while(pool->m_taskQueue->getTaskNum()==0&&!pool->shutdown)
        {
            // 等待任务队列不为空
            pthread_cond_wait(&pool->notEmpty, &pool->threadPoolMutex);

            // 退出线程
            if(pool->exitThreadNum>0)
            {
                pool->exitThreadNum--;
                if(pool->liveThreadNum>pool->minThreadNum)
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
        task_t task=pool->m_taskQueue->getTask();

        // 增加忙线程数
        pool->busyThreadNum++;
        // 解锁
        pthread_mutex_unlock(&pool->threadPoolMutex);
        // 执行任务
        cout<<"thread "<<pthread_self()<<" start work, busyThreadNum is "<<pool->busyThreadNum<<endl;
        task.function(task.arg);
        delete task.arg;
        task.arg=nullptr;

        // 加锁
        pthread_mutex_lock(&pool->threadPoolMutex);
        // 减少忙线程数
        pool->busyThreadNum--;
        cout<<"thread "<<pthread_self()<<" end work, busyThreadNum is "<<pool->busyThreadNum<<endl;
        // 解锁
        pthread_mutex_unlock(&pool->threadPoolMutex);
    }
    return nullptr;
}

void* threadPool::managerFunc(void* arg)
{
    threadPool* pool=static_cast<threadPool*>(arg);
    while(true)
    {
        // 线程 sleep 3s
        sleep(3);
        // 加锁
        pthread_mutex_lock(&pool->threadPoolMutex);
        // 获取队列大小
        int taskNum=pool->m_taskQueue->getTaskNum();
        // 获取存活线程数量
        int liveThreadNum=pool->liveThreadNum;
        // 获取忙线程数量
        int busyThreadNum=pool->busyThreadNum;
        // 解锁
        pthread_mutex_unlock(&pool->threadPoolMutex);

        const int number=2;
        // 添加线程
        if(liveThreadNum<=(pool->maxThreadNum-2)&&taskNum>liveThreadNum-busyThreadNum)
        {
            // 加锁
            pthread_mutex_lock(&pool->threadPoolMutex);
            
            int count;
            for(int i=0;i<pool->maxThreadNum&&liveThreadNum<pool->maxThreadNum&&count<number;i++)
            {
                if(pool->threadArray[i]==0)
                {
                    pthread_create(&pool->threadArray[i], NULL, threadFunc, pool);
                    liveThreadNum++;
                    count++;
                    break;
                }
            }
            pool->liveThreadNum=liveThreadNum;
            // 解锁
            pthread_mutex_unlock(&pool->threadPoolMutex);
        }
        // 销毁线程
        if(liveThreadNum>pool->minThreadNum&&busyThreadNum*2<liveThreadNum)
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


// 线程退出
void threadPool::threadExit()
{
    pthread_t threadID=pthread_self();
    for(int i=0;i<this->maxThreadNum;i++)
    {
        if(this->threadArray[i]==threadID)
        {
            this->threadArray[i]=0;
            cout<<"thread "<<threadID<<" exit"<<endl;
            break;
        }
    }
    pthread_exit(NULL);
}
