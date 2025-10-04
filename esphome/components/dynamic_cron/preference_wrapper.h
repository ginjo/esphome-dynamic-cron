#pragma once

#include "esphome/core/preferences.h"

// Usage:
//   StringPreference<64> name_pref_{global_preferences, 0};
//   StringPreference<128> email_pref_{global_preferences, 1};
//
//   Option 1: Check if value exists
//     std::string name;
//     if (name_pref_.load(name)) {
//       ESP_LOGD(TAG, "Found saved name: %s", name.c_str());
//     } else {
//       ESP_LOGD(TAG, "No saved name");
//     }
// 
//   Option 2: Get with default (auto-saves if missing)
//     std::string email = email_pref_.load("user@example.com");
//     // First run: saves "user@example.com" and returns it
//     // Subsequent runs: returns saved value
// 
//   Manual save still works
//     name_pref_.save("New Name");
//


namespace esphome {
namespace dynamic_cron {


// This version doesn't do the default values.
//
template<size_t MAX_LEN = 64>
class StringPreference {
 private:
  struct Storage {
    uint16_t length;
    char data[MAX_LEN];
  };
  
  ESPPreferenceObject pref_;

 public:
  StringPreference(ESPPreferences *prefs, uint32_t hash) {
    pref_ = prefs->make_preference<Storage>(hash);
  }

  bool save(const std::string &value) {
    Storage storage;
    storage.length = std::min(value.length(), (size_t)MAX_LEN);
    memcpy(storage.data, value.c_str(), storage.length);
    return pref_.save(&storage);
  }

  bool load(std::string &value) {
    Storage storage;
    if (pref_.load(&storage)) {
      value.assign(storage.data, storage.length);
      return true;
    }
    return false;
  }
};



// template<size_t MAX_LEN = 64>
// class StringPreference {
//  private:
//   struct Storage {
//     uint16_t length;
//     char data[MAX_LEN];
//   };
//   
//   ESPPreferenceObject pref_;
// 
//  public:
//   StringPreference(ESPPreferences *prefs, uint32_t hash) {
//     pref_ = prefs->make_preference<Storage>(hash);
//   }
// 
//   bool save(const std::string &value) {
//     Storage storage;
//     storage.length = std::min(value.length(), (size_t)MAX_LEN);
//     memcpy(storage.data, value.c_str(), storage.length);
//     return pref_.save(&storage);
//   }
// 
//   // Original load with bool return
//   bool load(std::string &value) {
//     Storage storage;
//     if (pref_.load(&storage)) {
//       value.assign(storage.data, storage.length);
//       return true;
//     }
//     return false;
//   }
// 
//   // Load with default - saves default if not found
//   std::string load(const std::string &default_value) {
//     Storage storage;
//     if (pref_.load(&storage)) {
//       return std::string(storage.data, storage.length);
//     }
//     // Not found - save and return default
//     save(default_value);
//     return default_value;
//   }
// };

} // dynamic_cron
} // esphome

