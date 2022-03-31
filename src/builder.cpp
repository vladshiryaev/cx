#include "builder.h"
#include "compiler.h"
#include "output.h"
#include "dirs.h"
#include "blob.h"
#include "runner.h"
#include "async.h"

#include <cstring>


Builder::~Builder() {
    if (master == this) {
        delete compiler;
        delete profile;
        delete currentDirectory;
    }
}


char* Builder::rebase(const char* relPath, char* absPath) {
    rebasePath(unitPath, relPath, absPath);
    return absPath;
}


// Get tag for file 'name' (module-local name). Cache it.
uint64_t Builder::lookupFileTag(const char* name, FileStateDict& files) {
    FileStateDict::Entry* p;
    if (files.add(0, name, p)) {
        char absName[maxPath];
        p->tag = makeFileTag(rebase(name, absName));
    }
    return p->tag;
}


bool Builder::scanDirectory() {
    sources.clear();
    fileStateCache.clear();
    Directory dir(unitPath);
    for (Directory::Entry entry; dir.read(entry); ) {
        FileStateDict::Entry* p;
        FileType type = getFileType(entry.name);
        if (type == typeCSource || type == typeCppSource) {
            sources.add(entry.tag, entry.name);    
            fileStateCache.add(entry.tag, entry.name, p);    
        }
        else if (type == typeHeader) {
            fileStateCache.add(entry.tag, entry.name, p);    
        }
    }
    return true;
}


bool Builder::checkDeps(const char* targetPath, uint32_t toolTag, uint32_t optTag, Dependencies& deps) {
    char absTargetPath[maxPath];
    if (!fileExists(rebase(targetPath, absTargetPath))) {
        TRACE("File %s does not exist", absTargetPath);
        return false;
    }
    char depsPath[maxPath];
    addSuffix(targetPath, ".deps", depsPath);
    char absDepsPath[maxPath];
    if (!deps.load(rebase(depsPath, absDepsPath))) {
        TRACE("Failed to load dependency file %s", absDepsPath);
        return false;
    }
    DepsHeader& header = deps.getHeader();
    if (header.toolTag != toolTag) {
        TRACE("Tool that created %s has changed", absDepsPath);
        return false;
    }
    if (header.optTag != optTag) {
        TRACE("Options with which %s was created have changed", absDepsPath);
        return false;
    }
    std::lock_guard<std::mutex> lock(mutex);
    for (FileStateList::Iterator dep(deps); dep; dep.next()) {
        if (dep->tag != lookupFileTag(dep->string, fileStateCache)) {
            char absDepName[maxPath];
            TRACE("File %s has changed", rebase(dep->string, absDepName));
            return false;
        }
    }
    // TRACE("File %s is fresh", targetPath);
    return true;
}


bool Builder::checkDeps(const char* targetPath, uint32_t toolTag, uint32_t optTag, uint64_t inputsTag, uint8_t& flags) {
    char absTargetPath[maxPath];
    if (!fileExists(rebase(targetPath, absTargetPath))) {
        TRACE("File %s does not exist", absTargetPath);
        return false;
    }
    char depsPath[maxPath];
    addSuffix(targetPath, ".deps", depsPath);
    char absDepsPath[maxPath];
    DepsHeader header;
    if (!header.load(rebase(depsPath, absDepsPath))) {
        TRACE("Failed to load dependency file %s", absDepsPath);
        return false;
    }
    flags = header.flags;
    if (header.toolTag != toolTag) {
        TRACE("Tool that created %s has changed", absDepsPath);
        return false;
    }
    if (header.optTag != optTag) {
        TRACE("Options with which %s was created have changed", absDepsPath);
        return false;
    }
    if (header.inputsTag != inputsTag) {
        TRACE("File %s needs rebuilding", absTargetPath);
        return false;
    }
    return true;
}


void Builder::extractUnitDirDeps(Dependencies& deps) {
    for (Dependencies::Iterator i(deps); i; i.next()) {
        char dir[maxPath];
        char rebased[maxPath];
        char normalized[maxPath];
        normalizePath(rebase(getDirectory(i->string, dir), rebased), normalized);
        if (dir[0]) {
            master->unitDirDeps.put(normalized);
        }
    }
}


bool Builder::buildUnitDirDeps(uint64_t& tag) {
    for (FileStateDict::Iterator i(unitDirDeps); i; i.next()) {
        if (!i->tag) {
            Builder other;
            other.master = master;
            other.options.force = options.force;
            char name[maxPath];
            strcpy(name, i->string);
            if (!other.build(name)) {
                return false;
            }
        }
    }
    tag = 0;
    for (FileStateDict::Iterator i(unitDirDeps); i; i.next()) {
        char libPath[maxPath];
        i->tag = makeFileTag(makeDerivedPath(i->string, "library", libPath));
        tag = tag * 11 + i->tag;
    }
    return true;
}


void Builder::addUnitLibDeps(FileStateList& libList) {
    if (unitDirDeps.isEmpty()) {
        return;
    }
    for (FileStateDict::Iterator i(unitDirDeps); i; i.next()) {
        if (i->tag) {
            char libPath[maxPath];
            makeDerivedPath(i->string, "library", libPath);
            TRACE("Depend on library %s", libPath);
            char absLibPath[maxPath];
            libList.add(0, rebasePath(currentDirectory, libPath, absLibPath));
        }
    }
    for (FileStateDict::Iterator i(libDeps); i; i.next()) {
        libList.add(0, i->string, i->length);
    }
}


bool Builder::processPath(const char* path) {
    if (master == this) {
        if (!currentDirectory) {
            currentDirectory = new char[maxPath];
        }
        getCurrentDirectory(currentDirectory);
    }
    else {
        currentDirectory = master->currentDirectory;
    }
    unitPath[0] = 0;
    objectToRun[0] = 0;
    if (*path == 0) {
        strcpy(unitPath, currentDirectory);
        options.skipRunning = true;
        return true;
    }
    FileType type = getFileType(path);
    if (type == typeCSource || type == typeCppSource) {
        if (fileExists(path)) {
            char dir[maxPath];
            char name[maxPath];
            if (splitPath(path, dir, name)) {
                rebasePath(currentDirectory, dir, unitPath);
            }
            else {
                strcpy(unitPath, currentDirectory);
                strcpy(name, path);
            }
            makeDerivedPath(name, ".o", objectToRun);
            return true;
        }
    }
    if (!directoryExists(path)) {
        if (fileExists(path)) {
            FAILURE("Cannot run files of this type: %s", path);
        }
        else {
            FAILURE("Directory %s does not exist", path);
        }
        return false;
    }
    rebasePath(currentDirectory, path, unitPath);
    int len = strlen(unitPath);
    if (len && unitPath[len - 1] != '/') {
        unitPath[len] = '/';
        unitPath[len + 1] = 0;
    }
    return true;
}


bool Builder::clean(const char* path) {
    char dir[maxPath];
    if (fileExists(path)) {
        path = getDirectory(path, dir);
    }
    if (!*path) {
        path = ".";
    }
    if (!directoryExists(path)) {
        FAILURE("Path %s does not exist, or is not a directory", path);
        return false;
    }
    TRACE("Cleaning %s", path);
    Runner runner;
    runner.args.add("find");
    runner.args.add(path);
    runner.args.add("-type");
    runner.args.add("d");
    runner.args.add("-name");
    runner.args.add(cacheDirName);
    runner.args.add("-exec");
    runner.args.add("rm");
    runner.args.add("-rf");
    runner.args.add("{}");
    runner.args.add(";");
    return runner.run();
}


struct UpdateSourceJob: public Job {
    Builder& builder;
    const char* name;
    bool skipDepsCheck;
    bool ok;
    bool recompiled;
    bool hasMain;
    Dependencies deps;
    UpdateSourceJob(Builder& b, const char* n, bool s): builder(b), name(n), skipDepsCheck(s) {}
    void run() override {
        ok = builder.updateSource(name, skipDepsCheck, recompiled, deps);
        hasMain = deps.getHeader().flags & Compiler::flagHasMain;
    }
};


bool Builder::updateSource(const char* sourcePath, bool skipDepsCheck, bool& recompiled, Dependencies& deps) {
    char objPath[maxPath];
    makeDerivedPath(sourcePath, ".o", objPath);
    recompiled = false;
    uint8_t flags;
    if (!(skipDepsCheck || options.force) && checkDeps(objPath, profile->tag, compiler->getCompilerOptionsTag(config, sourcePath), deps)) {
        return true;
    }
    recompiled = true;
    return compiler->compile(config, sourcePath, deps);
}


bool Builder::loadProfile() {
    if (master == this) {
        if (profile) {
            delete profile;
        }
        profile = new Profile();
        topPath[0] = 0;
        char absProfilePath[maxPath];
        strcpy(absProfilePath, unitPath);
        short slashPos[maxPath / 2];
        int slashCount = 0;
        for (int i = 0; absProfilePath[i]; i++) {
            if (absProfilePath[i] == '/') {
                slashPos[slashCount++] = i;
            }
        }
        for ( ; slashCount >= 2; slashCount--) {
            int length = slashPos[slashCount - 1] + 1;
            memcpy(absProfilePath + length, "cx.top", 7); 
            if (fileExists(absProfilePath)) {
                TRACE("Found %s", absProfilePath);
                if (!profile->commonConfig.load(absProfilePath)) {
                    return false;
                }
                memcpy(topPath, absProfilePath, length);
                topPath[length] = 0;
                profile->commonConfig.path = topPath;
                break;
            }
        }
        if (compiler) {
            delete compiler;
        }
        compiler = new GccCompiler(*profile); // For now GCC only.
        compiler->keepDeps = options.keepDeps;
    }
    else {
        currentDirectory = master->currentDirectory;
        profile = master->profile;
        compiler = master->compiler;
    }
    return true;
}


bool Builder::loadConfig() {
    config = profile->commonConfig;
    config.profile = nullptr;
    char unitConfigPath[maxPath];
    if (!config.load(catPath(unitPath, "cx.unit", unitConfigPath))) {
        return false;
    }
    config.path = unitPath;
    return true;
}


bool Builder::build(const char* path) {
    if (!processPath(path)) {
        return false;
    }
    TRACE("Building %s", unitPath);
    if (!(loadProfile() && loadConfig())) {
        return false;
    }
    for (StringList::Iterator i(config.externalLibs); i; i.next()) {
        master->libDeps.put(i->string, i->length);
    }
    scanDirectory();
    if (sources.isEmpty()) {
        TRACE("No sources found in %s", unitPath);
        return true;
    }
    // Create cache directory.
    char cachePath[maxPath];
    catPath(unitPath, cacheDirName, cachePath);
    bool skipDepsCheck = false;
    if (!directoryExists(cachePath)) {
        skipDepsCheck = true;
        if (!makeDirectory(cachePath)) {
            FAILURE("Failed to create directory %s", cachePath);
            return false;
        }
    }
    // Compile unit sources.
    bool anyRecompiled = false;
    FileStateList objListMain;
    FileStateList objList;
    char objPath[maxPath];
    for (FileStateList::Iterator i(sources); i; i.next()) {
        batch.add(new UpdateSourceJob(*this, i->string, skipDepsCheck));
    }
    batch.run();
    unitDirDeps.put(1, unitPath);
    int n = sources.getCount();
    for (int i = 0; i < n; i++) {
        UpdateSourceJob* job = (UpdateSourceJob*)batch.get(i);
        if (!job->ok) {
            batch.clear();
            return false;
        }
        if (job->recompiled) {
            anyRecompiled = true;
        }
        makeDerivedPath(job->name, ".o", objPath);
        if (job->hasMain) {
            objListMain.add(lookupFileTag(objPath, fileStateCache), objPath);
            char absSourcePath[maxPath];
            TRACE("File %s defines main()", rebase(job->name, absSourcePath));
        }
        else {
            objList.add(lookupFileTag(objPath, fileStateCache), objPath);
        }
        extractUnitDirDeps(job->deps);
    }
    batch.clear();
    // Make unit library.
    uint64_t objTag = 0;
    char libPath[maxPath];
    makeDerivedPath("library", "", libPath);
    if (!objList.isEmpty()) {
        for (FileStateList::Iterator i(objList); i; i.next()) {
            objTag += i->tag;
        }
        uint8_t flags;
        if (anyRecompiled || options.force || !checkDeps(libPath, profile->tag, 0, objTag, flags)) {
            char libDepsPath[maxPath];
            char absLibDepsPath[maxPath];
            rebase(addSuffix(libPath, ".deps", libDepsPath), absLibDepsPath);
            if (!compiler->makeLibrary(config, libPath, objList)) {
                deleteFile(absLibDepsPath);
                return false;
            }
            DepsHeader header;
            header.toolTag = profile->tag;
            header.inputsTag = objTag;
            save(absLibDepsPath, &header, sizeof(header));
        }
    }
    // Build dependencies.
    uint64_t libsTag;
    if (!buildUnitDirDeps(libsTag)) {
        return false;
    }
    if (master != this) {
        return true;
    }
    // Link all (or one) objects with main().
    if (options.skipLinking || objListMain.isEmpty()) {
        return true;
    }
    char execPath[maxPath];
    execPath[0] = 0;
    for (FileStateList::Iterator i(objListMain); i; i.next()) {
        if (objectToRun[0] != 0 && strcmp(i->string, objectToRun) != 0) {
            continue;
        }
        uint64_t execTag = i->tag + libsTag;
        addSuffix(i->string, ".exe", execPath);
        uint8_t flags;
        if (anyRecompiled || options.force || !checkDeps(execPath, profile->tag, config.linkerOptionsTag, execTag, flags)) {
            char execDepsPath[maxPath];
            char absExecDepsPath[maxPath];
            rebase(addSuffix(execPath, ".deps", execDepsPath), absExecDepsPath);
            FileStateList execObjList;
            execObjList.add(i->tag, i->string, i->length);
            FileStateList execLibList;
            addUnitLibDeps(execLibList);
            if (!compiler->link(config, execPath, execObjList, execLibList)) {
                deleteFile(absExecDepsPath);
                return false;
            }
            DepsHeader header;
            header.toolTag = profile->tag;
            header.optTag = config.linkerOptionsTag;
            header.inputsTag = execTag;
            save(absExecDepsPath, &header, sizeof(header));
        }
    }
    // Run.
    if (options.skipRunning) {
        return true;
    }
    if (objectToRun[0] == 0) {
        if (objListMain.getCount() > 1) {
            FAILURE("In %s there are %d sources defining main(), specify source file name", unitPath, objListMain.getCount());
            return false;
        }
    }
    else if (execPath[0] == 0) {
        if (objectToRun[0] == '.') {
            FAILURE("Source names starting with . are not compiled, and not executed");
        }
        return false;
    }
    const char* var = "EXECUTED_BY_CX";
    if (getVariable(var)) {
        FAILURE("Running itself is asking for an endless loop... Won't do that.");
        return false;
    }
    else {
        setVariable(var, "1");
        char absExecPath[maxPath];
        Runner runner;
        runner.args.add(rebase(execPath, absExecPath));
        if (options.runArgs) {
            for (StringList::Iterator i(*options.runArgs); i; i.next()) {
                runner.args.add(i->string, i->length);
            }
        }
        runner.exec();
    }
    return true;
}




