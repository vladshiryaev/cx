#pragma once

#include "lists.h"
#include "dirs.h"
#include "config.h"

enum FileType {
    typeUnknown,
    typeHeader,
    typeCSource,
    typeCppSource,
};

extern const char* cacheDirName;
FileType getFileType(const char* path);
char* makeDerivedPath(const char* configId, const char* source, const char* suffix, char* derived);


class Compiler {
public:
    enum {
        flagHasMain = 1,
    };
    bool keepDeps = false;
    Profile& profile;
    Compiler(Profile& p): profile(p) {}
    virtual ~Compiler();
    uint32_t getCompilerOptionsTag(const Config& config, FileType type) const { return type == typeCppSource ? config.cxxOptionsTag : type == typeCSource ? config.cOptionsTag : 0; }
    uint32_t getCompilerOptionsTag(const Config& config, const char* path) const { return getCompilerOptionsTag(config, getFileType(path)); }
    virtual bool compile(const Config&, const char* sourcePath, Dependencies&) = 0;
    virtual bool link(const Config&, const char* exec, const StringList& objList, const StringList& libList) = 0;
    virtual bool makeLibrary(const Config&, const char* name, const StringList& objList) = 0;
    virtual bool containsMain(const Config&, const char* objPath) = 0;
};


class GccCompiler: public Compiler {
public:
    GccCompiler(Profile&);
    bool compile(const Config&, const char* sourcePath, Dependencies&) override;
    bool link(const Config&, const char* exec, const StringList& objList, const StringList& libList) override;
    bool makeLibrary(const Config&, const char* name, const StringList& objList);
    bool containsMain(const Config&, const char* objPath) override;
protected:
    bool convertGccDeps(const char*, const char*, const char*, bool, uint32_t, Dependencies&);
};


