#!/bin/bash

if [ -e /dev/snd/timer ]; then
  GID=$(stat -c '%g' /dev/snd/timer)
  sudo delgroup fakeaudio 2>/dev/null || true
  sudo getent group $GID > /dev/null || sudo groupadd -r -g $GID fakeaudio
  sudo usermod -aG fakeaudio bc
fi
sudo su bc "$@"
