// Main c++ test file for dynamic_cron external component.

// Loading order matters.
// If you load croncpp after ArduinoFake, it will bomb due to the macros & functions overridden by ArduinoFake.
// So ArduinoFake (or Arduino.h) should be loaded after croncpp.
// See https://forum.arduino.cc/t/include-chrono-causes-a-compile-error-in-an-otherwise-empty-skeleton-sketch/1147518/9
//
// This is the main test file for the dynamic_cron component.
// See https://interrupt.memfault.com/blog/unit-testing-basics
///
// These tests use the Unity test framework.
//   https://docs.platformio.org/en/stable/advanced/unit-testing/frameworks/unity.html
//   https://registry.platformio.org/libraries/throwtheswitch/Unity
//
// If you foul up the system time, you can do this on debian to restore it:
//   sudo apt-get install ntpdate
//   sudo ntpdate -u pool.ntp.org # (or 169.254.169.123 if on AWS EC2)
//
// TODO: Let's stop manipulating system time. It affects the host - BAD.
//
// TODO: Split tests with multiple assertions into their own test.
//
// FIX: ArduinoMock is throwing error when testing in native mode.
// https://github-wiki-see.page/m/Task-Tracker-Systems/Task-Tracker-Device/wiki/tipps-for-using-FakeIt
//
// REMEMBER: Default object creation syntax. See my perplexity question for more info.
//           Note that it's slightly different if done at the global level.
//
//   MyClass my_obj;               // Classic, works most places
//   MyClass my_obj{};             // Modern, recommended (C++11+)
//   MyClass my_obj = MyClass();   // Explicit default construction


#ifndef IS_NATIVE
#error "Must define IS_NATIVE in platformio.ini, either 1 or 0"
#endif

#include <chrono>
#include <thread>
#include <unity.h>
#include <ctime>

// We already load 'time.h' and 'ctime' in dynamic_cron.h
// This is for direct manipulation of system time using timeval struct.
#include <sys/time.h>
// See here for faketime library, which could help isolate datetime manipulations:
//   https://github.com/wolfcw/libfaketime

#if IS_NATIVE == 1
//  #include <ArduinoFake.h>
#else
  #include <Arduino.h>
#endif

#include <test_dynamic_cron.h>

// #if IS_NATIVE == 1
//   #include <ArduinoFake.h>
//   // #include <../esphome/components/dynamic_cron/dynamic_cron.h>
//   // #include "esphome_tests/test_dynamic_cron.cpp"
//   #include <test_dynamic_cron.h>
// #else
//   // #include "esphome_tests/esphome_tests.cpp"
//   #include <test_dynamic_cron_esphome.h>
// #endif


// In actual firmware build, python scripts set the firmware TIMESTAMP.
// See dynamic_cron.h
// Note: static global variables are only accessible from the file where they're declared/defined.
//static int TIMESTAMP_MOCK = (int)std::time(NULL);
#define TIMESTAMP_MOCK std::time(nullptr)


namespace esphome {
namespace dynamic_cron {

  // Example of getting system time with gettimeofday() and converting it to other formats.
  // This is currently only used for local logs.
  int getSystemTime() {
    struct timeval tv;
    gettimeofday(&tv, nullptr);

    // Convert to time_t
    time_t now = tv.tv_sec;

    // Use ctime to get a string representation
    std::string timeString = ctime(&now);

    // Remove trailing newline
    timeString.erase(timeString.size() - 1);

    // Append microseconds
    timeString += "." + std::to_string(tv.tv_usec);

    std::cout << timeString << std::endl;
  
    return int(now);
  }

  // Sets system clock to a specific time, given seconds-since-epoch.
  // Search google for 'c++ settimeofday()' and AI will show you about this.
  //
  int setSystemTime(std::time_t seconds = 1600000000) { // 2020-09-13 12:26:40
    struct timeval tv;
    tv.tv_sec = seconds; // Set a specific time (seconds since Epoch)
    tv.tv_usec = 0;
    std::cout << "Setting system time: " << ScheduleMockInst->timeToString(seconds).c_str() << "\n";
    return settimeofday(&tv, NULL);
  }

  // This will crash if called from main() or setup(), since ScheduleMockInst doesn't exist yet.
  void printSystemTime() {
    std::string time_string = ScheduleMock::timeToFormattedString(std::time(NULL));
    //ScheduleMock::SLOGI("dynamic_cron", "System time %s", time_string.c_str());
    SLOGI("dynamic_cron", "System time %s", time_string.c_str());
  }

  void setFirmwareTimestamp(std::time_t val = TIMESTAMP_MOCK) {
    //LoggerLocal<void>::SLOGI("dynamic_cron", "Current firmware TIMESTAMP %li", TIMESTAMP);
    SLOGI("dynamic_cron", "Current firmware TIMESTAMP %li", TIMESTAMP);
  
    TIMESTAMP = val; //TIMESTAMP_MOCK;
    //LoggerLocal<void>::SLOGI("dynamic_cron", "New firmware TIMESTAMP %li", TIMESTAMP);
    SLOGI("dynamic_cron", "New firmware TIMESTAMP %li", TIMESTAMP);
  }


} // esphome
} // dynamic_cron


// IMPLEMENTATIONS - you can remove unnecessary implementations if not using them //

// For native dev-platform or for some embedded frameworks
//int main(void) {
int main( int argc, char **argv ) {
  esphome::dynamic_cron::printVersion();
  
  esphome::dynamic_cron::printSystemTime();
  
  #if IS_NATIVE != 1
    esphome::dynamic_cron::setSystemTime();
  #endif
  
  esphome::dynamic_cron::setFirmwareTimestamp();
    
  esphome::dynamic_cron::RunDynamicCronTests();
}

// For Arduino framework
void setup() {  
  esphome::dynamic_cron::printVersion();
  
  esphome::dynamic_cron::printSystemTime();
  
  #if IS_NATIVE != 1
    esphome::dynamic_cron::setSystemTime();
  #endif
  
  esphome::dynamic_cron::setFirmwareTimestamp();
  
  // Wait ~2 seconds before the Unity test runner
  // establishes connection with a board Serial interface
  // We need the preceding '::' here because there is another delay() function
  // within this same scope, provided by Arduino or Esphome.
  //::delay(2000);

  esphome::dynamic_cron::RunDynamicCronTests();
}

void loop() {}

