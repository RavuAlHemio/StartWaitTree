# StartWaitTree for Windows

`StartWaitTree` is a program which launches another program and waits until it
and all its descendants have terminated before terminating itself.

## Building

### Building using MSBuild

The easiest way is to build using MSBuild, which is the build system used by
Visual Studio.

    C:\Program Files (x86)\MSBuild\14.0\Bin\MSBuild.exe
    
### Building using `cl.exe`

You can also build the program using `cl.exe`, Microsoft's C compiler.

    cl.exe /FeStartWaitTree.exe .\StartWaitTree.c /link /nodefaultlib /subsystem:console /entry:noCrtMain kernel32.lib

## Testing

A contrived example to test the functionality is provided by `cmd.exe`'s `start`
command. If executed without the `/wait` option, `start` returns immediately
after launching the process. Since `start` is a `cmd.exe` command and not an
actual program, it must be started via `cmd.exe`; this is possible using the
`/c` option (*run command and then terminate*). As an example for a
longer-running process, we can start Notepad. Thus:

    StartWaitTree.exe cmd.exe /c notepad.exe

While `cmd.exe` terminates immediately, `StartWaitTree.exe` only terminates once
`notepad.exe` exits.

## No-CRT design

Since the Visual C++ Redistributable is a rather annoying dependency for a tiny
utility such as this, `StartWaitTree` has been implemented in strict avoidance
of it. In fact, it depends only on functions of `kernel32.dll`.

(The *Universal C Runtime Library*, or *UCRT*, while solving the problem with
the numerous Visual C++ Redistributables, was not an option for this project as
it only ships natively with Windows 10 and would require installation on older
Windows versions.)

`StartWaitTree` should be compatible with all Windows versions since Windows
2000, where the concept of job objects was introduced.