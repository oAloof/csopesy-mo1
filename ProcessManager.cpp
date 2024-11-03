#include "ProcessManager.h"
#include <iostream>
#include <iomanip>
#include <sstream>
#include <chrono>
#include "Utils.h"

void ProcessManager::createProcess(const std::string &name)
{
    if (name.empty())
    {
        throw std::runtime_error("Process name cannot be empty");
    }

    std::lock_guard<std::mutex> lock(processesMutex);

    if (processes.find(name) != processes.end())
    {
        throw std::runtime_error("Process with name '" + name + "' already exists");
    }

    try
    {
        auto process = std::make_shared<Process>(nextPID++, name);
        processes[name] = process;
        Scheduler::getInstance().addProcess(process);
    }
    catch (const std::exception &e)
    {
        std::cerr << "Failed to create process: " << e.what() << std::endl;
        throw;
    }
}

std::shared_ptr<Process> ProcessManager::getProcess(const std::string &name)
{
    std::lock_guard<std::mutex> lock(processesMutex);

    auto it = processes.find(name);
    if (it != processes.end())
    {
        return it->second;
    }

    return nullptr;
}

void ProcessManager::listProcesses()
{
    std::lock_guard<std::mutex> lock(processesMutex);

    int totalCores = Config::getInstance().getNumCPU();
    int activeCount = 0;
    for (const auto &pair : processes)
    {
        const auto &process = pair.second;
        if (process->getState() == Process::RUNNING)
        {
            activeCount++;
        }
    }

    std::cout << "CPU utilization: " << (activeCount * 100 / totalCores) << "%\n";
    std::cout << "Cores used: " << activeCount << "\n";
    std::cout << "Cores available: " << (totalCores - activeCount) << "\n\n";

    std::cout << "Running processes:\n";
    for (const auto &pair : processes)
    {
        const auto &process = pair.second;
        if (process->getState() == Process::RUNNING)
        {
            std::cout << process->getName()
                      << " (" << formatTimestamp(std::chrono::system_clock::now()) << ")   "
                      << "Core: " << process->getCPUCoreID() << "    "
                      << process->getCommandCounter() << " / " << process->getLinesOfCode() << "\n";
        }
    }

    std::cout << "\nFinished processes:\n";
    for (const auto &pair : processes)
    {
        const auto &process = pair.second;
        if (process->getState() == Process::FINISHED)
        {
            std::cout << process->getName()
                      << " (" << formatTimestamp(std::chrono::system_clock::now()) << ")   "
                      << "Finished    "
                      << process->getLinesOfCode() << " / " << process->getLinesOfCode() << "\n";
        }
    }
}

void ProcessManager::startBatchProcessing()
{
    if (!Config::getInstance().isInitialized())
    {
        throw std::runtime_error("System must be initialized before starting batch processing");
    }

    if (!batchProcessingActive)
    {
        batchProcessingActive = true;
        batchProcessThread = std::thread(&ProcessManager::batchProcessingLoop, this);
    }
}

void ProcessManager::stopBatchProcessing()
{
    if (batchProcessingActive)
    {
        batchProcessingActive = false;
        if (batchProcessThread.joinable())
        {
            batchProcessThread.join();
        }
    }
}

void ProcessManager::batchProcessingLoop()
{
    int processCounter = 1;
    lastProcessCreationCycle = 0;
    uint64_t batchFreq = Config::getInstance().getBatchProcessFreq();

    while (batchProcessingActive)
    {
        uint64_t currentCycle = Scheduler::getInstance().getCPUCycles();

        if (currentCycle > lastProcessCreationCycle + batchFreq)
        {
            std::ostringstream processName;
            processName << "p" << std::setfill('0') << std::setw(2) << processCounter++;

            try
            {
                createProcess(processName.str());
                lastProcessCreationCycle = currentCycle;
            }
            catch (const std::exception &e)
            {
                std::cerr << "Error creating batch process: " << e.what() << std::endl;
            }
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
}

std::string ProcessManager::generateProcessName() const
{
    std::ostringstream oss;
    oss << 'p' << std::setfill('0') << std::setw(2) << nextPID;
    return oss.str();
}