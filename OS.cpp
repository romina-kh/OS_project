#include <iostream>
#include <vector>
#include <queue>
#include <thread>
#include <atomic>
#include <condition_variable>
#include <functional>
#include <chrono>
#include <fstream>
#include <sstream>
#include <string>


using namespace std ;

class SimpleMutex 
{
private:

    atomic<bool> locked;
    
public:

    SimpleMutex() : locked(false) {}
    void lock() 
    {
        while (locked.exchange(true,memory_order_acquire));
    }
    void unlock() 
    {
        locked.store(false,memory_order_release);
    }
};

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class TaskQueue 
{
private:

    queue<function<void()>> tasks;
    SimpleMutex queue_mutex;
    condition_variable condition;
    bool stop;

public:

    TaskQueue() : stop(false) {}

    void pushTask(function<void()> task) 
    {
        queue_mutex.lock();
        tasks.push(task);
        queue_mutex.unlock();
        condition.notify_one();
    }

    function<void()> popTask() 
    {
        queue_mutex.lock();
        while (tasks.empty() && !stop) 
        {
            queue_mutex.unlock();
            this_thread::yield(); // Prevent busy waiting
            queue_mutex.lock();
        }
        if (tasks.empty()) 
        {
            queue_mutex.unlock();
            return nullptr;
        }
        function<void()> task = tasks.front();
        tasks.pop();
        queue_mutex.unlock();
        return task;
    }

    void shutdown() 
    {
        queue_mutex.lock();
        stop = true;
        queue_mutex.unlock();
    }
};

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class ThreadPool 
{
private:
    TaskQueue taskQueue;
    bool stop;
    int totalTasksExecuted;
    int totalThreadsAssigned;
    chrono::steady_clock::time_point startTime;

public:
    ThreadPool() : stop(false), totalTasksExecuted(0), totalThreadsAssigned(0) 
    {
        startTime = chrono::steady_clock::now();
    }

    void addTask(function<void()> task) 
    {
        totalThreadsAssigned++;
        auto start = chrono::steady_clock::now();
        thread taskThread([this, task, start]() 
        {
            this_thread::sleep_for(chrono::milliseconds(100)); // Simulate work
            task();
            auto end = chrono::steady_clock::now();
            chrono::duration<double> taskDuration = end - start;
            totalTasksExecuted++;

            cout << "Task executed in thread " << this_thread::get_id()
                 << " | Task duration: "       << taskDuration.count() << " seconds\n";
        });
        
        taskThread.detach();  // Detach the thread so it runs independently
    }

    void shutdown() 
    {
        stop = true;
        taskQueue.shutdown();
    }

    void generateReport() 
    {
        auto endTime = chrono::steady_clock::now();
        chrono::duration<double> simulationDuration = endTime - startTime;
       
        cout << "\n--- Simulation Report ---\n";
        cout << "Total simulation time: " << simulationDuration.count() << " seconds\n";
        cout << "Total tasks executed: " << totalTasksExecuted << "\n";
        cout << "Total threads assigned: " << totalThreadsAssigned << "\n";
        cout << "Number of unused threads: " << (totalThreadsAssigned - totalTasksExecuted) << "\n";
    }

    ~ThreadPool() 
    {
        shutdown();
    }
};

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// Helper function to read input files
void readInputFiles(const string& commandsFile, const string& arrivalFile, const string& executionFile,
                    vector<vector<int>>& scenarioData, vector<vector<int>>& arrivalTimes,
                    vector<vector<int>>& executionTimes) 
{

    // Open the commands file
    ifstream commands(commandsFile);
    if (!commands.is_open()) 
    {
        cerr << "Error opening commands file.\n";
        exit(1);
    }

    // Read the number of scenarios
    int numScenarios;
    commands >> numScenarios;
    scenarioData.resize(numScenarios);
    arrivalTimes.resize(numScenarios);
    executionTimes.resize(numScenarios);

    for (int scenario = 0; scenario < numScenarios; ++scenario) 
    {
         int numThreads, queueSize;
        commands >> numThreads >> queueSize;
        scenarioData[scenario] = {numThreads, queueSize};
    }

    commands.close();

    // Open the arrival times file
    ifstream arrivalFileStream(arrivalFile);
    if (!arrivalFileStream.is_open()) 
    {
        cerr << "Error opening arrival times file.\n";
        exit(1);
    }

    int arrivalTime;
    for (int scenario = 0; scenario < numScenarios; ++scenario) 
    {
        arrivalTimes[scenario].clear();
        for (int task = 0; task < scenarioData[scenario][0]; ++task) 
        {
            arrivalFileStream >> arrivalTime;
            arrivalTimes[scenario].push_back(arrivalTime);
        }
    }
    arrivalFileStream.close();

    // Open the execution times file
    ifstream executionFileStream(executionFile);
    if (!executionFileStream.is_open()) 
    {
        std::cerr << "Error opening execution times file.\n";
        exit(1);
    }

    int executionTime;
    for (int scenario = 0; scenario < numScenarios; ++scenario) 
    {
        executionTimes[scenario].clear();
        for (int task = 0; task < scenarioData[scenario][0]; ++task) 
        {
            executionFileStream >> executionTime;
            executionTimes[scenario].push_back(executionTime);
        }
    }
    executionFileStream.close();
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void runScenario(int scenarioIndex, const vector<vector<int>>& scenarioData,
                 const vector<vector<int>>& arrivalTimes, const vector<vector<int>>& executionTimes) 
{
    // Extract scenario data
    int numThreads = scenarioData[scenarioIndex][0];
    int queueSize = scenarioData[scenarioIndex][1];
    cout << "\n--- Running Scenario " << scenarioIndex + 1 << " ---\n";
    cout << "Number of Threads: " << numThreads << "\n";
    cout << "Queue Size: " << queueSize << "\n";

    // Create the thread pool for each scenario
    ThreadPool pool;

    // Simulate tasks for each scenario
    vector<thread> taskThreads;

    // Simulate tasks for each scenario
    for (int i = 0; i < numThreads; ++i) 
    {
        taskThreads.push_back(thread([arrivalTimes, executionTimes, i, scenarioIndex] 
        {
            this_thread::sleep_for(chrono::seconds(arrivalTimes[scenarioIndex][i])); // Simulate arrival delay

            cout << "Task " << i << " started with execution time: " << executionTimes[scenarioIndex][i] << " sec " 
                 << "and arrival time: " << arrivalTimes[scenarioIndex][i] << " sec.\n" ;
            cout << "executed in thread " << this_thread::get_id() << ".\n\n" ;
            this_thread::sleep_for(chrono::seconds(executionTimes[scenarioIndex][i])); // Simulate work
            cout << "Task " << i << " completed\n";
        }));
    }

    // Wait for all task threads to complete
    for (auto& t : taskThreads) 
    {
        t.join();
    }

    // Generate report after simulation
    pool.generateReport();
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

int main() 
{
    // Files containing input
    string commandsFile = "commands.txt";
    string arrivalFile = "arrival_times.txt";
    string executionFile = "execution_times.txt";

    // Vector to hold scenario data: [numThreads, queueSize]
    vector<std::vector<int>> scenarioData;
    vector<std::vector<int>> arrivalTimes;
    vector<std::vector<int>> executionTimes;

    // Read the input files
    readInputFiles(commandsFile, arrivalFile, executionFile, scenarioData, arrivalTimes, executionTimes);

    // Create a vector of threads for running scenarios sequentially
    for (int i = 0; i < scenarioData.size(); ++i) 
    {
        runScenario(i, ref(scenarioData), ref(arrivalTimes), ref(executionTimes));
    }

    return 0;
}

