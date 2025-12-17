#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

#define NUM_PROCESSES 5
#define NUM_ITERATIONS 101
#define BLOCK_SIZE 1024

uchar readB[NUM_PROCESSES][BLOCK_SIZE];
uchar writeB[NUM_PROCESSES][BLOCK_SIZE];

void child_process(int id) {

    uchar* write_buffer = writeB[id];
    uchar* read_buffer = readB[id];

    for (int i = 0; i < NUM_ITERATIONS; i++) {
        int block_number = id * NUM_ITERATIONS + i; // Assign unique block numbers for each process

        // Create unique data for each process
        //memset(write_buffer, 'A' + id, BLOCK_SIZE);
        for (int j = 0; j < BLOCK_SIZE; j++) write_buffer[j] = 'A' + id;

        // Write to the RAID structure
        if (write_raid(block_number, write_buffer) < 0) {
            printf("Process %d: Failed to write to block %d\n", id, block_number);
            exit(1);
        }

        // Read back and verify
        if (read_raid(block_number, read_buffer) < 0) {
            printf("Process %d: Failed to read from block %d\n", id, block_number);
            exit(1);
        }

        // Verify the data integrity
        for (int j = 0; j < BLOCK_SIZE; j++) {
            if (read_buffer[j] != 'A' + id) {
                printf("Process %d: Data mismatch at block %d, iteration %d -> %d %d\n", id, block_number, i, read_buffer[j], 'A' + id);
                exit(1);
            }
        }
    }
    //printf("Process %d: Completed successfully\n", id);
    exit(0);
}

int main() {

  	int status, successful = 1;

    init_raid(RAID5);

    // Spawn multiple processes
    for (int i = 0; i < NUM_PROCESSES; i++) {
        int pid = fork();
        if (pid < 0) {
            printf("Fork failed\n");
            exit(1);
        } else if (pid == 0) {
            child_process(i);
        }
    }

    // Wait for all child processes to finish
    for (int i = 0; i < NUM_PROCESSES; i++) {
        wait(&status);
        if (status != 0) successful = 0;
    }

    if (successful) printf("Test passed.\n");
    else printf("Test failed\n");

    exit(0);
}











//void check_data(uint blocks, uchar *blk, uint block_size);
//
//int
//main(int argc, char *argv[])
//{
//  if (init_raid(RAID4) != 0) printf("init err\n");
//
//  uint disk_num, block_num, block_size;
//  if (info_raid(&block_num, &block_size, &disk_num) < 0) printf("info err\n");
//
//  uint blocks = (512 > block_num ? block_num : 512);
//
//  uchar* blk = malloc(block_size);
//  for (uint i = 0; i < blocks; i++) {
//    for (uint j = 0; j < block_size; j++) {
//      blk[j] = j + i;
//    }
//    if (write_raid(i, blk) < 0) printf("write err\n");
//  }
//
//  for (int i = 0; i < block_size; i++) blk[i] = 0;
//
//  check_data(blocks, blk, block_size);
//
//  disk_fail_raid(2);
//
//  for (int i = 0; i < blocks; i++) blk[i] = 0;
//
//  check_data(blocks, blk, block_size);
//
//  disk_repaired_raid(2);
//
//  for (int i = 0; i < blocks; i++) blk[i] = 0;
//
//  check_data(blocks, blk, block_size);
//
//  free(blk);
//
//
//  printf("OK\n");
//
//  exit(0);
//}
//
//void check_data(uint blocks, uchar *blk, uint block_size)
//{
//  for (uint i = 0; i < blocks; i++)
//  {
//    if (read_raid(i, blk) != 0) {
//      printf("read err\n");
//      return;
//    }
//    for (uint j = 0; j < block_size; j++)
//    {
//      if ((uchar)(j + i) != blk[j])
//      {
//        printf("expected=%d got=%d ", j + i, blk[j]);
//        printf("Data in the block %d faulty\n", i);
//        return;
//      }
//    }
//  }
//}
