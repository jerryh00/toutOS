# Introduction

toutOS is an operating system running on arm64. Currently it has minimal
memory management, process management, synchronization primitives, system
calls and a shell.

# Software requirements:

1. Linaro aarch64-linux-gnu toolchain.
2. QEMU 2.9 or later(for multi-thread support).

# Compiling the project:

make

# Running the project:

make qemu

# Quitting:

ctrl-a x
