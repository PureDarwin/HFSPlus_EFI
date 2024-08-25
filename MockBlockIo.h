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
#  MockBlockio.h
#  This file is the header for the Mock Block IO Test operations
#
# @AUTHOR
# Created by Cliff Sekel  for The PureDarwin Project github.com/PureDarwin
#

#ifndef MOCK_BLOCK_IO_H
#define MOCK_BLOCK_IO_H

#include <Uefi.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/DebugLib.h>

typedef struct {
    UINT32 MediaId;
    UINTN BlockSize;
    UINT64 LastBlock;
    UINT8 *DiskData;  // Simulated disk data
} MockBlockIoProtocol;

EFI_STATUS MockReadBlocks(
    MockBlockIoProtocol *BlockIo,
    UINT32 MediaId,
    UINT64 LBA,
    UINTN BufferSize,
    VOID *Buffer
);

EFI_STATUS MockWriteBlocks(
    MockBlockIoProtocol *BlockIo,
    UINT32 MediaId,
    UINT64 LBA,
    UINTN BufferSize,
    VOID *Buffer
);

MockBlockIoProtocol *InitializeMockDisk(UINT64 TotalBlocks, UINTN BlockSize);

#endif  // MOCK_BLOCK_IO_H