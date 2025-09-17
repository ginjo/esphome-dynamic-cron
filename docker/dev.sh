#!/bin/bash
#
# Boots a Docker container from esphome/esphome image, into this project directory,
# and sets up the container for esphome external component development.
#
docker/compose.sh run -it --rm -w /DynamicCron esphome "docker/bash_session_init.sh"

