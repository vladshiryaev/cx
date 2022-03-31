#ifdef NDEBUG
#undef NDEBUG
#endif

#include <cassert> 
#include <cstring> 
#include <cstdlib> 
#include <cstdio> 

#include "output.h"
#include "dirs.h"
#include "lists.h"
#include "compiler.h"
#include "config.h"
#include "async.h"


void testDirFunc() {
    char path[maxPath];
    getCurrentDirectory(path);
    assert(path[0] && path[strlen(path) - 1] == '/');

    assert(strcmp(catPath("d", "f", path), "d/f") == 0);
    assert(strcmp(catPath("d/", "f", path), "d/f") == 0);
    assert(strcmp(catPath("", "f", path), "f") == 0);
    assert(strcmp(catPath("d", "", path), "d/") == 0);
    assert(strcmp(catPath("d/", "", path), "d/") == 0);

    assert(strcmp(rebasePath("d", "f", path), "d/f") == 0);
    assert(strcmp(rebasePath("d", "/f", path), "/f") == 0);
    assert(strcmp(rebasePath("", "f", path), "f") == 0);
    assert(strcmp(rebasePath(".", "f", path), "f") == 0);
    assert(strcmp(rebasePath("./", "f", path), "f") == 0);
    assert(strcmp(rebasePath("d/", "./f", path), "d/f") == 0);
    assert(strcmp(rebasePath("d/", "././f", path), "d/f") == 0);
    assert(strcmp(rebasePath("d/.", "././f", path), "d/f") == 0);

    char dir[maxPath];
    char name[maxPath];
    assert(splitPath("d/f", dir, name));
    assert(strcmp(dir, "d/") == 0);
    assert(strcmp(name, "f") == 0);
    assert(!splitPath("d", dir, name));
    assert(splitPath("d/", dir, name));
    assert(strcmp(dir, "d/") == 0);
    assert(strcmp(name, "") == 0);
    assert(splitPath("/f", dir, name));
    assert(strcmp(dir, "/") == 0);
    assert(strcmp(name, "f") == 0);

    assert(strcmp(stripBasePath("a/b",  "a/b/c", path), "c") == 0);
    assert(strcmp(stripBasePath("a/b/", "a/b/c", path), "c") == 0);
    assert(strcmp(stripBasePath("", "a/b/c", path), "a/b/c") == 0);
    assert(strcmp(stripBasePath("x/y", "a/b/c", path), "a/b/c") == 0);
    assert(strcmp(stripBasePath("/a/", "/a/b/c", path), "b/c") == 0);

    assert(strcmp(normalizePath("a/b/../c", path), "a/c") == 0);
    assert(strcmp(normalizePath("a/b/c/../../d", path), "a/d") == 0);
    assert(strcmp(normalizePath("a/b/../../c", path), "c") == 0);
    assert(strcmp(normalizePath("a/b/../../../c", path), "../c") == 0);
    assert(strcmp(normalizePath("a/b/..", path), "a/") == 0);
    assert(strcmp(normalizePath("../a/b", path), "../a/b") == 0);
    assert(strcmp(normalizePath("/a//b", path), "/a/b") == 0);
    assert(strcmp(normalizePath("/a///b", path), "/a/b") == 0);
    assert(strcmp(normalizePath("/a////b", path), "/a/b") == 0);
    assert(strcmp(normalizePath("/a/../b", path), "/b") == 0);
    assert(strcmp(normalizePath("./", path), "") == 0);
    assert(strcmp(normalizePath("./a", path), "a") == 0);
    assert(strcmp(normalizePath("a/./b", path), "a/b") == 0);
    assert(strcmp(normalizePath("a/././b", path), "a/b") == 0);
    //assert(strcmp(normalizePath(".//", path), "") == 0); ?????
    assert(strcmp(normalizePath(".", path), "") == 0);
    assert(strcmp(normalizePath("", path), "") == 0);

}


void testStringList() {
    StringList list(64);

    char buf[64];
    for (int i = 0; i < 2000; i++) {
        list.add(buf, sprintf(buf, "s-%09d-%d", i, i));
    }
    assert(list.getCount() == 2000);

    StringList::Iterator i(list);
    for (int j = 0; j < 2000; j++, i.next()) {
        sprintf(buf, "s-%09d-%d", j, j);
        assert(i);
        assert(strcmp(i->string, buf) == 0);
    }
    assert(!i);
}


void testFileStateList() {
    FileStateList list(256);

    char name[64];
    for (int i = 0; i < 1000; i++) {
        list.add(i, name, sprintf(name, "key-%09d-%d", i, i));
    }
    assert(list.getCount() == 1000);

    auto i = FileStateList::Iterator(list);
    for (int j = 0; j < 1000; j++, i.next()) {
        sprintf(name, "key-%09d-%d", j, j);
        assert(i);
        assert(i->tag == j);
        assert(strcmp(i->string, name) == 0);
    }
    assert(!i);
}


void testFileStateDict() {
    FileStateDict dict;
    FileStateDict::Entry* entry;

    char name[64];
    for (int i = 0; i < 1000; i++) {
        sprintf(name, "key-%09d-%d", i, i);
        assert(dict.add(i, name, entry));
    }
    assert(dict.getCount() == 1000);

    for (int i = 0; i < 1000; i++) {
        sprintf(name, "key-%09d-%d", i, i);
        assert((entry = dict.find(name)) && strcmp(entry->string, name) == 0);
    }

    auto i = FileStateDict::Iterator(dict);
    for (int j = 0; j < 1000; j++, i.next()) {
        sprintf(name, "key-%09d-%d", j, j);
        assert(i);
        assert(i->tag == j);
        assert(strcmp(i->string, name) == 0);
    }
    assert(!i);
}


void testFileType() {
    assert(getFileType("x.c") == typeCSource);
    assert(getFileType("x.cc") == typeCppSource);
    assert(getFileType("x.cp") == typeCppSource);
    assert(getFileType("x.cpp") == typeCppSource);
    assert(getFileType("x.cxx") == typeCppSource);
    assert(getFileType("x.c++") == typeCppSource);
    assert(getFileType("x.CPP") == typeCppSource);
    assert(getFileType("x.C") == typeCppSource);
    assert(getFileType("x.h") == typeHeader);
    assert(getFileType("x.H") == typeHeader);
    assert(getFileType("x.hxx") == typeHeader);
    assert(getFileType("x.hpp") == typeHeader);
    assert(getFileType("x.h++") == typeHeader);
}


void testConfig() {
    const char* text = 
        "# Comment\n"
        "c_options:-O2 -Df=\" \" \\\n-O3\n"  // Escaped end of line, so continues.
        "cxx_options: \"-Df= \" -Df='\" \"'  -Df=\"' '\"#Tail comment\n"
        "external_libs : a \"b c\" c\\ d \n"  // Escaped space in 'c d'.
        "#";
    Config config;
    assert(config.parse("config", text));
    {
        StringList::Iterator i(config.compilerCOptions);
        assert(i); assert(strcmp(i->string, "-O2") == 0); i.next();
        assert(i); assert(strcmp(i->string, "-Df= ") == 0); i.next();
        assert(i); assert(strcmp(i->string, "-O3") == 0); i.next();
        assert(!i);
    }
    {
        StringList::Iterator i(config.compilerCppOptions);
        assert(i); assert(strcmp(i->string, "-Df= ") == 0); i.next();
        assert(i); assert(strcmp(i->string, "-Df=\" \"") == 0); i.next();
        assert(i); assert(strcmp(i->string, "-Df=' '") == 0); i.next();
        assert(!i);
    }
    {
        StringList::Iterator i(config.externalLibs);
        assert(i); assert(strcmp(i->string, "a") == 0); i.next();
        assert(i); assert(strcmp(i->string, "b c") == 0); i.next();
        assert(i); assert(strcmp(i->string, "c d") == 0); i.next();
        assert(!i);
    }
}


const int jobCount = 16;
int jobInstanceCount = 0;

struct IncJob: public Job {
    int count = 0;
    IncJob() { jobInstanceCount++; }
    ~IncJob() { jobInstanceCount--; }
    void run() override { while (count < 10) count++; }
};

void testBatch() {
    {
        Batch batch; 
        for (int j = 0; j < 3; j++) {
            for (int i = 0; i < jobCount; i++) {
                batch.add(new IncJob());
            }
            batch.run();
            assert(batch.getCount() == jobCount);
            assert(jobInstanceCount == jobCount);
            for (int i = 0; i < jobCount; i++) {
                IncJob* job = (IncJob*)batch.get(i);
                assert(job->done);
                assert(job->count == 10);
            }
            batch.clear();
            assert(batch.getCount() == 0);
            assert(jobInstanceCount == 0);
        }
        Batch batch2;
        for (int i = 0; i < 2; i++) {
            batch.add(new IncJob());
        }
    }
    assert(jobInstanceCount == 0);
}


#define RUN(WHAT) do { \
    say(logLevelInfo, "Testing %s", #WHAT); \
    WHAT(); \
} while (0)
  
void test() {
    RUN(testDirFunc);
    RUN(testStringList);
    RUN(testFileStateList);
    RUN(testFileStateDict);
    RUN(testFileType);
    RUN(testConfig);
    RUN(testBatch);
}



