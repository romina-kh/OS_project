#include <iostream>
#include <vector>
#include <queue>
#include <thread>
#include <atomic>
#include <chrono>
#include <fstream>
#include <sstream>
#include <string>

using namespace std;

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

struct TaskArgs
{
    int taskId = -1;
    int arrivalTime = 0;
    int executionTime = 0;
};

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void exeTaskStatic(int taskId, int arrivalTime, int executionTime)
{
    this_thread::sleep_for(chrono::seconds(arrivalTime));

    cout << "               Task " << taskId << ": started with " << "\n"
    << "                       execution time --> "   << executionTime << " sec.\n";
    cout << "                       arrival time   --> " << arrivalTime << " sec.\n";
    cout << "                       Executing in thread (" << this_thread::get_id() << ").\n";
    this_thread::sleep_for(chrono::seconds(executionTime));
    cout << "               Task " << taskId << ": completed.\n";
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

class SimpleMutex
{

private:
    atomic<bool> locked;

public:
    SimpleMutex() : locked(false) {}

    void lock()
    {
        while (locked.exchange(true, memory_order_acquire))
        {
            this_thread::yield(); // its for busy waiting
        }
    }
    void unlock()
    {
        locked.store(false, memory_order_release);
    }
};

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

class TaskQueue
{

private:
    queue<TaskArgs> tasks;
    SimpleMutex mtx;
    bool stop;
    int MaxSize ;

public:
    TaskQueue(int qsize) : stop(false) , MaxSize(qsize){}

    void pushTask(TaskArgs task)
    {
        //mtx.lock();
        //tasks.push(task);
        //mtx.unlock();
        while (true)
        {
            mtx.lock();
            if (tasks.size() < MaxSize)
            {
                tasks.push(task);
                mtx.unlock();
                break;
            }
            mtx.unlock();
            this_thread::yield();
        }
    }

    TaskArgs popTask()
    {
        while (true)
        {
            mtx.lock();
            if (!tasks.empty())
            {
                TaskArgs task = tasks.front();
                tasks.pop();
                mtx.unlock();
                return task;
            }
            else if (stop)
            {
                mtx.unlock();
                return TaskArgs(); // Return an empty task
            }
            mtx.unlock();
            this_thread::yield(); //  again for busy waiting
        }
    }

    void shutdown()
    {
        mtx.lock();
        stop = true;
        mtx.unlock();
    }
};

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

class ThreadPool
{

private:
    vector<thread> workers;
    TaskQueue taskQueue;
    atomic<bool> stop;
    atomic<int> totalTExe;
    atomic<int> totalThsign;
    chrono::steady_clock::time_point startTime;

public:
    ThreadPool(int numThreads , int qsize): stop(false), totalTExe(0), totalThsign(0), taskQueue(qsize)
    {
        startTime = chrono::steady_clock::now();

        // Create workers
        for (int i = 0; i < numThreads; ++i)
        {
            workers.emplace_back([this]()
            {
                while (!stop)
                {
                    TaskArgs task = taskQueue.popTask();
                    if (task.taskId != -1)
                    {
                        exeTaskStatic(task.taskId, task.arrivalTime, task.executionTime);
                        totalTExe.fetch_add(1, memory_order_relaxed);
                    }
                }
            });
        }
    }

    void addTask(TaskArgs task)
    {
        totalThsign.fetch_add(1, memory_order_relaxed);
        taskQueue.pushTask(task);
    }

    void wait_to_finish()
    {
        while (totalTExe.load(memory_order_relaxed) < totalThsign.load(memory_order_relaxed))
        {
            this_thread::yield();
        }
    }

    void shutdown()
    {
        stop = true;
        taskQueue.shutdown();
        for (auto& worker : workers)
        {
            if (worker.joinable())
            {
                worker.join();
            }
        }
    }

    void Report()
    {
        auto endTime = chrono::steady_clock::now();
        chrono::duration<double> simulation = endTime - startTime;
cout << "             ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^" << "\n" ;
        cout << "\n                        <<<  Simulation Report  >>> \n\n";
        cout << "                   Total  simulation time   --> " << simulation.count() << " seconds\n";
        cout << "                   Total  tasks executed    --> " << totalTExe.load(memory_order_relaxed) << "\n";
        cout << "                   Total  threads assigned  --> " << totalThsign.load(memory_order_relaxed) << "\n";
        cout << "                   Number of unused threads --> " << (totalThsign.load(memory_order_relaxed) - totalTExe.load(memory_order_relaxed)) << "\n";
    }

    ~ThreadPool()
    {
        shutdown();
    }
};

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void readInputFiles(const string& commandsF, const string& arrivalF, const string& exeF,vector<vector<int>>& senData,
                                                          vector<vector<int>>& arrivalT, vector<vector<int>>& exeT)
{
    ifstream commands(commandsF);
    if (!commands.is_open())
    {
        cerr << "!!! Error opening commands file. !!!\n";
        exit(1);
    }

    int numSen;
    commands >> numSen;
    senData.resize(numSen);
    arrivalT.resize(numSen);
    exeT.resize(numSen);

    for (int scenario = 0; scenario < numSen; ++scenario)
    {
        int numThreads, queueSize;
        commands >> numThreads >> queueSize;
        senData[scenario] = {numThreads, queueSize};
    }
    commands.close();

    ifstream arrivalFile(arrivalF);
    if (!arrivalFile.is_open())
    {
        cerr << "!!! Error opening arrival times file.!!!\n";
        exit(1);
    }

    for (int scenario = 0; scenario < numSen; ++scenario)
    {
        arrivalT[scenario].clear();
        for (int task = 0; task < senData[scenario][0]; ++task)
        {
            int arrivalTime;
            arrivalFile >> arrivalTime;
            arrivalT[scenario].push_back(arrivalTime);
        }
    }
    arrivalFile.close();

    ifstream executionFile(exeF);
    if (!executionFile.is_open())
    {
        cerr << "Error opening execution times file.\n";
        exit(1);
    }
    for (int scenario = 0; scenario < numSen; ++scenario)
    {
        exeT[scenario].clear();

        for (int task = 0; task < senData[scenario][0]; ++task)
        {
            int executionTime;
            executionFile >> executionTime;
            exeT[scenario].push_back(executionTime);
        }
    }
    executionFile.close();
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void runSenario(int senIndex, const vector<vector<int>>& senData,const vector<vector<int>>& arrivalTimes,
                                                                 const vector<vector<int>>& executionTimes)

{
    int numThreads = senData[senIndex][0];
    int qSize      = senData[senIndex][1];

    cout << "\n" <<"********************************************************************************";
    cout << "\n                    ---<<< Running Scenario (" << senIndex + 1 << ") >>>---\n\n";
    cout << "                       Number of Threads --> " << numThreads << "\n";
    cout << "                       Queue Size --> " << qSize << "\n\n";

    ThreadPool pool(numThreads , qSize);

    for (int i = 0; i < arrivalTimes[senIndex].size(); ++i)
    {
        int arriveT = arrivalTimes[senIndex][i];
        int exeT    = executionTimes[senIndex][i];

        TaskArgs task = {i, arriveT, exeT};
        pool.addTask(task);
    }

    pool.wait_to_finish();
    pool.Report();
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

int main()
{
    string commandF = "commands.txt";
    string arrivalF = "arrival_times.txt";
    string exeF     = "execution_times.txt";

    vector<vector<int>> senarioD, arrTimes, exeTimes;
    readInputFiles(commandF, arrivalF, exeF, senarioD, arrTimes, exeTimes);

    for (int i = 0; i < senarioD.size(); ++i)
    {
        runSenario(i, senarioD, arrTimes, exeTimes);
    }

    return 0;
}
