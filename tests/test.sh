#/usr/bin/env sh

echo "Temp"
echo "---------------"
echo -n "/sys/class/ec_su_axb35/temp1/temp: "
cat /sys/class/ec_su_axb35/temp1/temp
echo -n "/sys/class/ec_su_axb35/temp1/min: "
cat /sys/class/ec_su_axb35/temp1/min
echo -n "/sys/class/ec_su_axb35/temp1/max: "
cat /sys/class/ec_su_axb35/temp1/max


for i in 1 2 3; do
  echo -e "\nFan $i"
  echo "---------------"
  echo -n "/sys/class/ec_su_axb35/fan${i}/rpm: "
  cat /sys/class/ec_su_axb35/fan${i}/rpm
  echo -n "/sys/class/ec_su_axb35/fan${i}/mode: "
  cat /sys/class/ec_su_axb35/fan${i}/mode
  echo -n "/sys/class/ec_su_axb35/fan${i}/level: "
  cat /sys/class/ec_su_axb35/fan${i}/level
  echo -n "/sys/class/ec_su_axb35/fan${i}/rampup_curve: "
  cat /sys/class/ec_su_axb35/fan${i}/rampup_curve
  echo -n "/sys/class/ec_su_axb35/fan${i}/rampdown_curve: "
  cat /sys/class/ec_su_axb35/fan${i}/rampdown_curve
done
