# TracyTest - Programmatic Tracy Profiling Control

This project demonstrates how to programmatically control the start and stop of [Tracy Profiler](https://github.com/wolfpld/tracy) data capture, independent of any console window or user interaction.

## 1. How It Works

### 1.1 Key Preprocessor Defines

Tracy must be configured for manual lifetime control:

```cpp
#define TRACY_MANUAL_LIFETIME   // profiler does not start automatically
#define TRACY_DELAYED_INIT      // defer internal initialization
#define TRACY_CALLSTACK 5       // capture call stacks (depth 5)
```

These defines **must appear before** `#include <Tracy.hpp>`.

### 1.2 Profiler Lifecycle

The profiler lifecycle is fully controlled in code:

1. **Start** - `tracy::StartupProfiler()` initializes the profiler. No Tracy macros (`ZoneScoped`, `FrameMark`, `TracyPlot`, etc.) may be called before this.
2. **Capture** - `tracy-capture.exe` is launched as a child process to record data to a `.tracy` file. It connects to the profiler over a local socket.
3. **Stop** - All threads using Tracy macros must be stopped first. Then `tracy::ShutdownProfiler()` disconnects the capture tool, which triggers it to flush and save the file.
4. **Wait** - `WaitForSingleObject` on the capture process handle ensures the `.tracy` file is fully written before the program exits.

### 1.3 Two Operating Modes

A compile-time flag controls the behavior:

```cpp
constexpr bool hideConsole = true;
```

| `hideConsole` | Console Window | Profiling Control |
|---|---|---|
| `false` | Visible | Press **s** to stop profiling and save, **q** to quit |
| `true` | Hidden | Auto-stops profiling after 3 seconds, auto-quits after 5 seconds |

When the console is hidden, the program runs silently and uses elapsed time to trigger the stop and quit actions. This makes it suitable for automated test runs, CI pipelines, or embedding into other applications.

### 1.4 Critical Shutdown Order

The shutdown sequence **must** follow this exact order to avoid assertion failures:

1. **Stop all worker threads** that use Tracy macros (set atomic flag, join threads)
2. **Stop the main loop** from calling Tracy macros (guard with a `profiling` flag)
3. **Call `tracy::ShutdownProfiler()`** only after no Tracy macros are in flight
4. **Wait for `tracy-capture.exe`** to finish writing the output file

Calling `tracy::ShutdownProfiler()` while any thread is still inside a `ZoneScoped` block will trigger an assertion failure in `tracy::GetProfilerData`.

## 2. Applying This Technique to Other Programs

### 2.1 Console Applications

**Step 1: Add Tracy to your project**

Add the Tracy client source (or the `tracy` submodule) to your project. Include `Tracy.hpp` in files you want to profile.

**Step 2: Add the preprocessor defines**

At the top of your main source file (before any Tracy include):

```cpp
#define TRACY_MANUAL_LIFETIME
#define TRACY_DELAYED_INIT
#define TRACY_CALLSTACK 5  // optional, set desired stack depth
```

Alternatively, define these in your project settings / CMakeLists.txt for all translation units.

**Step 3: Start the profiler explicitly**

At the beginning of `main()`:

```cpp
tracy::StartupProfiler();
```

**Step 4: Launch tracy-capture as a child process**

```cpp
PROCESS_INFORMATION pi = {};
STARTUPINFOA        si = {};
si.cb                  = sizeof(si);
char cmdLine[]         = "tracy-capture.exe -o output.tracy -f";
BOOL captureStarted = CreateProcessA(
    nullptr, cmdLine, nullptr, nullptr,
    FALSE, 0, nullptr, nullptr, &si, &pi);

if (captureStarted)
    std::this_thread::sleep_for(std::chrono::milliseconds(500)); // let it connect
```

The `-f` flag tells tracy-capture to overwrite the output file if it already exists.

**Step 5: Add Tracy instrumentation to your code**

Use Tracy macros in the code sections you want to profile:

```cpp
void MyFunction()
{
    ZoneScopedN("MyFunction");    // named zone
    // ... your code ...
    TracyPlot("metric", value);   // plot a value over time
    TracyMessage(msg, len);       // log a message
}
```

**Step 6: Implement the shutdown sequence**

When you want to stop profiling:

```cpp
// 1. Stop all threads that use Tracy macros
stopFlag = true;
workerThread.join();

// 2. Shut down the profiler
tracy::ShutdownProfiler();

// 3. Wait for tracy-capture to save the file
if (captureStarted)
{
    WaitForSingleObject(pi.hProcess, 10000);  // 10s timeout
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
}
```

**Step 7: Choose your trigger mechanism**

Depending on your use case, trigger the shutdown with:

- **Keyboard input** - `_kbhit()` / `_getch()` for interactive console apps
- **Timer** - `std::chrono::steady_clock` for automated/headless runs
- **External signal** - a named event, file, or pipe for cross-process control
- **Application event** - after a specific operation completes (e.g., file processed, test finished)

### 2.2 MFC Applications

MFC apps have no console by default, making timer-based or event-based control the natural fit.

**Step 1: Add Tracy defines to `stdafx.h` or `pch.h`**

```cpp
#define TRACY_MANUAL_LIFETIME
#define TRACY_DELAYED_INIT
#define TRACY_CALLSTACK 5
#include <Tracy.hpp>
```

Also add `TRACY_ENABLE` to your project preprocessor definitions (for both Debug and Release, or whichever configurations you want to profile).

**Step 2: Start the profiler in `InitInstance()`**

In your `CWinApp`-derived class:

```cpp
BOOL CMyApp::InitInstance()
{
    tracy::StartupProfiler();

    // Launch tracy-capture
    PROCESS_INFORMATION pi = {};
    STARTUPINFOA        si = {};
    si.cb                  = sizeof(si);
    char cmdLine[]         = "tracy-capture.exe -o profile.tracy -f";
    m_captureStarted = CreateProcessA(
        nullptr, cmdLine, nullptr, nullptr,
        FALSE, 0, nullptr, nullptr, &si, &m_capturePi);

    Sleep(500); // let capture connect

    // ... rest of InitInstance ...
}
```

Store `m_capturePi` and `m_captureStarted` as member variables of your app class.

**Step 3: Instrument your message handlers and worker code**

```cpp
void CMyView::OnDraw(CDC* pDC)
{
    ZoneScopedN("OnDraw");
    // ... drawing code ...
}

LRESULT CMainFrame::OnMyMessage(WPARAM wp, LPARAM lp)
{
    ZoneScopedN("OnMyMessage");
    // ... handler code ...
}
```

**Step 4: Stop profiling on a menu command or timer**

Add a menu item (e.g., "Stop Profiling") or use `SetTimer()`:

```cpp
// Menu handler approach
void CMainFrame::OnStopProfiling()
{
    // Stop any worker threads using Tracy macros first
    StopWorkerThreads();

    tracy::ShutdownProfiler();

    if (m_captureStarted)
    {
        WaitForSingleObject(m_capturePi.hProcess, 10000);
        CloseHandle(m_capturePi.hProcess);
        CloseHandle(m_capturePi.hThread);
        m_captureStarted = FALSE;
    }
    AfxMessageBox(_T("Profile saved."));
}

// Timer approach - stop after N seconds
void CMainFrame::OnTimer(UINT_PTR nIDEvent)
{
    if (nIDEvent == TIMER_STOP_PROFILING)
    {
        KillTimer(TIMER_STOP_PROFILING);
        OnStopProfiling();  // reuse the same logic
    }
}
```

**Step 5: Clean up in `ExitInstance()`**

As a safety net, ensure the profiler is shut down if the app closes without explicitly stopping:

```cpp
int CMyApp::ExitInstance()
{
    if (m_profilingActive)
    {
        tracy::ShutdownProfiler();
        if (m_captureStarted)
        {
            WaitForSingleObject(m_capturePi.hProcess, 10000);
            CloseHandle(m_capturePi.hProcess);
            CloseHandle(m_capturePi.hThread);
        }
    }
    return CWinApp::ExitInstance();
}
```

## 3. Requirements

- [Tracy Profiler](https://github.com/wolfpld/tracy) client library (included as submodule in `tracy/`)
- `tracy-capture.exe` on PATH or in the working directory
- Windows (for `CreateProcessA` / `WaitForSingleObject`; Linux alternatives exist using `fork`/`waitpid`)

## 4. Viewing the Profile

Open the saved `.tracy` file with the Tracy Profiler GUI:

```
tracy-profiler.exe test.tracy
```
