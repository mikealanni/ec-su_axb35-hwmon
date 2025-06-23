#/usr/bin/env sh

echo "Temp"
echo "---------------"
echo -n "/sys/class/evox2_ec/temp1/temp: "
cat /sys/class/evox2_ec/temp1/temp
echo -n "/sys/class/evox2_ec/temp1/min: "
cat /sys/class/evox2_ec/temp1/min
echo -n "/sys/class/evox2_ec/temp1/max: "
cat /sys/class/evox2_ec/temp1/max


for i in 1 2 3; do
  echo -e "\nFan $i"
  echo "---------------"
  echo -n "/sys/class/evox2_ec/fan${i}/rpm: "
  cat /sys/class/evox2_ec/fan${i}/rpm
  echo -n "/sys/class/evox2_ec/fan${i}/mode: "
  cat /sys/class/evox2_ec/fan${i}/mode
  echo -n "/sys/class/evox2_ec/fan${i}/level: "
  cat /sys/class/evox2_ec/fan${i}/level
  echo -n "/sys/class/evox2_ec/fan${i}/rampup_curve: "
  cat /sys/class/evox2_ec/fan${i}/rampup_curve
  echo -n "/sys/class/evox2_ec/fan${i}/rampdown_curve: "
  cat /sys/class/evox2_ec/fan${i}/rampdown_curve
done
