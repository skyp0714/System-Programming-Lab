#include <linux/debugfs.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/uaccess.h>
#include <linux/slab.h>

#define LEN_MAX 1000

MODULE_LICENSE("GPL");

static struct dentry *dir, *inputdir, *ptreedir;
static struct task_struct *curr;

typedef struct debugfs_blob_wrapper blob;

blob *wblob;
char *out_str;

static ssize_t write_pid_to_input(struct file *fp, 
                                const char __user *user_buffer, 
                                size_t length, 
                                loff_t *position)
{
        char tmp[LEN_MAX];
        pid_t input_pid;
        memset(out_str, 0, sizeof(char));
        sscanf(user_buffer, "%u", &input_pid);
        curr = pid_task(find_vpid(input_pid), PIDTYPE_PID);

        // Tracing process tree from input_pid to init(1) process
        while(curr->pid != 0){
                snprintf(tmp, LEN_MAX, "%s (%d)\n%s", curr->comm, curr->pid, out_str);
                strcpy(out_str, tmp);
                curr = curr->parent;
        }

        return length;
}

static const struct file_operations dbfs_fops = {
        .write = write_pid_to_input,
};

static int __init dbfs_module_init(void)
{
        // Implement init module code

        dir = debugfs_create_dir("ptree", NULL);
        
        if (!dir) {
                printk("Cannot create ptree dir\n");
                return -1;
        }
        //setting wblob
        out_str = (char *) kmalloc(LEN_MAX*sizeof(char), GFP_KERNEL);
        wblob = (blob *) kmalloc(sizeof(blob), GFP_KERNEL);
        wblob->data = (void *) out_str;
        wblob->size = LEN_MAX * sizeof(char);

        inputdir = debugfs_create_file("input", 0644, dir, NULL, &dbfs_fops);
        ptreedir = debugfs_create_blob("ptree", 0444, dir, wblob); 

	
	printk("dbfs_ptree module initialize done\n");

        return 0;
}

static void __exit dbfs_module_exit(void)
{
        // Implement exit module code
	debugfs_remove_recursive(dir);
        kfree(wblob);
        kfree(out_str);
	printk("dbfs_ptree module exit\n");
}

module_init(dbfs_module_init);
module_exit(dbfs_module_exit);
