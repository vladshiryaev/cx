#pragma once

#include "lists.h"
#include "dirs.h"

struct Profile;

struct Config {
    Profile* profile = nullptr;
    const char* path = nullptr;
    StringList compilerOptions;
    StringList compilerCOptions;
    StringList compilerCppOptions;
    StringList linkerOptions;
    StringList externalLibs;
    StringList includeSearchPath;
    uint32_t cOptionsTag;
    uint32_t cxxOptionsTag;
    uint32_t linkerOptionsTag;
    bool load(const char* path, const char* configId = "default");
    bool parse(const char* path, const char* text, const char* configId = "default");
private:
    void afterParse();
};

static constexpr int maxConfigId = 32;

struct Profile {
    uint32_t tag;
    char id[maxConfigId];
    char version[128];
    char c[maxPath];
    char cxx[maxPath];
    char linker[maxPath];
    char librarian[maxPath];
    char symList[maxPath];
    Config commonConfig;
    Profile();
    void init();
};

