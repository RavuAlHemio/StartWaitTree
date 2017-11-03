/**
 * @file StartWaitTree.c
 *
 * Starts a process and waits until it and all its descendants have terminated.
 *
 * The mechanism used is a Windows NT job object. Waiting for processes to terminate is done using a completion port.
 *
 * To escape the vastly annoying dependency on the compiler-specific version of the C runtime library (which requires
 * the corresponding redistributable to be installed), care has been taken to depend only on the operating system
 * libraries; currently, this program only depends on functions from kernel32.dll.
 */

#include <stdbool.h>
#include <wchar.h>
#include <windows.h>

/**
 * @brief Skips the first token in the command line, including leading and trailing whitespace, and returns a pointer
 *        to the location in the string at which the next token starts (or to the terminating NUL character if it is
 *        encountered).
 */
const wchar_t *skipCommandLineToken(const wchar_t *token)
{
    const wchar_t *walker = token;

    // skip leading whitespace
    while (walker[0] == L' ')
    {
        ++walker;
    }

    bool escaping = false;
    bool quoting = false;
    for (;; ++walker)
    {
        if (walker[0] == '\0')
        {
            // we're done
            return walker;
        }
        
        if (escaping)
        {
            // don't interpret this character, but reset the escaping flag
            escaping = false;
        }
        else if (walker[0] == L'"')
        {
            // toggle quoting
            quoting = !quoting;
        }
        else if (walker[0] == L'\\')
        {
            // flag escaping
            escaping = true;
        }
        else if (walker[0] == L' ' && !quoting)
        {
            // this is it, found the space
            break;
        }
    }

    // skip trailing whitespace
    while (walker[0] == L' ')
    {
        ++walker;
    }

    return walker;
}

/**
 * @brief Calculates the length of the wide character string by searching for the NUL character.
 */
size_t wideStringLength(const wchar_t *str)
{
    size_t count = 0;
    while (str[0] != L'\0')
    {
        ++count;
        ++str;
    }
    return count;
}

/**
 * @brief Writes a string to the given console or file handle.
 */
bool writeToConsoleOrFile(HANDLE output, const wchar_t *str)
{
    DWORD neverMind;
    if (GetConsoleMode(output, &neverMind) == 0)
    {
        // console redirected to a file
        return (WriteFile(output, str, (DWORD)wideStringLength(str) * sizeof(str[0]), &neverMind, NULL) != 0);
    }
    else
    {
        // regular console
        return (WriteConsoleW(output, str, (DWORD)wideStringLength(str), &neverMind, NULL) != 0);
    }
}

bool appendToString(wchar_t *appendToMe, size_t appendToMeMaxSize, const wchar_t *newTail)
{
    if (wideStringLength(appendToMe) + wideStringLength(newTail) + 1 > appendToMeMaxSize)
    {
        return false;
    }
    wchar_t *insertionPoint = appendToMe + wideStringLength(appendToMe);
    while (newTail[0] != L'\0')
    {
        insertionPoint[0] = newTail[0];
        ++insertionPoint;
        ++newTail;
    }

    // NUL-terminate!
    insertionPoint[0] = L'\0';

    return true;
}

bool appendHexToString(wchar_t *appendToMe, size_t appendToMeMaxSize, DWORD appendAsHex)
{
    if (wideStringLength(appendToMe) + 9 > appendToMeMaxSize)
    {
        return false;
    }

    wchar_t *insertionPoint = appendToMe + wideStringLength(appendToMe);
    for (size_t i = 0; i < 8; ++i)
    {
        wchar_t nibble = (wchar_t)((appendAsHex >> ((7 - i) * 4)) & 0xF);
        if (nibble < 10)
        {
            insertionPoint[i] = (nibble + L'0');
        }
        else
        {
            insertionPoint[i] = (nibble - 10 + L'a');
        }
    }
    insertionPoint[8] = L'\0';
    return true;
}

/**
 * @brief Outputs a description of the given error prefixed with the supplied string and terminates the process.
 */
_declspec(noreturn)
void explode(DWORD err, const wchar_t *prefix)
{
    wchar_t *windowsErrorBuffer = NULL;
    wchar_t *completeErrorBuffer;

    HANDLE heap = GetProcessHeap();
    if (heap == NULL)
    {
        // somebody stole our heap!
        ExitProcess(err);
    }

    HANDLE stdErr = GetStdHandle(STD_ERROR_HANDLE);
    if (stdErr == NULL)
    {
        // oh well, we tried
        ExitProcess(err);
    }

    if (FormatMessageW(
        FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
        NULL,
        err,
        MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
        (wchar_t *)&windowsErrorBuffer,
        0,
        NULL
    ) == 0)
    {
        // can't do much here
        ExitProcess(err);
    }

    size_t completeErrorBufferCharacters = wideStringLength(prefix) + wideStringLength(L": [0x0123abcd] ") + wideStringLength(windowsErrorBuffer) + 1;
    completeErrorBuffer = HeapAlloc(heap, 0, completeErrorBufferCharacters * sizeof(wchar_t));
    if (completeErrorBuffer == NULL)
    {
        // no allocation...
        ExitProcess(err);
    }

    completeErrorBuffer[0] = L'\0';
    appendToString(completeErrorBuffer, completeErrorBufferCharacters, prefix);
    appendToString(completeErrorBuffer, completeErrorBufferCharacters, L": [0x");
    appendHexToString(completeErrorBuffer, completeErrorBufferCharacters, err);
    appendToString(completeErrorBuffer, completeErrorBufferCharacters, L"] ");
    appendToString(completeErrorBuffer, completeErrorBufferCharacters, windowsErrorBuffer);

    writeToConsoleOrFile(stdErr, completeErrorBuffer);

    HeapFree(heap, 0, completeErrorBuffer);
    LocalFree(windowsErrorBuffer);
    ExitProcess(err);
}

void zeroMemory(void *ptr, size_t byteCount)
{
    char *c = (char *)ptr;
    for (size_t i = 0; i < byteCount; ++i)
    {
        c[i] = 0;
    }
}

/**
 * @brief The main entry point of the application, bypassing the C Runtime.
 */
int noCrtMain(void)
{
    // take the command line (with our own name stripped away)
    const wchar_t *fullCommandLine = GetCommandLineW();
    const wchar_t *commandLineNotMe = skipCommandLineToken(fullCommandLine);

    // <nothing>, -?, -h, -H, /?, /h, /H => usage
    bool showUsage = false;
    if (wideStringLength(commandLineNotMe) == 0)
    {
        showUsage = true;
    }
    if (commandLineNotMe[0] == L'-' || commandLineNotMe[0] == L'/')
    {
        if (commandLineNotMe[1] == L'?' || commandLineNotMe[1] == L'h' || commandLineNotMe[1] == L'H')
        {
            if (commandLineNotMe[2] == L'\0')
            {
                showUsage = true;
            }
        }
    }

    if (showUsage)
    {
        // output usage
        HANDLE stdOut = GetStdHandle(STD_OUTPUT_HANDLE);
        wchar_t *body =
            L"Usage: StartWaitTree.exe PROGRAM [ARGUMENTS...]\r\n\r\n"
            L"Starts a program with the given arguments and waits until it and all its descendants have terminated.\r\n";
        DWORD whatever;
        WriteConsoleW(stdOut, body, (DWORD)wideStringLength(body), &whatever, NULL);
        ExitProcess(0);
    }

    // prepare the job object that will stand guard over the descendants
    HANDLE jobObject = CreateJobObjectW(NULL, NULL);
    if (jobObject == NULL)
    {
        explode(GetLastError(), L"Could not create job object");
    }

    // fetch an I/O completion port that will receive the notification that all descendants have terminated
    HANDLE completionPort = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, 1);
    if (completionPort == NULL)
    {
        DWORD err = GetLastError();
        CloseHandle(jobObject);
        explode(err, L"Could not create completion port");
    }

    // marry the two
    JOBOBJECT_ASSOCIATE_COMPLETION_PORT assPort;
    assPort.CompletionKey = jobObject;
    assPort.CompletionPort = completionPort;
    if (!SetInformationJobObject(jobObject, JobObjectAssociateCompletionPortInformation, &assPort, sizeof(assPort)))
    {
        DWORD err = GetLastError();
        CloseHandle(completionPort);
        CloseHandle(jobObject);
        explode(err, L"Could not assign job object to completion port");
    }

    // launch the child process in a suspended state
    STARTUPINFOW startupInfo;
    zeroMemory(&startupInfo, sizeof(startupInfo));
    startupInfo.cb = sizeof(startupInfo);

    PROCESS_INFORMATION processInfo;
    if (!CreateProcessW(
        NULL,
        (wchar_t *)commandLineNotMe,
        NULL,
        NULL,
        TRUE,
        CREATE_SUSPENDED,
        NULL,
        NULL,
        &startupInfo,
        &processInfo
    ))
    {
        DWORD err = GetLastError();
        CloseHandle(completionPort);
        CloseHandle(jobObject);
        explode(err, L"Could not start process");
    }

    // assign the process to the job object
    if (!AssignProcessToJobObject(jobObject, processInfo.hProcess))
    {
        DWORD err = GetLastError();
        CloseHandle(processInfo.hThread);
        CloseHandle(processInfo.hProcess);
        CloseHandle(completionPort);
        CloseHandle(jobObject);
        explode(err, L"Failed to assign process to job object");
    }

    // resume the process
    if (ResumeThread(processInfo.hThread) == -1)
    {
        DWORD err = GetLastError();
        CloseHandle(processInfo.hThread);
        CloseHandle(processInfo.hProcess);
        CloseHandle(completionPort);
        CloseHandle(jobObject);
        explode(err, L"Failed to awaken the newly started process");
    }

    // I don't care about these two handles anymore
    CloseHandle(processInfo.hThread);
    CloseHandle(processInfo.hProcess);

    // wait for process tree to end it all
    for (;;)
    {
        DWORD completionCode;
        ULONG_PTR completionKey;
        OVERLAPPED *overlapped;
        if (!GetQueuedCompletionStatus(completionPort, &completionCode, &completionKey, &overlapped, INFINITE))
        {
            DWORD err = GetLastError();
            CloseHandle(completionPort);
            CloseHandle(jobObject);
            explode(err, L"Failed to get queued completion status");
        }
        if ((HANDLE)completionKey == jobObject)
        {
            if (completionCode == JOB_OBJECT_MSG_ACTIVE_PROCESS_ZERO)
            {
                // no more processes
                break;
            }
        }
    }

    // drop the port and job object
    CloseHandle(completionPort);
    CloseHandle(jobObject);

    ExitProcess(0);
}
