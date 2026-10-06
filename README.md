# Microjail 🛡️

A minimal, OCI-inspired Linux container runtime written from scratch in raw C. 

`microjail` was built to demonstrate deep operating system literacy, kernel boundary security, and low-level Linux systems programming. It spawns a process and completely isolates it from the host machine using standard Linux kernel APIs, acting similarly to low-level runtimes like `runc` or `crun`.

## 🏗️ Core Architecture

This runtime implements the four fundamental pillars of Linux containerization:

1. **Namespace Virtualization (`clone`)**
   - **UTS Namespace:** Isolates system identifiers (hostname).
   - **PID Namespace:** Isolates the process tree (the container shell runs as `PID 1`).
   - **Mount Namespace:** Provides a private mount table to prevent host filesystem corruption.

2. **Filesystem Jailing (`pivot_root`)**
   - securely swaps the root filesystem to a minimal Alpine Linux rootfs.
   - Detaches and safely unmounts the host machine's root directory, preventing directory traversal or escape.

3. **Resource Throttling (cgroups v2)**
   - Programmatically interacts with `/sys/fs/cgroup`.
   - Enforces a strict `pids.max` limit to protect the host machine from malicious fork-bombs (`:(){ :|:& };:`).

4. **Security & Privilege Dropping**
   - **Linux Capabilities:** Aggressively wipes the Capability Bounding Set (`PR_CAPBSET_DROP`) and active sets using `libcap`, stripping the container's `root` user of all administrative privileges.
   - **Seccomp-BPF:** Implements a strict system call firewall via `libseccomp` to filter malicious syscalls, reducing the kernel attack surface (e.g., blocking `mkdir`, `chroot`, etc.).

## 🚀 How to Build & Run

### Prerequisites
You must be running a modern Linux distribution with `cgroups v2` enabled. You also need the C compiler and the development libraries for `libcap` and `libseccomp`.

**Fedora/RHEL:**
```bash
sudo dnf install gcc libcap-devel libseccomp-devel
```

**Ubuntu/Debian:**
```bash
sudo apt update
sudo apt install gcc libcap-dev libseccomp-dev
```

### Setup the Rootfs
Download a minimal Alpine Linux root filesystem to act as the container's environment:
```bash
mkdir rootfs
wget https://dl-cdn.alpinelinux.org/alpine/v3.20/releases/x86_64/alpine-minirootfs-3.20.0-x86_64.tar.gz
tar -xzf alpine-minirootfs-*.tar.gz -C rootfs
```

### Compile
Compile the C program and link the security libraries:
```bash
gcc microjail.c -o microjail -lcap -lseccomp
```

### Execute
Because creating namespaces and manipulating cgroups requires host privileges, the runtime must be executed with `sudo`.
```bash
sudo ./microjail
```
*Once spawned, you will be dropped into an isolated `/#` shell where you can verify the namespace limits, try to run a fork-bomb, or test the Seccomp firewall.*

## 🧠 Educational Purpose
This project is an educational proof-of-concept designed to map the exact syscalls and kernel mechanisms used by Docker and Kubernetes. It is not intended for production workloads.
