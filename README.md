# CS 4348 - Operating Systems
## Project #1:  Exploring Multiple Processes and IPC

### Overview:
- The project will simulate a simple computer system consisting of a CPU and Memory.
- The CPU and Memory will be simulated by separate processes that communicate.
- Memory will contain one program that the CPU will execute and then the simulation will end.

### How to run:
NOTE: This program can only be run on Linux machines, because it relies on the use of pipes and forks, which as far as I know only Linux supports. GCC must also be installed.
1. Open a terminal session, go to the directory where proj_1.c is located, and type:
    ```bash
    gcc proj_1.c -std=gnu99
    ```
2. To execute, run:
    ```bash
    ./a.out <filename> <timer>
    ```
    where timer is an value for the number of instructions to run before an interrupt occurs.
    