// evox2_ec.c

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/device.h>
#include <linux/uaccess.h>
#include <linux/io.h>
#include <linux/version.h>
#include <linux/mutex.h>
#include <linux/workqueue.h>
#include <linux/delay.h>

#define EC_CMD_PORT  0x66
#define EC_DATA_PORT 0x62
#define EC_CMD_READ  0x80
#define EC_CMD_WRITE 0x81

static DEFINE_MUTEX(ec_lock);

struct ec_fan {
    const char *name;
    u8 speed_reg_high;
    u8 speed_reg_low;
    u8 mode_reg;
    u8 rampup_curve[6];
    u8 rampdown_curve[6];
    struct device *dev;
};

struct ec_temp {
    const char *name;
    u8 reg;
    u8 temp_min;
    u8 temp_max;
    struct device *dev;
};

static struct class *ec_class;

static struct ec_fan ec_fans[] = {
    { .name = "fan1", .speed_reg_high = 0x35, .speed_reg_low = 0x36, .mode_reg = 0x21, .rampup_curve = {0,40,50,60,70,80} , .rampdown_curve = {0,35,45,55,65,75} },
    { .name = "fan2", .speed_reg_high = 0x37, .speed_reg_low = 0x38, .mode_reg = 0x23, .rampup_curve = {0,40,50,60,70,80} , .rampdown_curve = {0,35,45,55,65,75} },
    { .name = "fan3", .speed_reg_high = 0x28, .speed_reg_low = 0x29, .mode_reg = 0x25, .rampup_curve = {0,40,50,60,70,80} , .rampdown_curve = {0,35,45,55,65,75} },
};

static struct ec_temp ec_temp = {
    .name = "temp1",
    .reg = 0x70,
};

static u8 ec_read(u8 reg)
{
    u8 val;
    
    mutex_lock(&ec_lock);
    
    while (inb(EC_CMD_PORT) & 0x02)
        cpu_relax();
    outb(EC_CMD_READ, EC_CMD_PORT);

    while (inb(EC_CMD_PORT) & 0x02)
        cpu_relax();
    outb(reg, EC_DATA_PORT);

    while (!(inb(EC_CMD_PORT) & 0x01))
        cpu_relax();
    val = inb(EC_DATA_PORT);
    
    mutex_unlock(&ec_lock);

    return val;
}

static void ec_write(u8 reg, u8 val)
{
    mutex_lock(&ec_lock);

    while (inb(EC_CMD_PORT) & 0x02)
        cpu_relax();
    outb(EC_CMD_WRITE, EC_CMD_PORT);

    while (inb(EC_CMD_PORT) & 0x02)
        cpu_relax();
    outb(reg, EC_DATA_PORT);

    while (inb(EC_CMD_PORT) & 0x02)
        cpu_relax();
    outb(val, EC_DATA_PORT);

    mutex_unlock(&ec_lock);
}

static ssize_t fan_rpm_show(struct device *dev,
                              struct device_attribute *attr, char *buf)
{
    struct ec_fan *fan = dev_get_drvdata(dev);
    u8 hi = ec_read(fan->speed_reg_high);
    u8 lo = ec_read(fan->speed_reg_low);
    u16 rpm = (hi << 8) | lo;
    // wired fan3 behavior, displaying 8000 before turning to 0
    if (strcmp(fan->name,"fan3") == 0 && rpm == 8000)
        rpm = 0;
    return sprintf(buf, "%u\n", rpm);
}

static struct device_attribute dev_attr_fan_rpm = __ATTR(rpm, 0444, fan_rpm_show, NULL);

static ssize_t fan_mode_show(struct device *dev,
                         struct device_attribute *attr, char *buf)
{
    struct ec_fan *fan = dev_get_drvdata(dev);
    u8 val = ec_read(fan->mode_reg);

    const char *mode = "unknown";
    switch (val) {
        case 0x10: case 0x20: case 0x30:
            mode = "auto"; break;
        case 0x11: case 0x21: case 0x31:
            mode = "fixed"; break;
    }

    return sprintf(buf, "%s\n", mode);
}

static ssize_t fan_mode_store(struct device *dev,
                          struct device_attribute *attr,
                          const char *buf, size_t count)
{
    struct ec_fan *fan = dev_get_drvdata(dev);
    u8 val;

    if (sysfs_streq(buf, "auto")) {
        switch (fan->mode_reg) {
            case 0x21: val = 0x10; break;
            case 0x23: val = 0x20; break;
            case 0x25: val = 0x30; break;
            default: return -EINVAL;
        }
    } else if (sysfs_streq(buf, "fixed")) {
        switch (fan->mode_reg) {
            case 0x21: val = 0x11; break;
            case 0x23: val = 0x21; break;
            case 0x25: val = 0x31; break;
            default: return -EINVAL;
        }
    } else {
        return -EINVAL;
    }

    ec_write(fan->mode_reg, val);

    return count;
}

static struct device_attribute dev_attr_fan_mode = __ATTR(mode, 0644, fan_mode_show, fan_mode_store);

static ssize_t fan_level_show(struct device *dev,
                         struct device_attribute *attr, char *buf)
{
    struct ec_fan *fan = dev_get_drvdata(dev);
    u8 val = ec_read(fan->mode_reg + 1);

    const char *speed = "unknown";
    switch (val & 0xF) {
        case 0x7: // off
            speed = "0"; break;
        case 0x2: // 20%
            speed = "1"; break;
        case 0x3: // 40%
            speed = "2"; break;
        case 0x4: // 60%
            speed = "3"; break;
        case 0x5: // 80%
            speed = "4"; break;
        case 0x6: // 100%
            speed = "5"; break;
    }

    return sprintf(buf, "%s\n", speed);
}

static ssize_t fan_level_store(struct device *dev,
                          struct device_attribute *attr,
                          const char *buf, size_t count)
{
    struct ec_fan *fan = dev_get_drvdata(dev);
    u8 val;

    switch (fan->mode_reg) {
        case 0x21: val = 0x10; break;
        case 0x23: val = 0x20; break;
        case 0x25: val = 0x30; break;
        default: return -EINVAL;
    }
    if (sysfs_streq(buf, "0")) {
        val += 0x7;
    } else if (sysfs_streq(buf, "1")) {
        val += 0x2;
    } else if (sysfs_streq(buf, "2")) {
        val += 0x3;
    } else if (sysfs_streq(buf, "3")) {
        val += 0x4;
    } else if (sysfs_streq(buf, "4")) {
        val += 0x5;
    } else if (sysfs_streq(buf, "5")) {
        val += 0x6;
    } else {
        return -EINVAL;
    }

    ec_write(fan->mode_reg + 1, val);

    return count;
}

static struct device_attribute dev_attr_fan_level = __ATTR(level, 0644, fan_level_show, fan_level_store);

static ssize_t fan_curve_show(u8 *curve, char *buf)
{
    int i;
    char *p = buf;

    for (i = 1; i < 6; i++) {
        p += sprintf(p, "%d", curve[i]);
        if (i < 5) {
            *p++ = ',';
        }
    }
    *p++ = '\n';
    *p = '\0';

    return p - buf;
}

static ssize_t fan_curve_store(u8 *curve, const char *buf, size_t count)
{
    char *str = kstrndup(buf, count, GFP_KERNEL);
    char *token;
    int values[5];
    int i = 0;
    int ret = 0;

    if (!str)
        return -ENOMEM;

    token = strsep(&str, ",");
    while (token && i < 5) {
        if (kstrtoint(token, 10, &values[i]) < 0) {
            ret = -EINVAL;
            break;
        }
        if (values[i] < 0 || values[i] > 100) {
            ret = -EINVAL;
            break;
        }
        i++;
        token = strsep(&str, ",");
    }

    if (i != 5)
        ret = -EINVAL;

    if (ret == 0) {
        for (i = 0; i < 5; i++) {
            curve[i + 1] = values[i];
        }
    }

    kfree(str);
    return ret ? ret : count;
}

static ssize_t fan_rampup_curve_show(struct device *dev,
                          struct device_attribute *attr, char *buf)
{
    struct ec_fan *fan = dev_get_drvdata(dev);
    return fan_curve_show(fan->rampup_curve, buf);
}

static ssize_t fan_rampup_curve_store(struct device *dev,
                           struct device_attribute *attr,
                           const char *buf, size_t count)
{
    struct ec_fan *fan = dev_get_drvdata(dev);
    return fan_curve_store(fan->rampup_curve, buf, count);
}

static struct device_attribute dev_attr_fan_rampup_curve = __ATTR(rampup_curve, 0644, fan_rampup_curve_show, fan_rampup_curve_store);

static ssize_t fan_rampdown_curve_show(struct device *dev,
                          struct device_attribute *attr, char *buf)
{
    struct ec_fan *fan = dev_get_drvdata(dev);
    return fan_curve_show(fan->rampdown_curve, buf);
}

static ssize_t fan_rampdown_curve_store(struct device *dev,
                           struct device_attribute *attr,
                           const char *buf, size_t count)
{
    struct ec_fan *fan = dev_get_drvdata(dev);
    return fan_curve_store(fan->rampdown_curve, buf, count);
}

static struct device_attribute dev_attr_fan_rampdown_curve = __ATTR(rampdown_curve, 0644, fan_rampdown_curve_show, fan_rampdown_curve_store);

static ssize_t temp_current_show(struct device *dev,
                            struct device_attribute *attr, char *buf)
{
    struct ec_temp *temp = dev_get_drvdata(dev);
    u8 value = ec_read(temp->reg);
    return sprintf(buf, "%u\n", value);
}

static struct device_attribute dev_attr_temp_cur = __ATTR(temp, 0444, temp_current_show, NULL);

static ssize_t temp_min_show(struct device *dev,
                            struct device_attribute *attr, char *buf)
{
    struct ec_temp *temp = dev_get_drvdata(dev);
    return sprintf(buf, "%u\n", temp->temp_min);
}

static struct device_attribute dev_attr_temp_min = __ATTR(min, 0444, temp_min_show, NULL);

static ssize_t temp_max_show(struct device *dev,
                            struct device_attribute *attr, char *buf)
{
    struct ec_temp *temp = dev_get_drvdata(dev);
    return sprintf(buf, "%u\n", temp->temp_max);
}

static struct device_attribute dev_attr_temp_max = __ATTR(max, 0444, temp_max_show, NULL);

static struct delayed_work ec_update_work;

static void ec_update_worker(struct work_struct *work)
{
    u8 temp = ec_read(ec_temp.reg);

    // Update min/max
    if (ec_temp.temp_min == 0 || temp < ec_temp.temp_min)
        ec_temp.temp_min = temp;

    if (temp > ec_temp.temp_max)
        ec_temp.temp_max = temp;

    // TODO: implement curve logic

    // Requeue the work
    schedule_delayed_work(&ec_update_work, msecs_to_jiffies(1000)); // every 1 sec
}

static dev_t evox2_ec_dev;
static struct class *ec_class;

static int __init evox2_ec_init(void)
{
    int i;
    int ret;

    ret = alloc_chrdev_region(&evox2_ec_dev, 0, ARRAY_SIZE(ec_fans) + 1, "evox2_ec");
    if (ret <0) {
        pr_err("evox2_ec: Failed to allocation major number\n");
        return ret;
    }

    #if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 4, 0)
        ec_class = class_create("evox2_ec");
    #else
        ec_class = class_create(THIS_MODULE, "evox2_ec");
    #endif

    if (IS_ERR(ec_class)) {
        unregister_chrdev_region(evox2_ec_dev, ARRAY_SIZE(ec_fans) + 1);
        return PTR_ERR(ec_class);
    }

    for (i = 0; i < ARRAY_SIZE(ec_fans); i++) {
        struct ec_fan *fan = &ec_fans[i];

        fan->dev = device_create(ec_class, NULL, MKDEV(MAJOR(evox2_ec_dev), i), fan, fan->name);
        if (IS_ERR(fan->dev))
            continue;

        dev_set_drvdata(fan->dev, fan);
        device_create_file(fan->dev, &dev_attr_fan_rpm);
        device_create_file(fan->dev, &dev_attr_fan_mode);
        device_create_file(fan->dev, &dev_attr_fan_level);
        device_create_file(fan->dev, &dev_attr_fan_rampup_curve);
        device_create_file(fan->dev, &dev_attr_fan_rampdown_curve);
    }

    ec_temp.dev = device_create(ec_class, NULL, MKDEV(MAJOR(evox2_ec_dev), ARRAY_SIZE(ec_fans)), &ec_temp, ec_temp.name);
    if (!IS_ERR(ec_temp.dev)) {
        dev_set_drvdata(ec_temp.dev, &ec_temp);
        device_create_file(ec_temp.dev, &dev_attr_temp_cur);
        device_create_file(ec_temp.dev, &dev_attr_temp_min);
        device_create_file(ec_temp.dev, &dev_attr_temp_max);
    }

    INIT_DELAYED_WORK(&ec_update_work, ec_update_worker);
    schedule_delayed_work(&ec_update_work, msecs_to_jiffies(1000));

    pr_info("evox2_ec: GMKTec EVO-X2 EC driver loaded\n");
    return 0;
}

static void __exit evox2_ec_exit(void)
{
    int i;
    for (i = 0; i < ARRAY_SIZE(ec_fans); i++) {
        if (!IS_ERR(ec_fans[i].dev)) {
            device_remove_file(ec_fans[i].dev, &dev_attr_fan_rpm);
            device_remove_file(ec_fans[i].dev, &dev_attr_fan_mode);
            device_remove_file(ec_fans[i].dev, &dev_attr_fan_level);
            device_remove_file(ec_fans[i].dev, &dev_attr_fan_rampup_curve);
            device_remove_file(ec_fans[i].dev, &dev_attr_fan_rampdown_curve);
            device_destroy(ec_class, MKDEV(MAJOR(evox2_ec_dev), i));
        }
    }

    if (!IS_ERR(ec_temp.dev)) {
        device_remove_file(ec_temp.dev, &dev_attr_temp_cur);
        device_remove_file(ec_temp.dev, &dev_attr_temp_min);
        device_remove_file(ec_temp.dev, &dev_attr_temp_max);
        device_destroy(ec_class, MKDEV(MAJOR(evox2_ec_dev), ARRAY_SIZE(ec_fans)));
    }

    cancel_delayed_work_sync(&ec_update_work);

    class_destroy(ec_class);
    unregister_chrdev_region(evox2_ec_dev, ARRAY_SIZE(ec_fans) + 1);
    pr_info("evox2_ec: Modul unloaded\n");
}

module_init(evox2_ec_init);
module_exit(evox2_ec_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("loom@mopper.de");
MODULE_DESCRIPTION("GMKtec EVO-x2 Embedded Controller Driver");
