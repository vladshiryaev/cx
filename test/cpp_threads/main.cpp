#include <cstdio>
#include <mutex> 

int main() {
    std::mutex mutex;
    std::lock_guard<std::mutex> lock(mutex);
    printf("OK\n");
}

