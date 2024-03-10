# Reportman Project

This project consists of four main programs: `reportmand`, `reportmanc`, `reportman_mon`, and `reportman_fm`. 

## Programs

### reportmand

`reportmand` is the main daemon process of the project. It is responsible for managing the other processes and ensuring they are running correctly. It listens to client and child processes, handles commands, and maintains the health of the system.

### reportmanc

`reportmanc` is the client process that communicates with the `reportmand` daemon. It sends commands and receives responses from the daemon.

### reportman_mon

`reportman_mon` is a monitoring process that is managed by the `reportmand` daemon. It monitors the file system and can report to the daemon.

### reportman_fm

`reportman_fm` is a file management process that is also managed by the `reportmand` daemon. It handles file transfer and backup operations, and can be controlled by the daemon

## Requirements

1. GCC
2. GNU Make
3. lsof (used for singleton management)

## Build Instructions

1. Clone the repository to your local machine.
2. Navigate to the project directory.
3. Compile the project using `make`.

## Installation Instructions - WIP unstable and not working

1. Clone the repository to your local machine.
2. Navigate to the project directory.
3. Compile the project using `make install` - needs root priveleges

