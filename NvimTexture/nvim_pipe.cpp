#include "nvim_pipe.h"
#include <Windows.h>
#include <string>

class NvimPipeImpl {
  HANDLE _stdin_read = nullptr;
  HANDLE _stdin_write = nullptr;
  HANDLE _stdout_read = nullptr;
  HANDLE _stdout_write = nullptr;
  PROCESS_INFORMATION _process_info = {0};

  NvimPipeImpl() {
    SECURITY_ATTRIBUTES sec_attribs = {0};
    sec_attribs.nLength = sizeof(SECURITY_ATTRIBUTES);
    sec_attribs.bInheritHandle = true;
    CreatePipe(&_stdin_read, &_stdin_write, &sec_attribs, 0);
    CreatePipe(&_stdout_read, &_stdout_write, &sec_attribs, 0);
  }

public:
  ~NvimPipeImpl() {
    DWORD exit_code;
    GetExitCodeProcess(_process_info.hProcess, &exit_code);
    if (exit_code == STILL_ACTIVE) {
      CloseHandle(ReadHandle());
      CloseHandle(WriteHandle());
      TerminateProcess(_process_info.hProcess, 0);
      CloseHandle(_process_info.hProcess);
    }
  }

  HANDLE ReadHandle() const { return _stdout_read; }
  HANDLE WriteHandle() const { return _stdin_write; }

  static std::unique_ptr<NvimPipeImpl> Launch(const char *command) {

    auto p = std::unique_ptr<NvimPipeImpl>(new NvimPipeImpl);
    STARTUPINFOA startup_info = {0};
    startup_info.cb = sizeof(STARTUPINFO);
    startup_info.dwFlags = STARTF_USESTDHANDLES;
    startup_info.hStdInput = p->_stdin_read;
    startup_info.hStdOutput = p->_stdout_write;
    startup_info.hStdError = p->_stdout_write;
    if (!CreateProcessA(nullptr, (char *)command, nullptr, nullptr, true,
                        CREATE_NO_WINDOW, nullptr, nullptr, &startup_info,
                        &p->_process_info)) {
      return nullptr;
    }

    HANDLE job_object = CreateJobObjectW(nullptr, nullptr);
    JOBOBJECT_EXTENDED_LIMIT_INFORMATION job_info = {0};
    job_info.BasicLimitInformation.LimitFlags =
        JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
    SetInformationJobObject(job_object, JobObjectExtendedLimitInformation,
                            &job_info, sizeof(job_info));
    AssignProcessToJobObject(job_object, p->_process_info.hProcess);

    return p;
  }
};

NvimPipe::NvimPipe() {}

NvimPipe::~NvimPipe() {}

bool NvimPipe::Launch(const char *command) {

  _impl = std::move(NvimPipeImpl::Launch(command));
  if (!_impl) {
    return false;
  }

  return true;
}
