#include "coddle.hpp"
#include "config.hpp"
#include "driver.hpp"
#include "exec_pool.hpp"
#include "file_exist.hpp"
#include "file_extention.hpp"
#include "file_name.hpp"
#include "make_path.hpp"
#include "osal.hpp"
#include "solution.hpp"
#include "source.hpp"
#include <algorithm>
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>

static std::unique_ptr<Resolver> buildDependencies(Config *config, ProjectConfig *project)
{
  makeDir(makePath(".coddle", project->dir));
  auto root = config->driver->makeBinaryResolver(config, project);
  auto filesList = getFilesList(project->dir);
  for (const auto &filename: filesList)
  {
    auto ext = getFileExtention(filename);
    if (ext != "c" &&
        ext != "cpp" &&
        ext != "c++" &&
        ext != "C" &&
        ext != "h" &&
        ext != "hpp" &&
        ext != "h++" &&
        ext != "H")
    {
      continue;
    }
    std::ifstream srcFile(makePath(project->dir, filename));
    std::string line;
    while (std::getline(srcFile, line))
    {
      line = [](const std::string &x)
        {
          std::string res;
          for (auto ch: x)
          {
            if (ch <= ' ' && ch >= 0)
              continue;
            if (ch == '\\')
              ch = '/';
            res += ch;
          }
          return res;
        }(line);
      if (line.find("#include<") != 0)
        continue;
      auto p = line.find(">");
      if (p != line.size() - 1)
        continue;
      auto header = line.substr(9);
      header.resize(header.size() - 1);
      {
        auto pkgs = Config::merge(config->common.pkgs, project->pkgs);
        auto iter = config->incToPkg.find(header);
        if (iter != std::end(config->incToPkg))
          for (const auto &pkg: iter->second)
            if (std::find(std::begin(pkgs), std::end(pkgs), pkg) == std::end(pkgs))
            {
              project->pkgs.push_back(pkg);
              pkgs.push_back(pkg);
            }
      }
      {
        auto libs = Config::merge(config->common.libs, project->libs);
        auto iter = config->incToLib.find(header);
        if (iter != std::end(config->incToLib))
          for (const auto &lib: iter->second)
            if (std::find(std::begin(libs), std::end(libs), lib) == std::end(libs))
            {
              project->libs.push_back(lib);
              libs.push_back(lib);
            }
      }
    }
    if (ext != "c" &&
        ext != "cpp" &&
        ext != "c++" &&
        ext != "C")
    {
      continue;
    }
    if (project->language == Language::Unknown && ext == "c")
    {
      project->language = Language::C;
    }
    else if ((project->language == Language::Unknown || project->language == Language::C) && ext != "c")
    {
      project->language = Language::Cpp17;
    }
    auto obj = root->add(config->driver->makeObjectResolver(makePath(project->dir, filename), config, project));
    obj->add(std::make_unique<Source>(getExecPath(), config, project));
    obj->add(std::make_unique<Source>(makePath(project->dir, filename), config, project));
    std::ifstream hs(makePath(".coddle", project->dir, filename + ".hs"));
    std::string header;
    while (std::getline(hs, header))
      if (!header.empty())
        obj->add(std::make_unique<Source>(header, config, project));
  }
  return root;
}

int coddle(Config *config)
{
  try
  {
    Solution root("root", config, &config->common);
    for (auto &prj: config->projects)
      root.add(buildDependencies(config, &prj));
    try
    {
      root.resolveTree();
    }
    catch (...)
    {
      ExecPool::instance(config).finalize();
      throw;
    }
    if (!root.isRunResolve())
      std::cout << "coddle: '" << root.fileName << "' is up to date.\n";
  }
  catch (std::exception &e)
  {
    std::cerr << e.what() << std::endl;
    return 1;
  }
  return 0;
}
