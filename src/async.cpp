#include "async.h"
#include "output.h"


// Using STL for now... But at least it's hidden.

#include <vector>
#include <queue>
#include <thread>
#include <mutex>
#include <condition_variable>

int maxThreads = int(std::thread::hardware_concurrency()); // Global.

// For now there is a single implicit thread pool.
int producerCount = 0;
std::vector<std::thread> workers;
std::queue<Job*> pendingJobs;
std::condition_variable signalPending;
std::mutex mutex;

void worker();


class Batch::Impl {
public:
    Impl(Batch* b): batch(b) {
        std::unique_lock<std::mutex> lock(mutex);
        producerCount++;
    }
    ~Impl() {
        discard();
        std::unique_lock<std::mutex> lock(mutex);
        // Last work producer, stop worker threads.
        if (--producerCount == 0) {
            TRACE("Stopping %d threads", maxThreads);
            // Null jobs signal exit to workers.
            for (auto& worker: workers) {
                pendingJobs.push(nullptr);
                pendingJobs.push(nullptr);
                signalPending.notify_all();
            }
            // Wait for them to finish.
            lock.unlock();
            for (auto& worker: workers) {
                worker.join();
            }
            // Free abandoned jobs.
            lock.lock();
            while (!pendingJobs.empty()) {
                delete pendingJobs.front();
                pendingJobs.pop();
            }
            workers.clear();
        }
    }
    void send(Job* job) {
        std::unique_lock<std::mutex> lock(mutex);
        if (workers.empty()) {
            // Start lazily, on first request.
            // Stop when last work producer dies.
            TRACE("Starting %d threads", maxThreads);
            while (int(workers.size()) < maxThreads) {
                workers.push_back(std::thread(worker));
            }
        }
        job->batch = batch;
        pendingJobs.push(job);
        sentCount++;
        signalPending.notify_one();
    }
    Job* receive() {
        std::unique_lock<std::mutex> lock(mutex);
        if (sentCount == receivedCount) {
            return nullptr;
        }
        signalPendingne.wait(lock, [this]{return !doneJobs.empty();});
        receivedCount++;
        Job* job = doneJobs.front();
        doneJobs.pop();
        return job;
    }
    void discard() {
        while (Job* job = receive()) {
            delete job;
        }
    }
    Batch* batch = nullptr;
    int sentCount = 0;
    int receivedCount = 0;
    std::queue<Job*> doneJobs;
    std::condition_variable signalPendingne;
};


Batch::Batch(): impl(new Batch::Impl(this)) {}
Batch::~Batch() { delete impl; }
void Batch::send(Job* job) { impl->send(job); }
Job* Batch::receive() { return impl->receive(); }
void Batch::discard() { return impl->discard(); }


void worker() {
    std::unique_lock<std::mutex> lock(mutex);
    for (;;) {
        signalPending.wait(lock, []{return !pendingJobs.empty();});
        Job* job = pendingJobs.front();
        if (!job) {
            break;
        }
        pendingJobs.pop();

        lock.unlock();
        job->run();
        lock.lock();

        Batch::Impl* receiver = job->batch->impl;
        receiver->doneJobs.push(job);
        receiver->signalPendingne.notify_one();
    }
}



