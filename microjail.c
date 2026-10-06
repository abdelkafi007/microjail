#define _GNU_SOURCE // Required to unlock the clone() system call in C
#include <sched.h>  // For clone() and namespace flags
#include <stdio.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <unistd.h>
#include <string.h>
#include <sys/mount.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <errno.h>
#include <sys/capability.h>
#include <sys/prctl.h>
#include <seccomp.h>


#define STACK_SIZE (1024 * 1024) 

// This is the function the child process will execute when spawned
int child_payload(void *arg) {

        // 1. Make all mounts private first
    if (mount(NULL, "/", NULL, MS_PRIVATE | MS_REC, NULL) == -1) {
        perror("private mount failed");
        return -1;
    }

    // 2. Bind-mount the rootfs folder BEFORE we step into it
    if (mount("./rootfs", "./rootfs", NULL, MS_BIND | MS_REC, NULL) == -1) {
        perror("BIND mount failed");
        return -1;
    }

    
    if (chdir("./rootfs") == -1) {
        perror("chdir failed");
        return -1;
    }

    // 4. Create stash and pivot!
    mkdir("oldroot", 0755);
    if (syscall(SYS_pivot_root, ".", "oldroot") == -1) {
        perror("pivot_root failed");
        return -1;
    }
    chdir("/");
    umount2("oldroot", MNT_DETACH);
    rmdir("oldroot");
    mount("proc", "/proc", "proc", 0, NULL);

    char *new_hostname = "container";
    char *argv[] = {"/bin/sh", NULL};
    char *envp[] = {"PATH=/usr/bin:/bin", NULL };
    if (sethostname(new_hostname, strlen(new_hostname)) == -1) {
        perror("sethostname failed");
        return -1;
    }
    for (int i = 0; i < 64; i++) {
    prctl(PR_CAPBSET_DROP, i, 0, 0, 0);
}
    cap_t caps = cap_init();
    cap_clear(caps);
    if (cap_set_proc(caps) == -1) {
        perror("Failed to drop root");
        return -1;
    }
    cap_free(caps);
    scmp_filter_ctx ctx = seccomp_init(SCMP_ACT_ALLOW);
    seccomp_rule_add(ctx, SCMP_ACT_KILL, SCMP_SYS(mkdir), 0);
    seccomp_load(ctx);
    seccomp_release(ctx);

    execve("/bin/sh", argv, envp);
    perror("execve failed");
    return -1;



}

int main(int argc, char *argv[]) {
    printf("Parent: Starting microjail...\n");

    // 1. Allocate memory for the child process's stack
    char *child_stack = malloc(STACK_SIZE);
    if (child_stack == NULL) {
        perror("malloc failed");
        exit(EXIT_FAILURE);
    }

    
    int flags = CLONE_NEWUTS | CLONE_NEWNS | CLONE_NEWPID | SIGCHLD; 


    pid_t child_pid = clone(child_payload, child_stack + STACK_SIZE, flags, NULL);

    if (child_pid == -1) {
        perror("clone failed");
        exit(EXIT_FAILURE);
    }

    printf("Parent: Spawned container process with PID %d\n", child_pid);
    if (mkdir("/sys/fs/cgroup/microjail", 0755) == -1 && errno != EEXIST) {
    // If it fails, AND the error is not "Folder Already Exists" (EEXIST), then crash.
        perror("Failed to create cgroup");
        exit(EXIT_FAILURE);
    }
    FILE *f = fopen("/sys/fs/cgroup/microjail/pids.max", "w");
    if (f != NULL) {
        fprintf(f, "20");
        fclose(f);
    }
     FILE *f_procs = fopen("/sys/fs/cgroup/microjail/cgroup.procs", "w");
     if (f_procs != NULL) {
        fprintf(f_procs, "%d", child_pid);
        fclose(f_procs);
     }
    // 4. Parent process waits here until the child exits
    waitpid(child_pid, NULL, 0);
    printf("Parent: Container exited. Shutting down.\n");

    free(child_stack);
    return 0;
}