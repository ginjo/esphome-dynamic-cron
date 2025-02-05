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

//#include <croncpp.h>

#if IS_NATIVE == 1
  #include <ArduinoFake.h>
  #include <../esphome/components/dynamic_cron/dynamic_cron.h>
#else
  #include "esphome_tests/esphome_tests.cpp"
#endif

// In actual firmware build, python scripts set the firmware TIMESTAMP.
// See dynamic_cron.h
// Note: static global variables are only accessible from the file where they're declared/defined.
//static uint64_t TIMESTAMP_MOCK = (uint64_t)std::time(NULL);
#define TIMESTAMP_MOCK std::time(nullptr)


class ScheduleMock : public esphome::dynamic_cron::ScheduleCore {
public:
    
  ScheduleMock() :
    ScheduleCore("test-name", "test-id", []() { std::cout << "Test lambda called\n"; return true; })
  {}
  
  // Added this destructor to debug mystery exception, didn't help
  //~ScheduleMock() noexcept(false) {}

  bool callLambda() {
    return target_action_fptr();
  }
  
  // Helper method to access protected ScheduleCore::splitString().
  // Returns single member of string vector from splitString() function.
  std::string getStringVectorMember(std::string _string, std::string _regexp, size_t index) {
    if (_string == "") { return ""; }
    std::string result = splitString(_string, _regexp)[index];
    return result;
  }
  
  // Helper method to set cronnext with older time,
  // since the official setCronNext() is protected.
  void setCronNextRaw(std::time_t input) {
    cronnext = input;
  }

  // Cuz cronLoop() is protected.
  void callCronLoop() {
    cronLoop();
  }    
};

// Declards a ScheduleMock object with default constructor.
// This will be used during each test run. See setUp().
ScheduleMock* ScheduleMockInst = nullptr;

void initScheduleMock(ScheduleMock* obj) {
  //std::cout << "inside initScheduleMock()\n";
  ScheduleMockInst = obj;
}

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
int setSystemTime(int seconds = 1600000000) { // 2020-09-13 12:26:40
  struct timeval tv;
  tv.tv_sec = seconds; // Set a specific time (seconds since Epoch)
  tv.tv_usec = 0;
  std::cout << "Setting system time: " << ScheduleMockInst->timeToString((std::time_t)seconds).c_str() << "\n";
  return settimeofday(&tv, NULL);
}

void printSystemTime() {
  std::time_t now = std::time(NULL);
  std::cout << "System time: " << ScheduleMockInst->timeToString(now).c_str() << "\n";
}

void setFirmwareTimestamp(uint64_t val = TIMESTAMP_MOCK) {
  std::cout << "Current firmware TIMESTAMP is " << std::to_string(esphome::dynamic_cron::TIMESTAMP) << "\n";
  
  esphome::dynamic_cron::TIMESTAMP = val; //esphome::dynamic_cron::TIMESTAMP_MOCK;
  std::cout << "New firmware TIMESTAMP is " << std::to_string(esphome::dynamic_cron::TIMESTAMP) << "\n";
}


void setUp(void) {
  // Performed before ever test is run.
  
  std::cout << "SETUP\n";
  
  // We need to use dynamic (heap?) memory,
  // otherwise the object goes out of scope and is deleted,
  // even though the var is declared at the top-level.
  // This way, the object persists until we delete it.
  // Note: I think 'new' always returns a pointer.
  ScheduleMockInst = new ScheduleMock;
}

void tearDown(void) {
  // Performed after every test is run.
  
  std::cout << "TEARDOWN\n";
  
  delete ScheduleMockInst;
}


// TESTS - Covers most but not all functions in dynamic_cron.h, either directly or indirectly.
//         Does NOT cover anything in dynamic_cron_esphome.h (yet), as that file
//         requires links to arduino and esphome hardware objects.

void test_schedule_receives_name(void) {
  TEST_ASSERT_TRUE(ScheduleMockInst->getNameString() == "test-name");
}

void test_schedule_receives_id(void) {
  //std::cout << "Running a test\n";
  TEST_ASSERT_TRUE(ScheduleMockInst->getIdString() == "test-id");
}

void test_schedule_receives_lambda(void) {
  bool rslt = ScheduleMockInst->callLambda();
  TEST_ASSERT_TRUE(rslt);
}

void test_schedule_contains_schedules(void) {
  // Outputs the Schedules() array size, for debugging.
  //std::cout << esphome::dynamic_cron::ScheduleCore::Schedules().size() << "\n";
  TEST_ASSERT_TRUE(esphome::dynamic_cron::ScheduleCore::Schedules()[0] == ScheduleMockInst);
}

void test_schedule_calculates_cronnext(void) {
  ScheduleMockInst->setCrontab("1 2 3 * * *");
  std::string cron_next = ScheduleMockInst->cronNextString();
  std::string now = ScheduleMockInst->timeToString();
  std::string time_only = ScheduleMockInst->getStringVectorMember(cron_next, " ", 1);
  // Test-message is not supported in the Unity framework provided with platformio.
  //TEST_MESSAGE(now.c_str());
  //TEST_MESSAGE(cron_next.c_str());
  //TEST_MESSAGE(time_only.c_str());
  TEST_ASSERT_TRUE(time_only == "03:02:01");
  // Bad crontab should be handled
  ScheduleMockInst->setCrontab("1 2 3 * * * | foo bar baz");
  TEST_ASSERT_TRUE(ScheduleMockInst->getCronNext() == (std::time_t)0);
  // Fixed crontab should resolve
  ScheduleMockInst->setCrontab("1 2 3 * * *");
  TEST_ASSERT_TRUE(ScheduleMockInst->getCronNext() > (std::time_t)0);
  // Bypassed schedule should have no cronnext
  ScheduleMockInst->setBypass(true);
  TEST_ASSERT_TRUE(ScheduleMockInst->getCronNext() == (std::time_t)0);
  // Re-enabled schedule should resolve
  ScheduleMockInst->setBypass(false);
  TEST_ASSERT_TRUE(ScheduleMockInst->getCronNext() > (std::time_t)0);
}

void test_schedule_cronNextExpired(void) {
  ScheduleMockInst->setCrontab("1 2 3 * * *");
  // Default cronnext should be legit.
  TEST_ASSERT_FALSE(ScheduleMockInst->cronNextExpired());
  // Old cronnext should be considered expired.
  std::time_t old_time = ScheduleMockInst->stringToTime("2020-01-01 12:34:56");
  ScheduleMockInst->setCronNextRaw(old_time);
  TEST_ASSERT_TRUE(ScheduleMockInst->cronNextExpired());
  // Missing crontab prevents expired from returning true, even if cronnext is expired.
  // Do we really want that?
  ScheduleMockInst->setCrontab("");
  TEST_ASSERT_FALSE(ScheduleMockInst->cronNextExpired());
  // setCronNext() with an old time is not legit user operation and will be filtered out.
  // resulting in legit cronnext.
  ScheduleMockInst->setCrontab("1 2 3 * * *");
  ScheduleMockInst->setCronNext(old_time);
  TEST_ASSERT_FALSE(ScheduleMockInst->cronNextExpired());
}

void test_schedule_cronLoop(void) {
  // cronnext should not have changed (and lambda should not have been called).
  ScheduleMockInst->setCrontab("1 2 3 * * *");
  std::time_t cronnext1 = ScheduleMockInst->getCronNext();
  ScheduleMockInst->callCronLoop();
  std::time_t cronnext2 = ScheduleMockInst->getCronNext();
  TEST_ASSERT_TRUE(std::difftime(cronnext1, cronnext2) == 0);
  // cronnext should have changed (and lambda should have been called).
  std::time_t old_time = ScheduleMockInst->stringToTime("2020-01-01 12:34:56");
  ScheduleMockInst->setCronNextRaw(old_time);
  ScheduleMockInst->callCronLoop();
  std::time_t new_time = ScheduleMockInst->getCronNext();
  TEST_ASSERT_TRUE(std::difftime(new_time, old_time) > 0);
}

// TODO: Cover GetHash(), and Schedules(string-key).


int runUnityTests(void) {
  UNITY_BEGIN();
  RUN_TEST(test_schedule_receives_name);
  RUN_TEST(test_schedule_receives_id);
  RUN_TEST(test_schedule_receives_lambda);
  RUN_TEST(test_schedule_contains_schedules);
  RUN_TEST(test_schedule_calculates_cronnext);
  RUN_TEST(test_schedule_cronNextExpired);
  RUN_TEST(test_schedule_cronLoop);
  
  #if IS_NATIVE != 1
    RUN_TEST(test_prefs_initialized);
    RUN_TEST(test_prefs_updates_timestamp);
  #endif
  
  return UNITY_END();
}



// IMPLEMENTATIONS - you can remove unnecessary implementations if not using them //

// For native dev-platform or for some embedded frameworks
//int main(void) {
int main( int argc, char **argv ) {
  //getSystemTime();
  printSystemTime();
  
  #if IS_NATIVE != 1
    setSystemTime();
  #endif
  
  setFirmwareTimestamp();
    
  return runUnityTests();
}

// For Arduino framework
void setup() {  
  //getSystemTime();
  printSystemTime();
  
  #if IS_NATIVE != 1
    setSystemTime();
  #endif
  
  setFirmwareTimestamp();
  
  // Wait ~2 seconds before the Unity test runner
  // establishes connection with a board Serial interface
  // We need the preceding '::' here because there is another delay() function
  // within this same scope, provided by Arduino or Esphome.
  ::delay(2000);

  runUnityTests();
}

void loop() {}

