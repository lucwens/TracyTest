// TracyTest.cpp : This file contains the 'main' function. Program execution begins and ends there.
//

#include <iostream>
#include <Tracy.hpp>
#include <conio.h>
#include <thread>
#include <chrono>
#include <atomic>

// Thread function that prints message and sleeps
void threadFunction(std::atomic<bool> &stopFlag)
{
    while (!stopFlag)
    {
        {
            ZoneScoped;
            std::cout << "Hello from thread\n";
            std::this_thread::sleep_for(std::chrono::milliseconds(200));
            FrameMark;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
    }
}

int main()
{
    // Atomic flag to stop the thread
    std::atomic<bool> stopThread(false);

    // Start the thread
    std::thread workerThread(threadFunction, std::ref(stopThread));

    while (!_kbhit())
    {
        {
            ZoneScoped;
            std::cout << "Hello World!\n";
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            FrameMark;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    // Stop the thread and wait for it to finish
    stopThread = true;
    workerThread.join();
}
