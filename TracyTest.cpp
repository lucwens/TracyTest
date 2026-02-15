// TracyTest.cpp : This file contains the 'main' function. Program execution begins and ends there.
//

#define TRACY_MANUAL_LIFETIME
#define TRACY_DELAYED_INIT
#define TRACY_CALLSTACK 5

#include <iostream>
#include <Tracy.hpp>
#ifdef _WIN32
#  include <conio.h>
#else
#  include <unistd.h>
#  include <termios.h>
#  include <sys/ioctl.h>
#  include <stdio.h>

struct TerminalState {
    struct termios original;
    bool initialized = false;

    TerminalState() {
        if (tcgetattr(STDIN_FILENO, &original) == 0) {
            initialized = true;
            struct termios term = original;
            term.c_lflag &= ~ICANON;
            tcsetattr(STDIN_FILENO, TCSANOW, &term);
            setbuf(stdin, NULL);
        }
    }

    ~TerminalState() {
        if (initialized) {
            tcsetattr(STDIN_FILENO, TCSANOW, &original);
        }
    }
};

int _kbhit() {
    static TerminalState state;
    int bytesWaiting;
    ioctl(STDIN_FILENO, FIONREAD, &bytesWaiting);
    return bytesWaiting;
}
#endif
#include <thread>
#include <chrono>
#include <atomic>
#include <cmath>
#include <mutex>

TracyLockable(std ::mutex, mutex);

double t = 0.0;
double timestep = 0.01;
double s        = 0.0;

void WorkerFunction(void)
{
    for (int i = 0; i < 100; i++)
    {
        ZoneScopedS(5);
        t += timestep;
        s = sin(t);
        TracyPlot("sin", s);
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
            std ::lock_guard<LockableBase(std ::mutex)> lock(mutex);
            ZoneScopedNC("thread",tracy::Color::Red);
            std::this_thread::sleep_for(std::chrono::milliseconds(80));
            WorkerFunction();
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
}

int main()
{
    // Explicitly start the profiler
    tracy::StartupProfiler();

    tracy::SetThreadName("main");
    // Atomic flag to stop the thread
    std::atomic<bool> stopThread(false);

    // Start the thread
    std::thread workerThread(threadFunction, std::ref(stopThread));

    while (!_kbhit())
    {
        {
            std ::lock_guard<LockableBase(std ::mutex)> lock(mutex);
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

    // Explicitly stop the profiler.
    // This will flush data and disconnect from the server (tracy-capture),
    // which causes the capture tool to save and exit.
    tracy::ShutdownProfiler();
}
