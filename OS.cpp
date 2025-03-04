
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

class SimpleMutex {
private:
    atomic<bool> locked;

public:
    SimpleMutex() : locked(false) {}
    void lock() {
        while (locked.exchange(true, memory_order_acquire));
    }
    void unlock() {
        locked.store(false, memory_order_release);
    }
};

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class TaskQueue {
private:
    queue<void (*)()> tasks;
    SimpleMutex queue_mutex;
    bool stop;

public:
    TaskQueue() : stop(false) {}

    void pushTask(void (*task)()) {
        queue_mutex.lock();
        tasks.push(task);
        queue_mutex.unlock();
    }

    void (*popTask())() {
        queue_mutex.lock();
        while (tasks.empty() && !stop) {
            queue_mutex.unlock();
            this_thread::yield(); // Prevent busy waiting
            queue_mutex.lock();
        }
        if (tasks.empty()) {
            queue_mutex.unlock();
            return nullptr;
        }
        void (*task)() = tasks.front();
        tasks.pop();
        queue_mutex.unlock();
        return task;
    }

    void shutdown() {
        queue_mutex.lock();
        stop = true;
        queue_mutex.unlock();
    }
};

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class ThreadPool {
private:
    TaskQueue taskQueue;
    bool stop;
    atomic<int> totalTasksExecuted;
    atomic<int> totalThreadsAssigned;
    chrono::steady_clock::time_point startTime;

public:
    ThreadPool() : stop(false), totalTasksExecuted(0), totalThreadsAssigned(0) {
        startTime = chrono::steady_clock::now();
    }

   void addTask(void (*task)()) {
        totalThreadsAssigned.fetch_add(1, memory_order_relaxed);
        thread taskThread([this, task]() {
            this_thread::sleep_for(chrono::milliseconds(100)); // شبیه‌سازی اجرای وظیفه
            task();
            totalTasksExecuted.fetch_add(1, memory_order_relaxed);
            //cout << "Task executed in thread " << this_thread::get_id() << endl;
        });
        taskThread.detach();
    }

    void shutdown() {
        stop = true;
        taskQueue.shutdown();
    }

    void generateReport(int numofth) {
        auto endTime = chrono::steady_clock::now();
        chrono::duration<double> simulationDuration = endTime - startTime;
        cout << "\n--- Simulation Report ---\n";
        cout << "Total simulation time: " << simulationDuration.count() << " seconds\n";
        cout << "Total tasks executed: " << numofth << "\n";
        cout << "Total threads assigned: " << numofth << "\n";
        cout << "Number of unused threads: " << (totalThreadsAssigned - totalTasksExecuted) << "\n";
    }

    ~ThreadPool() {
        shutdown();
    }
};

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void readInputFiles(const string& commandsFile, const string& arrivalFile, const string& executionFile,
                    vector<vector<int>>& scenarioData, vector<vector<int>>& arrivalTimes,
                    vector<vector<int>>& executionTimes) {
    ifstream commands(commandsFile);
    if (!commands.is_open()) {
        cerr << "Error opening commands file.\n";
        exit(1);
    }
    int numScenarios;
    commands >> numScenarios;
    scenarioData.resize(numScenarios);
    arrivalTimes.resize(numScenarios);
    executionTimes.resize(numScenarios);
    for (int scenario = 0; scenario < numScenarios; ++scenario) {
        int numThreads, queueSize;
        commands >> numThreads >> queueSize;
        scenarioData[scenario] = {numThreads, queueSize};
    }
    commands.close();
    ifstream arrivalFileStream(arrivalFile);
    if (!arrivalFileStream.is_open()) {
        cerr << "Error opening arrival times file.\n";
        exit(1);
    }
    int arrivalTime;
    for (int scenario = 0; scenario < numScenarios; ++scenario) {
        arrivalTimes[scenario].clear();
        for (int task = 0; task < scenarioData[scenario][0]; ++task) {
            arrivalFileStream >> arrivalTime;
            arrivalTimes[scenario].push_back(arrivalTime);
        }
    }
    arrivalFileStream.close();
    ifstream executionFileStream(executionFile);
    if (!executionFileStream.is_open()) {
        cerr << "Error opening execution times file.\n";
        exit(1);
    }
    int executionTime;
    for (int scenario = 0; scenario < numScenarios; ++scenario) {
        executionTimes[scenario].clear();
        for (int task = 0; task < scenarioData[scenario][0]; ++task) {
            executionFileStream >> executionTime;
            executionTimes[scenario].push_back(executionTime);
        }
    }
    executionFileStream.close();
}

void readCommandsFile(const string& commandsFile, vector<int>& threadCounts) {
    ifstream commands(commandsFile);
    if (!commands.is_open()) {
        cerr << "Error opening commands file.\n";
        exit(1);
    }
    int numScenarios;
    commands >> numScenarios;
    threadCounts.resize(numScenarios);
    for (int i = 0; i < numScenarios; ++i) {
        commands >> threadCounts[i];
    }
    commands.close();
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////


void runScenario(int scenarioIndex, const vector<vector<int>>& scenarioData,
                 const vector<vector<int>>& arrivalTimes, const vector<vector<int>>& executionTimes ,int numth) {
    int numThreads = scenarioData[scenarioIndex][0];
    int queueSize = scenarioData[scenarioIndex][1];
    cout << "\n--- Running Scenario " << scenarioIndex + 1 << " ---\n";
    cout << "Number of Threads: " << numThreads << "\n";
    cout << "Queue Size: " << queueSize << "\n";
    ThreadPool pool;
    vector<thread> taskThreads;
    for (int i = 0; i < numThreads; ++i) {
        taskThreads.push_back(thread([arrivalTimes, executionTimes, i, scenarioIndex] {
            this_thread::sleep_for(chrono::seconds(arrivalTimes[scenarioIndex][i]));
            cout << "Task " << i << " started with execution time: " << executionTimes[scenarioIndex][i]
                 << " sec and arrival time: " << arrivalTimes[scenarioIndex][i] << " sec.\n";
            cout << "executed in thread " << this_thread::get_id() << ".\n\n";
            this_thread::sleep_for(chrono::seconds(executionTimes[scenarioIndex][i]));
            cout << "Task " << i << " completed\n";
        }));
    }
    for (auto& t : taskThreads) {
        t.join();
    }
    pool.generateReport(numth);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
int main() {
    string commandsFile = "commands.txt";
    string arrivalFile = "arrival_times.txt";
    string executionFile = "execution_times.txt";
    vector<vector<int>> scenarioData, arrivalTimes, executionTimes;
    vector<int> threadCounts;
    readCommandsFile(commandsFile, threadCounts);
    readInputFiles(commandsFile, arrivalFile, executionFile, scenarioData, arrivalTimes, executionTimes);
    for (int i = 0; i < scenarioData.size(); ++i) {
        runScenario(i, ref(scenarioData), ref(arrivalTimes), ref(executionTimes) ,threadCounts[i]);
    }
    return 0;
}


