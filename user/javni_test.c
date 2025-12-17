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

void test() {
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

int main() {
    test()
}
