// Copyright (c) 2013 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#include <cstdlib>
#include <string>

#include <windows.h>
#include <shellapi.h>
#include <direct.h>

#include "include/cef_command_line.h"
#include "include/cef_sandbox_win.h"
#include "simple_app.h"

// When generating projects with CMake the CEF_USE_SANDBOX value will be defined
// automatically if using the required compiler version. Pass -DUSE_SANDBOX=OFF
// to the CMake command-line to disable use of the sandbox.
// Uncomment this line to manually enable sandbox support.
// #define CEF_USE_SANDBOX 1

#if defined(CEF_USE_SANDBOX)
// The cef_sandbox.lib static library may not link successfully with all VS
// versions.
#pragma comment(lib, "cef_sandbox.lib")
#endif

static std::string get_dir(const char* av0) {
  char buf[4096] = {};
  const char* cwd = _getcwd(buf, sizeof(buf));
  std::string prog(av0);
  const size_t sp = prog.find_last_of('\\');
  if (sp != std::string::npos)
    prog = prog.substr(0, sp);  // cut program file name
  else
    prog.clear();  // no /, no program path

  if (prog.size() >= 2 && prog.substr(0, 2) == "./")
    prog = prog.substr(2);
  if (prog.size() >= 1 && prog.substr(0, 1) == ".")
    prog = prog.substr(1);

  std::string dir = cwd;

  if (!prog.empty()) {
    if (prog[0] == '\\' || (prog.size () > 1 && prog[1] == ':'))
      dir = prog;
    else
      dir += "\\" + prog;
  }

  if (dir.substr(dir.size() - 4) == "\\bin")
    dir.resize(dir.size() - 4);
  else if (dir.substr(dir.size() - 6) == "\\Debug")
    dir.resize(dir.size() - 6);

  printf("cwd '%s' prog '%s' dir '%s'\n", cwd, av0, dir.c_str());

  return dir;
}

// Entry point function for all processes.
int APIENTRY wWinMain(HINSTANCE hInstance,
                      HINSTANCE hPrevInstance,
                      LPTSTR lpCmdLine,
                      int nCmdShow) {
  UNREFERENCED_PARAMETER(hPrevInstance);
  UNREFERENCED_PARAMETER(lpCmdLine);

  int argc = 0;
  auto* argvw = ::CommandLineToArgvW(GetCommandLineW(), &argc);
  if (!argc)
    return 1;

  std::wstring w(argvw[0]);
  char buffer[4096] = {};
  const auto r = std::wcstombs(buffer, w.c_str(), sizeof(buffer));
  if (r >= sizeof(buffer))
    buffer[sizeof(buffer) - 1] = 0;

  const auto dir = get_dir(buffer);
  //const auto dir = get_dir("C:\\Users\\bruge\\Desktop\\cef-project\\build\\recall\\bin\\recall.exe");

  // Enable High-DPI support on Windows 7 or newer.
  CefEnableHighDPISupport();

  void* sandbox_info = nullptr;

#if defined(CEF_USE_SANDBOX)
  // Manage the life span of the sandbox information object. This is necessary
  // for sandbox support on Windows. See cef_sandbox_win.h for complete details.
  CefScopedSandboxInfo scoped_sandbox;
  sandbox_info = scoped_sandbox.sandbox_info();
#endif

  // Provide CEF with command-line arguments.
  CefMainArgs main_args(hInstance);

  // CEF applications have multiple sub-processes (render, plugin, GPU, etc)
  // that share the same executable. This function checks the command-line and,
  // if this is a sub-process, executes the appropriate logic.
  int exit_code = CefExecuteProcess(main_args, nullptr, sandbox_info);
  if (exit_code >= 0) {
    // The sub-process has completed so return here.
    return exit_code;
  }

  // Parse command-line arguments for use in this method.
  CefRefPtr<CefCommandLine> command_line = CefCommandLine::CreateCommandLine();
  command_line->InitFromString(::GetCommandLineW());

  // Specify CEF global settings here.
  CefSettings settings;
  
  if (command_line->HasSwitch("enable-chrome-runtime")) {
    // Enable experimental Chrome runtime. See issue #2969 for details.
    settings.chrome_runtime = true;
  }

#if !defined(CEF_USE_SANDBOX)
  settings.no_sandbox = true;
#endif

  auto* sapp = new SimpleApp;
  if (!sapp->Init(dir.c_str(), 1920, 1080))  // FIXME get width and height
    return 1;

  // SimpleApp implements application-level callbacks for the browser process.
  // It will create the first browser instance in OnContextInitialized() after
  // CEF has initialized.
  CefRefPtr<SimpleApp> app(sapp);

  // Initialize CEF.
  CefInitialize(main_args, settings, app.get(), sandbox_info);

  // Run the CEF message loop. This will block until CefQuitMessageLoop() is
  // called.
  CefRunMessageLoop();

  // Shut down CEF.
  CefShutdown();

  return 0;
}
