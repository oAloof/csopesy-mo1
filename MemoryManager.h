#ifndef MEMORY_MANAGER_H
#define MEMORY_MANAGER_H

#include <vector>
#include <map>
#include <memory>
#include <mutex>
#include <set>
#include <atomic>
#include "Process.h"

struct MemoryStatistics
{
    size_t totalMemory;
    size_t usedMemory;
    size_t freeMemory;
    size_t externalFragmentation;
    int processCount;
};

class MemoryManager
{
public:
    static MemoryManager &getInstance()
    {
        static MemoryManager instance;
        return instance;
    }

    // Core memory operations
    bool allocateMemory(std::shared_ptr<Process> process);
    void releaseMemory(const std::string &processName);
    void generateMemorySnapshot(uint32_t quantumCycle);

    // Memory status and statistics
    MemoryStatistics getMemoryStatistics() const;
    size_t getExternalFragmentation() const;
    int getProcessesInMemory() const { return processesInMemory.size(); }
    bool hasAvailableMemory() const { return !isMemoryFull(); }
    void printMemoryUsage() const;

private:
    MemoryManager();

    struct Frame
    {
        std::string processName;
        bool isFree{true};
        size_t startAddress{0};
        size_t endAddress{0};
    };

    struct ProcessMemoryInfo
    {
        size_t startFrame;
        size_t numFrames;
        size_t startAddress;
        size_t endAddress;
    };

    size_t totalFrames;
    size_t frameSize;
    size_t processSize;
    size_t framesPerProcess;
    std::map<std::string, ProcessMemoryInfo> processMemoryMap;
    std::vector<Frame> frames;
    mutable std::timed_mutex memoryMutex;
    std::atomic<size_t> currentFragmentation{0};
    std::set<std::string> processesInMemory;

    // Helper methods
    size_t findFirstFit() const;
    void printMemoryMap(std::ofstream &file) const;
    void updateFragmentation();
    void mergeAdjacentFreeFrames();
    size_t calculateRequiredFrames() const;
    bool isMemoryFull() const;
};

#endif