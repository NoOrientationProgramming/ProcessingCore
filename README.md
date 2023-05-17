
![GitHub](https://img.shields.io/github/license/NoOrientationProgramming/ProcessingCore?style=plastic)
![Lines of code](https://img.shields.io/tokei/lines/github/NoOrientationProgramming/ProcessingCore?style=plastic&type=Cpp)

These files provide a basic structure for almost all applications as well as a powerful debugging system.

But still: Your coding style is not restricted! Similar to LaTeX: Focus on your content, spend less time on structural topics and debugging.

# Status

- Mature code created in 2018
- Finished

# Core Advantages

When using the Processing() class the system structure is recursive. This has a big and very beneficial impact during development, runtime, documentation and communication with other team members independent of their background.

There is no low- or high-level code. Just **one essential looped function**: `process()` .. everywhere

```cpp
Success Supervising::process()
{
  ++mCounter; // do something wild
  return Pending;
}
```

# Debugging

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

After that you can connect to three different TCP channels.

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
    <img src="https://raw.githubusercontent.com/NoOrientationProgramming/ProcessingTutorials/main/doc/channel-dbg-1_tree-proc.png"/>
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

You want to trigger something? Just register a command **anywhere** in you application.

```cpp
void yourCommand(const char *pArgs, char *pBuf, char *pBufEnd)
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
    <img src="https://raw.githubusercontent.com/NoOrientationProgramming/ProcessingTutorials/main/doc/channel-dbg-3_cmd.png"/>
  </kbd>
</p>

# Supported Targets

- STM32G030
  - Bare Metal
- ESP32
- Linux / Raspberry Pi
  - GCC
- Windows
  - MinGW
  - MSVC

# Learn how to use it

The [Processing() Tutorials](https://github.com/NoOrientationProgramming/ProcessingTutorials) provide more information on how to enter this wonderful (recursive) world ..

# How to add to your project

`git submodule add https://github.com/NoOrientationProgramming/ProcessingCore.git`
