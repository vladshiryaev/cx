#pragma once

#include "lists.h" 

class Runner {
public:
    const char* currentDirectory = nullptr;
    StringList args;
    StringList output;
    int exitStatus = 0;
    Runner();
    ~Runner();
    bool run();
    void exec();
private:
};

