# HFS+ UEFI File System Operations

## Overview

This project implements  HFS+ file system operations within a UEFI application built on the EDKII Enviorment. 
The application includes the ability to:

- **Read and write files** using HFS+ file structures (including support for fragmented files) and journaled file systems.
- **Traverse the HFS+ catalog B-tree** to locate files and directories.
- **Find and load `boot.efi`** from a system drive formatted in HFS+, typically located at `/System/Library/CoreServices/boot.efi`.

## Features

1. **HFS+ File System Support**:
   - Reading and writing of files in HFS+ volumes.
   - Handling of fragmented files using the HFS+ extent overflow file.
   
2. **Catalog B-tree Traversal**:
   - Parsing and traversal of the HFS+ catalog B-tree to locate files and directories.
   - Support for reading metadata to identify files such as `boot.efi`.

3. **Boot.efi Loading**:
   - Locates `boot.efi` in the `/System/Library/CoreServices` directory of an HFS+ system drive.
   - Loads the contents of `boot.efi` for potential execution by the UEFI firmware.

## Project Structure

- **HFSPlusFileOps.h/c**: Implements the core HFS+ file system logic, including file reading, writing, and catalog B-tree traversal.
- **MockBlockIo.h/c**: Provides a mock block I/O protocol for simulating disk read and write operations, useful for testing.
- **TestLargeFile.c**: Contains test cases to validate file read and write operations, as well as the process for locating `boot.efi`.
- **HfsPlusFileOpsTest.inf**: The build configuration file for EDK II, describing the application's source files, dependencies, and build settings.

## Building the Application

To build this UEFI application using the EDK II environment.


## This code is experimental and should be used with caution. 


### Copyright (c) 2007-Present The PureDarwin Project.
### All rights reserved.