// TracyTest.cpp : This file contains the 'main' function. Program execution begins and ends there.
//

#include <iostream>
#include <Tracy.hpp>
#include <conio.h>
#include <thread>
#include <chrono>
#include <atomic>

std::mutex mutex;

void WorkerFunction(void)
{
    for (int i = 0; i < 100; i++)
    {
        std::cout << i << std::endl;
    }
}

// Thread function that prints message and sleeps
void threadFunction(std::atomic<bool> &stopFlag)
{
    tracy::SetThreadName("thread");
    while (!stopFlag)
    {
        ZoneScopedNC("THREAD", tracy::Color::PeachPuff);
        {
            std::lock_guard<std::mutex> LockGuard(mutex);
            ZoneScopedNC("thread",tracy::Color::Red);
            std::this_thread::sleep_for(std::chrono::milliseconds(80));
            WorkerFunction();
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
}

int main()
{
    TRACY_CALLSTACK(5);
    tracy::SetThreadName("main");
    // Atomic flag to stop the thread
    std::atomic<bool> stopThread(false);

    // Start the thread
    std::thread workerThread(threadFunction, std::ref(stopThread));

    while (!_kbhit())
    {
        {
            std::lock_guard<std::mutex> LockGuard(mutex);
            ZoneScopedN("Main");
            std::cout << "Hello World!\n";
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        {
            FrameMarkStart("frame");
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
            FrameMarkEnd("frame");
        }
    }

    // Stop the thread and wait for it to finish
    stopThread = true;
    workerThread.join();
}
