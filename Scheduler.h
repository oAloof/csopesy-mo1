#ifndef SCHEDULER_H
#define SCHEDULER_H

#include <queue>
#include <thread>
#include <memory>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <vector>
#include "Process.h"
#include "Config.h"

class Scheduler
{
public:
    static Scheduler &getInstance()
    {
        static Scheduler instance;
        return instance;
    }

    Scheduler(const Scheduler &) = delete;
    Scheduler &operator=(const Scheduler &) = delete;

    void addProcess(std::shared_ptr<Process> process);
    void startScheduling();
    void stopScheduling();
    void getCPUUtilization() const;
    uint64_t getCPUCycles() const { return cpuCycles.load(); }

private:
    Scheduler();
    ~Scheduler()
    {
        stopScheduling();
    }

    std::atomic<bool> isInitialized{false};

    // Process queues
    std::queue<std::shared_ptr<Process>> readyQueue;
    std::vector<std::shared_ptr<Process>> runningProcesses;
    std::vector<std::shared_ptr<Process>> finishedProcesses;

    // Synchronization
    mutable std::mutex mutex;       // Mutex for queue access
    mutable std::mutex syncMutex;   // Mutex for synchronization between threads
    std::condition_variable cv;     // Condition variable for process availability
    std::condition_variable syncCv; // Condition variable for cycle synchronization
    std::atomic<int> coresWaiting{0};
    std::atomic<bool> processingActive{false};

    // CPU management
    std::vector<std::thread> cpuThreads;
    std::vector<bool> coreStatus;
    std::atomic<uint64_t> cpuCycles{0};

    // Core methods
    void executeProcesses();
    std::shared_ptr<Process> getNextProcess();
    std::shared_ptr<Process> roundRobinSchedule();
    std::shared_ptr<Process> fcfsSchedule();
    void handleQuantumExpiration(std::shared_ptr<Process> process);
    bool isQuantumExpired(const std::shared_ptr<Process> &process) const;
    void updateCoreStatus(int coreID, bool active);
    void incrementCPUCycles() { ++cpuCycles; }
    void waitForCycleSync();

    // For scheduler-test
    std::thread cycleCounterThread;
    std::atomic<bool> cycleCounterActive{false};
    void cycleCounterLoop();
};

#endif