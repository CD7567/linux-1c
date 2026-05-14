#include <linux/module.h>
#include <linux/interrupt.h>
#include <asm/io.h>

#define KBD_IRQ 1

static int dev_id;

static irqreturn_t kb_irq(int irq, void *dev)
{
    unsigned char scancode;

    scancode = inb(0x60);

    pr_info("edu_kbd_monitor: scan=0x%x\n", scancode);

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
