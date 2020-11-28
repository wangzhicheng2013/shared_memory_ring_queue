#include <thread>
#include <chrono>

#include "shm_mdb.hpp"
shmd_mdb::ring_mdb rmdb;
void thread0() {
    char buf[128] = "";
    while (true) {
        snprintf(buf, sizeof(buf), "I put timestamp:%ld", time(0));
        rmdb.put((unsigned char *)buf, strlen(buf));
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
}
void thread1() {
    std::string str;
    while (true) {
        rmdb.get(str);
        std::cout << "I am thread1 get value:" << str << std::endl;
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
}
int main() {
    if (false == rmdb.init()) {
        return -1;
    }
    std::thread th0(thread0);
    std::thread th1(thread1);
    th0.join();
    th1.join();

    return 0;
}