Simple program to read temperature and humidity from Adafruit SHT4x Trinkey M0 device.
I am using this to monitor the conditions in my ham radio shack. You will need to modify
the DEVICE setting in shtemp.c to match your device. Or alternatively use /dev/ttyACM0.
Make sure you have read permissions to this device. Perhaps add yourself to uucp or whatever
group your distro uses for granting serial device access.

The output format of the device is: <device-id>, <temperature>, <humidity>, <touch sensor>

Only temperature and humidity are recorded. This device is USB A half-key so I used 
electrical tape to secure it to the USB extension cord.

