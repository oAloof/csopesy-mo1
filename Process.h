#ifndef PROCESS_H
#define PROCESS_H

#include <string>
#include <vector>
#include <memory>
#include <atomic>
#include "ICommand.h"
#include "Config.h"
#include "PrintCommand.h"

class Process
{
public:
    enum ProcessState
    {
        READY,
        RUNNING,
        WAITING,
        FINISHED
    };

    // Constructor
    Process(int pid, const std::string &name);

    // Command management
    void addCommand(ICommand::CommandType commandType);
    void executeCurrentCommand(int coreID) const;
    void moveToNextLine();

    // Process status
    bool isFinished() const;
    int getCommandCounter() const;
    int getLinesOfCode() const;
    ProcessState getState() const;
    void setState(ProcessState state);
    std::chrono::system_clock::time_point getCreationTime() const { return creationTime; }

    // Core assignment
    void setCPUCoreID(int cpuCoreID);
    int getCPUCoreID() const;

    // Process identification
    int getPID() const;
    std::string getName() const;

    // Round Robin support
    void resetQuantumTime() { quantumTime = 0; }
    uint32_t getQuantumTime() const { return quantumTime.load(); }
    void incrementQuantumTime() { ++quantumTime; }

    // Process-smi command
    void displayProcessInfo() const;

private:
    // Basic process information
    const int pid;
    const std::string name;
    ProcessState state;
    int cpuCoreID;
    std::chrono::system_clock::time_point creationTime;

    // Command management
    std::vector<std::shared_ptr<ICommand>> commandList;
    std::atomic<int> commandCounter; // Made atomic for thread safety

    // Round Robin timing
    std::atomic<uint32_t> quantumTime; // Made atomic for thread safety

    // Helper methods
    int generateInstructionCount() const;
};

#endif