#pragma once


class Batch;


// Something to execute asynchronously.
class Job {
public:
    Batch* batch = nullptr; // For internal use.
    Job() {}
    virtual ~Job() {}
    virtual void run() {}
};


// Work producer. Submit work items, receive back when done some time later.
// Receive() returns null when all submitted work is done, otherwise
// it blocks until one item is done.
class Batch {
public:
    Batch();
    Batch(const Batch&) = delete;
    Batch& operator=(const Batch&) = delete;
    ~Batch();

    void send(/*new*/ Job*);
    Job* receive(); /*delete*/

    void discard(); // Wait for jobs, delete them.

private:
    friend void worker();
    class Impl;
    Impl* impl;
};


extern int maxThreads;
