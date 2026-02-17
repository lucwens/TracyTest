// TracyTest.cpp : This file contains the 'main' function. Program execution begins and ends there.
//

#define TRACY_MANUAL_LIFETIME
#define TRACY_DELAYED_INIT
#define TRACY_CALLSTACK 5

#include <iostream>
#include <Tracy.hpp>
#ifdef _WIN32
#include <windows.h>
#include <conio.h>
#else
#include <unistd.h>
#include <termios.h>
#include <sys/ioctl.h>
#include <stdio.h>

struct TerminalState
{
    struct termios original;
    bool           initialized = false;

    TerminalState()
    {
        if (tcgetattr(STDIN_FILENO, &original) == 0)
        {
            initialized         = true;
            struct termios term = original;
            term.c_lflag &= ~ICANON;
            tcsetattr(STDIN_FILENO, TCSANOW, &term);
            setbuf(stdin, NULL);
        }
    }

    ~TerminalState()
    {
        if (initialized)
        {
            tcsetattr(STDIN_FILENO, TCSANOW, &original);
        }
    }
};

int _kbhit()
{
    static TerminalState state;
    int                  bytesWaiting;
    ioctl(STDIN_FILENO, FIONREAD, &bytesWaiting);
    return bytesWaiting;
}
#endif
#include <thread>
#include <chrono>
#include <atomic>
#include <cmath>
#include <mutex>

std::mutex mutex;

double t        = 0.0;
double timestep = 0.01;
double s        = 0.0;

void WorkerFunction(void)
{
    ZoneScopedS(30);
    for (int i = 0; i < 100; i++)
    {
        char Message[201];
        t += timestep;
        s = sin(t);
        TracyPlot("sin", s);
        sprintf_s(Message, 200, "Sinus value is %0.3f", s);
        TracyMessage(Message, strlen(Message));
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
            std::lock_guard<std::mutex> lock(mutex);
            ZoneScopedNC("thread", tracy::Color::Red);
            std::this_thread::sleep_for(std::chrono::milliseconds(80));
            WorkerFunction();
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
}

constexpr bool hideConsole = true;

int main()
{
    if (hideConsole)
        ShowWindow(GetConsoleWindow(), SW_HIDE);

    tracy::StartupProfiler();

    // Launch tracy-capture as a child process to record profiling data
    PROCESS_INFORMATION pi = {};
    STARTUPINFOA        si = {};
    si.cb                  = sizeof(si);
    char cmdLine[]         = "tracy-capture.exe -o test.tracy -f";
    BOOL captureStarted    = CreateProcessA(nullptr, cmdLine, nullptr, nullptr, FALSE, 0, nullptr, nullptr, &si, &pi);
    if (captureStarted)
    {
        // Give capture time to connect to the profiler
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
        std::cout << "tracy-capture started, recording to test.tracy\n";
    }
    else
    {
        std::cerr << "Warning: Could not start tracy-capture.exe (not on PATH?)\n";
    }

    tracy::SetThreadName("main");
    // Atomic flag to stop the thread
    std::atomic<bool> stopThread(false);

    // Start the thread
    std::thread workerThread(threadFunction, std::ref(stopThread));

    std::cout << "Press 's' to stop profiling and save, 'q' to quit.\n";
    bool profiling = true;
    bool running   = true;
    auto startTime = std::chrono::steady_clock::now();

    // Helper lambda to stop profiling and save
    auto stopProfiling = [&]()
    {
        // Stop the worker thread first so no Tracy macros are in flight
        stopThread = true;
        workerThread.join();

        profiling = false;
        std::cout << "Stopping Tracy profiler...\n";
        tracy::ShutdownProfiler();

        if (captureStarted)
        {
            std::cout << "Waiting for tracy-capture to save test.tracy...\n";
            WaitForSingleObject(pi.hProcess, 10000);
            CloseHandle(pi.hProcess);
            CloseHandle(pi.hThread);
            captureStarted = FALSE;
            std::cout << "Profile saved to test.tracy\n";
        }
    };

    while (running)
    {
        auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(
                            std::chrono::steady_clock::now() - startTime)
                            .count();

        if (hideConsole)
        {
            // Auto-stop profiling at 3 seconds, auto-quit at 5 seconds
            if (profiling && elapsed >= 3)
                stopProfiling();
            if (elapsed >= 5)
            {
                running = false;
                continue;
            }
        }
        else if (_kbhit())
        {
            int key = _getch();
            if ((key == 's' || key == 'S') && profiling)
                stopProfiling();
            else if (key == 'q' || key == 'Q')
                running = false;
        }

        if (profiling)
        {
            {
                std::lock_guard<std::mutex> lock(mutex);
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
        else
        {
            std::cout << "Running (profiling stopped)...\n";
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
    }

    // Stop the thread and wait for it to finish (if not already joined)
    if (workerThread.joinable())
    {
        stopThread = true;
        workerThread.join();
    }

    // If profiling is still active (quit without saving), shut it down now
    if (profiling)
    {
        tracy::ShutdownProfiler();

        if (captureStarted)
        {
            std::cout << "Waiting for tracy-capture to save test.tracy...\n";
            WaitForSingleObject(pi.hProcess, 10000);
            CloseHandle(pi.hProcess);
            CloseHandle(pi.hThread);
            std::cout << "Profile saved to test.tracy\n";
        }
    }
}
