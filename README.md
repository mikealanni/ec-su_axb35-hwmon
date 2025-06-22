# evox2-ec-linux
Linux driver (prototype) for the GMKtec EVO-X2 embedded controller

# Build instructions
```
$ make
$ sudo insmod evox2-ec.ko
```

# Devices
```
# Fan devices
/sys/class/evox2_ec/fan1/ <-- CPU fan 1
/sys/class/evox2_ec/fan2/ <-- CPU fan 2
/sys/class/evox2_ec/fan3/ <-- System fan

../fanX/speed <-- current speed in rpm
../fanX/mode <-- fan-mode (auto/manual) - writable
../fanX/manual_speed <-- fixed speed in manual from 0 - 5 = 0 to 100% in 20% steps

# Temp device
/sys/class/evox2_ec/temp/temp <-- temperature in C
```
