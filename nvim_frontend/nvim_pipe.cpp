#include "nvim_pipe.h"
// #include "window_messages.h"
#include <Windows.h>
#include <msgpackpp/msgpackpp.h>
#include <plog/Log.h>
#include <stdint.h>
#include <thread>

static HANDLE _stdin_read = nullptr;
static HANDLE _stdin_write = nullptr;
void *NvimPipe::WriteHandle() { return _stdin_write; }
static HANDLE _stdout_read = nullptr;
void *NvimPipe::ReadHandle() { return _stdout_read; }
static HANDLE _stdout_write = nullptr;
static PROCESS_INFORMATION _process_info = {0};

std::thread _watch;

NvimPipe::NvimPipe() {}

NvimPipe::~NvimPipe() {
  DWORD exit_code;
  GetExitCodeProcess(_process_info.hProcess, &exit_code);

  if (exit_code == STILL_ACTIVE) {
    CloseHandle(_stdin_read);
    CloseHandle(_stdin_write);
    CloseHandle(_stdout_read);
    CloseHandle(_stdout_write);
    CloseHandle(_process_info.hThread);
    TerminateProcess(_process_info.hProcess, 0);
    WaitForSingleObject(_process_info.hProcess, INFINITE);
    CloseHandle(_process_info.hProcess);
  }

  if (_watch.joinable()) {
    _watch.join();
  }
}

// wchar_t command_line[] = L"nvim --embed";
bool NvimPipe::Launch(const wchar_t *command_line,
                      const on_terminated_t &callback) {
  SECURITY_ATTRIBUTES sec_attribs = {sizeof(SECURITY_ATTRIBUTES)};
  sec_attribs.bInheritHandle = true;
  if (!CreatePipe(&_stdin_read, &_stdin_write, &sec_attribs, 0)) {
    PLOGE << "(nvim) fail to CreatePipe for stdin";
    return false;
  }
  if (!CreatePipe(&_stdout_read, &_stdout_write, &sec_attribs, 0)) {
    PLOGE << "(nvim) fail to CreatePipe for stdout";
    return false;
  }

  STARTUPINFOW startup_info = {sizeof(STARTUPINFOW)};
  startup_info.dwFlags = STARTF_USESTDHANDLES;
  startup_info.hStdInput = _stdin_read;
  startup_info.hStdOutput = _stdout_write;
  startup_info.hStdError = _stdout_write;
  if (!CreateProcessW(nullptr, std::wstring(command_line).data(), nullptr,
                      nullptr, true, CREATE_NO_WINDOW, nullptr, nullptr,
                      &startup_info, &_process_info)) {
    PLOGE << "(nvim) fail to CreateProcess";

    LPVOID lpvMessageBuffer;
    FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM,
                  NULL, GetLastError(),
                  MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
                  (LPTSTR)&lpvMessageBuffer, 0, NULL);
    PLOGE << (const char *)lpvMessageBuffer;
    LocalFree(lpvMessageBuffer);

    return false;
  }

  _watch = std::thread([callback]() {
    WaitForSingleObject(_process_info.hProcess, INFINITE);
    callback();
    PLOGD << "(nvim) terminated";
  });

  HANDLE job_object = CreateJobObjectW(nullptr, nullptr);
  JOBOBJECT_EXTENDED_LIMIT_INFORMATION job_info = {0};
  job_info.BasicLimitInformation.LimitFlags =
      JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;

  SetInformationJobObject(job_object, JobObjectExtendedLimitInformation,
                          &job_info, sizeof(job_info));
  AssignProcessToJobObject(job_object, _process_info.hProcess);

  return true;
}
