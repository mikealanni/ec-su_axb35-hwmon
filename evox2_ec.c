// evox2_ec.c

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/device.h>
#include <linux/uaccess.h>
#include <linux/io.h>
#include <linux/version.h>

#define EC_CMD_PORT  0x66
#define EC_DATA_PORT 0x62
#define EC_CMD_READ  0x80

struct ec_fan {
    const char *name;
    u8 reg_high;
    u8 reg_low;
    struct device *dev;
};

static struct class *ec_class;

static struct ec_fan ec_fans[] = {
    { .name = "fan1", .reg_high = 0x35, .reg_low = 0x36 },
    { .name = "fan2", .reg_high = 0x37, .reg_low = 0x38 },
    { .name = "fan3", .reg_high = 0x28, .reg_low = 0x29 },
};

static u8 ec_read(u8 reg)
{
    while (inb(EC_CMD_PORT) & 0x02)
        cpu_relax();
    outb(EC_CMD_READ, EC_CMD_PORT);

    while (inb(EC_CMD_PORT) & 0x02)
        cpu_relax();
    outb(reg, EC_DATA_PORT);

    while (!(inb(EC_CMD_PORT) & 0x01))
        cpu_relax();
    return inb(EC_DATA_PORT);
}

static ssize_t fan_speed_show(struct device *dev,
                              struct device_attribute *attr, char *buf)
{
    struct ec_fan *fan = dev_get_drvdata(dev);
    u8 hi = ec_read(fan->reg_high);
    u8 lo = ec_read(fan->reg_low);
    u16 rpm = (hi << 8) | lo;
    return sprintf(buf, "%u\n", rpm);
}

static DEVICE_ATTR_RO(fan_speed);

static int __init evox2_ec_init(void)
{
    int i;

    #if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 4, 0)
        ec_class = class_create("evox2_ec");
    #else
        ec_class = class_create(THIS_MODULE, "evox2_ec");
    #endif
    if (IS_ERR(ec_class))
        return PTR_ERR(ec_class);

    for (i = 0; i < ARRAY_SIZE(ec_fans); i++) {
        struct ec_fan *fan = &ec_fans[i];

        fan->dev = device_create(ec_class, NULL, MKDEV(0, i), fan, fan->name);
        if (IS_ERR(fan->dev))
            continue;

        dev_set_drvdata(fan->dev, fan);
        device_create_file(fan->dev, &dev_attr_fan_speed);
    }

    pr_info("evox2_ec: GMKTec EVO-X2 EC driver loaded\n");
    return 0;
}

static void __exit evox2_ec_exit(void)
{
    int i;
    for (i = 0; i < ARRAY_SIZE(ec_fans); i++) {
        if (!IS_ERR(ec_fans[i].dev)) {
            device_remove_file(ec_fans[i].dev, &dev_attr_fan_speed);
            device_destroy(ec_class, MKDEV(0, i));
        }
    }
    class_destroy(ec_class);
    pr_info("evox2_ec: Modul unloaded\n");
}

module_init(evox2_ec_init);
module_exit(evox2_ec_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("loom@mopper.de");
MODULE_DESCRIPTION("GMKtec EVO-x2 Embedded Controller Driver");
