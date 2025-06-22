# evox2-ec-linux
Linux driver (prototype) for the GMKtec EVO-X2 embedded controller

# Build instructions
```
$ make
$ sudo insmod evox2-ec.ko
```

# Devices
```
/sys/class/evox2_ec/fan1/fan_speed <-- CPU fan 1
/sys/class/evox2_ec/fan2/fan_speed <-- CPU fan 2
/sys/class/evox2_ec/fan3/fan_speed <-- System fan
```
