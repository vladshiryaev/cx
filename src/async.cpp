#include "async.h"
#include "output.h"


// Using STL for now... But at least it's hidden.

#include <vector>
#include <queue>
#include <thread>
#include <mutex>
#include <condition_variable>

int maxThreads = int(std::thread::hardware_concurrency()); // Global.
int runningJobCount = 0;
int producerCount = 0;

std::vector<std::thread> workers;
std::queue<Job*> pendingJobs;
std::condition_variable signalDo;
std::condition_variable signalDone;
std::mutex mutex;

void worker() {
    std::unique_lock<std::mutex> lock(mutex);
    for (;;) {
        signalDo.wait(lock, []{return !pendingJobs.empty();});
        Job* job = pendingJobs.front();
        if (!job) {
            break;
        }
        pendingJobs.pop();
        runningJobCount++;
        lock.unlock();
        job->run();
        job->done = true;
        lock.lock();
        runningJobCount--;
        signalDone.notify_all();
    }
}


class Batch::Impl {
public:
    Impl() {
        std::unique_lock<std::mutex> lock(mutex);
        producerCount++;
    }
    ~Impl() {
        std::unique_lock<std::mutex> lock(mutex);
        if (--producerCount == 0) {
            //TRACE("Stopping %d threads", maxThreads);
            for (auto& worker: workers) {
                pendingJobs.push(nullptr);
                pendingJobs.push(nullptr);
                signalDo.notify_all();
            }
            lock.unlock();
            for (auto& worker: workers) {
                worker.join();
            }
            lock.lock();
            while (!pendingJobs.empty()) {
                delete pendingJobs.front();
                pendingJobs.pop();
            }
            workers.clear();
        }
        clear();
    }
    void add(Job* job) {
        jobs.push_back(job);
    }
    void run() {
        std::unique_lock<std::mutex> lock(mutex);
        if (workers.empty()) {
            //TRACE("Starting %d threads", maxThreads);
            while (int(workers.size()) < maxThreads) {
                workers.push_back(std::thread(worker));
            }
        }
        int n = int(jobs.size());
        for (int i = 0; i < n; i++) {
            if ((i & 7) == 0) {
                signalDone.wait(lock, []{return int(pendingJobs.size()) <= 4 + maxThreads * 2;});
            }
            Job* job = jobs[i];
            job->done = false;
            pendingJobs.push(job);
            signalDo.notify_all();
        }
        signalDone.wait(lock, [this]{return allDone();});
    }
    void clear() {
        for (Job* job: jobs) {
            delete job;
        }
        jobs.clear();
    }
    int getCount() const {
        return int(jobs.size());
    }
    Job* get(int i) {
        return jobs[i];
    }
    bool allDone() {
        for (Job* job: jobs) {
            if (!job->done) {
                return false;
            }
        }
        return true;
    }
private:
    std::vector<Job*> jobs;
};

Batch::Batch(): impl(new Batch::Impl()) {}
Batch::~Batch() { delete impl; }
void Batch::add(Job* job) { impl->add(job); }
void Batch::run() { impl->run(); }
void Batch::clear() { impl->clear(); }
int Batch::getCount() const { return impl->getCount(); }
Job* Batch::get(int i) const { return impl->get(i); }


