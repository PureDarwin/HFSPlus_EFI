//
// Copyright (c) 2007-Present The PureDarwin Project.
// All rights reserved.
//
// @LICENSE_HEADER_START@
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions
// are met:
// 1. Redistributions of source code must retain the above copyright
//    notice, this list of conditions and the following disclaimer.
// 2. Redistributions in binary form must reproduce the above copyright
//    notice, this list of conditions and the following disclaimer in the
//    documentation and/or other materials provided with the distribution.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS
// IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
// THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
// PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR
// CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
// EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
// PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
// PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
// LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
// NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
// SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
//
// @LICENSE_HEADER_END@
//
//
// @FILE
//  HfsPlusFileOps.h
//  This file is the header for the HFS+ file operations
//
// @AUTHOR
// Created by Cliff Sekel  for The PureDarwin Project github.com/PureDarwin
//

#ifndef HFSPLUS_FILE_OPS_H
#define HFSPLUS_FILE_OPS_H

#include <Uefi.h>
#include <Library/UefiLib.h>
#include <Library/DebugLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Protocol/BlockIo.h>
#include <Protocol/SimpleFileSystem.h>
#include <Guid/Gpt.h>

#define HFSPLUS_VOL_JOURNALED  0x00800000  // HFS+ Journaled attribute flag
#define HFSPLUS_SIGNATURE 0x482B  // The HFS+ signature ('H+' in ASCII)
#define HFSPLUS_BOOT_FOLDER_ID  0x00000002  // Example folder ID for the boot directory

// Structures for HFS+ extents, catalog keys, volume header, and journal info
typedef struct HFSPlusExtentDescriptor {
    UINT32 startBlock;
    UINT32 blockCount;
} HFSPlusExtentDescriptor;

typedef struct HFSPlusForkData {
    UINT64 logicalSize;
    UINT32 clumpSize;
    UINT32 totalBlocks;
    HFSPlusExtentDescriptor extents[8];
} HFSPlusForkData;

typedef struct HFSPlusCatalogKey {
    UINT16 keyLength;
    UINT32 parentID;
    // Variable-length file/folder name follows
} HFSPlusCatalogKey;

typedef struct BTNodeDescriptor {
    UINT32 fLink;
    UINT32 bLink;
    UINT8 kind;
    UINT8 height;
    UINT16 numRecords;
    UINT16 reserved;
} BTNodeDescriptor;

typedef struct HFSPlusVolumeHeader {
    UINT16 signature;
    UINT32 attributes;
    UINT32 journalInfoBlock;
    HFSPlusForkData catalogFile;
    HFSPlusForkData allocationFile;
    // Additional fields omitted for brevity
} HFSPlusVolumeHeader;

typedef struct HFSPlusJournalInfoBlock {
    UINT64 offset;
    UINT64 size;
} HFSPlusJournalInfoBlock;

// Function declarations for file system and journal operations
EFI_STATUS WriteFileWithFragmentation(
    EFI_HANDLE ImageHandle,
    EFI_BLOCK_IO_PROTOCOL *BlockIo,
    HFSPlusForkData *ForkData,
    UINT32 TotalBlocks,
    VOID *Data,
    UINT64 DataSize,
    HFSPlusForkData *AllocationFile,
    HFSPlusForkData *ExtentOverflowFile
);

EFI_STATUS ReadFileWithFragmentation(
    EFI_HANDLE ImageHandle,
    EFI_BLOCK_IO_PROTOCOL *BlockIo,
    HFSPlusForkData *ForkData,
    UINT32 TotalBlocks,
    VOID **FileData,
    HFSPlusForkData *ExtentOverflowFile
);

EFI_STATUS DetectHfsPlusPartitions(
    EFI_BLOCK_IO_PROTOCOL **BlockIoProtocol,
    UINTN *HfsPartitionCount
);

EFI_STATUS MountHfsPlusVolume(
    EFI_BLOCK_IO_PROTOCOL *BlockIo,
    HFSPlusForkData *CatalogFile,
    HFSPlusForkData *AllocationFile,
    BOOLEAN *IsJournaled
);

EFI_STATUS FindAndLoadBootEfi(
    EFI_BLOCK_IO_PROTOCOL *BlockIo
);

EFI_STATUS LoadBootEfi(
    EFI_BLOCK_IO_PROTOCOL *BlockIo,
    HFSPlusForkData *CatalogFile,
    HFSPlusForkData *AllocationFile,
    VOID **BootEfiData
);

EFI_STATUS TraverseCatalogBTree(
    EFI_BLOCK_IO_PROTOCOL *BlockIo,
    HFSPlusForkData *CatalogFile,
    UINT32 ParentFolderID,
    CHAR16 *FileName,
    VOID **CatalogRecord
);

EFI_STATUS TraverseBTreeNodeRecursively(
    EFI_BLOCK_IO_PROTOCOL *BlockIo,
    HFSPlusForkData *CatalogFile,
    UINT8 *NodeBuffer,
    UINT32 ParentFolderID,
    CHAR16 *FileName,
    VOID **CatalogRecord
);

EFI_STATUS SearchIndexNodeAndRecurse(
    EFI_BLOCK_IO_PROTOCOL *BlockIo,
    HFSPlusForkData *CatalogFile,
    UINT8 *NodeBuffer,
    UINT32 ParentFolderID,
    CHAR16 *FileName,
    VOID **CatalogRecord
);

EFI_STATUS SearchLeafNodeForFile(
    EFI_BLOCK_IO_PROTOCOL *BlockIo,
    UINT8 *NodeBuffer,
    UINT32 ParentFolderID,
    CHAR16 *FileName,
    VOID **CatalogRecord
);

BTNodeDescriptor *GetChildNode(
    EFI_BLOCK_IO_PROTOCOL *BlockIo,
    HFSPlusForkData *CatalogFile,
    HFSPlusCatalogKey *CatalogKey
);

#endif  // HFSPLUS_FILE_OPS_H
