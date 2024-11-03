#include "Scheduler.h"
#include <iostream>
#include <iomanip>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <thread>
#include <ctime>
#include "Utils.h"

Scheduler::Scheduler()
{
    size_t numCPUs = Config::getInstance().getNumCPU();
    coreStatus.resize(numCPUs, false);
}

void Scheduler::startScheduling()
{
    if (isInitialized)
        return;

    processingActive = true;
    isInitialized = true;

    // Reset CPU cycles
    cpuCycles.store(0);

    int numCPUs = Config::getInstance().getNumCPU();
    for (int i = 0; i < numCPUs; ++i)
    {
        cpuThreads.emplace_back(&Scheduler::executeProcesses, this);
    }

    // Start the cycle counter thread
    cycleCounterActive = true;
    cycleCounterThread = std::thread(&Scheduler::cycleCounterLoop, this);
}

void Scheduler::stopScheduling()
{
    processingActive = false;
    cycleCounterActive = false;
    cv.notify_all();

    for (auto &thread : cpuThreads)
    {
        if (thread.joinable())
        {
            thread.join();
        }
    }
    cpuThreads.clear();

    // Stop the cycle counter thread
    if (cycleCounterThread.joinable())
    {
        cycleCounterThread.join();
    }
}

void Scheduler::addProcess(std::shared_ptr<Process> process)
{
    {
        std::lock_guard<std::mutex> lock(mutex);
        readyQueue.push(process);
    }
    cv.notify_one();
}

void Scheduler::executeProcesses()
{
    while (processingActive)
    {
        std::shared_ptr<Process> currentProcess = nullptr;
        {
            std::unique_lock<std::mutex> lock(mutex);
            cv.wait(lock, [this]
                    { return !readyQueue.empty() || !processingActive; });
            if (!processingActive)
                break;
            currentProcess = getNextProcess();
        }

        if (currentProcess)
        {
            currentProcess->setState(Process::RUNNING);
            int delays = Config::getInstance().getDelaysPerExec();
            int currentDelay = 0;

            while (!currentProcess->isFinished())
            {
                if (Config::getInstance().getSchedulerType() == "rr" &&
                    currentProcess->getQuantumTime() >= Config::getInstance().getQuantumCycles())
                {
                    break;
                }

                // Handle delay cycles
                if (currentDelay < delays)
                {
                    currentDelay++;
                }
                else
                {
                    // Execute instruction without holding the mutex
                    currentProcess->executeCurrentCommand(currentProcess->getCPUCoreID());
                    currentProcess->moveToNextLine();
                    currentDelay = 0;

                    if (Config::getInstance().getSchedulerType() == "rr")
                    {
                        currentProcess->incrementQuantumTime();
                    }
                }

                // Synchronize with other threads
                waitForCycleSync();
            }

            // Update process state and lists with proper locking
            {
                std::lock_guard<std::mutex> lock(mutex);
                if (currentProcess->isFinished())
                {
                    currentProcess->setState(Process::FINISHED);
                    finishedProcesses.push_back(currentProcess);
                }
                else
                {
                    currentProcess->setState(Process::READY);
                    currentProcess->resetQuantumTime();
                    readyQueue.push(currentProcess);
                }
                auto it = std::find(runningProcesses.begin(), runningProcesses.end(), currentProcess);
                if (it != runningProcesses.end())
                    runningProcesses.erase(it);
                updateCoreStatus(currentProcess->getCPUCoreID(), false);
            }
        }
        else
        {
            // No current process, still need to sync cycles
            waitForCycleSync();
        }
    }
}

std::shared_ptr<Process> Scheduler::getNextProcess()
{
    // Mutex should already be locked by the caller
    if (readyQueue.empty())
        return nullptr;

    int availableCore = -1;
    for (size_t i = 0; i < coreStatus.size(); ++i)
    {
        if (!coreStatus[i])
        {
            availableCore = static_cast<int>(i);
            break;
        }
    }

    if (availableCore == -1)
        return nullptr;

    std::shared_ptr<Process> nextProcess;
    if (Config::getInstance().getSchedulerType() == "rr")
    {
        nextProcess = roundRobinSchedule();
    }
    else
    {
        nextProcess = fcfsSchedule();
    }

    if (nextProcess)
    {
        nextProcess->setCPUCoreID(availableCore);
        coreStatus[availableCore] = true;
        runningProcesses.push_back(nextProcess);
    }

    return nextProcess;
}

std::shared_ptr<Process> Scheduler::fcfsSchedule()
{
    if (readyQueue.empty())
    {
        return nullptr;
    }

    auto process = readyQueue.front();
    readyQueue.pop();
    return process;
}

std::shared_ptr<Process> Scheduler::roundRobinSchedule()
{
    if (readyQueue.empty())
        return nullptr;

    auto process = readyQueue.front();
    readyQueue.pop();

    if (!isQuantumExpired(process))
    {
        return process;
    }

    handleQuantumExpiration(process);
    return nullptr; // Let the next cycle handle the next process
}

bool Scheduler::isQuantumExpired(const std::shared_ptr<Process> &process) const
{
    return process->getQuantumTime() >= Config::getInstance().getQuantumCycles();
}

void Scheduler::handleQuantumExpiration(std::shared_ptr<Process> process)
{
    process->resetQuantumTime();
    process->setState(Process::READY);
    readyQueue.push(process);
}

void Scheduler::getCPUUtilization() const
{
    std::lock_guard<std::mutex> lock(mutex);
    std::stringstream report;

    int totalCores = Config::getInstance().getNumCPU();
    int usedCores = runningProcesses.size();

    report << "CPU utilization: " << (usedCores * 100 / totalCores) << "%\n";
    report << "Cores used: " << usedCores << "\n";
    report << "Cores available: " << (totalCores - usedCores) << "\n\n";

    report << "Running processes:\n";
    for (const auto &process : runningProcesses)
    {
        report << process->getName()
               << " (" << formatTimestamp(std::chrono::system_clock::now()) << ")   "
               << "Core: " << process->getCPUCoreID() << "    "
               << process->getCommandCounter() << " / " << process->getLinesOfCode() << "\n";
    }

    report << "\nFinished processes:\n";
    for (const auto &process : finishedProcesses)
    {
        report << process->getName()
               << " (" << formatTimestamp(std::chrono::system_clock::now()) << ")   "
               << "Finished    "
               << process->getLinesOfCode() << " / " << process->getLinesOfCode() << "\n";
    }

    std::cout << report.str();

    std::ofstream logFile("csopesy-log.txt", std::ios::app);
    if (logFile.is_open())
    {
        logFile << report.str() << "\n";
        logFile.close();
        std::cout << "Report generated at c:/csopesy-log.txt\n";
    }
}

void Scheduler::waitForCycleSync()
{
    std::unique_lock<std::mutex> lock(syncMutex);
    coresWaiting++;

    int activeCores = std::max(1, static_cast<int>(runningProcesses.size()));

    if (coresWaiting >= activeCores)
    {
        coresWaiting = 0;
        incrementCPUCycles();
        cv.notify_all();
    }
    else
    {
        cv.wait(lock, [this]
                { return coresWaiting == 0; });
    }
}

void Scheduler::updateCoreStatus(int coreID, bool active)
{
    std::lock_guard<std::mutex> lock(mutex);
    if (coreID >= 0 && coreID < static_cast<int>(coreStatus.size()))
    {
        coreStatus[coreID] = active;
    }
}

void Scheduler::cycleCounterLoop()
{
    // This now acts as a backup to ensure cycles increment even when no cores are active
    while (cycleCounterActive)
    {
        std::this_thread::sleep_for(std::chrono::microseconds(100));
        if (runningProcesses.empty())
        {
            incrementCPUCycles();
        }
    }
}