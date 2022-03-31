#include "runner.h"
#include "dirs.h"
#include "output.h"

#include <unistd.h>
#include <fcntl.h>
#include <sys/wait.h>

#include <cstdio>
#include <cstdlib>
#include <vector>

Runner::Runner() {}

Runner::~Runner() {}

static bool haveDir(const char* dir) {
    return dir && *dir && !(dir[0] == '.' && (dir[1] == 0 || (dir[1] == '/' && dir[2] == 0)));
}

static const char** prepareArgs(const StringList& args, const char* dir) {
    const char** argPtrs = new const char*[args.getCount() + 1];
    int argCount = 0;
    if (logLevel >= logLevelDebug) {
        Blob text;
        for (StringList::Iterator i(args); i; i.next()) {
            argPtrs[argCount++] = i->string;
            text.add(i->string, i->length);
            text.add(" ", 1);
        }
        text.data[text.size] = 0;
        if (text.size) {
            text.size--;
        }
        if (haveDir(dir)) {
            TRACE("Running in %s: %s", dir, text.data);
        }
        else {
            TRACE("Running: %s", text.data);
        }
    }
    else {
        for (StringList::Iterator i(args); i; i.next()) {
            argPtrs[argCount++] = i->string;
        }
    }
    argPtrs[argCount] = nullptr;
    return argPtrs;
}


static void doExec(const char** argPtrs, const char* currentDirectory) {
    if (haveDir(currentDirectory)) {
        changeDirectory(currentDirectory);
    }
    execvp(argPtrs[0], (char* const*)&argPtrs[0]);
    PANIC("Failed to run %s", argPtrs[0]);
}


void Runner::exec() {
    const char** argPtrs = prepareArgs(args, currentDirectory);
    doExec(argPtrs, currentDirectory);
}


bool Runner::run() {
    output.clear();
    if (args.isEmpty()) {
        return false;
    }
    int fd[2];
    if (pipe(fd) == -1)  {
        return false;
    }
    const char** argPtrs = prepareArgs(args, currentDirectory);
    pid_t pid = fork();
    if (pid == -1) {
        close(fd[0]);
        close(fd[1]);
        delete[] argPtrs;
        return false;
    }
    if (pid == 0) {
        // Child.
        close(fd[0]);
        dup2(fd[1], 1);
        dup2(fd[1], 2);
        close(fd[1]);
        doExec(argPtrs, currentDirectory);
    }
    else {
        // Parent.
        close(fd[1]);
        FILE* processOutput = fdopen(fd[0], "r");
        char* line = nullptr;
        size_t size = 0;
        int length;
        while ((length = getline(&line, &size, processOutput)) != -1) {
            if (length && line[length - 1] == '\n') {
                length--;
            }
            output.add(line, length);
        }
        free(line);
        fclose(processOutput);
        waitpid(pid, &exitStatus, 0);
        //exitStatus = WIFEXITED(exitStatus) ? WEXITSTATUS(exitStatus) : -1;
    }
    delete[] argPtrs;
    return true;
}


