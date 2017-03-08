#include "osal.hpp"
#include "error.hpp"

#ifndef __WIN32__
#include <cstdlib>
#include <cstring>
#include <dirent.h>
#include <limits.h>
#include <sstream>
#include <stdexcept>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#ifdef __APPLE__
#include <mach-o/dyld.h>
#endif

char getDirSeparator()
{
  return '/';
}

std::string getCurrentWorkingDir()
{
  std::string res;
  char *cres = getcwd(nullptr, 0);
  res = cres;
  free(cres);
  return res;
}

std::string getExecPath()
{
  char buf[PATH_MAX];
#ifdef __APPLE__
  uint32_t size = sizeof(buf);
  if (_NSGetExecutablePath(buf, &size) == 0)
    return buf;
#else
  int count = readlink("/proc/self/exe", buf, sizeof(buf));
  if (count >= 0)
    return buf;
#endif
  return std::string();
}

std::vector<std::string> getFilesList(const std::string &dirPath)
{
  std::vector<std::string> res;
  DIR *dir;
  struct dirent *ent;
  if ((dir = opendir(dirPath.c_str())) != NULL)
  {
    while ((ent = readdir (dir)) != NULL)
      if (ent->d_type == DT_LNK || ent->d_type == DT_REG)
        res.push_back(ent->d_name);
    closedir(dir);
  }
  return res;
}

time_t getFileModification(const std::string &fileName)
{
  struct stat buffer;
  if (stat(fileName.c_str(), &buffer) != 0)
    return 0;
  return buffer.st_mtime;
}

void changeDir(const std::string &dir)
{
  ::chdir(dir.c_str());
}

void exec(const std::string &cmd)
{
  auto res = system(cmd.c_str());
  if (res != 0)
  {
    if (WIFSIGNALED(res) && (WTERMSIG(res) == SIGINT || WTERMSIG(res) == SIGQUIT))
      THROW_ERROR("Interrupt");
    if (WIFEXITED(res))
      THROW_ERROR("Error " << WEXITSTATUS(res));
  }
}

void makeDir(const std::string &dir)
{
  std::istringstream strm(dir);
  std::string subDir;
  std::string tmp;
  while (std::getline(strm, tmp, '/'))
  {
    subDir += tmp;
    subDir += "/";
    auto res = mkdir(subDir.c_str(), 0777);
    if (res != 0)
    {
      auto err = errno;
      if (err != EEXIST)
        THROW_ERROR("makeDir(" << dir << "): " << strerror(err));
    }
  }
}

#else
#include <windows.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

char getDirSeparator()
{
  return '\\';
}
std::string getCurrentWorkingDir()
{
  TCHAR NPath[MAX_PATH];
  GetCurrentDirectory(MAX_PATH, NPath);
  return NPath;
}

std::string getExecPath()
{
  char buffer[MAX_PATH];
  GetModuleFileName(nullptr, buffer, MAX_PATH);
  return buffer;
}

std::vector<std::string> getFilesList(const std::string &dirPath)
{
  std::vector<std::string> res;
  WIN32_FIND_DATA fd;
  HANDLE handle;
  handle = FindFirstFile(dirPath.c_str(), &fd);
  if (handle != INVALID_HANDLE_VALUE)
  { 
    do
    { 
      if ((fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0)
        continue;
      res.push_back(fd.cFileName);
    } while(::FindNextFile(handle, &fd)); 
    ::FindClose(handle);
  }
  return res;
}

time_t getFileModification(const std::string &fileName)
{
  struct _stat buffer;
  if (_stat(fileName.c_str(), &buffer) != 0)
    return 0;
  return buffer.st_mtime;
}

void changeDir(const std::string &dir)
{
  ::chdir(dir.c_str());
}

void exec(const std::string &cmd)
{
  auto res = system(cmd.c_str());
  if (res != 0)
    THROW_ERROR("Error " << res);
}

void makeDir(const std::string &dir)
{
  CreateDirectory(dir.c_str(), nullptr);
}

#endif