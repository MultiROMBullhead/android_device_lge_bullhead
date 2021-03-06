/*
 * This file contains device specific hooks.
 * Always enclose hooks to #if MR_DEVICE_HOOKS >= ver
 * with corresponding hook version!
 */

#include <stdio.h>
#include <unistd.h>
#include <sys/stat.h>
#include <stdint.h>
#include <time.h>
#include <string.h>

#include <errno.h>
#include <signal.h>
#include <sys/wait.h>
#include <fcntl.h>

#include <private/android_filesystem_config.h>

#include <log.h>
#include <util.h>
#include <multirom.h>

#define GATEKEEPER_DATA_DIR "/data/misc/gatekeeper"
#define GATEKEEPER_COLDBOOT_PATH "/data/misc/gatekeeper/.coldboot"

#if MR_DEVICE_HOOKS >= 1
int mrom_hook_after_android_mounts(const char *busybox_path, const char *base_path, int type)
{
    // wiping the data of a primary or secondary ROM causes the ROM to delete
    // all lockscreen accounts from the gatekeeper on next boot, preventing
    // the user from logging into the other ROMs.
    // To work around this, create the .coldboot file to prevent the wipe.
    if (access(GATEKEEPER_COLDBOOT_PATH, F_OK) == -1)
    {
        // the permission should be fixed during the first boot
        int err = mkdir_recursive(GATEKEEPER_DATA_DIR, 0700);
        if (err)
            ERROR("failed to mkdir " GATEKEEPER_DATA_DIR ": %s", strerror(err));
        else
        {
            int fd = open(GATEKEEPER_COLDBOOT_PATH, O_WRONLY|O_TRUNC|O_CREAT,
                S_IRUSR|S_IWUSR);
            if (fd < 0)
                ERROR("failed to open " GATEKEEPER_COLDBOOT_PATH ": %s",
                    strerror(errno));
            else
            {
                fchown(fd, AID_SYSTEM, AID_SYSTEM);
                close(fd);
            }
        }
    }

    if (type == ROM_DEFAULT)
        return 0;

    return 0;
}
#endif /* MR_DEVICE_HOOKS >= 1 */


#if MR_DEVICE_HOOKS >= 2
void mrom_hook_before_fb_close(void)
{
}
#endif /* MR_DEVICE_HOOKS >= 2 */


#if MR_DEVICE_HOOKS >= 3
void tramp_hook_before_device_init(void)
{
}
#endif /* MR_DEVICE_HOOKS >= 3 */

#if MR_DEVICE_HOOKS >= 4
int mrom_hook_allow_incomplete_fstab(void)
{
    return 0;
}
#endif

#if MR_DEVICE_HOOKS >= 5
void mrom_hook_fixup_bootimg_cmdline(char *bootimg_cmdline, size_t bootimg_cmdline_cap)
{
}

int mrom_hook_has_kexec(void)
{
    // check for fdt blob
    static const char *checkfile = "/sys/firmware/fdt";
    if(access(checkfile, R_OK) < 0)
    {
        ERROR("%s was not found!\n", checkfile);
        return 0;
    }
    return 1;
}
#endif

#if MR_DEVICE_HOOKS >= 6
static int fork_and_exec(char **cmd, char** env)
{
    pid_t pID = fork();
    if(pID == 0)
    {
        stdio_to_null();
        setpgid(0, getpid());
        execve(cmd[0], cmd, env);
        ERROR("Failed to exec %s: %s\n", cmd[0], strerror(errno));
        _exit(127);
    }
    return pID;
}

static int qseecomd_pid = -1;

void tramp_hook_encryption_setup(void)
{
    // start qseecomd
    char* cmd[] = {"/mrom_enc/qseecomd", NULL};
    char* env[] = {"LD_LIBRARY_PATH=/mrom_enc", NULL};
    // setup links and permissions based on TWRP's init.recovery.bullhead.rc
    symlink("/dev/block/platform/soc.0/f9824900.sdhci", "/dev/block/bootdevice");
    chmod("/dev/qseecom", 0660);
    chown("/dev/qseecom", AID_SYSTEM, AID_DRMRPC);
    chown("/dev/ion", AID_SYSTEM, AID_SYSTEM);
    chmod("/mrom_enc/qseecomd", 0755);
    qseecomd_pid = fork_and_exec(cmd, env);
    if (qseecomd_pid == -1)
        ERROR("Failed to fork for qseecomd; should never happen!\n");
    else
        INFO("qseecomd started: pid=%d\n", qseecomd_pid);
}

void tramp_hook_encryption_cleanup(void)
{
    struct stat info;
    if (qseecomd_pid != -1)
    {
        kill(-qseecomd_pid, SIGTERM); // kill the entire process group
        waitpid(qseecomd_pid, NULL, 0);
    }
    // make sure we're removing our symlink
    if (lstat("/dev/block/bootdevice", &info) >= 0 && S_ISLNK(info.st_mode))
        remove("/dev/block/bootdevice");
    INFO("cleaned up after qseecomd\n");
}

void mrom_hook_fixup_full_cmdline(char *bootimg_cmdline, size_t bootimg_cmdline_cap)
{
}
#endif
