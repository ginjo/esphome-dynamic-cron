find . test/ esphome/components/dynamic_cron/ -maxdepth 1 -type f -regextype gnu-awk -regex ".*\.(h|py|yml|md)" -exec sed 's/ignore_missed/remember_next/g' {} \;
