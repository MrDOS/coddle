#pragma once
#include "driver.hpp"
#include <functional>
#include <memory>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

class Resolver;

enum class TargetType { Unknown, Executable, StaticLib, SharedLib };

struct ProjectConfig
{
  std::string dir;
  std::string target;
  TargetType targetType = TargetType::Unknown;
  std::vector<std::string> cflags;
  std::vector<std::string> ldflags;
  std::vector<std::string> libs;
  std::vector<std::string> pkgs;
  std::unordered_set<std::string> providedSymbols;
  std::unordered_set<std::string> neededSymbols;
};

class Config
{
public:
  Config(int argc, char **argv);
  std::shared_ptr<Driver> driver;
  int njobs = 4;
  std::unordered_map<std::string, std::vector<std::string> > incToLib;
  std::unordered_map<std::string, std::vector<std::string> > incToPkg;
  std::unordered_map<std::string, std::vector<std::string> > symToObj;
  std::vector<std::string> args;
  bool multithread = false;
  ProjectConfig common;
  std::vector<ProjectConfig> projects;
  static std::vector<std::string> merge(const std::vector<std::string> &x, const std::vector<std::string> &y);
};
