#include <linux/debugfs.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/uaccess.h>
#include <asm/pgtable.h>
#include <linux/slab.h>
#include <asm/io.h>
#include <linux/pgtable.h>

MODULE_LICENSE("GPL");

static struct dentry *dir, *output;
static struct task_struct *task;

struct packet {
        pid_t pid;
        unsigned long vaddr;
        unsigned long paddr;
};

static ssize_t read_output(struct file *fp,
                        char __user *user_buffer,
                        size_t length,
                        loff_t *position)
{
        // Implement read file operation
        // get inputs
        struct packet *pbuf = (struct packet *) user_buffer;
        pid_t pid = pbuf->pid;
        unsigned long vad = pbuf->vaddr;
        unsigned long ppn, ppo;
        // page table walk
        task = pid_task(find_vpid(pid), PIDTYPE_PID);
        pgd_t *pgdp = pgd_offset(task->mm, vad);
        p4d_t *p4dp = p4d_offset(pgdp, vad);
        pud_t *pudp = pud_offset(p4dp, vad);
        pmd_t *pmdp = pmd_offset(pudp, vad);
        pte_t *ptep = pte_offset_kernel(pmdp, vad);
        // setting paddr
        ppn = pte_pfn(*ptep) << PAGE_SHIFT;
        ppo = vad & (~PAGE_MASK);
        pbuf->paddr = ppn | ppo;
        return 0;
}

static const struct file_operations dbfs_fops = {
        // Mapping file operations with your functions
        .read = read_output,
};

static int __init dbfs_module_init(void)
{
        // Implement init module

        dir = debugfs_create_dir("paddr", NULL);

        if (!dir) {
                printk("Cannot create paddr dir\n");
                return -1;
        }

        // Fill in the arguments below
        output = debugfs_create_file("output", 0444, dir, NULL, &dbfs_fops);

	printk("dbfs_paddr module initialize done\n");

        return 0;
}

static void __exit dbfs_module_exit(void)
{
        // Implement exit module
        debugfs_remove_recursive(dir);
	printk("dbfs_paddr module exit\n");
}

module_init(dbfs_module_init);
module_exit(dbfs_module_exit);
