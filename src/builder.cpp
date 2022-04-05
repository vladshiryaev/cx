#include "builder.h"
#include "compiler.h"
#include "output.h"
#include "dirs.h"
#include "blob.h"
#include "runner.h"
#include "async.h"

#include <cstring>


enum JobType {
    jobTypeCompile,
    jobTypeLibrary,
};


struct BuilderJob: public Job {
    bool ok;
    JobType type;
    BuilderJob() {}
};


// Souce checking/compilation in the context of unit builder.
struct CompileJob: public BuilderJob {
    Builder& builder;
    const char* name;
    bool skipDepsCheck;
    bool recompiled;
    bool hasMain;
    Dependencies deps;
    CompileJob(Builder& b, const char* n, bool s):
        builder(b),
        name(n),
        skipDepsCheck(s)
    {
        type = jobTypeCompile;
    }
    void run() override {
        ok = builder.updateSource(name, skipDepsCheck, recompiled, deps);
        hasMain = deps.getHeader().flags & Compiler::flagHasMain;
    }
};


// Library making (once compilation/checking is done) in a new dedicated unit Builder.
struct LibraryJob: public BuilderJob {
    Builder builder;
    LibraryJob() {
        type = jobTypeLibrary;
    }
    void run() override {
        ok = builder.buildPhase2();
    }
};


Builder::~Builder() {
    if (master == this) {
        delete compiler;
        delete profile;
        delete currentDirectory;
    }
}


// Either the provided config id, or $CX_CONFIG, or "default".
const char* Builder::getConfigId(const char* configId) {
    if (configId && *configId) {
        return configId;
    }
    configId = getVariable("CX_CONFIG");
    if (configId && *configId) {
        return configId;
    }
    return "default";
}


// Rebase local name relative unit's abs path.
char* Builder::rebase(const char* relPath, char* absPath) {
    rebasePath(unitPath, relPath, absPath);
    return absPath;
}


// Get tag for file 'name' (unit-local name). Cache it.
uint64_t Builder::lookupFileTag(const char* name) {
    FileStateDict::Entry* p;
    if (fileStateCache.add(0, name, p)) {
        char absName[maxPath];
        p->tag = makeFileTag(rebase(name, absName));
    }
    return p->tag;
}


// Scan unit directory for sources to compile.
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
    std::lock_guard<std::mutex> lock(fileStateCacheMutex);
    for (FileStateList::Iterator dep(deps); dep; dep.next()) {
        if (dep->tag != lookupFileTag(dep->string)) {
            char absDepName[maxPath];
            TRACE("File %s has changed", rebase(dep->string, absDepName));
            return false;
        }
    }
    TRACE("File %s is fresh", targetPath);
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
    TRACE("File %s is fresh", absTargetPath);
    return true;
}


// We have just compiled one source (or made sure it's fresh).
// Check its dependency list generated by the compiler (or loaded from previously saved dep file).
// If this source introduces dependency on a new unit (i.e. another directory),
// start building that, immediately.
bool Builder::extractUnitDirDeps(Dependencies& deps) {
    for (Dependencies::Iterator i(deps); i; i.next()) {
        char dir[maxPath];
        char rebased[maxPath];
        char normalized[maxPath];
        normalizePath(rebase(getDirectory(i->string, dir), rebased), normalized);
        if (dir[0]) {
            std::unique_lock<std::mutex> lock(master->masterMutex);
            FileStateDict::Entry* entry;
            if (master->unitDirDeps.add(1, normalized, entry)) {
                // We have new unit dependency.
                // Enqueue its sources for compilation/freshness checking ("phase 1").
                // Start a library job, which will wait for those to finish, and make
                // unit library in "phase 2".
                lock.unlock();
                LibraryJob* job = new LibraryJob();
                job->builder.master = master;
                job->builder.options.force = options.force;
                if (!job->builder.buildPhase1(entry->string, nullptr)) {
                    delete job;
                    return false;
                }
                batch.send(job);
            }
        }
    }
    return true;
}


// Prepare lib list for the linker. Both discovered unit dependencies
// and external libs collected recursively.
void Builder::fillUnitLibList(StringList& libList) {
    for (FileStateDict::Iterator i(unitDirDeps); i; i.next()) {
        if (i->tag == 2) {
            char libPath[maxPath];
            makeDerivedPath(profile->id, i->string, "library", libPath);
            TRACE("Depend on library %s", libPath);
            char absLibPath[maxPath];
            libList.add(rebasePath(currentDirectory, libPath, absLibPath));
        }
    }
    for (FileStateDict::Iterator i(libDeps); i; i.next()) {
        libList.add(i->string, i->length);
    }
}


bool Builder::processPath(const char* path) {
    unitPath[0] = 0;
    sourceToRun[0] = 0;
    if (master != this) {
        currentDirectory = master->currentDirectory;
        strcpy(unitPath, path);
        return true;
    }
    if (!currentDirectory) {
        currentDirectory = new char[maxPath];
    }
    getCurrentDirectory(currentDirectory);
    if (*path == 0) {
        strcpy(unitPath, currentDirectory);
        options.skipRunning = true;
        return true;
    }
    FileType type = getFileType(path);
    if (type == typeCSource || type == typeCppSource) {
        if (fileExists(path)) {
            char dir[maxPath];
            if (splitPath(path, dir, sourceToRun)) {
                rebasePath(currentDirectory, dir, unitPath);
            }
            else {
                strcpy(unitPath, currentDirectory);
                strcpy(sourceToRun, path);
            }
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


bool Builder::clean(const char* path, const char* configId) {
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
    Runner runner;
    runner.args.add("find");
    runner.args.add(path);
    runner.args.add("-type");
    runner.args.add("d");
    if (!configId || !*configId) {
        TRACE("Cleaning %s for all configurations", path);
        runner.args.add("-name");
        runner.args.add(cacheDirName);
    }
    else {
        TRACE("Cleaning %s for configuration [%s]", path, configId);
        runner.args.add("-wholename");
        char name[maxPath];
        runner.args.add(name, sprintf(name, "**/%s/%s", cacheDirName, configId));
    }
    runner.args.add("-exec");
    runner.args.add("rm");
    runner.args.add("-rf");
    runner.args.add("{}");
    runner.args.add(";");
    return runner.run();
}


bool Builder::updateSource(const char* sourcePath, bool skipDepsCheck, bool& recompiled, Dependencies& deps) {
    char objPath[maxPath];
    makeDerivedPath(profile->id, sourcePath, ".o", objPath);
    recompiled = false;
    uint8_t flags;
    if (!(skipDepsCheck || options.force) && checkDeps(objPath, profile->tag, compiler->getCompilerOptionsTag(config, sourcePath), deps)) {
        return true;
    }
    recompiled = true;
    return compiler->compile(config, sourcePath, deps);
}


bool Builder::loadProfile(const char* configId) {
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
                if (!profile->commonConfig.load(absProfilePath, configId)) {
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
        strcpy(profile->id, configId);
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


bool Builder::loadConfig(const char* configId) {
    config = profile->commonConfig;
    config.profile = nullptr;
    char unitConfigPath[maxPath];
    if (!config.load(catPath(unitPath, "cx.unit", unitConfigPath), configId)) {
        return false;
    }
    config.path = unitPath;
    return true;
}


bool Builder::createCacheDir(bool& created) {
    created = false;
    char cacheCommonPath[maxPath];
    catPath(unitPath, cacheDirName, cacheCommonPath);
    if (!directoryExists(cacheCommonPath)) {
        created = true;
        if (!makeDirectory(cacheCommonPath)) {
            FAILURE("Failed to create directory %s", cacheCommonPath);
            return false;
        }
    }
    char cachePath[maxPath];
    catPath(cacheCommonPath, profile->id, cachePath);
    if (!directoryExists(cachePath)) {
        created = true;
        if (!makeDirectory(cachePath)) {
            FAILURE("Failed to create directory %s", cachePath);
            return false;
        }
    }
    return true;
}


// Start compiling sources.
bool Builder::buildPhase1(const char* path, const char* configId) {
    configId = master == this ? getConfigId(configId) : configId ? configId : master->profile->id;
    if (!processPath(path)) {
        return false;
    }
    TRACE("Building %s", unitPath);
    if (!(loadProfile(configId) && loadConfig(configId))) {
        return false;
    }
    {
        std::lock_guard<std::mutex> lock(master->masterMutex);
        for (StringList::Iterator i(config.externalLibs); i; i.next()) {
            master->libDeps.put(i->string, i->length);
        }
    }
    // Find sources.
    scanDirectory();
    if (sources.isEmpty()) {
        TRACE("No sources found in %s", unitPath);
        return true;
    }
    // Create cache directory. Skip checking deps if it's empty.
    bool skipDepsCheck = false;
    if (!createCacheDir(skipDepsCheck)) {
        return false;
    }
    // Start compiling unit sources.
    unitDirDeps.put(1, unitPath);
    for (FileStateList::Iterator i(sources); i; i.next()) {
        batch.send(new CompileJob(*this, i->string, skipDepsCheck));
    }
    return true;
}


// Wait for source compilation end, and do the rest.
bool Builder::buildPhase2() {
    bool anyRecompiled = false;
    StringList objListMain;
    StringList objList;
    char objPath[maxPath];
    libsTag = 0;
    uint64_t objTag = 0;
    for (;;) {
        BuilderJob* j = (CompileJob*)(batch.receive());
        if (!j) {
            break;
        }
        if (!j->ok) {
            delete j;
            return false;
        }
        if (j->type == jobTypeCompile) {
            CompileJob* job = (CompileJob*)j;
            anyRecompiled |= job->recompiled;
            makeDerivedPath(profile->id, job->name, ".o", objPath);
            if (job->hasMain) {
                objListMain.add(objPath);
                char absSourcePath[maxPath];
                TRACE("Source %s defines main()", rebase(job->name, absSourcePath));
            }
            else {
                objList.add(objPath);
                objTag += lookupFileTag(objPath);
            }
            if (!extractUnitDirDeps(job->deps)) {
                delete job;
                return false;
            }
        }
        else if (j->type == jobTypeLibrary) {
            LibraryJob* job = (LibraryJob*)j;
            char libPath[maxPath];
            uint64_t libTag = makeFileTag(makeDerivedPath(profile->id, job->builder.unitPath, "library", libPath));
            std::lock_guard<std::mutex> lock(master->masterMutex);
            master->unitDirDeps.put(2, job->builder.unitPath);
            master->libsTag += libTag;
        }
        delete j;
    }
    // Make unit library.
    char libPath[maxPath];
    makeDerivedPath(profile->id, "library", "", libPath);
    if (!objList.isEmpty()) {
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
        libsTag += makeFileTag(libPath);
        master->unitDirDeps.put(2, unitPath);
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
    char objectToRun[maxPath];
    objectToRun[0] = 0;
    if (sourceToRun[0]) {
        makeDerivedPath(profile->id, sourceToRun, ".o", objectToRun);
    }
    for (StringList::Iterator i(objListMain); i; i.next()) {
        if (objectToRun[0] != 0 && strcmp(i->string, objectToRun) != 0) {
            continue;
        }
        uint64_t execTag = lookupFileTag(i->string) + libsTag;
        addSuffix(i->string, ".exe", execPath);
        uint8_t flags;
        if (anyRecompiled || options.force || !checkDeps(execPath, profile->tag, config.linkerOptionsTag, execTag, flags)) {
            char execDepsPath[maxPath];
            char absExecDepsPath[maxPath];
            rebase(addSuffix(execPath, ".deps", execDepsPath), absExecDepsPath);
            StringList execObjList;
            execObjList.add(i->string, i->length);
            StringList execLibList;
            fillUnitLibList(execLibList);
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


bool Builder::build(const char* path, const char* configId) {
    return buildPhase1(path, configId) && buildPhase2();
}



