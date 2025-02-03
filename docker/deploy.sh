#!/bin/bash

# Deploys the esphome-dashboard gui server in a docker container.
#
# I would recommend that that you run the esphome dashboard app from its
# own dedicated esphome directory, not from the DynamicCron directory.
mkdir -p .platformio
docker stack deploy -c docker/compose.yml esp

