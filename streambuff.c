/*
 * Stream Buffer
 * Copyright (c) 2023 "dEajL3kA" <Cumpoing79@web.de>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy of this software and
 * associated documentation files (the "Software"), to deal in the Software without restriction,
 * including without limitation the rights to use, copy, modify, merge, publish, distribute,
 * sub license, and/or sell copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions: The above copyright notice and this
 * permission notice shall be included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT
 * NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
 * DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT
 * OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */
#define WIN32_LEAN_AND_MEAN 1
#include <Windows.h>

#define BUFF_LEN 4096U
#define BUFF_CNT   64U

static CRITICAL_SECTION g_mutex;
static CONDITION_VARIABLE g_condv_free, g_condv_used;
static BYTE g_buffer[BUFF_CNT][BUFF_LEN];
static DWORD g_length[BUFF_CNT];
static SIZE_T g_used = 0U, g_free = BUFF_CNT;
static BOOL g_stopping = FALSE;

#define INCREMENT(VAR) do { if (++(VAR) >= BUFF_CNT) { (VAR) = 0U; } } while (0)

static int get_envvar(const wchar_t* const name)
{
    wchar_t buffer[128U];
    const DWORD result = GetEnvironmentVariableW(name, buffer, ARRAYSIZE(buffer));
    if ((result > 0U) && (result < ARRAYSIZE(buffer)))
    {
        const wchar_t *ptr = buffer;
        while ((*ptr) && (*ptr <= 0x20))
        {
            ++ptr;
        }
        if (!lstrcmpiW(ptr, L"0"))
        {
            return 0;
        }
        if (!lstrcmpiW(ptr, L"1"))
        {
            return 1;
        }
        if (!lstrcmpiW(ptr, L"2"))
        {
            return 2;
        }
    }
    return 0;
}

static BOOL WINAPI ctrl_handler(DWORD ctrlType)
{
    switch (ctrlType)
    {
    case CTRL_C_EVENT:
    case CTRL_BREAK_EVENT:
        return TRUE;
    }
    return FALSE;
}

static BOOL write_all(const HANDLE h_stream, const BYTE *const data, const DWORD length)
{
    DWORD num_written;
    for (DWORD offset = 0U; offset < length; offset += num_written)
    {
        if (!WriteFile(h_stream, &data[offset], length - offset, &num_written, NULL))
        {
            return FALSE;
        }
    }
    return TRUE;
}

static DWORD WINAPI write_thread(VOID *const param)
{
    const HANDLE h_stream = (HANDLE)param;
    SIZE_T index = 0U;
    BOOL stopping = FALSE;

    for (;;)
    {
        EnterCriticalSection(&g_mutex);
        while ((g_used < 1U) && (!g_stopping))
        {
            SleepConditionVariableCS(&g_condv_used, &g_mutex, INFINITE);
        }
        if (!(stopping = (g_used < 1U)))
        {
            --g_used;
        }
        LeaveCriticalSection(&g_mutex);

        if (stopping)
        {
            break;
        }

        if (!write_all(h_stream, g_buffer[index], g_length[index]))
        {
            break;
        }

        EnterCriticalSection(&g_mutex);
        ++g_free;
        LeaveCriticalSection(&g_mutex);

        WakeAllConditionVariable(&g_condv_free);
        INCREMENT(index);
    }

    return 0U;
}

int main(void)
{
    HANDLE h_stdin = INVALID_HANDLE_VALUE, h_stdout = INVALID_HANDLE_VALUE, h_thread = NULL;
    SIZE_T index = 0U;
    int exit_code = 1U;

    SetConsoleCtrlHandler(ctrl_handler, TRUE);

    if (!InitializeCriticalSectionAndSpinCount(&g_mutex, MAXSHORT))
    {
        goto do_exit;
    }

    InitializeConditionVariable(&g_condv_free);
    InitializeConditionVariable(&g_condv_used);

    h_stdin = GetStdHandle(STD_INPUT_HANDLE);
    if ((h_stdin == NULL) || (h_stdin == INVALID_HANDLE_VALUE))
    {
        goto do_exit;
    }

    h_stdout = GetStdHandle(STD_OUTPUT_HANDLE);
    if ((h_stdout == NULL) || (h_stdout == INVALID_HANDLE_VALUE))
    {
        goto do_exit;
    }

    const int sleep_mode = get_envvar(L"STREAMBUFF_SLEEP");

    h_thread = CreateThread(NULL, 0U, write_thread, h_stdout, 0U, NULL);
    if (!h_thread)
    {
        goto do_exit;
    }

    for (;;)
    {
        EnterCriticalSection(&g_mutex);
        while (g_free < 1U)
        {
            SleepConditionVariableCS(&g_condv_free, &g_mutex, INFINITE);
        }
        --g_free;
        LeaveCriticalSection(&g_mutex);

        if (!ReadFile(h_stdin, g_buffer[index], BUFF_LEN, &g_length[index], NULL))
        {
            break;
        }

        if (g_length[index] > 0U)
        {
            EnterCriticalSection(&g_mutex);
            ++g_used;
            LeaveCriticalSection(&g_mutex);
            WakeAllConditionVariable(&g_condv_used);
            INCREMENT(index);
        }
        else
        {
            if (GetFileType(h_stdin) != FILE_TYPE_PIPE)
            {
                break; /* Pipe may read zero bytes, even if more data can become available later */
            }
        }

        if (sleep_mode > 0)
        {
            Sleep((sleep_mode > 1) ? 1U : 0U);
        }
    }

    EnterCriticalSection(&g_mutex);
    g_stopping = TRUE;
    LeaveCriticalSection(&g_mutex);

    WakeAllConditionVariable(&g_condv_used);

    if (WaitForSingleObject(h_thread, 15000U) != WAIT_OBJECT_0)
    {
        TerminateThread(h_thread, 1U);
    }

    exit_code = 0;

do_exit:

    if (h_thread)
    {
        CloseHandle(h_thread);
    }

    return exit_code;
}

#ifndef _DEBUG
int mainCRTStartup(void)
{
    SetErrorMode(SEM_FAILCRITICALERRORS);
    ExitProcess(main());
}
#endif
