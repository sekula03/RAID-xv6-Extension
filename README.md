# RAID xv6 Extension

A robust RAID implementation built on the xv6 operating system, providing advanced storage reliability and performance features for RISC-V architecture. 

## ⭐ Features

### RAID Level Support
- **RAID 0:** Striping for enhanced performance through parallel data distribution
- **RAID 1:** Mirroring for complete data redundancy and fault tolerance
- **RAID 4:** Block-level striping with dedicated parity disk
- **RAID 5:** Distributed parity for balanced performance and redundancy
- **RAID 01:** Nested RAID combining striping and mirroring for optimal performance and reliability

### Concurrent Disk Access
- Thread-safe disk operations with proper synchronization
- Support for multiple simultaneous read/write requests
- Lock-based concurrency control for data integrity
- Minimized lock contention through fine-grained locking strategies

### Error Detection and Recovery
- Comprehensive error detection mechanisms
- Automatic data recovery using parity information (RAID 4, 5)
- Checksum validation for data integrity verification
- Seamless failover to redundant copies when available

### Disk Failure Simulation and Repair
- Simulated disk failure scenarios for testing resilience
- Hot-swap capability for failed disk replacement
- Automated data restoration from parity and redundant copies


## 🛠️ Getting Started

This project requires a RISC-V cross-compiler and the QEMU emulator. 

### 1. Installing Toolchain
- **Ubuntu:**  
```bash
# Option 1: Bare-metal toolchain (preferred)
sudo apt install gcc-riscv64-unknown-elf binutils-riscv64-unknown-elf qemu-system-misc

# Option 2: If the above is unavailable
sudo apt install gcc-riscv64-linux-gnu binutils-riscv64-linux-gnu qemu-system-misc
```
- **macOS:**  
```bash
brew tap riscv/riscv
brew install riscv-tools qemu
```
Verify installation with:  
```bash
riscv64-unknown-elf-gcc --version
# OR
riscv64-linux-gnu-gcc --version
```

### 2. Building and Running

```bash
# Clean the project (optional)
make clean

# Build and run in QEMU emulator
make qemu
```

The `make qemu` command will build the kernel with RAID support and launch it in the QEMU emulator. 

### 3. Running Your Programs

To run your own programs and experiment with RAID features:

1. Open `user/javni_test.c`
2. Modify the `main` to include your code
3. Rebuild with `make qemu`
4. When the xv6 shell prompt appears, type `javni_test` and press Enter

```bash
$ javni_test
# Your RAID tests will execute
```

### 4. Configuring the System Variables
In `makefile`, you are able to change the following variables:
- Number of disks. It can be between 1 and 7. Search for `DISKS`.
- Size of each disk in bytes. Search for `DISK_SIZE`.
- Number of cores. Search for `CPUS`.
- Size of available memory. Can be in bytes or with suffix (K, M, G). Search for `MEM`.

## 🧪 Testing Scenarios

The implementation includes comprehensive testing for:

- **Striping correctness:** Verify data distribution across disks
- **Mirror synchronization:** Ensure data consistency between mirrored disks
- **Parity integrity:** Validate parity calculations and storage
- **Concurrent operations:** Test race conditions and deadlock prevention
- **Single disk failure:** Test degraded mode operation
- **Multiple disk failures:** Test failure threshold limits (RAID-dependent)
- **Data consistency:** Verify no data corruption during failures

## ⚙️ Technical Details

- **Target Architecture:** RISC-V
- **Language:** C
- **Base System:** xv6 operating system
- **Emulation Platform:** QEMU
- **Supported RAID Levels:** 0, 1, 4, 5, 0+1
- **Concurrency Model:** Lock-based synchronization with spinlocks
- **Recovery Model:** Automatic parity-based reconstruction
