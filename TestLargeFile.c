#
# Copyright (c) 2007-Present The PureDarwin Project.
# All rights reserved.
#
# @LICENSE_HEADER_START@
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
# 1. Redistributions of source code must retain the above copyright
#    notice, this list of conditions and the following disclaimer.
# 2. Redistributions in binary form must reproduce the above copyright
#    notice, this list of conditions and the following disclaimer in the
#    documentation and/or other materials provided with the distribution.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS
# IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
# THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
# PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR
# CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
# EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
# PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
# PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
# LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
# NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
# SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
#
# @LICENSE_HEADER_END@
#
#
# @FILE
#  TestLargeFile.c
#  This file is the c source for the Large File Test operations
#
# @AUTHOR
# Created by Cliff Sekel  for The PureDarwin Project github.com/PureDarwin
#

#include "HFSPlusFileOps.h"
#include "MockBlockIo.h"

EFI_STATUS TestWriteLargeFile(MockBlockIoProtocol *BlockIo) {
    UINTN BlockSize = BlockIo->BlockSize;
    UINT64 DataSize = BlockSize * 15;  // File spanning 15 blocks

    UINT8 *TestData = AllocateZeroPool(DataSize);
    for (UINT64 i = 0; i < DataSize; i++) {
        TestData[i] = (UINT8)(i % 256);  // Fill pattern
    }

    HFSPlusForkData FileForkData = {0};
    HFSPlusForkData ExtentOverflowFile = {0};

    EFI_STATUS Status = WriteFileWithFragmentation(
        NULL, (EFI_BLOCK_IO_PROTOCOL *)BlockIo, &FileForkData, BlockIo->LastBlock,
        TestData, DataSize, &FileForkData, &ExtentOverflowFile
    );

    FreePool(TestData);
    return Status;
}

EFI_STATUS TestReadLargeFile(MockBlockIoProtocol *BlockIo, HFSPlusForkData *FileForkData, HFSPlusForkData *ExtentOverflowFile) {
    UINT64 FileSize = 15 * BlockIo->BlockSize;
    VOID *ReadData = NULL;

    EFI_STATUS Status = ReadFileWithFragmentation(
        NULL, (EFI_BLOCK_IO_PROTOCOL *)BlockIo, FileForkData, BlockIo->LastBlock,
        &ReadData, ExtentOverflowFile
    );

    UINT8 *TestData = AllocateZeroPool(FileSize);
    for (UINT64 i = 0; i < FileSize; i++) {
        TestData[i] = (UINT8)(i % 256);
    }

    if (CompareMem(ReadData, TestData, FileSize) == 0) {
        DEBUG((DEBUG_INFO, "File read matches written data.\n"));
        Status = EFI_SUCCESS;
    } else {
        DEBUG((DEBUG_ERROR, "File read does NOT match written data.\n"));
        Status = EFI_ABORTED;
    }

    FreePool(TestData);
    FreePool(ReadData);
    return Status;
}

EFI_STATUS TestLoadBootEfi(MockBlockIoProtocol *BlockIo) {
    HFSPlusForkData CatalogFile = {0};       // Simulated catalog B-tree file
    HFSPlusForkData ExtentOverflowFile = {0};  // Simulated overflow file
    VOID *BootEfiData = NULL;

    EFI_STATUS Status = LoadBootEfi(
        (EFI_BLOCK_IO_PROTOCOL *)BlockIo,
        &CatalogFile,
        &ExtentOverflowFile,
        &BootEfiData
    );

    if (EFI_ERROR(Status)) {
        DEBUG((DEBUG_ERROR, "Failed to load boot.efi: %r\n", Status));
    } else {
        DEBUG((DEBUG_INFO, "boot.efi loaded successfully.\n"));
    }

    if (BootEfiData != NULL) {
        FreePool(BootEfiData);
    }

    return Status;
}

EFI_STATUS RunTests() {
    UINT64 TotalBlocks = 100;
    UINTN BlockSize = 512;
    MockBlockIoProtocol *MockBlockIo = InitializeMockDisk(TotalBlocks, BlockSize);

    DEBUG((DEBUG_INFO, "Testing large file write...\n"));
    EFI_STATUS Status = TestWriteLargeFile(MockBlockIo);
    if (EFI_ERROR(Status)) {
        DEBUG((DEBUG_ERROR, "Error writing large file: %r\n", Status));
        return Status;
    }

    HFSPlusForkData FileForkData = {0};
    HFSPlusForkData ExtentOverflowFile = {0};

    DEBUG((DEBUG_INFO, "Testing large file read...\n"));
    Status = TestReadLargeFile(MockBlockIo, &FileForkData, &ExtentOverflowFile);
    if (EFI_ERROR(Status)) {
        DEBUG((DEBUG_ERROR, "Error reading large file: %r\n", Status));
    }

    DEBUG((DEBUG_INFO, "Testing boot.efi load...\n"));
    Status = TestLoadBootEfi(MockBlockIo);

    FreePool(MockBlockIo->DiskData);
    FreePool(MockBlockIo);
    return Status;
}

EFI_STATUS
EFIAPI
UefiMain(
    IN EFI_HANDLE ImageHandle,
    IN EFI_SYSTEM_TABLE *SystemTable
) {
    DEBUG((DEBUG_INFO, "Running tests...\n"));
    EFI_STATUS Status = RunTests();

    if (EFI_ERROR(Status)) {
        DEBUG((DEBUG_ERROR, "Tests failed: %r\n", Status));
    } else {
        DEBUG((DEBUG_INFO, "All tests passed successfully.\n"));
    }

    return Status;
}