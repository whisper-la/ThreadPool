#include "threadpool.hpp"
#include <unistd.h>
#include <iostream>

using namespace std;

void taskFunc(void* arg)
{
    int* num = static_cast<int*>(arg);
    cout<<"thread "<<pthread_self()<<" is working, num is "<<*num<<endl;
}

int main(int argc, char const *argv[])
{
    cout<<"threadpool test"<<endl;
    threadPool<int> pool(5,10);
    // 向线程池中添加任务
    for(int i=0;i<100;i++)
    {
        int* num = new int(i+50);
        pool.addTask(task_t<int>(taskFunc,num));
    }
    sleep(10);
    return 0;
}


