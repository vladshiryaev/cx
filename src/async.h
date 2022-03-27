#pragma once


class Job {
public:
    bool done = false; // Used internally, don't touch.
    Job() {}
    virtual ~Job() {}
    virtual void run() {}
};


class Batch {
public:
    Batch();
    Batch(const Batch&) = delete;
    Batch& operator=(const Batch&) = delete;
    ~Batch();
    void add(/* new */ Job*);
    void run();
    void clear();
    int getCount() const;
    Job* get(int) const;
private:
    class Impl;
    Impl* impl;
};


extern int maxThreads;
