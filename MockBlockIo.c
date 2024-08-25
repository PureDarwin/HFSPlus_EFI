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
#  MockBlockio.c
#  This file is the c source for the Mock Block IO Test operations
#
# @AUTHOR
# Created by Cliff Sekel  for The PureDarwin Project github.com/PureDarwin
#

#include "MockBlockIo.h"

EFI_STATUS
MockReadBlocks(
    MockBlockIoProtocol *BlockIo,
    UINT32 MediaId,
    UINT64 LBA,
    UINTN BufferSize,
    VOID *Buffer
) {
    if (LBA + BufferSize / BlockIo->BlockSize > BlockIo->LastBlock) {
        return EFI_DEVICE_ERROR;
    }

    CopyMem(Buffer, BlockIo->DiskData + LBA * BlockIo->BlockSize, BufferSize);
    return EFI_SUCCESS;
}

EFI_STATUS
MockWriteBlocks(
    MockBlockIoProtocol *BlockIo,
    UINT32 MediaId,
    UINT64 LBA,
    UINTN BufferSize,
    VOID *Buffer
) {
    if (LBA + BufferSize / BlockIo->BlockSize > BlockIo->LastBlock) {
        return EFI_DEVICE_ERROR;
    }

    CopyMem(BlockIo->DiskData + LBA * BlockIo->BlockSize, Buffer, BufferSize);
    return EFI_SUCCESS;
}

MockBlockIoProtocol *
InitializeMockDisk(UINT64 TotalBlocks, UINTN BlockSize) {
    MockBlockIoProtocol *MockBlockIo = AllocateZeroPool(sizeof(MockBlockIoProtocol));
    MockBlockIo->BlockSize = BlockSize;
    MockBlockIo->LastBlock = TotalBlocks - 1;
    MockBlockIo->DiskData = AllocateZeroPool(TotalBlocks * BlockSize);
    return MockBlockIo;
}