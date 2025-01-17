
#include "log.h"

#ifdef LOG

#include <cstdarg>
#include <stdio.h>
#include <string>
#include <sstream>

#include <thread>
#include <mutex>

namespace slog
{
  static const level default_level = level::TRACE;
  static std::string header("%llu (thread id: %s [%7.7s]: ");

  static std::mutex level_mutex;
  static std::mutex write_mutex;

  static level get_default_level()
  {
    std::lock_guard<std::mutex> guard(level_mutex);
    return default_level;
  }
  
  static unsigned long long get_timestamp()
  {
    return static_cast<unsigned long long> (std::clock()/(CLOCKS_PER_SEC/1000));
  }

  static std::string get_thread_id()
  {
    std::thread::id id (std::this_thread::get_id());
    std::stringstream ss;
    ss << id;
    return (ss.str());
  }

  static const char* levels[] = 
    {
      "  TRACE",
      "  DEBUG",
      "   INFO",
      "WARNING",
      "  ERROR",
      "  FATAL"
    };

  static const char* level_to_string(level lvl)
  {
    if (static_cast<std::size_t>(lvl) < sizeof(levels) / sizeof(*levels))
      return levels[lvl];
    return "";
    //return static_cast<const char*>(lvl);
  }

  static void log(level lvl, const char* fmt, va_list args)
  {
    std::lock_guard<std::mutex> guard(write_mutex);
    /*if (lvl >= get_default_level()){
      printf(header.c_str(),
             get_timestamp(),get_thread_id().c_str(),level_to_string(lvl));
      std::string format(fmt);
      format.append("\n");
      vprintf(format.c_str(), args);
    }*/
  }

  void trace(const char* format, ...)
  {
    va_list args;
    va_start(args, format);
    log(level::TRACE, format, args);
    va_end(args);
  }
  void debug(const char* format, ...)
  {
    va_list args;
    va_start(args, format);
    log(level::DEBUG, format, args);
    va_end(args);
  }
  void info(const char* format, ...)
  {
    va_list args;
    va_start(args, format);
    log(level::INFO, format, args);
    va_end(args);
  }
  void warning(const char* format, ...)
  {
    va_list args;
    va_start(args, format);
    log(level::WARNING, format, args);
    va_end(args);
  }
  void error(const char* format, ...)
  {
    va_list args;
    va_start(args, format);
    log(level::ERR, format, args);
    va_end(args);
  }
  void fatal(const char* format, ...)
  {
    va_list args;
    va_start(args, format);
    log(level::FATAL, format, args);
    va_end(args);
  }
}

#endif
