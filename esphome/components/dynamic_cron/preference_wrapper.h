#pragma once

#include <cstdint>
#include "esphome/core/preferences.h"
#include "logger_local.h"

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

// Just a declaration, see bottom for definition of this function.
constexpr uint32_t fnv1a_hash(const char*);

template<typename T>
class MyPreference {
 protected:
  ESPPreferenceObject pref_;
  bool initialized_{false};

 public:
  MyPreference() = default;

  void init(ESPPreferences *prefs, uint32_t hash) {
    if (!initialized_) {
      pref_ = prefs->make_preference<T>(hash);
      initialized_ = true;
    }
  }

  void init(std::string key) {
    if (!initialized_) {
      uint32_t hash = fnv1a_hash(key.c_str());
      init(global_preferences, hash);
      SLOGD(LOGTAG, "Preference '%lu' defined for %s", (unsigned long)hash, key.c_str());
    }
  }

  // Save with change detection
  bool save(const T &value) {
    if (!initialized_) return false;
    
    T current;
    if (load(current) && current == value) {
      return true;  // No change, skip write
    }
    
    return pref_.save(&value);
  }

  // Load with reference (returns success)
  bool load(T &value) {
    if (!initialized_) return false;
    return pref_.load(&value);
  }

  // Load with default (auto-saves if not found)
  // Returns loaded value.
  T load_with_default(const T &default_value) {
    T value;
    if (load(value)) {
      return value;
    }
    save(default_value);
    return default_value;
  }
  
}; // MyPreference


template<size_t MAX_LEN = 64>
class StringPreference {
 private:
  struct Storage {
    uint16_t length;
    char data[MAX_LEN];
  };
  
  ESPPreferenceObject pref_;
  bool initialized_{false};

 public:
  StringPreference() = default;

  void init(ESPPreferences *prefs, uint32_t hash) {
    if (!initialized_) {
      pref_ = prefs->make_preference<Storage>(hash);
      initialized_ = true;
    }
  }
  
  void init(std::string key) {
    if (!initialized_) {
      uint32_t hash = fnv1a_hash(key.c_str());
      init(global_preferences, hash);
      SLOGD(LOGTAG, "Preference '%lu' defined for %s", (unsigned long)hash, key.c_str());
    }
  }

  bool save(const std::string &value) {
    if (!initialized_) return false;
    
    // Check if changed
    std::string current;
    if (load(current) && current == value) {
      return true;
    }
    
    Storage storage;
    storage.length = std::min(value.length(), (size_t)MAX_LEN);
    memcpy(storage.data, value.c_str(), storage.length);
    return pref_.save(&storage);
  }

  // Load with reference (returns success)
  bool load(std::string &value) {
    if (!initialized_) return false;

    Storage storage;
    if (pref_.load(&storage)) {
      value.assign(storage.data, storage.length);
      return true;
    }
    return false;
  }

  // Load with default (auto-saves if not found)
  // Returns loaded value.
  std::string load_with_default(const std::string &default_value) {
    std::string value;
    if (load(value)) {
      return value;
    }
    save(default_value);
    return default_value;
  }
  
}; // StringPreference


// Global numeric hash generator
constexpr uint32_t fnv1a_hash(const char* str) {
  uint32_t hash = 2166136261u;
  while (*str) {
    hash ^= static_cast<uint32_t>(*str++);
    hash *= 16777619u;
  }
  return hash;
}


} // dynamic_cron
} // esphome

