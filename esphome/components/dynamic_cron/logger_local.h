// Creates local LOGx functions to work with Schedule class(s).
// This should work with or without the ESP logger macros.


#pragma once

#include <iostream>

#if !defined(IS_NATIVE) || IS_NATIVE != 1
  #include "esphome/core/log.h"
  // Note that "esp_log.h" is also available but is part of esp-idf library.
#endif

namespace esphome {
namespace dynamic_cron {

const char *LOGTAG = "dynamic_cron";

// Forward declarations.
class Schedule;


class LoggerLocal {
public:
  std::string schedule_name;
  
  LoggerLocal() {}
  
  LoggerLocal(std::string _name) :
    schedule_name(_name)
  {}
  
  // Wherever you inherit this class, make sure to call the LoggerLocal(_name)
  // constructor.
  
  // We created our own LOGx functions, since we need to access them independently
  // from esphome during test runs. We also wanted to add some boilerplate to
  // log lines called from withing Schedule instances.
  //
  // Remember that templated methods can't be virtual.
  //
  //
  // STATIC CLASS METHODS
  // 
  // These methods can be called from anywhere.
  //
  //
  // MEMBER METHODS
  //
  // These methds add boilerplate tags-and-schedule-name from the schedule instance, to the log line.
  //
  // You must set schedule_name in the inherited class, for these to work.
  // Example: schedule_name = schedule->schedule_name;
  //
  // OR, you must define a pointer *schedule that points to the relevant schedule (or schedule-core) instance.
  // This pointer method would only work if you had header files for each class (ScheduleCore, LoggerLocal, Schedule, etc.).
  //
  //
  // These functions are created with a Macro '#define' and each line of the definition
  // MUST be terminated with an escaped literal newline '\<newline>'.
  // Use the stringizing character '#' to resolve the macro vars to a string of their name.
  // NOTE: There's a lot of fancy stuff going on in this macro definition with preprocessor directives.
  
  #if defined(IS_NATIVE) && IS_NATIVE == 1
  
    #define CREATE_LOG_FUNC(level) \
      template<typename... Args> \
      static void SLOG##level(const char *tag, const char *fmt, Args... args) { \
        printf("[%s][%s]: ", #level, tag); \
        printf(fmt, args...); \
        printf("\n"); \
      } \
      template<typename... Args> \
      void LOG##level(const char *fmt, Args... args) { \
        std::string tag = LOGTAG; \
        SLOG##level((tag + " " + schedule_name).c_str(), fmt, args...); \
      }
      
  #else
    #define CREATE_LOG_FUNC(level) \
      template<typename... Args> \
      static void SLOG##level(const char *tag, const char *fmt, Args... args) { \
        ESP_LOG##level(tag, fmt, args...); \
      } \
      template<typename... Args> \
      void LOG##level(const char *fmt, Args... args) { \
        std::string tag = LOGTAG; \
        SLOG##level((tag + " " + schedule_name).c_str(), fmt, args...); \
      }
      
  #endif
  

  
  CREATE_LOG_FUNC(E)
  CREATE_LOG_FUNC(W)
  CREATE_LOG_FUNC(I)
  CREATE_LOG_FUNC(D)
  CREATE_LOG_FUNC(V)
  //CREATE_LOG_FUNC(VV)

  
  // MEMBER METHODS
  //
  // These methds add boilerplate tags-and-schedule-name from the schedule instance, to the log line.
  //
  // You must set schedule_name in the inherited class, for these to work.
  // Example: schedule_name = schedule->schedule_name;
  //
  // OR, you must define a pointer *schedule that points to the relevant schedule (or schedule-core) instance.
  // This pointer method would only work if you had header files for each class (ScheduleCore, LoggerLocal, Schedule, etc.).
  //
  // template<typename... Args>
  // void LOGD(const char *fmt, Args... args) {
  //   std::string tag = LOGTAG;
  //   SLOGD((tag + " " + schedule_name).c_str(), fmt, args...);
  // }
  // 
  // template<typename... Args>
  // void LOGE(const char *fmt, Args... args) {
  //     std::string tag = LOGTAG;
  //     SLOGE((tag + " " + schedule_name).c_str(), fmt, args...);
  // }
  
}; // LoggerLocal
}  // namespace dynamie_cron
}  // namespace esphome

