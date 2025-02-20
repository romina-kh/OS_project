
// comments downblow!
#include <iostream>
#include <thread>
#include <vector>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <atomic>

using namespace std;

class Task
{

public:
    virtual void execute() = 0;
    virtual ~Task() {}
};

class ThreadPool
{
private:
    vector<thread> workers;
    queue<Task*> tasks;
    mutex queueMutex;
    condition_variable condition;
    atomic<bool> stop;

public:
    ThreadPool(size_t numThreads) : stop(false)
    {
        for (int i = 0; i < numThreads; i++)
        {
            workers.emplace_back(&ThreadPool::workerThread, this);
        }
    }


    void enqueue(Task* task)
    {
        {
            unique_lock<mutex> lock(queueMutex);
            tasks.push(task);
        }

        condition.notify_one();
    }



    void finish()
    {
        {
            unique_lock<mutex> lock(queueMutex);
            stop = true;
        }

        condition.notify_all(); // Wake up all threads

        for (thread& worker : workers)
        {
            worker.join(); // Wait for all threads to finish
        }
    }

    void workerThread()
    {
        while (true)
        {
            Task* task;

            unique_lock<mutex> lock(queueMutex);
            condition.wait(lock, [this] { return stop || !tasks.empty(); });

            if (stop && tasks.empty()) return; // Exit if stopping

            task = tasks.front();
            tasks.pop();
           

            task->execute();
            delete task;
        }
    }
};

// Example for testing
class example : public Task
{
private:
    int taskId;
public:
    example(int id) : taskId(id) {}
    void execute() override {
        cout << "Executing task " << taskId << endl;
    }

};


int main()
{
    ThreadPool pool(5);

    // for (int i = 0; i < 5; ++i)
    // {
    //     pool.enqueue(new example(i));
    // }

    // this_thread::sleep_for(chrono::seconds(2));

    // pool.finish();

    return 0;
}


/* =====================================================================
mutex: only one thread accesses the queue
condition_variable: makes worker threads wait until new tasks arrive.
stop: an atomic boolean variable. It controls when the thread pool should continue running
===================================================================== */