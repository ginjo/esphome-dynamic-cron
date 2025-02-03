#!/bin/bash
#
# Boots a Docker container from esphome/esphome image, into this directory,
# and and starts a bash session.
# 
docker compose -f "docker/compose.yml" ${@:-run -it --rm -w /DynamicCron esphome bash}

