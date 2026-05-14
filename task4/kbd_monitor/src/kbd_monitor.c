#include <linux/module.h>
#include <linux/interrupt.h>
#include <asm/io.h>

#define KBD_IRQ 1

static int dev_id;

static irqreturn_t kb_irq(int irq, void *dev)
{
    unsigned char scancode;
    unsigned char code;
    bool pressed;

    scancode = inb(0x60);
    pressed = !(scancode & 0x80);
    code = scancode & 0x7f;    

    pr_info("kbd_monitor: raw=0x%02x -> code=0x%02x type=%s\n",
        scancode, code, pressed ? "press" : "release");

    return IRQ_HANDLED;
}

static int __init kb_init(void)
{
    int ret;

    ret = request_irq(
        KBD_IRQ,
        kb_irq,
        IRQF_SHARED,
        "edu_kbd_monitor",
        &dev_id
    );

    if (ret) {
        pr_err("failed to register irq\n");
        return ret;
    }

    pr_info("keyboard monitor loaded\n");

    return 0;
}

static void __exit kb_exit(void)
{
    free_irq(KBD_IRQ, &dev_id);

    pr_info("keyboard monitor unloaded\n");
}

module_init(kb_init);
module_exit(kb_exit);

MODULE_LICENSE("GPL");
