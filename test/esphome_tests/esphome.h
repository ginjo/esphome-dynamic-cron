// This file was coppied from an esphome build using the dynamic_cron external component.
// This is not currently used in the testing scripts.

#pragma once

#include "esphome/core/macros.h"
#include "esphome/components/api/api_connection.h"
#include "esphome/components/api/api_frame_helper.h"
#include "esphome/components/api/api_noise_context.h"
#include "esphome/components/api/api_pb2.h"
#include "esphome/components/api/api_pb2_service.h"
#include "esphome/components/api/api_server.h"
#include "esphome/components/api/custom_api_device.h"
#include "esphome/components/api/homeassistant_service.h"
#include "esphome/components/api/list_entities.h"
#include "esphome/components/api/proto.h"
#include "esphome/components/api/subscribe_state.h"
#include "esphome/components/api/user_services.h"
#include "esphome/components/captive_portal/captive_index.h"
#include "esphome/components/captive_portal/captive_portal.h"
#include "esphome/components/esp32/gpio.h"
#include "esphome/components/esp32/preferences.h"
#include "esphome/components/esphome/ota/ota_esphome.h"
#include "esphome/components/json/json_util.h"
#include "esphome/components/logger/logger.h"
#include "esphome/components/md5/md5.h"
#include "esphome/components/mdns/mdns_component.h"
#include "esphome/components/network/ip_address.h"
#include "esphome/components/network/util.h"
#include "esphome/components/ota/automation.h"
#include "esphome/components/ota/ota_backend.h"
#include "esphome/components/ota/ota_backend_arduino_esp32.h"
#include "esphome/components/ota/ota_backend_arduino_esp8266.h"
#include "esphome/components/ota/ota_backend_arduino_libretiny.h"
#include "esphome/components/ota/ota_backend_arduino_rp2040.h"
#include "esphome/components/ota/ota_backend_esp_idf.h"
#include "esphome/components/preferences/syncer.h"
#include "esphome/components/safe_mode/automation.h"
#include "esphome/components/safe_mode/safe_mode.h"
#include "esphome/components/sntp/sntp_component.h"
#include "esphome/components/socket/headers.h"
#include "esphome/components/socket/socket.h"
#include "esphome/components/switch/automation.h"
#include "esphome/components/switch/switch.h"
#include "esphome/components/text/automation.h"
#include "esphome/components/text/text.h"
#include "esphome/components/text/text_call.h"
#include "esphome/components/text/text_traits.h"
#include "esphome/components/text_sensor/automation.h"
#include "esphome/components/text_sensor/filter.h"
#include "esphome/components/text_sensor/text_sensor.h"
#include "esphome/components/time/automation.h"
#include "esphome/components/time/real_time_clock.h"
#include "esphome/components/web_server/list_entities.h"
#include "esphome/components/web_server/server_index_v2.h"
#include "esphome/components/web_server/server_index_v3.h"
#include "esphome/components/web_server/web_server.h"
#include "esphome/components/web_server_base/web_server_base.h"
#include "esphome/components/wifi/wifi_component.h"
#include "esphome/core/application.h"
#include "esphome/core/automation.h"
#include "esphome/core/base_automation.h"
#include "esphome/core/color.h"
#include "esphome/core/component.h"
#include "esphome/core/component_iterator.h"
#include "esphome/core/controller.h"
#include "esphome/core/datatypes.h"
#include "esphome/core/defines.h"
#include "esphome/core/entity_base.h"
#include "esphome/core/gpio.h"
#include "esphome/core/hal.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"
#include "esphome/core/macros.h"
#include "esphome/core/optional.h"
#include "esphome/core/preferences.h"
#include "esphome/core/ring_buffer.h"
#include "esphome/core/scheduler.h"
#include "esphome/core/string_ref.h"
#include "esphome/core/time.h"
#include "esphome/core/util.h"
#include "esphome/core/version.h"

// When esphome macros like ESP_LOGx are resolved, they pull in other libs.
// Sometimes calls to those libs' functions need to be in a namespace?
// See https://github.com/esphome/issues/issues/4751
// Here's a brute-force solution (only for testing).
using namespace esphome;
using esphome::Component;

// using namespace time;
// using namespace switch_;
// using namespace text;
// using namespace text_sensor;
// using namespace api;
// using namespace json;

