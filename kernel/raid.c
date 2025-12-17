#include "types.h"
#include "riscv.h"
#include "defs.h"
#include "param.h"
#include "spinlock.h"
#include "proc.h"
#include "fs.h"
#include "raid.h"

/*
ERROR CODES:
-1:  no raid initialized
-2:  raid already initialized
-3:  number of disks doesn't match the raid type
-4:  block number is invalid
-5:  disk number is invalid
-6:  block cannot be read because the disk is broken
-7:  block cannot be written in because the disk is broken
-8:  copyout failed
-9:  copyin failed
-10: disk is already broken
-11: disk is not broken
-12: heap is out of memory
*/

int raid_cache = 0;
int system_raid;
int broken[DISKS];
int max_block[DISKS];
int read_write = 0;
int special = 0;
struct spinlock raid_lock = {0, "RAID", 0};
struct spinlock disk_locks[DISKS];
int disk_used[DISKS];
struct spinlock W45;
int state_W45 = 0;

int init_raid(enum RAID_TYPE raid) {
  uchar* buffer = (uchar*)kalloc();
  if (!buffer) return -12;
  start_mutex(SPEC, buffer);
  int ret = __init_raid(raid, buffer);
  kfree(buffer);
  end_mutex(SPEC);
  return ret;
}

int read_raid(int blkn, uchar* data) {
  uchar* buffer = (uchar*)kalloc();
  if (!buffer) return -12;
  start_mutex(RW, buffer);
  int ret = __read_raid(blkn, buffer);
  if (ret == 0) {
    pagetable_t pt = myproc()->pagetable;
    if (copyout(pt, (uint64)data, (char*)buffer, BSIZE) < 0) ret = -8;
  }
  kfree(buffer);
  end_mutex(RW);
  return ret;
}

int write_raid(int blkn, uchar* data) {
  uchar* buffer = (uchar*)kalloc();
  if (!buffer) return -12;
  start_mutex(RW, buffer);
  pagetable_t pt = myproc()->pagetable;
  int ret = -9;
  if (copyin(pt, (char*)buffer, (uint64)data, BSIZE) == 0)
    ret = __write_raid(blkn, buffer);
  kfree(buffer);
  end_mutex(RW);
  return ret;
}

int disk_fail_raid(int diskn) {
  uchar* buffer = (uchar*)kalloc();
  if (!buffer) return -12;
  start_mutex(SPEC, buffer);
  int ret = __disk_fail_raid(diskn, buffer);
  kfree(buffer);
  end_mutex(SPEC);
  return ret;
}

int disk_repaired_raid(int diskn) {
  uchar* buffer = (uchar*)kalloc();
  if (!buffer) return -12;
  start_mutex(SPEC, buffer);
  int ret = __disk_repaired_raid(diskn, buffer);
  kfree(buffer);
  end_mutex(SPEC);
  return ret;
}

int info_raid(uint *blkn, uint *blks, uint *diskn) {
  uchar* buffer = (uchar*)kalloc();
  if (!buffer) return -12;
  start_mutex(RW, buffer);
  kfree(buffer);
  int ret = __info_raid(blkn, blks, diskn);
  end_mutex(RW);
  return ret;
}

int destroy_raid(void) {
  uchar* buffer = (uchar*)kalloc();
  if (!buffer) return -12;
  start_mutex(SPEC, buffer);
  int ret = __destroy_raid(buffer);
  kfree(buffer);
  end_mutex(SPEC);
  return ret;
}

int __init_raid(enum RAID_TYPE raid, uchar* buffer) {
  if (system_raid != NO_RAID) return -2;
  if ((raid == RAID1 || raid == RAID0_1) && DISKS % 2 != 0) return -3;
  if (raid == RAID0_1 && DISKS < 4) return -3;
  if ((raid == RAID4 || raid == RAID5) && DISKS < 3) return -3;
  system_raid = raid;
  struct raid_header* header = (struct raid_header*) buffer;
  header->state = RAID_ACTIVE;
  header->type = raid;
  header->max_accessed = 0;
  for (int i = 1; i <= DISKS; i++) {
    header->disk_broken = broken[i-1];
    max_block[i-1] = 0;
    write_block(i, 0, buffer);
  }
  return 0;
}

int __read_raid(int blkn, uchar* buffer) {
  if (system_raid == NO_RAID) return -1;
  if (num_of_blocks() <= blkn || blkn < 0) return -4;
  int disk = disk_number(blkn), block = block_number(blkn);
  if (broken[disk-1] == 0) {
    read_mutex(disk, block, buffer);
    return 0;
  }
  if (system_raid == RAID0) return -6;
  int backup = backup_disk_number(disk, blkn);
  if (broken[backup-1] == 1) return -6;
  read_mutex(backup, block, buffer);
  if (system_raid == RAID1 || system_raid == RAID0_1) return 0;
  for (int i = 1; i <= DISKS; i++) {
    if (i != disk && broken[i-1] == 1) return -6;
  }
  uchar* temp = buffer + 1024;
  for (int i = 1; i <= DISKS; i++) {
    if (i == disk || i == backup) continue;
    read_mutex(i, block, temp);
    for (int j = 0; j < BSIZE; j++) buffer[j] ^= temp[j];
  }
  return 0;
}

int __write_raid(int blkn, uchar* buffer) {
  if (system_raid == NO_RAID) return -1;
  if (num_of_blocks() <= blkn || blkn < 0) return -4;
  int successful = -7;
  int disk = disk_number(blkn), block = block_number(blkn);
  if (broken[disk-1] == 0) {
    write_mutex(disk, block, buffer);
    successful = 0;
  }
  if (system_raid == RAID0) return successful;
  int backup = backup_disk_number(disk, blkn);
  if (broken[backup-1] == 1) return successful;
  if (system_raid == RAID1 || system_raid == RAID0_1) {
    write_mutex(backup, block, buffer);
    return 0;
  }
  for (int i = 1; i <= DISKS; i++) {
    if (i != disk && broken[i-1] == 1) return successful;
  }
  uchar* temp = buffer + 1024;
  acquire(&W45);
  while (state_W45 != 0) sleep(&state_W45, &W45);
  state_W45 = LOCKED;
  release(&W45);
  for (int i = 1; i <= DISKS; i++) {
    if (i == disk || i == backup) continue;
    read_block(i, block, temp);
    for (int j = 0; j < BSIZE; j++) buffer[j] ^= temp[j];
  }
  write_block(backup, block, buffer);
  acquire(&W45);
  state_W45 = 0;
  wakeup(&state_W45);
  release(&W45);
  return 0;
}

int __disk_fail_raid(int diskn, uchar* buffer) {
  if (diskn < 1 || diskn > DISKS) return -5;
  if (broken[diskn-1] == 1) return -10;
  //fill_blank(diskn, max_block[diskn-1], buffer + 1024);
  broken[diskn-1] = 1;
  max_block[diskn-1] = 0;
  struct raid_header* header = (struct raid_header*) buffer;
  header->disk_broken = DISK_BROKEN;
  header->state = system_raid == NO_RAID ? 0 : RAID_ACTIVE;
  header->type = system_raid;
  header->max_accessed = max_block[diskn-1];
  write_block(diskn, 0, buffer);
  return 0;
}

int __disk_repaired_raid(int diskn, uchar* buffer) {
  if (diskn < 1 || diskn > DISKS) return -5;
  if (broken[diskn-1] == 0) return -11;
  struct raid_header* header = (struct raid_header*) buffer;
  header->disk_broken = 0;
  header->state = system_raid == NO_RAID ? 0 : RAID_ACTIVE;
  header->type = system_raid;
  header->max_accessed = 0;
  if (system_raid != NO_RAID)
    for (int i = 1; i <= DISKS; i++)
      if (i != diskn && broken[i-1] == 0 && max_block[i-1] > header->max_accessed) header->max_accessed = max_block[i-1];
  max_block[diskn-1] = header->max_accessed;
  broken[diskn-1] = 0;
  write_block(diskn, 0, buffer);
  restore_data(diskn, buffer + 1024);
  return 0;
}

int __info_raid(uint *blkn, uint *blks, uint *diskn) {
  if (system_raid == NO_RAID) return -1;
  int disks = num_of_disks(), block_size = BSIZE;
  int blocks = num_of_blocks();
  pagetable_t pt = myproc()->pagetable;
  if (copyout(pt, (uint64)blkn, (char*)&blocks, sizeof(int)) < 0) return -8;
  if (copyout(pt, (uint64)blks, (char*)&block_size, sizeof(int)) < 0) return -8;
  if (copyout(pt, (uint64)diskn, (char*)&disks, sizeof(int)) < 0) return -8;
  return 0;
}

int __destroy_raid(uchar* buffer) {
  if (system_raid == NO_RAID) return -1;
  system_raid = NO_RAID;
  struct raid_header* header = (struct raid_header*) buffer;
  header->state = 0;
  header->max_accessed = 0;
  for (int i = 1; i <= DISKS; i++) {
    header->disk_broken = broken[i-1];
    max_block[i-1] = 0;
    write_block(i, 0, buffer);
  }
  return 0;
}

void load_cache(uchar* buffer) {
  system_raid = NO_RAID;
  struct raid_header* header = (struct raid_header*) buffer;
  for (int i = 1; i <= DISKS; i++) {
    read_block(i, 0, buffer);
    max_block[i-1] = 0;
    if (header->state == RAID_ACTIVE) {
      system_raid = header->type;
      max_block[i-1]  = header->max_accessed;
    }
    broken[i-1] = header->disk_broken == DISK_BROKEN ? 1 : 0;
    initlock(&disk_locks[i-1], "disklock");
    disk_used[i-1] = 0;
  }
  initlock(&W45, "W45");
}

void restore_data(int diskn, uchar* buffer) {
  if (system_raid == NO_RAID || system_raid == RAID0) return;
  int backup = backup_disk_number(diskn, diskn-1);
  if (broken[backup-1] == 1) return;
  if (system_raid == RAID4 || system_raid == RAID5) {
    for (int i = 1; i <= DISKS; i++)
      if (broken[i-1] == 1) return;
    if (system_raid == RAID4 && diskn == backup) backup--;
  }
  uchar* temp = buffer + 1024;
  for (int i = 1; i <= max_block[diskn-1]; i++) {
    read_block(backup, i, buffer);
    if (system_raid == RAID4 || system_raid == RAID5) {
      for (int j = 1; j <= DISKS; j++) {
        if (j == diskn || j == backup) continue;
        read_block(j, i, temp);
        for (int k = 0; k < BSIZE; k++) buffer[k] ^= temp[k];
      }
    }
    write_block(diskn, i, buffer);
  }
}

int num_of_disks(void) {
  if (system_raid == RAID4) return DISKS - 1;
  if (system_raid == RAID0 || system_raid == RAID5) return DISKS;
  return DISKS / 2;
}

int num_of_blocks(void) {
  int disks = num_of_disks();
  if (system_raid == RAID5) disks--;
  return disks * (DISK_SIZE / BSIZE - 1);
}

int disk_number(int blkn) {
  if (system_raid == RAID1) return blkn / (BSIZE-1) + 1;
  return blkn % num_of_disks() + 1;
}

int block_number(int blkn) {
  if (system_raid == RAID1) return blkn % (BSIZE-1) + 1;
  if (system_raid == RAID0 || system_raid == RAID0_1 || system_raid == RAID4) return blkn / num_of_disks() + 1;
  int original = blkn / DISKS;
  int backups = (blkn / (DISKS - 1)) / DISKS;
  int block = original + backups;
  if (block % DISKS >= DISKS - disk_number(blkn)) block++;
  return block + 1;
}

int backup_disk_number(int diskn, int blkn) {
  if (system_raid == RAID0) return -1;
  if (system_raid == RAID1 || system_raid == RAID0_1) return (diskn > DISKS / 2) ? diskn - DISKS / 2 : diskn + DISKS / 2;
  if (system_raid == RAID4) return DISKS;
  return (DISKS - 1) - (blkn / (DISKS - 1)) % DISKS + 1;
}

void new_max_block(int diskn, int blockn, uchar* buffer) {
  if (max_block[diskn-1] >= blockn) return;
  max_block[diskn-1] = blockn;
  struct raid_header* header = (struct raid_header*) buffer;
  header->state = system_raid == NO_RAID ? 0 : RAID_ACTIVE;
  header->type = system_raid;
  header->disk_broken = 0;
  header->max_accessed = max_block[diskn-1];
  write_block(diskn, 0, buffer);
}

void fill_blank(int disk, int reach, uchar* buffer) {
  if (reach <= 0) return;
  for (int i = 0; i < BSIZE; i++) buffer[i] = 0;
  for (int i = 1; i <= reach; i++)
    write_block(disk, i, buffer);
}

void start_mutex(int f, uchar* buffer) {
  acquire(&raid_lock);
  while (special != 0 || (f == SPEC && read_write != 0)) sleep(&raid_lock, &raid_lock);
  if (f == RW) read_write++;
  else special = 1;
  if (!raid_cache) {
  	raid_cache = LOCKED;
    release(&raid_lock);
    load_cache(buffer);
    acquire(&raid_lock);
    raid_cache = 1;
    wakeup(&raid_cache);
  }
  while (raid_cache == LOCKED) sleep(&raid_cache, &raid_lock);
  release(&raid_lock);
}

void end_mutex(int f) {
  acquire(&raid_lock);
  if (f == RW) read_write--;
  else special = 0;
  if (read_write == 0 && special == 0) wakeup(&raid_lock);
  release(&raid_lock);
}

void read_mutex(int disk, int block, uchar* buffer) {
  if (system_raid == RAID4 || system_raid == RAID5) {
    acquire(&W45);
    while (state_W45 == LOCKED) sleep(&state_W45, &W45);
    state_W45++;
    release(&W45);
  }
  acquire(&disk_locks[disk-1]);
  while (disk_used[disk-1] == 1) sleep(&disk_used[disk-1], &disk_locks[disk-1]);
  disk_used[disk-1] = 1;
  release(&disk_locks[disk-1]);
  read_block(disk, block, buffer);
  acquire(&disk_locks[disk-1]);
  disk_used[disk-1] = 0;
  wakeup(&disk_used[disk-1]);
  release(&disk_locks[disk-1]);
  if (system_raid == RAID4 || system_raid == RAID5) {
    acquire(&W45);
    state_W45--;
    if (state_W45 == 0) wakeup(&state_W45);
    release(&W45);
  }
}

void write_mutex(int disk, int block, uchar* buffer) {
  if (system_raid == RAID4 || system_raid == RAID5) {
    acquire(&W45);
    while (state_W45 == LOCKED) sleep(&state_W45, &W45);
    state_W45++;
    release(&W45);
  }
  acquire(&disk_locks[disk-1]);
  while (disk_used[disk-1] == 1) sleep(&disk_used[disk-1], &disk_locks[disk-1]);
  disk_used[disk-1] = 1;
  release(&disk_locks[disk-1]);
  write_block(disk, block, buffer);
  new_max_block(disk, block, buffer + 1024);
  acquire(&disk_locks[disk-1]);
  disk_used[disk-1] = 0;
  wakeup(&disk_used[disk-1]);
  release(&disk_locks[disk-1]);
  if (system_raid == RAID4 || system_raid == RAID5) {
    acquire(&W45);
    state_W45--;
    if (state_W45 == 0) wakeup(&state_W45);
    release(&W45);
  }
}