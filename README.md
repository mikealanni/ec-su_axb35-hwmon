# ec-su_axb35-linux
Linux driver (prototype) for embedded controller in the Sixunited AXB35-02 board.

Vendors using that board:
  - GMKTec EVO-X2

# Build instructions
```
$ make
$ sudo insmod ec_su_axb35.ko
```

# Devices
```
# Fan devices
/sys/class/ec_su_axb35/fan1/ <-- CPU fan 1
/sys/class/ec_su_axb35/fan2/ <-- CPU fan 2
/sys/class/ec_su_axb35/fan3/ <-- System fan

../fanX/speed <-- current speed in rpm
../fanX/mode <-- fan-mode (auto/manual) - writable
../fanX/manual_speed <-- fixed speed in manual from 0 - 5 = 0 to 100% in 20% steps

# Temp device
/sys/class/ec_su_axb35/temp/temp <-- temperature in C
```

