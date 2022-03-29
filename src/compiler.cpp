#include "compiler.h"
#include "runner.h"
#include "dirs.h"
#include "hash.h"
#include "output.h"
#include <cstring>

const char* cacheDirName = ".cx.cache";


FileType getFileType(const char* path) {
    const char* ext = getSuffix(path);
    if (*ext == '.') {
        int extLength = strlen(ext);
        switch (extLength) {
            case 2:
                switch (ext[1]) {
                    case 'c': return typeCSource;
                    case 'C': return typeCppSource;
                    case 'h':
                    case 'H': return typeHeader;
                }
                break;
            case 3:
                switch (ext[1]) {
                    case 'c':
                        if (ext[2] == 'c' || ext[2] == 'p') {
                            return typeCppSource;
                        }
                        break;
                }
            case 4:
                switch (ext[1]) {
                    case 'C':
                        if (ext[2] == 'P' && ext[3] == 'P') {
                            return typeCppSource;
                        }
                        break;
                    case 'c':
                        if (
                            (ext[2] == 'p' && ext[3] == 'p') ||
                            (ext[2] == 'x' && ext[3] == 'x') ||
                            (ext[2] == '+' && ext[3] == '+')
                        ) {
                            return typeCppSource;
                        }
                        break;
                    case 'h':
                        if (
                            (ext[2] == 'p' && ext[3] == 'p') ||
                            (ext[2] == 'x' && ext[3] == 'x') ||
                            (ext[2] == '+' && ext[3] == '+')
                        ) {
                            return typeHeader;
                        }
                        break;
                    case 'H':
                        if (
                            (ext[2] == 'P' && ext[3] == 'P') ||
                            (ext[2] == 'X' && ext[3] == 'X') ||
                            (ext[2] == '+' && ext[3] == '+')
                        ) {
                            return typeCppSource;
                        }
                        break;
                }
        }
    }
    return typeUnknown;
}

char* makeDerivedPath(const char* source, const char* suffix, char* derived) {
    const char* lastSlash = nullptr;
    const char* p = source;
    char temp[maxPath];
    for ( ; *p; p++) {
        if (*p == '/') {
            lastSlash = p;
        }
    }
    int pos = 0;
    if (lastSlash) {
        pos = lastSlash + 1 - source;
        memcpy(temp, source, pos);
    }
    int len = strlen(cacheDirName);
    memcpy(temp + pos, cacheDirName, len);
    pos += len;
    temp[pos++] = '/';
    if (lastSlash) {
        memcpy(temp + pos, lastSlash + 1, p - lastSlash - 1);
        pos += p - lastSlash - 1;
    }
    else {
        memcpy(temp + pos, source, p - source);
        pos += p - source;
    }
    strcpy(temp + pos, suffix);
    return normalizePath(temp, derived);
}


Compiler::~Compiler() {}


GccCompiler::GccCompiler(Profile& p): Compiler(p) {
    Runner runner;
    runner.args.add(profile.c);
    runner.args.add("-dumpfullversion");
    if (runner.run() && runner.exitStatus == 0 && !runner.output.isEmpty()) {
        StringList::Iterator i(runner.output);
        if (i->length + 1 <= sizeof(profile.version)) {
            memcpy(profile.version, i->data, i->length + 1);
            TRACE("gcc %s", profile.version);
            uint32_t versionHash = hash(i->data, i->length);
            profile.init();
            profile.tag += versionHash;
            return;
        }
    }
    PANIC("Cannot run %s", profile.c);
}


static void skipGccDepSpaces(const char*& p) {
   for (;;) {
       if (*p == ' ' || *p == '\t') {
           p++;
           continue;
       }
       if (*p == '\\') {
           p++;
           if (*p == '\r') {
               p++;
               if (*p == '\n') {
                   p++;
               }
               continue;
           }
           else if (*p == '\n') {
               p++;
               continue;
           }
           else {
               break;
           }
       }
       else {
           break;
       }
   }
}


static bool parseGccDepPath(const char*& p, char* name, int& length) {
   skipGccDepSpaces(p);
   length = 0;
   for (;;) {
       if (*p == 0 || *p == ' ' || *p == '\r' || *p == '\n' || *p == ':') {
           break;
       }
       if (*p == '\\') {
           if (p[1] == ' ') {
               p++;
           }
       }
       name[length++] = *p++;
       if (length >= maxPath - 1) {
           return false;
       }
   }
   name[length] = 0;
   return length != 0;
}


bool GccCompiler::convertGccDeps(
    const char* unitPath,
    const char* gccDepsPath,
    const char* depsPath,
    bool hasMain,
    uint32_t optTag,
    Dependencies& deps
) {
    char absGccDepsPath[maxPath];
    char absDepsPath[maxPath];
    rebasePath(unitPath, gccDepsPath, absGccDepsPath);
    rebasePath(unitPath, depsPath, absDepsPath);
    Blob source;
    if (!source.load(absGccDepsPath)) {
        FAILURE("Compiler is expected to produce %s file", absGccDepsPath);
        deleteFile(absDepsPath);
        return false;
    }
    const char* p = source.data;
    char name[maxPath];
    int length;
    if (!parseGccDepPath(p, name, length) || (skipGccDepSpaces(p), *p != ':')) {
        FAILURE("Bad format of %s make dependency file", absGccDepsPath);
        deleteFile(absDepsPath);
        return false;
    }
    p++;
    deps.clear();
    while (parseGccDepPath(p, name, length)) {
        char absName[maxPath];
        rebasePath(unitPath, name, absName);
        deps.add(makeFileTag(absName), name, length);
    }
    if (!keepDeps) {
        deleteFile(absGccDepsPath);
    }
    DepsHeader& header = deps.getHeader();
    header.toolTag = profile.tag;
    header.optTag = optTag,
    header.inputsTag = 0;
    header.flags = 0;
    if (hasMain) {
        header.flags |= flagHasMain;
    }
    return deps.save(absDepsPath);
}


static bool isValidGccOption(const char* opt, int len) {
    if (len != 2 || opt[0] != '-' || !(opt[1] == 'c' || opt[1] == 'o' || opt[1] == 'S' || opt[1] == 'E')) {
        return true;
    }
    FAILURE("Option %s is not allowed", opt);
    return false;
}


bool GccCompiler::compile(const Config& config, const char* sourcePath, Dependencies& deps) {
    char absSourcePath[maxPath];
    INFO("%s", rebasePath(config.path, sourcePath, absSourcePath));
    char objPath[maxPath];
    char gccDepsPath[maxPath];
    char depsPath[maxPath];
    makeDerivedPath(sourcePath, ".o", objPath);
    makeDerivedPath(sourcePath, ".d", gccDepsPath);
    addSuffix(objPath, ".deps", depsPath);
    Runner runner;
    runner.currentDirectory = config.path;
    FileType type = getFileType(sourcePath);
    runner.args.add(type == typeCppSource ? profile.cxx : profile.c);
    runner.args.add("-fdiagnostics-color=always");
    runner.args.add("-MMD"); // -MD
    for (StringList::Iterator i(config.includeSearchPath); i; i.next()) {
        char inc[maxPath + 16];
        int len = sprintf(inc, "-I%s", i->data);
        runner.args.add(inc, len);
    }
    runner.args.add("-I..");
    runner.args.add("-I../..");
    runner.args.add("-I../../..");
    runner.args.add("-I../../../..");
    for (StringList::Iterator i(config.compilerOptions); i; i.next()) {
        if (!isValidGccOption(i->data, i->length)) return false;
        runner.args.add(i->data, i->length);
    }
    for (StringList::Iterator i(type == typeCppSource ? config.compilerCppOptions : config.compilerCOptions); i; i.next()) {
        if (!isValidGccOption(i->data, i->length)) return false;
        runner.args.add(i->data, i->length);
    }
    runner.args.add("-c");
    runner.args.add(sourcePath);
    runner.args.add("-o");
    runner.args.add(objPath);
    if (runner.run()) {
        if (runner.exitStatus == 0) {
            printOutput(runner.output);
            bool hasMain = containsMain(config, objPath);
            return convertGccDeps(
                config.path,
                gccDepsPath,
                depsPath,
                hasMain,
                getCompilerOptionsTag(config, type),
                deps
            );
        }
        else {
            delayedError("While compiling %s%s%s", em, absSourcePath, noem);
            delayedError(runner.output);
        }
    }
    return false;
}


bool GccCompiler::containsMain(const Config& config, const char* objPath) {
    Runner runner;
    runner.currentDirectory = config.path;
    runner.args.add(profile.symList);
    runner.args.add("--no-sort");
    runner.args.add("--defined-only");
    runner.args.add("--portability");
    runner.args.add(objPath);
    if (runner.run() && runner.exitStatus == 0) {
        for (StringList::Iterator i(runner.output); i; i.next()) {
            if (i->length >= 8) {
                switch (i->data[0]) {
                    case 'm':
                        if (memcmp(i->data, "main T ", 7) == 0) {
                            return true;
                        }
                        break;
                    case '_':
                        if (memcmp(i->data, "_main T ", 8) == 0) {
                            return true;
                        }
                        break;
                }
            }
        }
    }
    return false;
}


bool GccCompiler::link(const Config& config, const char* execPath, const FileStateList& objList, const FileStateList& libList) {
    char absExecPath[maxPath];
    INFO("%s", rebasePath(config.path, execPath, absExecPath));
    Runner runner; 
    runner.currentDirectory = config.path;
    runner.args.add(profile.linker);
    runner.args.add("-fdiagnostics-color=always");
    for (StringList::Iterator i(config.linkerOptions); i; i.next()) {
        if (!isValidGccOption(i->data, i->length)) return false;
        runner.args.add(i->data, i->length);
    }
    runner.args.add("-o");
    runner.args.add(execPath);
    for (FileStateList::Iterator i(objList); i; i.next()) {
        runner.args.add(i->name, i->length);
    }
    if (!libList.isEmpty()) {
        runner.args.add("-Wl,--start-group");
        for (FileStateList::Iterator i(libList); i; i.next()) {
            runner.args.add(i->name, i->length);
        }
        runner.args.add("-Wl,--end-group");
    }
    runner.args.add("-lpthread");
    if (runner.run()) {
        if (runner.exitStatus == 0) {
            printOutput(runner.output);
            return true;
        }
        delayedError("While linking %s%s%s", em, absExecPath, noem);
        delayedError(runner.output);
    }
    deleteFile(absExecPath);
    return false;
}


bool GccCompiler::makeLibrary(const Config& config, const char* libPath, const FileStateList& objList) {
    char absLibPath[maxPath];
    INFO("%s", rebasePath(config.path, libPath, absLibPath));
    deleteFile(absLibPath);
    Runner runner; 
    runner.currentDirectory = config.path;
    runner.args.add(profile.librarian);
    runner.args.add("crs");
    runner.args.add(libPath);
    for (FileStateList::Iterator i(objList); i; i.next()) {
        runner.args.add(i->name, i->length);
    }
    if (runner.run()) {
        if (runner.exitStatus == 0) {
            printOutput(runner.output);
            return true;
        }
        delayedError("While packaging %s%s%s", em, absLibPath, noem);
        delayedError(runner.output);
    }
    deleteFile(absLibPath);
    return false;
}

