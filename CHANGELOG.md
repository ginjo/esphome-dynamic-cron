# CHANGELOG.md


## 0.2.0 (2025-09-30)

Compatibility:

  - Works with ESPHome 2025.9.1.
  - *Should* work with ESPHome 2025.x.x

Features:

  - Added basic support for customizing the four control/display entities provided by 
  	this component.
  	* `disabled_switch`
  	* `remember_next_switch`
  	* `cron_next_sensor`
  	* `crontab_text`

Fix:

  - Refactored build steps to handle ESPHome breaking changes that have occurred since
    late-2024/early-2025.

## 0.1.1 (2025-03-09)

Compatibility:

  - Works with ESPHome 2024.7.3, and possibly through 2024.11.x.
  - Not compatible with ESPHome 2025.x.x or later.