#include "Process.h"
#include <iostream>
#include "PrintCommand.h"
#include <random>
#include <chrono>
#include <thread>
#include <iomanip>
#include "Utils.h"

Process::Process(int pid, const std::string &name)
    : pid(pid),
      name(name),
      state(READY),
      cpuCoreID(-1),
      commandCounter(0),
      quantumTime(0),
      creationTime(std::chrono::system_clock::now())
{
    // Generate random number of instructions based on config
    int numInstructions = generateInstructionCount();

    // Initialize with dummy instructions
    for (int i = 0; i < numInstructions; ++i)
    {
        addCommand(ICommand::PRINT);
    }
}

void Process::addCommand(ICommand::CommandType commandType)
{
    if (commandType == ICommand::PRINT)
    {
        // std::string output = "Output from process " + name + " (PID: " + std::to_string(pid) + ")";
        // auto command = std::make_shared<PrintCommand>(pid, output);
        // commandList.push_back(command);
        auto command = std::make_shared<PrintCommand>(pid, name);
        commandList.push_back(command);
    }
}

void Process::executeCurrentCommand(int coreID) const
{
    if (commandCounter < commandList.size())
    {
        try
        {
            commandList[commandCounter]->execute();
        }
        catch (const std::exception &e)
        {
            std::cerr << "Error executing command in process " << pid
                      << ": " << e.what() << std::endl;
        }
    }
}

void Process::moveToNextLine()
{
    if (commandCounter < commandList.size())
    {
        ++commandCounter;
    }
}

bool Process::isFinished() const
{
    return commandCounter >= commandList.size();
}

int Process::generateInstructionCount() const
{
    auto &config = Config::getInstance();
    int min = config.getMinInstructions();
    int max = config.getMaxInstructions();

    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(min, max);

    return dis(gen);
}

void Process::displayProcessInfo() const
{
    std::cout << "\n"
              << name << " ("
              << formatTimestamp(creationTime) << ") ";

    if (state == FINISHED)
    {
        std::cout << "Finished   "
                  << getLinesOfCode() << " / " << getLinesOfCode() << "\n";
    }
    else
    {
        std::cout << "Core: " << cpuCoreID << "    "
                  << getCommandCounter() << " / " << getLinesOfCode() << "\n";
    }
}

// Getters and setters
int Process::getPID() const { return pid; }
std::string Process::getName() const { return name; }
Process::ProcessState Process::getState() const { return state; }
void Process::setState(ProcessState newState) { state = newState; }

void Process::setCPUCoreID(int id)
{
    if (id >= -1 && id < Config::getInstance().getNumCPU())
    {
        cpuCoreID = id;
    }
    else
    {
        throw std::invalid_argument("Invalid CPU core ID");
    }
}

int Process::getCPUCoreID() const { return cpuCoreID; }
int Process::getCommandCounter() const { return commandCounter.load(); }
int Process::getLinesOfCode() const { return commandList.size(); }