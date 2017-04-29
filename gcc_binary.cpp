#include "gcc_binary.hpp"
#include "config.hpp"
#include "error.hpp"
#include "gcc_object.hpp"
#include "osal.hpp"
#include <algorithm>
#include <fstream>
#include <iostream>
#include <sstream>
#include <unordered_set>

static void resolveLibs(const ProjectConfig *project, const Config *config, std::vector<std::string> &internalLibs, std::vector<std::string> &externalLibs)
{
  for (const auto &p: config->projects)
  {
    if (p.targetType != TargetType::SharedLib && p.targetType != TargetType::StaticLib)
      continue;
    for (const auto &sym: project->neededSymbols)
    {
      if (p.providedSymbols.find(sym) != std::end(p.providedSymbols))
      {
        if (std::find(std::begin(internalLibs), std::end(internalLibs), p.target) == std::end(internalLibs))
        {
          internalLibs.push_back(p.target);
          if (p.targetType == TargetType::StaticLib)
            resolveLibs(&p, config, internalLibs, externalLibs);
        }
        break;
      }
    }
    for (const auto &lib: project->libs)
    {
      auto iter = std::find(std::begin(externalLibs), std::end(externalLibs), lib);
      if (iter == std::end(externalLibs))
        externalLibs.push_back(lib);
    }
  }
}

namespace Gcc
{
void Binary::preResolve()
{
  project->target = fileName;
  project->neededSymbols.clear();
  project->providedSymbols.clear();
  for (auto &resolver: resolverList)
  {
    std::ifstream nmFile(resolver->fileName + ".nm");
    std::string line;
    while (std::getline(nmFile, line))
    {
      if (project->targetType == TargetType::Unknown)
      {
        if (line.find(" T main") != std::string::npos ||
            line.find(" T _main") != std::string::npos)
          project->targetType = TargetType::Executable;
      }
      auto p = line.rfind(" ");
      if (p == std::string::npos || p < 1)
        continue;
      auto sym = line.substr(p + 1);
      auto type = line[p - 1];
      if (type == 'T')
        project->providedSymbols.insert(sym);
      else
        project->neededSymbols.insert(sym);
    }
  }
}
void Binary::resolve()
{
  if (resolverList.empty())
  {
    std::cerr << "coddle: Nothing to build for: " << fileName;
    return;
  }

  std::string objList;
  for (auto &resolver: resolverList)
    if (dynamic_cast<Gcc::Object *>(resolver.get()))
      objList += resolver->fileName + " ";
  try
  {
    std::ostringstream strm;
    if (project->targetType == TargetType::Executable || project->targetType == TargetType::SharedLib)
    {
      strm << "g++";
      if (project->targetType == TargetType::SharedLib)
        strm << " -shared";
      strm << " " << objList << "-o " << fileName;
      auto ldflags = Config::merge(config->common.ldflags, project->ldflags);
      for (const auto &flag: ldflags)
        strm << " " << flag;
      if (config->multithread && std::find(std::begin(ldflags), std::end(ldflags), "-pthread") == std::end(ldflags))
        strm << " -pthread";
      auto libs = Config::merge(config->common.libs, project->libs);
      for (const auto &lib: libs)
        strm << " -l" << lib;
      {
        std::vector<std::string> internalLibs;
        std::vector<std::string> externalLibs;
        resolveLibs(project, config, internalLibs, externalLibs);
        if (!internalLibs.empty())
          strm << " -L.";
        for (const auto &lib: internalLibs)
          strm << " -l:" << lib;
        for (const auto &lib: externalLibs)
          strm << " -l" << lib;
      }
      auto pkgs = Config::merge(config->common.pkgs, project->pkgs);
      if (!pkgs.empty())
      {
        strm << " $(pkg-config --libs";
        for (const auto &pkg: pkgs)
          strm << " " << pkg;
        strm << ")";
      }
    }
    else if (project->targetType == TargetType::StaticLib)
    {
      strm <<
        "ar r " << fileName << " " << objList;
    }
    else
    {
      THROW_ERROR("Unknown binary type");
    }
    std::cout << strm.str() << std::endl;
    exec(strm.str());
  }
  catch (std::exception &e)
  {
    THROW_ERROR("coddle: *** [" << fileName << "] " << e.what())
  }
}
}
