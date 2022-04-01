#pragma once

#include "lists.h"
#include "dirs.h"
#include "compiler.h"
#include "config.h"
#include "async.h"
#include <mutex>

class Builder {
public:
    struct Options {
        bool force = false;
        bool keepDeps = false;
        bool skipRunning = false;
        bool skipLinking = false;
        StringList* runArgs = nullptr;
    };
    Options options;
    Builder() {};
    Builder(const Builder&) = delete;
    Builder& operator=(const Builder&) = delete;
    ~Builder();
    
    bool build(const char* path, const char* configId = nullptr);
    static bool clean(const char* path, const char* configId = nullptr);

private:
    
    char* currentDirectory = nullptr;
    Profile* profile = nullptr;
    Compiler* compiler = nullptr;
    Config config;
    char topPath[maxPath];
    char unitPath[maxPath];
    char sourceToRun[maxPath];
    FileStateList sources;
    FileStateDict unitDirDeps;
    FileStateDict libDeps;
    Builder* master = this;

    Batch batch;
    friend struct UpdateSourceJob;

    std::mutex mutex;
    FileStateDict fileStateCache;

    static const char* getConfigId(const char* configId);
    char* rebase(const char*, char*);
    uint64_t lookupFileTag(const char*, FileStateDict&);
    bool checkDeps(const char*, uint32_t toolTag, uint32_t optTag, Dependencies&);
    bool checkDeps(const char*, uint32_t toolTag, uint32_t optTag, uint64_t depsTag, uint8_t& flags);
    bool scanDirectory();
    bool processPath(const char*);
    bool loadProfile(const char* configId);
    bool loadConfig(const char* configId);
    bool updateSource(const char*, bool force, bool& recompiled, Dependencies&);
    void extractUnitDirDeps(Dependencies&);
    bool buildUnitDirDeps(uint64_t& depsTag);
    void addUnitLibDeps(FileStateList&);
};
