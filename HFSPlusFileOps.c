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
//  This file is the c source for the HFS+ file operations
//
// @AUTHOR
// Created by Cliff Sekel  for The PureDarwin Project github.com/PureDarwin
//

#include "HFSPlusFileOps.h"

// Write file with fragmentation handling
EFI_STATUS WriteFileWithFragmentation(
    EFI_HANDLE ImageHandle,
    EFI_BLOCK_IO_PROTOCOL *BlockIo,
    HFSPlusForkData *ForkData,
    UINT32 TotalBlocks,
    VOID *Data,
    UINT64 DataSize,
    HFSPlusForkData *AllocationFile,
    HFSPlusForkData *ExtentOverflowFile
) {
    UINTN BlockSize = BlockIo->Media->BlockSize;
    UINT32 RequiredBlocks = (DataSize + BlockSize - 1) / BlockSize;

    UINT32 *FreeBlocks = AllocateZeroPool(RequiredBlocks * sizeof(UINT32));
    if (FreeBlocks == NULL) {
        return EFI_OUT_OF_RESOURCES;
    }

    EFI_STATUS Status = FindFreeBlocks(BlockIo, AllocationFile, TotalBlocks, RequiredBlocks, FreeBlocks);
    if (EFI_ERROR(Status)) {
        FreePool(FreeBlocks);
        return Status;
    }

    UINT8 *DataPtr = (UINT8 *)Data;
    UINT64 TotalBytesWritten = 0;
    UINT32 ExtentIndex = 0;

    // Write the first 8 extents directly into ForkData
    for (ExtentIndex = 0; ExtentIndex < 8 && TotalBytesWritten < DataSize; ExtentIndex++) {
        UINT64 BytesToWrite = MIN(BlockSize, DataSize - TotalBytesWritten);

        Status = BlockIo->WriteBlocks(
            BlockIo,
            BlockIo->Media->MediaId,
            FreeBlocks[ExtentIndex],
            BlockSize,
            DataPtr
        );

        if (EFI_ERROR(Status)) {
            FreePool(FreeBlocks);
            return Status;
        }

        ForkData->extents[ExtentIndex].startBlock = FreeBlocks[ExtentIndex];
        ForkData->extents[ExtentIndex].blockCount = 1;

        DataPtr += BytesToWrite;
        TotalBytesWritten += BytesToWrite;
    }

    // Write the remaining extents to the extent overflow file with fragmentation handling
    if (TotalBytesWritten < DataSize) {
        Status = WriteFragmentedExtents(
            BlockIo,
            ExtentOverflowFile,
            DataPtr,
            FreeBlocks + ExtentIndex,
            RequiredBlocks - ExtentIndex,
            &TotalBytesWritten
        );
    }

    FreePool(FreeBlocks);
    return Status;
}

// Read file with fragmentation handling
EFI_STATUS ReadFileWithFragmentation(
    EFI_HANDLE ImageHandle,
    EFI_BLOCK_IO_PROTOCOL *BlockIo,
    HFSPlusForkData *ForkData,
    UINT32 TotalBlocks,
    VOID **FileData,
    HFSPlusForkData *ExtentOverflowFile
) {
    UINTN BlockSize = BlockIo->Media->BlockSize;
    UINT64 FileSize = ForkData->logicalSize;
    UINT64 TotalBytesRead = 0;

    *FileData = AllocateZeroPool(FileSize);
    if (*FileData == NULL) {
        return EFI_OUT_OF_RESOURCES;
    }

    UINT8 *DataPtr = *FileData;

    // Read the first 8 extents from ForkData
    for (UINT32 i = 0; i < 8 && TotalBytesRead < FileSize; i++) {
        HFSPlusExtentDescriptor *Extent = &ForkData->extents[i];

        if (Extent->blockCount == 0) {
            break;
        }

        UINT64 ExtentSize = Extent->blockCount * BlockIo->Media->BlockSize;
        if (ExtentSize + TotalBytesRead > FileSize) {
            ExtentSize = FileSize - TotalBytesRead;
        }

        UINT8 *ExtentBuffer = AllocateZeroPool(ExtentSize);
        if (ExtentBuffer == NULL) {
            FreePool(*FileData);
            return EFI_OUT_OF_RESOURCES;
        }

        EFI_STATUS Status = BlockIo->ReadBlocks(
            BlockIo,
            BlockIo->Media->MediaId,
            Extent->startBlock,
            Extent->blockCount * BlockIo->Media->BlockSize,
            ExtentBuffer
        );

        if (EFI_ERROR(Status)) {
            FreePool(*FileData);
            FreePool(ExtentBuffer);
            return Status;
        }

        CopyMem(DataPtr + TotalBytesRead, ExtentBuffer, ExtentSize);
        TotalBytesRead += ExtentSize;
        FreePool(ExtentBuffer);
    }

    // Read additional extents from the extent overflow file with fragmentation handling
    if (TotalBytesRead < FileSize) {
        EFI_STATUS Status = ReadFragmentedExtents(
            ImageHandle,
            BlockIo,
            ExtentOverflowFile,
            DataPtr,
            &TotalBytesRead,
            FileSize
        );

        if (EFI_ERROR(Status)) {
            FreePool(*FileData);
            return Status;
        }
    }

    return EFI_SUCCESS;
}

// Detect HFS+ partitions on all block devices
EFI_STATUS DetectHfsPlusPartitions(EFI_BLOCK_IO_PROTOCOL **BlockIoProtocol, UINTN *HfsPartitionCount) {
    EFI_STATUS Status;
    EFI_HANDLE *HandleBuffer;
    UINTN HandleCount;

    // Locate all Block I/O devices
    Status = gBS->LocateHandleBuffer(ByProtocol, &gEfiBlockIoProtocolGuid, NULL, &HandleCount, &HandleBuffer);
    if (EFI_ERROR(Status)) {
        return Status;
    }

    *HfsPartitionCount = 0;
    for (UINTN Index = 0; Index < HandleCount; Index++) {
        EFI_BLOCK_IO_PROTOCOL *BlockIo;
        Status = gBS->HandleProtocol(HandleBuffer[Index], &gEfiBlockIoProtocolGuid, (VOID **)&BlockIo);
        if (EFI_ERROR(Status) || !BlockIo->Media->LogicalPartition) {
            continue;  // Skip non-partition devices
        }

        // Read the first block (containing the volume header)
        VOID *Buffer = AllocateZeroPool(BlockIo->Media->BlockSize);
        if (Buffer == NULL) {
            continue;
        }

        Status = BlockIo->ReadBlocks(BlockIo, BlockIo->Media->MediaId, 0, BlockIo->Media->BlockSize, Buffer);
        if (EFI_ERROR(Status)) {
            FreePool(Buffer);
            continue;
        }

        // Check for HFS+ signature
        UINT16 *Signature = (UINT16 *)((UINT8 *)Buffer + 1024);  // Signature is at offset 1024 bytes
        if (*Signature == HFSPLUS_SIGNATURE) {
            // Found an HFS+ partition
            DEBUG((DEBUG_INFO, "HFS+ partition found on device %u\n", Index));
            BlockIoProtocol[*HfsPartitionCount] = BlockIo;  // Store the Block I/O protocol
            (*HfsPartitionCount)++;
        }

        FreePool(Buffer);
    }

    FreePool(HandleBuffer);
    return EFI_SUCCESS;
}

// Detect and handle HFS+ journaled volumes
EFI_STATUS MountHfsPlusVolume(EFI_BLOCK_IO_PROTOCOL *BlockIo, HFSPlusForkData *CatalogFile, HFSPlusForkData *AllocationFile, BOOLEAN *IsJournaled) {
    VOID *Buffer = AllocateZeroPool(BlockIo->Media->BlockSize);
    if (Buffer == NULL) {
        return EFI_OUT_OF_RESOURCES;
    }

    // Read the HFS+ volume header (located at block 2)
    EFI_STATUS Status = BlockIo->ReadBlocks(BlockIo, BlockIo->Media->MediaId, 2, BlockIo->Media->BlockSize, Buffer);
    if (EFI_ERROR(Status)) {
        FreePool(Buffer);
        return Status;
    }

    // Parse the volume header
    HFSPlusVolumeHeader *VolumeHeader = (HFSPlusVolumeHeader *)((UINT8 *)Buffer + 1024);  // Volume header starts at offset 1024

    // Check the HFS+ signature
    if (VolumeHeader->signature != HFSPLUS_SIGNATURE) {
        FreePool(Buffer);
        return EFI_VOLUME_CORRUPTED;
    }

    // Check if the volume is journaled
    if (VolumeHeader->attributes & HFSPLUS_VOL_JOURNALED) {
        DEBUG((DEBUG_INFO, "HFS+ journaled volume detected.\n"));
        *IsJournaled = TRUE;
    } else {
        *IsJournaled = FALSE;
    }

    // Store catalog and allocation file information
    *CatalogFile = VolumeHeader->catalogFile;
    *AllocationFile = VolumeHeader->allocationFile;

    FreePool(Buffer);
    return EFI_SUCCESS;
}

// Load boot.efi from the HFS+ partition
EFI_STATUS LoadBootEfi(
    EFI_BLOCK_IO_PROTOCOL *BlockIo,
    HFSPlusForkData *CatalogFile,
    HFSPlusForkData *AllocationFile,
    VOID **BootEfiData
) {
    // Use the HFS+ traversal functions to locate boot.efi and read its contents
    VOID *BootEfiRecord = NULL;
    EFI_STATUS Status = TraverseCatalogBTree(BlockIo, CatalogFile, HFSPLUS_BOOT_FOLDER_ID, L"boot.efi", &BootEfiRecord);
    if (EFI_ERROR(Status)) {
        DEBUG((DEBUG_ERROR, "Failed to locate boot.efi: %r\n", Status));
        return Status;
    }

    // Extract file information from the catalog record (e.g., size, fork data)
    HFSPlusCatalogFile *BootEfiCatalogFile = (HFSPlusCatalogFile *)BootEfiRecord;

    // Allocate memory for the file contents
    *BootEfiData = AllocateZeroPool(BootEfiCatalogFile->dataFork.logicalSize);
    if (*BootEfiData == NULL) {
        return EFI_OUT_OF_RESOURCES;
    }

    // Read the contents of boot.efi into memory
    Status = ReadFileWithFragmentation(BlockIo, &BootEfiCatalogFile->dataFork, AllocationFile->totalBlocks, BootEfiData, NULL);
    if (EFI_ERROR(Status)) {
        FreePool(*BootEfiData);
        *BootEfiData = NULL;
        DEBUG((DEBUG_ERROR, "Failed to read boot.efi: %r\n", Status));
    }

    return Status;
}

// Find and load boot.efi from HFS+ partition
EFI_STATUS FindAndLoadBootEfi(EFI_BLOCK_IO_PROTOCOL *BlockIo) {
    HFSPlusForkData CatalogFile = {0};
    HFSPlusForkData AllocationFile = {0};

    // Mount the HFS+ volume
    EFI_STATUS Status = MountHfsPlusVolume(BlockIo, &CatalogFile, &AllocationFile, NULL);
    if (EFI_ERROR(Status)) {
        DEBUG((DEBUG_ERROR, "Failed to mount HFS+ volume: %r\n", Status));
        return Status;
    }

    // Load boot.efi
    VOID *BootEfiData = NULL;
    Status = LoadBootEfi(BlockIo, &CatalogFile, &AllocationFile, &BootEfiData);
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

// Traverse the HFS+ catalog B-tree to find boot.efi
EFI_STATUS TraverseCatalogBTree(
    EFI_BLOCK_IO_PROTOCOL *BlockIo,
    HFSPlusForkData *CatalogFile,
    UINT32 ParentFolderID,
    CHAR16 *FileName,
    VOID **CatalogRecord
) {
    UINTN BlockSize = BlockIo->Media->BlockSize;
    UINT8 *Buffer = AllocateZeroPool(BlockSize);
    if (Buffer == NULL) {
        return EFI_OUT_OF_RESOURCES;
    }

    EFI_STATUS Status;

    // Read the root node (start traversal from the root node)
    Status = BlockIo->ReadBlocks(
        BlockIo,
        BlockIo->Media->MediaId,
        CatalogFile->extents[0].startBlock,  // Starting block for root node
        BlockSize,
        Buffer
    );
    if (EFI_ERROR(Status)) {
        FreePool(Buffer);
        return Status;
    }

    // Begin recursive search through index nodes to find the correct leaf node
    Status = TraverseBTreeNodeRecursively(BlockIo, CatalogFile, Buffer, ParentFolderID, FileName, CatalogRecord);

    FreePool(Buffer);
    return Status;
}

// Recursive traversal of B-tree nodes (index and leaf nodes)
EFI_STATUS TraverseBTreeNodeRecursively(
    EFI_BLOCK_IO_PROTOCOL *BlockIo,
    HFSPlusForkData *CatalogFile,
    UINT8 *NodeBuffer,
    UINT32 ParentFolderID,
    CHAR16 *FileName,
    VOID **CatalogRecord
) {
    BTNodeDescriptor *NodeDesc = (BTNodeDescriptor *)NodeBuffer;

    if (NodeDesc->kind == 0xFF) {
        // Leaf node: Perform linear search for the file within the leaf node
        return SearchLeafNodeForFile(BlockIo, NodeBuffer, ParentFolderID, FileName, CatalogRecord);
    } else {
        // Index node: Perform binary search for the child node
        return SearchIndexNodeAndRecurse(BlockIo, CatalogFile, NodeBuffer, ParentFolderID, FileName, CatalogRecord);
    }
}

// Binary search in index nodes, recurse into child nodes
EFI_STATUS SearchIndexNodeAndRecurse(
    EFI_BLOCK_IO_PROTOCOL *BlockIo,
    HFSPlusForkData *CatalogFile,
    UINT8 *NodeBuffer,
    UINT32 ParentFolderID,
    CHAR16 *FileName,
    VOID **CatalogRecord
) {
    BTNodeDescriptor *NodeDesc = (BTNodeDescriptor *)NodeBuffer;

    UINT8 *RecordPtr = NodeBuffer + sizeof(BTNodeDescriptor);
    INT32 Left = 0;
    INT32 Right = NodeDesc->numRecords - 1;

    // Perform binary search on the index node
    while (Left <= Right) {
        INT32 Mid = (Left + Right) / 2;
        HFSPlusCatalogKey *CatalogKey = (HFSPlusCatalogKey *)(RecordPtr + Mid * sizeof(HFSPlusCatalogKey));

        if (CatalogKey->parentID == ParentFolderID) {
            // Correct node found, now recurse into the child node
            BTNodeDescriptor *ChildNode = GetChildNode(BlockIo, CatalogFile, CatalogKey);
            return TraverseBTreeNodeRecursively(BlockIo, CatalogFile, (UINT8 *)ChildNode, ParentFolderID, FileName, CatalogRecord);
        } else if (CatalogKey->parentID < ParentFolderID) {
            Left = Mid + 1;
        } else {
            Right = Mid - 1;
        }
    }

    return EFI_NOT_FOUND;
}

// Search within leaf nodes for the target file or directory
EFI_STATUS SearchLeafNodeForFile(
    EFI_BLOCK_IO_PROTOCOL *BlockIo,
    UINT8 *NodeBuffer,
    UINT32 ParentFolderID,
    CHAR16 *FileName,
    VOID **CatalogRecord
) {
    BTNodeDescriptor *NodeDesc = (BTNodeDescriptor *)NodeBuffer;

    UINT8 *RecordPtr = NodeBuffer + sizeof(BTNodeDescriptor);
    for (UINT16 RecordIndex = 0; RecordIndex < NodeDesc->numRecords; RecordIndex++) {
        HFSPlusCatalogKey *CatalogKey = (HFSPlusCatalogKey *)RecordPtr;

        if (CatalogKey->parentID == ParentFolderID) {
            CHAR16 *RecordFileName = GetFileNameFromKey(CatalogKey);  // Assume filename extraction function
            if (StrCmp(RecordFileName, FileName) == 0) {
                *CatalogRecord = (VOID *)(RecordPtr + sizeof(HFSPlusCatalogKey));
                return EFI_SUCCESS;
            }
        }

        RecordPtr += sizeof(HFSPlusCatalogKey) + CatalogKey->keyLength;
    }

    return EFI_NOT_FOUND;
}

// Load child nodes from the disk for further traversal
BTNodeDescriptor *GetChildNode(
    EFI_BLOCK_IO_PROTOCOL *BlockIo,
    HFSPlusForkData *CatalogFile,
    HFSPlusCatalogKey *CatalogKey
) {
    UINTN BlockSize = BlockIo->Media->BlockSize;
    UINT8 *ChildNodeBuffer = AllocateZeroPool(BlockSize);
    if (ChildNodeBuffer == NULL) {
        return NULL;
    }

    // Read the child node (block specified in CatalogKey)
    BlockIo->ReadBlocks(
        BlockIo,
        BlockIo->Media->MediaId,
        CatalogKey->parentID,  // Assume child node is pointed to by parentID (this can vary based on HFS+ structure)
        BlockSize,
        ChildNodeBuffer
    );

    return (BTNodeDescriptor *)ChildNodeBuffer;
}
