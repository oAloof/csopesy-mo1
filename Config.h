#ifndef CONFIG_H
#define CONFIG_H

#include <string>
#include <stdexcept>
#include <cstdint>
#include <map>

class Config
{
public:
    // This allows only one instance of Config to exist
    static Config &getInstance()
    {
        static Config instance;
        return instance;
    }

    void loadConfig(const std::string &filename);
    bool isInitialized() const { return initialized; }

    // Getters
    int getNumCPU() const { return numCPU; }
    std::string getSchedulerType() const { return schedulerType; }
    uint32_t getQuantumCycles() const { return quantumCycles; }
    uint32_t getBatchProcessFreq() const { return batchProcessFreq; }
    uint32_t getMinInstructions() const { return minInstructions; }
    uint32_t getMaxInstructions() const { return maxInstructions; }
    uint32_t getDelaysPerExec() const { return delaysPerExec; }
    uint32_t getMaxOverallMem() const { return maxOverallMem; }
    uint32_t getMemPerFrame() const { return memPerFrame; }
    uint32_t getMemPerProc() const { return memPerProc; }

    // Exception class for Config
    class ConfigException : public std::runtime_error
    {
    public:
        ConfigException(const std::string &msg) : std::runtime_error(msg) {}
    };

private:
    Config() : initialized(false) {}

    int numCPU;                // Range: [1, 128]
    std::string schedulerType; // fcfs or rr
    uint32_t quantumCycles;    // Range: [1, 2^32]
    uint32_t batchProcessFreq; // Range: [1, 2^32]
    uint32_t minInstructions;  // Range: [1, 2^32]
    uint32_t maxInstructions;  // Range: [1, 2^32]
    uint32_t delaysPerExec;    // Range: [0, 2^32]
    bool initialized;

    uint32_t maxOverallMem{16384}; // 16KB
    uint32_t memPerFrame{16};      // 16 bytes per frame
    uint32_t memPerProc{4096};     // 4KB per proces

    void validateParameters();
};

#endif