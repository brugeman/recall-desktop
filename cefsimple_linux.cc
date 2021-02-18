// Copyright (c) 2013 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#include <unistd.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "simple_app.h"

#if defined(CEF_X11)
#include <X11/Xlib.h>
#endif

#include "include/base/cef_logging.h"
#include "include/cef_command_line.h"

#if defined(CEF_X11)
namespace {

int XErrorHandlerImpl(Display* display, XErrorEvent* event) {
  LOG(WARNING) << "X error received: "
               << "type " << event->type << ", "
               << "serial " << event->serial << ", "
               << "error_code " << static_cast<int>(event->error_code) << ", "
               << "request_code " << static_cast<int>(event->request_code)
               << ", "
               << "minor_code " << static_cast<int>(event->minor_code);
  return 0;
}

int XIOErrorHandlerImpl(Display* display) {
  return 0;
}

}  // namespace
#endif  // defined(CEF_X11)

void
ensure_daemon (const std::string & dir)
{
   const auto db = dir + "/db";
   // ensure db dir
   {
      const auto r = mkdir (db.c_str (), S_IRWXU);
      if (r && errno != EEXIST)
      {
	 fprintf (stderr, "Failed to create dir '%s': %s\n", db.c_str (), strerror (errno));
	 exit (EXIT_FAILURE);
      }
   }
      
   const auto lock = db + "/daemon.lock";
   {
      const auto lock_fd = open (lock.c_str (), O_CREAT | O_RDWR, S_IWUSR | S_IRUSR);
      if (lock_fd == -1)
      {
	 fprintf (stderr, "Failed to open '%s': %s\n",
		  lock.c_str (), strerror (errno));
	 exit (EXIT_FAILURE);
      }
   
      const auto r = lockf(lock_fd, F_TEST, 0);
      close (lock_fd);
      if (r != 0)
      {
	 if (errno == EACCES || errno == EAGAIN)
	 {
	    printf ("daemon already started\n");
	    return;
	 }
	 else
	 {
	    fprintf (stderr, "Failed to test lock '%s': %s\n",
		     lock.c_str (), strerror (errno));
	 }
	 exit (EXIT_FAILURE);
      }
   }

   const std::string cmd = dir + "/bin/recalld.sh \""+dir+"\"";
   printf ("starting daemon '%s'...\n", cmd.c_str ());
   const int r = system (cmd.c_str ());   
   if (r < 0)
   {
      fprintf (stderr, "Failed to start daemon with '%s': %s\n",
	       cmd.c_str (), strerror (errno));
      exit (EXIT_FAILURE);
   }
   printf ("daemon started %d, please wait...\n", r);
   sleep (5);
}


static std::string
get_dir (const char * av0)
{
   char buf[4096] = {};
   const char * cwd = getcwd (buf, sizeof (buf));
   std::string prog (av0);
   const size_t sp = prog.find_last_of ('/');
   if (sp != std::string::npos)
      prog = prog.substr (0, sp); // cut program file name
   else
      prog.clear (); // no /, no program path

   if (prog.size () >= 2 && prog.substr (0, 2) == "./")
      prog = prog.substr (2);     
   if (prog.size () >= 1 && prog.substr (0, 1) == ".")
      prog = prog.substr (1);     
	
   std::string dir = cwd;

   if (!prog.empty ())
   {
      if (prog[0] == '/')
	 dir = prog;
      else
	 dir += "/" + prog;
   }

   if (dir.substr (dir.size () - 4) == "/bin")
      dir.resize (dir.size () - 4);
   
   printf ("cwd '%s' prog '%s' dir '%s'\n",
	   cwd, av0, dir.c_str ());

   return dir;
}

// static std::string
// get_dir (const char * av0)
// {
//    char buf[4096] = {};
//    const char * cwd = getcwd (buf, sizeof (buf));
//    std::string prog (av0);
//    const size_t sp = prog.find_last_of ('/');
//    if (sp != std::string::npos)
//       prog = prog.substr (0, sp); // cut program file name
//    else
//       prog.clear (); // no /, no program path

//    if (prog.size () >= 2 && prog.substr (0, 2) == "./")
//       prog = prog.substr (2);     
//    if (prog.size () >= 1 && prog.substr (0, 1) == ".")
//       prog = prog.substr (1);     
	
//    std::string dir = cwd;
//    if (dir.substr (dir.size () - 4) == "/bin")
//       dir.resize (dir.size () - 4);
     
//    if (prog[0] == '/')
//       dir = prog;
//    else
//       dir += "/" + prog;

//    printf ("cwd '%s' prog '%s' dir '%s'\n",
// 	   cwd, av0, dir.c_str ());

//    return dir;
// }

// Entry point function for all processes.
int main(int argc, char* argv[]) {

  const bool is_debug = argc > 1 && !strcmp (argv[1], "--recall-debug");
  const bool is_main = argc == 1 || is_debug;
  
  // build our own set of arguments for 'main' process
  char arg_scale[] = "--force-device-scale-factor=0.9";
  char arg_verbose[] = "--log-severity=verbose";
  char arg_silent[] = "--log-severity=disable";
  char * argv_main[] = {
     argv[0],
     arg_scale,
     is_debug ? arg_verbose : arg_silent,
  };
  int argc_main = sizeof (argv_main) / sizeof (argv_main[0]);

  if (is_main)
  {
     argc = argc_main;
     argv = argv_main;
  }

  // Provide CEF with command-line arguments
  CefMainArgs main_args(argc, argv);

  // CEF applications have multiple sub-processes (render, plugin, GPU, etc)
  // that share the same executable. This function checks the command-line and,
  // if this is a sub-process, executes the appropriate logic.
  int exit_code = CefExecuteProcess(main_args, nullptr, nullptr);
  if (exit_code >= 0) {
    // The sub-process has completed so return here.
    return exit_code;
  }

  // don't allow the main process to launch in non-main mode
  if (!is_main)
  {
     fprintf (stderr, "Bad command line params\n");
     return -1;
  }

  const auto dir = get_dir (argv[0]);
  
  // make sure daemon is started
  ensure_daemon (dir);

  size_t width = 0;
  size_t height = 0;

#if defined(CEF_X11)
  // Install xlib error handlers so that the application won't be terminated
  // on non-fatal errors.
  XSetErrorHandler(XErrorHandlerImpl);
  XSetIOErrorHandler(XIOErrorHandlerImpl);
  
  {
     Display * d = XOpenDisplay(NULL);
     if (d)
     {
	const Screen * s = DefaultScreenOfDisplay(d);
	if (s)
	{
	   if (s->width > 0)
	      width = s->width;
	   if (s->height > 0)
	      height = s->height;
	   if (width > 1920)
	      width = 1920;
	   if (height > 1080)
	      height = 1080;
	   printf ("screen %lu %lu\n", width, height);
	}

	XCloseDisplay(d);
     }
  }

#endif

  // Parse command-line arguments for use in this method.
  CefRefPtr<CefCommandLine> command_line = CefCommandLine::CreateCommandLine();
  command_line->InitFromArgv(argc, argv);

  // Specify CEF global settings here.
  CefSettings settings;

  if (command_line->HasSwitch("enable-chrome-runtime")) {
    // Enable experimental Chrome runtime. See issue #2969 for details.
    settings.chrome_runtime = true;
  }

// When generating projects with CMake the CEF_USE_SANDBOX value will be defined
// automatically. Pass -DUSE_SANDBOX=OFF to the CMake command-line to disable
// use of the sandbox.
#if !defined(CEF_USE_SANDBOX)
  settings.no_sandbox = true;
#endif

  // SimpleApp implements application-level callbacks for the browser process.
  // It will create the first browser instance in OnContextInitialized() after
  // CEF has initialized.
  auto * sapp = new SimpleApp;
  if (!sapp->Init (dir.c_str (), width, height))
     return 1;
  
  CefRefPtr<SimpleApp> app(sapp);

  // Initialize CEF for the browser process.
  CefInitialize(main_args, settings, app.get(), nullptr);

  // Run the CEF message loop. This will block until CefQuitMessageLoop() is
  // called.
  CefRunMessageLoop();

  // Shut down CEF.
  CefShutdown();

  return 0;
}
