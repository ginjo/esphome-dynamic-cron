// LoggerLocal base class to be inherited by other classes.
//
// Creates local LOGx functions to work with Schedule class(s).
// This should work with or without the ESP logger macros.
//
// ALL derived classes should define a *schedule variable that points to the
// schedule instance they are associated with.


#pragma once

#include <iostream>

#if !defined(IS_NATIVE) || IS_NATIVE != 1
  #include "esphome/core/log.h"
  // Note that "esp_log.h" is also available but is part of esp-idf library.
#endif

namespace esphome {
namespace dynamic_cron {

const char *LOGTAG = "dynamic_cron";


// Top level "free" (but namespaced) log functions.
// These will work with or without esphome logging functions loaded.
//
#if defined(IS_NATIVE) && IS_NATIVE == 1
  #define CREATE_LOG_FUNC_FREE(level) \
    template<typename... Args> \
    void SLOG##level(const char *tag, const char *fmt, Args... args) { \
      printf("[%s][%s]: ", #level, tag); \
      printf(fmt, args...); \
      printf("\n"); \
    }
    
#else
  #define CREATE_LOG_FUNC_FREE(level) \
    template<typename... Args> \
    void SLOG##level(const char *tag, const char *fmt, Args... args) { \
      ESP_LOG##level(tag, fmt, args...); \
    }
    
#endif

CREATE_LOG_FUNC_FREE(E)
CREATE_LOG_FUNC_FREE(W)
CREATE_LOG_FUNC_FREE(I)
CREATE_LOG_FUNC_FREE(D)
CREATE_LOG_FUNC_FREE(V)
//CREATE_LOG_FUNC_FREE(VV)


// Templated logger class so we can access the derived instances from within it.
// These LOGx methods add contextual boilerplate and then call SLOGx function.
//
// If you need to call a static method on a templated class, you can do: LoggerLocal<void>::someMethod().
//
// Inherit this class wherever you need these LOGx methods.
// ! Wherever you inherit this class, make sure to call the LoggerLocal(_name) constructor.
// ! You must define a pointer *schedule that points to the relevant schedule (or schedule-core) instance.
//
template <typename Derived> 
class LoggerLocal {
  
public:
    
  Derived* derived() {
    return static_cast<Derived*>(this);
  }
  
  // We created our own LOGx functions, since we need to access them independently
  // from esphome during test runs. We also wanted to add some boilerplate to
  // log lines called from withing Schedule instances.
  //
  // Remember that templated methods can't be virtual.
  //
  //
  // MEMBER METHODS
  //
  // These methds add boilerplate tags-and-schedule-name from the schedule instance, to the log line.
  //
  // These functions are created with a Macro '#define' and each line of the definition
  // MUST be terminated with an escaped literal newline '\<newline>'.
  // Use the stringizing character '#' to resolve the macro vars to a string of their name.
  // NOTE: There's a lot of fancy stuff going on in this macro definition with preprocessor directives.
  //
  // NOTE: I think the steps within each of these conditions are currently identical.
  //       Do we still need the IS_NATIVE condition here?
  //
  #if defined(IS_NATIVE) && IS_NATIVE == 1
    #define CREATE_LOG_FUNC(level) \
      template<typename... Args> \
      void LOG##level(const char *fmt, Args... args) { \
        std::string tag = LOGTAG; \
        SLOG##level((tag + "." + derived()->schedule->getId()).c_str(), fmt, args...); \
      }
      // This will not work! Tried many many things, but will not compile due to access restrictions.
      // SLOG##level((tag + "_" + derived()->schedule->schedule_id).c_str(), fmt, args...); \

  #else
    #define CREATE_LOG_FUNC(level) \
      template<typename... Args> \
      void LOG##level(const char *fmt, Args... args) { \
        std::string tag = LOGTAG; \
        SLOG##level((tag + "." + derived()->schedule->getId()).c_str(), fmt, args...); \
      }
      
  #endif
  
  CREATE_LOG_FUNC(E)
  CREATE_LOG_FUNC(W)
  CREATE_LOG_FUNC(I)
  CREATE_LOG_FUNC(D)
  CREATE_LOG_FUNC(V)
  //CREATE_LOG_FUNC(VV)
  
}; // LoggerLocal

}  // namespace dynamic_cron
}  // namespace esphome

