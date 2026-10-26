#/bin/zsh
arduino-cli compile --fqbn arduino:avr:leonardo shooter.ino
arduino-cli upload -p /dev/cu.usbmodem22201 --fqbn arduino:avr:leonardo shooter.ino
