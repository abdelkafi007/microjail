# Microjail

A minimal, OCI-inspired Linux container runtime written from scratch in C.

Microjail spawns an arbitrary process and isolates it from the host machine using four core Linux kernel mechanisms: namespaces, filesystem jailing via `pivot_root`, cgroups v2 resource limits, and a seccomp-BPF syscall firewall. It mirrors the fundamental architecture of production runtimes like `runc` and `crun`.

## Architecture

### Namespace Virtualization

The child process is spawned via the `clone()` syscall with the following namespace flags:

- `CLONE_NEWUTS` — Isolates system identifiers. The container operates with its own hostname, independent of the host.
- `CLONE_NEWPID` — Isolates the process ID number space. The container's first process runs as PID 1 and cannot observe or signal host processes.
- `CLONE_NEWNS` — Provides a private mount table. All mount and unmount operations inside the container are invisible to the host.

### Filesystem Jailing (pivot_root)

The container's root filesystem is swapped to a minimal Alpine Linux rootfs using `pivot_root(2)`. The host's original root is temporarily stashed in a subdirectory, then immediately unmounted via `umount2()` with `MNT_DETACH` and removed. After this sequence, no path from within the container can reach the host filesystem.

A fresh `procfs` is mounted at `/proc` inside the new root so that utilities like `ps` function correctly under the isolated PID namespace.

### Resource Throttling (cgroups v2)

The runtime programmatically creates a cgroup at `/sys/fs/cgroup/microjail/` and writes the container's PID into `cgroup.procs`. Resource limits are enforced by writing to the corresponding control files:

- `pids.max` — Caps the maximum number of processes the container can spawn, providing direct mitigation against fork-bomb attacks.

### Security and Privilege Restriction

**Capability Bounding Set:** Before executing the user payload, the runtime iterates over all 64 capability indices and calls `prctl(PR_CAPBSET_DROP, ...)` to permanently clear the bounding set. It then constructs an empty capability state via `libcap` and applies it with `cap_set_proc()`. This prevents the kernel from restoring privileges across `execve()` boundaries, even for UID 0.

**Seccomp-BPF:** A BPF filter is compiled and loaded into the kernel via `libseccomp`. The default policy is set to `SCMP_ACT_ALLOW`, with explicit `SCMP_ACT_KILL` rules for dangerous syscalls (e.g., `mkdir`). Any violation results in immediate process termination via `SIGSYS`.

## Build

### Dependencies

Fedora / RHEL:
```
sudo dnf install gcc libcap-devel libseccomp-devel
```

Ubuntu / Debian:
```
sudo apt install gcc libcap-dev libseccomp-dev
```

### Rootfs Setup

Download and extract a minimal Alpine Linux root filesystem:
```
mkdir rootfs
wget https://dl-cdn.alpinelinux.org/alpine/v3.20/releases/x86_64/alpine-minirootfs-3.20.0-x86_64.tar.gz
tar -xzf alpine-minirootfs-*.tar.gz -C rootfs
```

### Compile

```
gcc microjail.c -o microjail -lcap -lseccomp
```

### Run

Creating namespaces and writing to cgroup control files requires root privileges:
```
sudo ./microjail
```

## Verification

From inside the container shell:

| Command | Expected Result |
|---|---|
| `hostname` | Prints `container`, not the host hostname |
| `ps` | Shows only the container's own processes, with the shell as PID 1 |
| `ls /` | Shows the Alpine rootfs only; host directories are not visible |
| `hostname hacked` | Denied with `Operation not permitted` (capabilities dropped) |
| `mkdir test` | Terminated with `Bad system call` (seccomp filter) |
| `:()\{ :\|:& \};:` | Fork-bomb hits `pids.max` ceiling and fails safely |

## Disclaimer

This project is an educational proof-of-concept. It is not intended for production use.
