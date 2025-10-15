// TracyTest.cpp : This file contains the 'main' function. Program execution begins and ends there.
//

#include <iostream>
#include <Tracy.hpp>
#include <conio.h>
#include <thread>
#include <chrono>

int main()
{
    while (!_kbhit())
    {
    ZoneScoped;
        std::cout << "Hello World!\n";
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    FrameMark;

    }
}
