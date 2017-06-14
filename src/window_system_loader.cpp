/*
 * Copyright © 2017 Collabora Ltd.
 *
 * This file is part of vkmark.
 *
 * vkmark is free software: you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation, either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * vkmark is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with vkmark. If not, see <http://www.gnu.org/licenses/>.
 *
 * Authors:
 *   Alexandros Frantzis <alexandros.frantzis@collabora.com>
 */

#include "window_system_loader.h"
#include "window_system.h"
#include "options.h"
#include "log.h"

#include <dlfcn.h>
#include <vector>
#include <string>
#include <algorithm>
#include <dirent.h>
#include <stdexcept>

namespace
{

using WindowSystemCreateFunc = std::unique_ptr<WindowSystem>(*)(Options const&);
using WindowSystemProbeFunc = int(*)();

void close_lib(void* handle)
{
    if (handle) dlclose(handle);
}

bool has_shared_object_extension(std::string const& name)
{
    static std::string const so{".so"};

    return name.size() > so.size() &&
           std::equal(so.rbegin(), so.rend(), name.rbegin());
}

std::vector<std::string> files_in_dir(std::string dir)
{
    std::vector<std::string> files;

    auto const dirp = opendir(dir.c_str());

    if (dirp != nullptr)
    {
        dirent const* ent;

        while ((ent = readdir(dirp)) != nullptr)
        {
            if (has_shared_object_extension(ent->d_name))
                files.push_back(dir + "/" + ent->d_name);
        }

        closedir(dirp);
    }

    return files;
}

}

WindowSystemLoader::WindowSystemLoader(Options const& options)
    : options{options},
      lib_handle{nullptr, close_lib}
{
}

WindowSystem& WindowSystemLoader::load_window_system()
{
    if (window_system)
        return *window_system;

    auto const lib = probe_for_best_window_system();

    lib_handle = LibHandle{dlopen(lib.c_str(), RTLD_LAZY), close_lib};

    auto const ws_create = reinterpret_cast<WindowSystemCreateFunc>(
        dlsym(lib_handle.get(), "vkmark_window_system_create"));

    if (!ws_create)
        throw std::runtime_error{"Selected window system module doesn't provide a create function"};
    
    window_system = ws_create(options);

    if (!window_system)
        throw std::runtime_error{"Selected window system module failed to create window system"};

    return *window_system;
}

std::string WindowSystemLoader::probe_for_best_window_system()
{
    std::vector<int> priorities;

    Log::debug("WindowSystemLoader: Looking in %s for window system modules\n",
               options.window_system_dir.c_str());

    auto const candidates = files_in_dir(options.window_system_dir);

    for (auto const& c : candidates)
    {
        Log::debug("WindowSystemLoader: Probing %s... ", c.c_str());

        auto const handle = LibHandle{dlopen(c.c_str(), RTLD_LAZY), close_lib};
        if (handle.get())
        {
            auto ws_probe = reinterpret_cast<WindowSystemProbeFunc>(
                dlsym(handle.get(), "vkmark_window_system_probe"));
            if (ws_probe)
            {
                priorities.push_back(ws_probe());

                auto const fmt = Log::continuation_prefix + "succeeded with priority %d\n";
                Log::debug(fmt.c_str(), priorities.back());
            }
            else
            {
                priorities.push_back(0);

                auto const fmt = Log::continuation_prefix + "failed to find probe function: %s\n";
                Log::debug(fmt.c_str(), dlerror());
            }
        }
        else
        {
            priorities.push_back(0);

            auto const fmt = Log::continuation_prefix + "failed to load file: %s\n";
            Log::debug(fmt.c_str(), dlerror());
        }
    }

    auto const iter = std::max_element(priorities.begin(), priorities.end());
    if (iter != priorities.end() && *iter > 0)
    {
        auto const index = std::distance(priorities.begin(), iter);
        Log::debug("WindowSystemLoader: Selected window system module %s\n",
                   candidates[index].c_str());
        return candidates[index];
    }

    throw std::runtime_error{"Failed to find usable window system, try using --window-system-dir"};
}
