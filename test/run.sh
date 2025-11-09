#!/bin/bash

# This runs the DynamicCron test suite. The setup for this is currently
# located on the private server (aws-wbr) esphome/ dir. The important parts of that will
# eventually be included in the test/ dir here.
#
# See below for issues running unit tests natively on linux.
#
# To use the above mentioned setup, enter its esphome/ dir, and run the following command:
#   docker/compose.sh run -w /DynamicCron esphome /bash_session.sh
#
# That will launch a Docker container from esphome/esphome image into the /DynamicCron dir.
# Then run the following command to run this script:
#   test/run.sh
# 
# Note: You will have to install build-essential to get this to work,
# but you will first need to make room on the aws-wbr root drive.
#
# Platformio 'native' testing uses the native gcc install (build-essential on debian).
# Platformio 'native' testing can't use any of the framework specific libraries,
# like Arduino or ESPHome, since they don't have compatible code for desktop systems.
# However, there IS a mock Arduino libaray called 'ArduinoFake' that will provide
# Arduino.h for testing purposes on native desktop systems.
# I don't know if anything exists for ESPHome.
#
# See here for various information about building/testing for esp32:
#
#   https://community.platformio.org/t/library-testing-pio-test/20354/4
#   https://docs.platformio.org/en/latest/advanced/unit-testing/index.html
#   https://espressif-docs.readthedocs-hosted.com/projects/esp-idf/en/stable/api-guides/unit-tests.html
#   https://community.platformio.org/t/problem-with-native-unit-testing-on-a-large-r-project/26683
#   https://community.home-assistant.io/t/how-to-setup-an-ide-to-test-new-esphome-components/702814/21?u=wbr999
#   https://github.com/FabioBatSilva/ArduinoFake
#
# Maybe search google for 'platformio pio test native .ini file'.
#
# See Calculator example.

###  The test is currently failing, even though we've isolated it to just C++ libraries,
###  the ArduinoFake library, and the Preferences library (no esphome libraries).
###
###  2025-01021: OK, I've tried testing with no dynamic_cron code or libs loaded, only
###    Croncpp
###    Preferences
###    ArduinoFake
###    unity
###
###  And it turns out Preferences library wont compile (I think cuz it tries to acces file system library for NVS on esp32).
###  So if we still want to run unit tests natively (on linux), we're gonna have to move all the Preferences code
###  out of the new core.h file. Doing that, we'll not be testing a large and complex portion of our library.
###  So... do we give up on native platform unit testing, and just test the old monolithic dynamic_cron.h file on
###  the esp32? Or should we mock all esp-dependent and any hardware-dependent classes and functions, so we
###  can test on the native host (linux)?


# NOTE: This native test can run using the .platformio directory from a
# legit esphome build environment. However, you may have to `esphome clean <config-file>`
# before running this test for 'native' environment. Otherwise the build may fail.
printf 'If this test fails in 'native' environment, you may need to clear\n'
printf 'the build from the esphome environment with `esphome clean <config-file>`\n\n'

pio test --without-uploading -c test/platformio.ini ${@--e native}
#
# If you want to suppress the error about not finding the serial port,
# when you run this for 'esphome', you can add --without-testing, and 
# it will still build the firmware.
#
# Usage:
#   test/run.sh [-e native|esphome] [other pio-test options...]
#
# To clean out the build cache run:
#   pio run --target clean -e native -c test/platformio.ini
#
# NOTE: I don't think we can use the 'esphome' test run any more.
#       Instead, use the esphome test config in test/esphome_config/,
#       and compile with `esphome compile test/esphome_config/dynamicron.yaml.
#       Then install the resulting firmware on your esp32 and watch the logs.
#
#       The reason for this is that esphome now (2024.12) uses its own fork of some libraries installed in .platformio.
#       Platformio will overwrite the esphom-generated .platformio directory, and tests will fail.
#
# See here for WokWi online esp32 simulator, allowing uploaded firmware runs.
#   https://wokwi.com/projects/305457271083631168
#   https://docs.wokwi.com/guides/esp32

