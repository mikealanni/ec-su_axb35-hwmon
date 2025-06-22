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
    struct device *dev;
};

struct ec_temp {
    const char *name;
    u8 reg;
    struct device *dev;
};

static struct class *ec_class;

static struct ec_fan ec_fans[] = {
    { .name = "fan1", .speed_reg_high = 0x35, .speed_reg_low = 0x36, .mode_reg = 0x21, },
    { .name = "fan2", .speed_reg_high = 0x37, .speed_reg_low = 0x38, .mode_reg = 0x23 },
    { .name = "fan3", .speed_reg_high = 0x28, .speed_reg_low = 0x29, .mode_reg = 0x25 },
};

static struct ec_temp ec_temp = {
    .name = "temp",
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

static ssize_t speed_show(struct device *dev,
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

static DEVICE_ATTR_RO(speed);

static ssize_t mode_show(struct device *dev,
                         struct device_attribute *attr, char *buf)
{
    struct ec_fan *fan = dev_get_drvdata(dev);
    u8 val = ec_read(fan->mode_reg);

    const char *mode = "unknown";
    switch (val) {
        case 0x10: case 0x20: case 0x30:
            mode = "auto"; break;
        case 0x11: case 0x21: case 0x31:
            mode = "manual"; break;
    }

    return sprintf(buf, "%s\n", mode);
}

static ssize_t mode_store(struct device *dev,
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
    } else if (sysfs_streq(buf, "manual")) {
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

static DEVICE_ATTR_RW(mode);

static ssize_t manual_speed_show(struct device *dev,
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

static ssize_t manual_speed_store(struct device *dev,
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

static DEVICE_ATTR_RW(manual_speed);

static ssize_t temp_show(struct device *dev,
                         struct device_attribute *attr, char *buf)
{
    struct ec_temp *temp = dev_get_drvdata(dev);
    u8 value = ec_read(temp->reg);
    return sprintf(buf, "%u\n", value);
}

static DEVICE_ATTR_RO(temp);

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
        device_create_file(fan->dev, &dev_attr_speed);
        device_create_file(fan->dev, &dev_attr_mode);
        device_create_file(fan->dev, &dev_attr_manual_speed);
    }

    // Create temperature device
    ec_temp.dev = device_create(ec_class, NULL, MKDEV(MAJOR(evox2_ec_dev), ARRAY_SIZE(ec_fans)), &ec_temp, ec_temp.name);
    if (!IS_ERR(ec_temp.dev)) {
        dev_set_drvdata(ec_temp.dev, &ec_temp);
        device_create_file(ec_temp.dev, &dev_attr_temp);
    }

    pr_info("evox2_ec: GMKTec EVO-X2 EC driver loaded\n");
    return 0;
}

static void __exit evox2_ec_exit(void)
{
    int i;
    for (i = 0; i < ARRAY_SIZE(ec_fans); i++) {
        if (!IS_ERR(ec_fans[i].dev)) {
            device_remove_file(ec_fans[i].dev, &dev_attr_speed);
            device_remove_file(ec_fans[i].dev, &dev_attr_mode);
            device_remove_file(ec_fans[i].dev, &dev_attr_manual_speed);
            device_destroy(ec_class, MKDEV(MAJOR(evox2_ec_dev), i));
        }
    }

    // Cleanup temperature device
    if (!IS_ERR(ec_temp.dev)) {
        device_remove_file(ec_temp.dev, &dev_attr_temp);
        device_destroy(ec_class, MKDEV(MAJOR(evox2_ec_dev), ARRAY_SIZE(ec_fans)));
    }

    class_destroy(ec_class);
    unregister_chrdev_region(evox2_ec_dev, ARRAY_SIZE(ec_fans) + 1);
    pr_info("evox2_ec: Modul unloaded\n");
}

module_init(evox2_ec_init);
module_exit(evox2_ec_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("loom@mopper.de");
MODULE_DESCRIPTION("GMKtec EVO-x2 Embedded Controller Driver");
