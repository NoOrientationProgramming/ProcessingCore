
![GitHub](https://img.shields.io/github/license/NoOrientationProgramming/ProcessingCore?style=plastic)
<!-- ![Lines of code](https://img.shields.io/tokei/lines/github/NoOrientationProgramming/ProcessingCore?style=plastic) -->

These files provide a basic structure for almost all applications and a powerful debugging system.

However, your coding style is not restricted in any way! Similar to LaTeX, focus on your content and spend less time on structural topics and debugging.

## Advantages

- **Reduced boilerplate code**. Just add lines of code, which are beneficial for the tasks
- **Relieved memory allocations**. Lifetime of requested memory very often matches the lifetime of the task itself, which is monitored. Therefore reducing the risk of memory leaks!
- **Application level debugging**. Spend less time on searching for strange bugs due to the improved overview of the system. Just focus on your tasks
- **Increased productivity**. A long-term relaxed and still speedy coding experience for *everyone*
- **Quality control**. Your code can be characterized and rated instead of calling it "clean" or "dirty"
- **Simple and useful documentation**. The structure allows for the creation of a visual description of your project which can be understood by tech and non-tech people.
- **Increased maintainability**. You get an easy-to-read code at any time. This remains true regardless of the size or complexity of the system. Use a set of simple instructions to reach the target architecture starting from the current state
- **Big support range**. Starting from small to very complex systems

## Requirements

- C++ standard as low as C++11 can be used
- On Microcontrollers: Minimum of 32k flash memory
- Nothing else

## Status

- Mature code created in 2018
- Finished

## Intro

When using the Processing() class the entire system structure is recursive. This has a big and very beneficial impact during development, runtime, documentation and communication with other team members independent of their background.

There is no low- or high-level code. Just **one essential looped function**: `process()` .. everywhere

```cpp
Success Supervising::process()
{
    ++mCounter; // do something wild
    return Pending;
}
```

## Debugging

Only a few lines of code are necessary to get a powerful debugging service integrated directly into your application. In this case we use the **optional** function `initialize()`

```cpp
Success Supervising::initialize()
{
    SystemDebugging *pDbg;

    pDbg = SystemDebugging::create(this);
    if (!pDbg)
        return procErrLog(-1, "could not create process");

    start(pDbg);

    return Positive;
}
```

After that, you can connect to three different TCP channels.

**Optionally**: Each process can show some useful stuff by creating a `processInfo()` function. The rest of your code is unaffected.

```cpp
void Supervising::processInfo(char *pBuf, char *pBufEnd)
{
    dInfo("Counter\t\t%d", mCounter);
}
```

### Channel 1 - Process Tree

Just one quick look is needed to see how your **entire** system is doing.

<p align="center">
  <kbd>
    <img src="https://raw.githubusercontent.com/NoOrientationProgramming/ProcessingTutorials/main/doc/channel-dbg-1_tree-proc.png" style="max-width:100%"/>
  </kbd>
</p>

### Channel 2 - Process Log

What is happening. But much more important: **Who** is doing what and when

<p align="center">
  <kbd>
    <img src="https://raw.githubusercontent.com/NoOrientationProgramming/ProcessingTutorials/main/doc/channel-dbg-2_log.png"/>
  </kbd>
</p>

### Channel 3 - Command interface

Do you want to trigger something? Just register a command **anywhere** in your application.

```cpp
void yourCommand(char *pArgs, char *pBuf, char *pBufEnd)
{
    dInfo("Executed with '%s'", pArgs);
}

Success Supervising::initialize()
{
    ...

    cmdReg("test", yourCommand);

    return Positive;
}
```

<p align="center">
  <kbd>
    <img src="https://raw.githubusercontent.com/NoOrientationProgramming/ProcessingTutorials/main/doc/channel-dbg-3_cmd.png" style="max-width:100%"/>
  </kbd>
</p>

## Supported Targets

- STM32G030
  - Bare Metal
- ESP32
- Linux / Raspberry Pi
  - GCC
- FreeBSD
- Windows
  - MinGW
  - MSVC

## Learn how to use it

The [Tutorials](https://github.com/NoOrientationProgramming/ProcessingTutorials) provide more information on how to delve into this wonderful (recursive) world ..

## How to add to your project

`git submodule add https://github.com/NoOrientationProgramming/ProcessingCore.git`
