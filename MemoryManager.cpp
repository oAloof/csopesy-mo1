#include "MemoryManager.h"
#include <fstream>
#include <sstream>
#include <iomanip>
#include "Utils.h"
#include "Config.h"
#include "Scheduler.h"
#include <iostream>
#include <direct.h>
#include <algorithm>

MemoryManager::MemoryManager()
{
    const auto &config = Config::getInstance();
    totalFrames = config.getMaxOverallMem() / config.getMemPerFrame();
    frames.resize(totalFrames);
    frameSize = config.getMemPerFrame();
    processSize = config.getMemPerProc();
    framesPerProcess = processSize / frameSize;
    updateFragmentation();
}

bool MemoryManager::allocateMemory(std::shared_ptr<Process> process)
{
    if (!process)
        return false;

    if (!memoryMutex.try_lock_for(std::chrono::milliseconds(100)))
    {
        return false;
    }

    std::lock_guard<std::timed_mutex> lock(memoryMutex, std::adopt_lock);

    size_t startFrame = findFirstFit();

    if (startFrame == frames.size())
    {
        if (Config::getInstance().getSchedulerType() == "rr")
        {
            process->setState(Process::READY);
            Scheduler::getInstance().addProcess(process);
        }
        return false;
    }

    // Allocate frames and track memory info
    ProcessMemoryInfo memInfo;
    memInfo.startFrame = startFrame;
    memInfo.numFrames = framesPerProcess;
    memInfo.startAddress = startFrame * frameSize;
    memInfo.endAddress = (startFrame + framesPerProcess) * frameSize - 1;

    for (size_t i = startFrame; i < startFrame + framesPerProcess; i++)
    {
        frames[i].isFree = false;
        frames[i].processName = process->getName();
        frames[i].startAddress = i * frameSize;
        frames[i].endAddress = (i + 1) * frameSize - 1;
    }

    processMemoryMap[process->getName()] = memInfo;
    processesInMemory.insert(process->getName());
    updateFragmentation();
    return true;
}

void MemoryManager::generateMemorySnapshot(uint32_t quantumCycle)
{
    std::lock_guard<std::timed_mutex> lock(memoryMutex);

    _mkdir("memory_stamps");

    std::stringstream filename;
    filename << "memory_stamps/memory_stamp_" << std::setw(2) << std::setfill('0')
             << quantumCycle << ".txt";

    std::ofstream file(filename.str());
    if (!file)
        return;

    auto stats = getMemoryStatistics();

    file << "Timestamp: " << getCurrentTimestamp() << "\n";
    file << "Number of processes in memory: " << stats.processCount << "\n";
    file << "Total external fragmentation in KB: "
         << stats.externalFragmentation / 1024 << "\n\n";

    printMemoryMap(file);
}

void MemoryManager::printMemoryMap(std::ofstream &file) const
{
    size_t totalMem = Config::getInstance().getMaxOverallMem();

    file << "----end---- = " << totalMem << "\n\n";

    // Sort processes by memory address for proper display
    std::vector<std::pair<size_t, const ProcessMemoryInfo *>> sortedProcesses;
    for (const auto &pair : processMemoryMap)
    {
        sortedProcesses.push_back({pair.second.startAddress, &pair.second});
    }
    std::sort(sortedProcesses.begin(), sortedProcesses.end(),
              std::greater<std::pair<size_t, const ProcessMemoryInfo *>>());

    // Print each process's memory boundaries
    for (const auto &pair : sortedProcesses)
    {
        const ProcessMemoryInfo *info = pair.second;
        file << info->endAddress + 1 << "\n";

        // Find process name
        std::string processName;
        for (size_t i = info->startFrame; i < info->startFrame + info->numFrames; i++)
        {
            if (!frames[i].processName.empty())
            {
                processName = frames[i].processName;
                break;
            }
        }
        file << processName << "\n";
        file << info->startAddress << "\n\n";
    }

    file << "----start---- = 0\n";
}

size_t MemoryManager::getExternalFragmentation() const
{
    std::lock_guard<std::timed_mutex> lock(memoryMutex, std::adopt_lock);
    size_t fragmentation = 0;
    size_t consecutiveFree = 0;

    for (size_t i = 0; i < frames.size(); i++)
    {
        if (frames[i].isFree)
        {
            consecutiveFree++;
        }
        else if (consecutiveFree > 0)
        {
            if (consecutiveFree < framesPerProcess)
            {
                fragmentation += consecutiveFree * frameSize;
            }
            consecutiveFree = 0;
        }
    }

    if (consecutiveFree > 0 && consecutiveFree < framesPerProcess)
    {
        fragmentation += consecutiveFree * frameSize;
    }

    return fragmentation;
}

MemoryStatistics MemoryManager::getMemoryStatistics() const
{
    std::lock_guard<std::timed_mutex> lock(memoryMutex, std::adopt_lock);
    MemoryStatistics stats;
    stats.totalMemory = Config::getInstance().getMaxOverallMem();
    stats.usedMemory = processMemoryMap.size() * processSize;
    stats.processCount = processMemoryMap.size();
    stats.freeMemory = stats.totalMemory - stats.usedMemory;
    stats.externalFragmentation = getExternalFragmentation();

    return stats;
}

void MemoryManager::printMemoryUsage() const
{
    auto stats = getMemoryStatistics();
    std::cout << "Memory Usage:\n"
              << "Total Memory: " << stats.totalMemory / 1024 << "KB\n"
              << "Used Memory: " << stats.usedMemory / 1024 << "KB\n"
              << "Free Memory: " << stats.freeMemory / 1024 << "KB\n"
              << "External Fragmentation: " << stats.externalFragmentation / 1024 << "KB\n"
              << "Processes in Memory: " << stats.processCount << "\n";
}

size_t MemoryManager::findFirstFit() const
{
    size_t consecutiveFree = 0;
    size_t startFrame = 0;

    for (size_t i = 0; i < frames.size(); i++)
    {
        if (frames[i].isFree)
        {
            if (consecutiveFree == 0)
                startFrame = i;
            consecutiveFree++;
            if (consecutiveFree == framesPerProcess)
                return startFrame;
        }
        else
        {
            consecutiveFree = 0;
        }
    }

    return frames.size(); // No suitable space found
}

void MemoryManager::releaseMemory(const std::string &processName)
{
    std::lock_guard<std::timed_mutex> lock(memoryMutex);

    auto processInfo = processMemoryMap.find(processName);
    if (processInfo != processMemoryMap.end())
    {
        size_t startFrame = processInfo->second.startFrame;
        for (size_t i = 0; i < processInfo->second.numFrames; i++)
        {
            frames[startFrame + i].isFree = true;
            frames[startFrame + i].processName.clear();
        }

        processMemoryMap.erase(processName);
        processesInMemory.erase(processName);
        updateFragmentation();
    }
}

void MemoryManager::updateFragmentation()
{
    currentFragmentation.store(getExternalFragmentation());
}

size_t MemoryManager::calculateRequiredFrames() const
{
    return framesPerProcess;
}

bool MemoryManager::isMemoryFull() const
{
    return findFirstFit() == frames.size();
}

void MemoryManager::mergeAdjacentFreeFrames()
{
    updateFragmentation();
}