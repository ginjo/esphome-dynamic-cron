#!/bin/bash

# Run this file within a container, as the docker container 'command'
# to launch the esphome container into a usable bash session.
# See the compose file for more info.
# Like this:
#
#   docker/compose.sh run -it --rm esphome /DynaicCron/docker/bash_session_init.sh
#

# Sets up an alias to fancy 'ls' command.
echo "alias ll='ls -lah --color'" >> /etc/bash.bashrc

apt update && apt-get install -y build-essential nano

# Execs bash, unless some other command was given.
exec ${@:-bash}
