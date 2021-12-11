/* _NVRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2014-2020 by NVIDIA Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to NVIDIA
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of NVIDIA Corporation is prohibited.
 *
 * _NVRM_COPYRIGHT_END_
 */

//
// This file provides common types for both UVM driver and RM's UVM interface.
//

#ifndef _NV_UVM_TYPES_H_
#define _NV_UVM_TYPES_H_

#include "nvtypes.h"
#include "nvgputypes.h"
#include "nvCpuUuid.h"
#include "uvmtypes.h"
#ifndef __KERNEL__

#endif

//
// Default Page Size if left "0" because in RM BIG page size is default & there
// are multiple BIG page sizes in RM. These defines are used as flags to "0"
// should be OK when user is not sure which pagesize allocation it wants
//
#define UVM_PAGE_SIZE_DEFAULT    0x0
#define UVM_PAGE_SIZE_4K         0x1000
#define UVM_PAGE_SIZE_64K        0x10000
#define UVM_PAGE_SIZE_128K       0x20000
#define UVM_PAGE_SIZE_2M         0x200000

#define UVM_PAGE_SIZE_512M       0x20000000


//
// When modifying flags, make sure they are compatible with the mirrored
// PMA_* flags in pma.h.
//
// Input flags
#define UVM_PMA_ALLOCATE_DONT_EVICT             NVBIT(0)
#define UVM_PMA_ALLOCATE_PINNED                 NVBIT(1)
#define UVM_PMA_ALLOCATE_SPECIFY_MINIMUM_SPEED  NVBIT(2)
#define UVM_PMA_ALLOCATE_SPECIFY_ADDRESS_RANGE  NVBIT(3)
#define UVM_PMA_ALLOCATE_SPECIFY_REGION_ID      NVBIT(4)
#define UVM_PMA_ALLOCATE_PREFER_SLOWEST         NVBIT(5)
#define UVM_PMA_ALLOCATE_CONTIGUOUS             NVBIT(6)
#define UVM_PMA_ALLOCATE_PERSISTENT             NVBIT(7)
#define UVM_PMA_ALLOCATE_FORCE_ALIGNMENT        NVBIT(9)
#define UVM_PMA_ALLOCATE_NO_ZERO                NVBIT(10)
#define UVM_PMA_ALLOCATE_TURN_BLACKLIST_OFF     NVBIT(11)
#define UVM_PMA_ALLOCATE_ALLOW_PARTIAL          NVBIT(12)

// Output flags
#define UVM_PMA_ALLOCATE_RESULT_IS_ZERO         NVBIT(0)

// Input flags to pmaFree
#define UVM_PMA_FREE_IS_ZERO                    NVBIT(0)

//
// Indicate that the PMA operation is being done from one of the PMA eviction
// callbacks.
//
// Notably this flag is currently used only by the UVM/RM interface and not
// mirrored in PMA.
//
#define UVM_PMA_CALLED_FROM_PMA_EVICTION        16384

#define UVM_UUID_LEN 16
#define UVM_SW_OBJ_SUBCHANNEL 5

typedef unsigned long long UvmGpuPointer;

//
// The following typedefs serve to explain the resources they point to.
// The actual resources remain RM internal and not exposed.
//
typedef struct uvmGpuSession_tag      *uvmGpuSessionHandle;      // gpuSessionHandle
typedef struct uvmGpuDevice_tag       *uvmGpuDeviceHandle;       // gpuDeviceHandle
typedef struct uvmGpuAddressSpace_tag *uvmGpuAddressSpaceHandle; // gpuAddressSpaceHandle
typedef struct uvmGpuChannel_tag      *uvmGpuChannelHandle;      // gpuChannelHandle
typedef struct uvmGpuCopyEngine_tag   *uvmGpuCopyEngineHandle;   // gpuObjectHandle

typedef struct UvmGpuMemoryInfo_tag
{
    // Out: Memory layout.
    NvU32 kind;

    // Out: Set to TRUE, if the allocation is in sysmem.
    NvBool sysmem;

    // Out: Set to TRUE, if the allocation is a constructed
    //      under a Device or Subdevice.
    //      All permutations of sysmem and deviceDescendant are valid.
    //      !sysmem && !deviceDescendant implies a fabric allocation.
    NvBool deviceDescendant;

    // Out: Page size associated with the phys alloc.
    NvU32 pageSize;

    // Out: Set to TRUE, if the allocation is contiguous.
    NvBool contig;

    // Out: Starting Addr if the allocation is contiguous.
    //      This is only valid if contig is NV_TRUE.
    NvU64 physAddr;

    // Out: Total size of the allocation.
    NvU64 size;

    // Out: Uuid of the GPU to which the allocation belongs.
    //      This is only valid if deviceDescendant is NV_TRUE.
    //      Note: If the allocation is owned by a device in
    //      an SLI group and the allocation is broadcast
    //      across the SLI group, this UUID will be any one
    //      of the subdevices in the SLI group.
    NvProcessorUuid uuid;
} UvmGpuMemoryInfo;

// Note: Global resources might have to be shared for all channels
// under a single GPU's VA space. If the resource is global, the same
// resourceDescriptor might be returned for more than one channel under
// that VA space. The RM-internal reference count is incremented each
// time it's returned.
// Local resources are 1:1 with a channel.
typedef struct UvmGpuChannelResourceInfo_tag
{
    // Out: Ptr to the RM memDesc of the channel resource.
    NvP64 resourceDescriptor;

    // Out: Set to NV_TRUE, if the resource is global.
    //      Set to NV_FALSE, if the resource is local.
    NvBool globalResource;

    // Out: Alignment needed for the resource allocation.
    NvU64 alignment;

    // Out: Info about the resource allocation.
    UvmGpuMemoryInfo resourceInfo;
} UvmGpuChannelResourceInfo;

typedef enum
{
    UVM_GPU_CHANNEL_ENGINE_TYPE_GR = 1,
    UVM_GPU_CHANNEL_ENGINE_TYPE_CE = 2,
} UVM_GPU_CHANNEL_ENGINE_TYPE;

typedef struct UvmGpuChannelInstanceInfo_tag
{
    // Out: Starting address of the channel instance.
    NvU64 base;

    // Out: Set to NV_TRUE, if the instance is in sysmem.
    //      Set to NV_FALSE, if the instance is in vidmem.
    NvBool sysmem;

    NvU32 chId;

    // Out: NV_TRUE if the channel belongs to a subcontext or NV_FALSE if it
    // belongs to a regular context.
    NvBool bInSubctx;

    // Out: ID of the subcontext to which the channel belongs.
    NvU32 subctxId;

    // Out: Whether the channel belongs to a TSG or not
    NvBool bTsgChannel;

    // Out: ID of the TSG to which the channel belongs
    NvU32 tsgId;

    // Out: Maximum number of subcontexts in the TSG to which the channel belongs
    NvU32 tsgMaxSubctxCount;

    // Out: Number of channel resources (local CTX buffers + global CTX buffers)
    // associated with the channel.
    NvU32 resourceCount;

    // Out: Type of the engine the channel is bound to
    NvU32 channelEngineType;

    // Out: Channel handle required to ring the doorbell
    NvU32 workSubmissionToken;

    // Out: Address of the doorbell
    volatile NvU32 *workSubmissionOffset;

    // Out: Channel handle to be used in the CLEAR_FAULTED method
    NvU32 clearFaultedToken;


    // Out: Address of the NV_CHRAM_CHANNEL register required to clear the
    // ENG_FAULTED/PBDMA_FAULTED bits after servicing non-replayable faults on
    // Ampere+ GPUs
    volatile NvU32 *pChramChannelRegister;

    // Out: SMC engine id to which the GR channel is bound, or zero if the GPU
    // does not support SMC or it is a CE channel
    NvU32 smcEngineId;

    // Out: Start of the VEID range assigned to the SMC engine the GR channel
    // is bound to, or zero if the GPU does not support SMC or it is a CE
    // channel
    NvU32 smcEngineVeIdOffset;

} UvmGpuChannelInstanceInfo;

typedef struct UvmGpuChannelResourceBindParams_tag
{
    // In: Ptr to the RM memDesc of the channel resource.
    NvP64 resourceDescriptor;

    // In: Starting VA at which the channel resource is mapped.
    NvU64 resourceVa;

    // In: Set to NV_TRUE, if the resource is global.
    //     Set to NV_FALSE, if the resource is local.
    // Note: This is a temporary parameter. NV2080_CTRL_CMD_GR_BIND_CHANNEL_CONTEXT should figure
    // out if the resourceDescriptor belongs to a Global/Local CTX resource. #Bug - 1732494
    NvBool globalResource;
} UvmGpuChannelResourceBindParams;

typedef struct UvmGpuChannelInfo_tag
{
    volatile unsigned *gpGet;
    volatile unsigned *gpPut;
    UvmGpuPointer     *gpFifoEntries;
    unsigned           numGpFifoEntries;
    unsigned           channelClassNum;

    // The errorNotifier is filled out when the channel hits an RC error.
    NvNotification    *errorNotifier;
    NvU32              hwChannelId;
    volatile unsigned *dummyBar1Mapping;

    // These values are filled by nvUvmInterfaceCopyEngineAlloc. The work
    // submission token requires the channel to be bound to a runlist and that
    // happens after CE allocation.
    volatile NvU32    *workSubmissionOffset;
    NvU32              workSubmissionToken;
} UvmGpuChannelInfo;

typedef enum
{
    // This value must be passed by Pascal and pre-Pascal GPUs for those
    // allocations for which a specific location cannot be enforced.
    UVM_BUFFER_LOCATION_DEFAULT = 0,

    UVM_BUFFER_LOCATION_SYS  = 1,
    UVM_BUFFER_LOCATION_VID  = 2,
} UVM_BUFFER_LOCATION;

typedef struct UvmGpuChannelAllocParams_tag
{
    NvU32 numGpFifoEntries;

    // The next two fields store UVM_BUFFER_LOCATION values
    NvU32 gpFifoLoc;
    NvU32 gpPutLoc;

    // Index of the engine the channel will be bound to
    NvU32 engineIndex;
} UvmGpuChannelAllocParams;

// The max number of Copy Engines supported by a GPU.
// The gpu ops build has a static assert that this is the correct number.
#define UVM_COPY_ENGINE_COUNT_MAX 10

typedef struct
{
    // True if the CE is supported at all
    NvBool supported:1;

    // True if the CE is synchronous with GR
    NvBool grce:1;

    // True if the CE shares physical CEs with any other CE
    //
    // The value returned by RM for this field may change when a GPU is
    // registered with RM for the first time, so UVM needs to query it
    // again each time a GPU is registered.
    NvBool shared:1;

    // True if the CE can give enhanced performance for SYSMEM reads over other CEs
    NvBool sysmemRead:1;

    // True if the CE can give enhanced performance for SYSMEM writes over other CEs
    NvBool sysmemWrite:1;

    // True if the CE can be used for SYSMEM transactions
    NvBool sysmem:1;

    // True if the CE can be used for P2P transactions using NVLINK
    NvBool nvlinkP2p:1;

    // True if the CE can be used for P2P transactions
    NvBool p2p:1;

    // Mask of physical CEs assigned to this LCE
    //
    // The value returned by RM for this field may change when a GPU is
    // registered with RM for the first time, so UVM needs to query it
    // again each time a GPU is registered.
    NvU32 cePceMask;
} UvmGpuCopyEngineCaps;

typedef struct UvmGpuCopyEnginesCaps_tag
{
    // Supported CEs may not be contiguous
    UvmGpuCopyEngineCaps copyEngineCaps[UVM_COPY_ENGINE_COUNT_MAX];
} UvmGpuCopyEnginesCaps;

typedef enum
{
    UVM_PEER_LINK_TYPE_NONE,
    UVM_PEER_LINK_TYPE_PCIE,
    UVM_PEER_LINK_TYPE_NVLINK_1,
    UVM_PEER_LINK_TYPE_NVLINK_2,

    UVM_PEER_LINK_TYPE_NVLINK_3,

} UVM_PEER_LINK_TYPE;

typedef struct UvmGpuCaps_tag
{
    NvU32    sysmemLink;            // UVM_PEER_LINK_TYPE
    NvU32    sysmemLinkRateMBps;    // See UvmGpuP2PCapsParams::totalLinkLineRateMBps
    NvBool   numaEnabled;
    NvU32    numaNodeId;

    // On ATS systems, GPUs connected to different CPU sockets can have peer
    // traffic. They are called indirect peers. However, indirect peers are
    // mapped using sysmem aperture. In order to disambiguate the location of a
    // specific memory address, each GPU maps its memory to a different window
    // in the System Physical Address (SPA) space. The following fields contain
    // the base + size of such window for the GPU. systemMemoryWindowSize
    // different than 0 indicates that the window is valid.
    //
    // - If the window is valid, then we can map GPU memory to the CPU as
    // cache-coherent by adding the GPU address to the window start.
    // - If numaEnabled is NV_TRUE, then we can also convert the system
    // addresses of allocated GPU memory to struct pages.
    //
    // TODO: Bug 1986868: fix window start computation for SIMICS
    NvU64    systemMemoryWindowStart;
    NvU64    systemMemoryWindowSize;

    // This tells if the GPU is connected to NVSwitch. On systems with NVSwitch
    // all GPUs are connected to it. If connectedToSwitch is NV_TRUE,
    // nvswitchMemoryWindowStart tells the base address for the GPU in the
    // NVSwitch address space. It is used when creating PTEs of memory mappings
    // to NVSwitch peers.
    NvBool   connectedToSwitch;
    NvU64    nvswitchMemoryWindowStart;
} UvmGpuCaps;

typedef struct UvmGpuAddressSpaceInfo_tag
{
    NvU32           bigPageSize;

    NvBool          atsEnabled;

    // Mapped registers that contain the current GPU time
    volatile NvU32  *time0Offset;
    volatile NvU32  *time1Offset;

    // Maximum number of subcontexts supported under this GPU address space
    NvU32           maxSubctxCount;


    NvBool          smcEnabled;

    NvU32           smcSwizzId;

    NvU32           smcGpcCount;

} UvmGpuAddressSpaceInfo;

typedef struct UvmGpuAllocInfo_tag
{
    NvU64   rangeBegin;             // Allocation will be made between
    NvU64   rangeEnd;               // rangeBegin & rangeEnd both included
    NvU64   gpuPhysOffset;          // Returns gpuPhysOffset if contiguous requested
    NvU32   pageSize;               // default is RM big page size - 64K or 128 K" else use 4K or 2M
    NvU64   alignment;              // Alignment of allocation
    NvBool  bContiguousPhysAlloc;   // Flag to request contiguous physical allocation
    NvBool  bMemGrowsDown;          // Causes RM to reserve physical heap from top of FB
    NvBool  bPersistentVidmem;      // Causes RM to allocate persistent video memory
    NvHandle hPhysHandle;           // Handle for phys allocation either provided or retrieved
} UvmGpuAllocInfo;

typedef struct UvmGpuExternalMappingInfo_tag
{
    // In: GPU caching ability.
    UvmGpuCachingType cachingType;

    // In: Virtual permissions.
    UvmGpuMappingType mappingType;

    // In: RM virtual mapping memory format
    UvmGpuFormatType formatType;

    // In: RM virtual mapping element bits
    UvmGpuFormatElementBits elementBits;

    // In: RM virtual compression type
    UvmGpuCompressionType compressionType;

    // In: Size of the buffer to store PTEs (in bytes).
    NvU64 pteBufferSize;

    // In: Pointer to a buffer to store PTEs.
    // Out: The interface will fill the buffer with PTEs
    NvU64 *pteBuffer;

    // Out: Number of PTEs filled in to the buffer.
    NvU64 numWrittenPtes;

    // Out: Number of PTEs remaining to be filled
    //      if the buffer is not sufficient to accommodate
    //      requested PTEs.
    NvU64 numRemainingPtes;

    // Out: PTE size (in bytes)
    NvU32 pteSize;
} UvmGpuExternalMappingInfo;

typedef struct UvmGpuP2PCapsParams_tag
{
    NvU32 peerIds[2];               // peerId[i] contains gpu[i]'s peer id of gpu[1 - i]
    NvU32 p2pLink;                  // UVM_PEER_LINK_TYPE
    NvU32 optimalNvlinkWriteCEs[2]; // optimalNvlinkWriteCEs[i] contains gpu[i]'s optimal
                                    // CE for writing to gpu[1 - i] over NVLink.
                                    // The CE indexes are valid only if the GPUs are direct
                                    // NVLink peers
                                    //
                                    // The value returned by RM for this field
                                    // may change when a GPU is registered with
                                    // RM for the first time, so UVM needs to
                                    // query it again each time a GPU is
                                    // registered.
    NvU32 totalLinkLineRateMBps;    // Maximum unidirectional bandwidth between
                                    // the peers in megabytes per second, not
                                    // taking into account the protocols
                                    // overhead. The reported bandwidth for
                                    // indirect peers is zero.
    NvU32 indirectAccess      : 1;  // Whether peers have a indirect link to communicate.
                                    // On P9 systems, this is true if peers are
                                    // connected to different NPUs that forward the
                                    // requests between them.
} UvmGpuP2PCapsParams;

// Platform-wide information
typedef struct UvmPlatformInfo_tag
{
    // Out: ATS (Address Translation Services) is supported
    NvBool atsSupported;
} UvmPlatformInfo;

typedef struct UvmGpuClientInfo_tag
{
    NvHandle hClient;


    NvHandle hSmcPartRef;

} UvmGpuClientInfo;

#define UVM_GPU_NAME_LENGTH 0x40

typedef struct UvmGpuInfo_tag
{
    // Printable gpu name
    char name[UVM_GPU_NAME_LENGTH];

    // Uuid of this gpu
    NvProcessorUuid uuid;

    // Gpu architecture; NV2080_CTRL_MC_ARCH_INFO_ARCHITECTURE_*
    NvU32 gpuArch;

    // Gpu implementation; NV2080_CTRL_MC_ARCH_INFO_IMPLEMENTATION_*
    NvU32 gpuImplementation;

    // Host (gpfifo) class; *_CHANNEL_GPFIFO_*, e.g. KEPLER_CHANNEL_GPFIFO_A
    NvU32 hostClass;

    // Copy engine (dma) class; *_DMA_COPY_*, e.g. KEPLER_DMA_COPY_A
    NvU32 ceClass;

    // Memory to memory class; *_MEMORY_*, e.g. FERMI_MEMORY_TO_MEMORY_FORMAT_A
    NvU32 memToMemClass;

    // Compute class; *_COMPUTE_*, e.g. KEPLER_COMPUTE_A
    NvU32 computeClass;

    // Fault buffer class; *_FAULT_BUFFER_*, e.g. MAXWELL_FAULT_BUFFER_A
    NvU32 faultBufferClass;

    // Access counter notification buffer class; ACCESS_COUNTER_NOTIFY_BUFFER
    NvU32 accessCounterBufferClass;

    // Set if GPU supports TCC Mode & is in TCC mode.
    NvBool gpuInTcc;

    // Number of subdevices in SLI group.
    NvU32 subdeviceCount;

    // NV_TRUE if this is a simulated/emulated GPU. NV_FALSE, otherwise.
    NvBool isSimulated;

    // Number of GPCs

    // If SMC is enabled, this is the currently configured number of GPCs for
    // the given partition (also see the smcSwizzId field below).

    NvU32 gpcCount;

    // Maximum number of GPCs; NV_SCAL_LITTER_NUM_GPCS

    // This number is independent of the partition configuration, and can be
    // used to conservatively size GPU-global constructs.

    NvU32 maxGpcCount;

    // Number of TPCs
    NvU32 tpcCount;

    // Maximum number of TPCs per GPC
    NvU32 maxTpcPerGpcCount;


    // NV_TRUE if SMC is enabled on this GPU.
    NvBool smcEnabled;

    // SMC partition ID (unique per GPU); note: valid when first looked up in
    // nvUvmInterfaceGetGpuInfo(), but not guaranteed to remain valid.
    // nvUvmInterfaceDeviceCreate() re-verifies the swizzId and fails if it is
    // no longer valid.
    NvU32 smcSwizzId;

    UvmGpuClientInfo smcUserClientInfo;

} UvmGpuInfo;

typedef struct UvmGpuVaAllocInfo_tag
{
    NvU64    vaStart;                    // Needs to be alinged to pagesize
    NvBool   bFixedAddressAllocate;      // rangeBegin & rangeEnd both included
    NvU32    pageSize;                   // default is 4k or 64k else use pagesize= 2M
} UvmGpuVaAllocInfo;

typedef struct UvmGpuFbInfo_tag
{
    // Max physical address that can be allocated by UVM. This excludes internal
    // RM regions that are not registered with PMA either.
    NvU64 maxAllocatableAddress;

    NvU32 heapSize;         // RAM in KB available for user allocations
    NvU32 reservedHeapSize; // RAM in KB reserved for internal RM allocation
    NvBool bZeroFb;         // Zero FB mode enabled.
} UvmGpuFbInfo;

typedef struct UvmGpuEccInfo_tag
{
    unsigned eccMask;
    unsigned eccOffset;
    void    *eccReadLocation;
    NvBool  *eccErrorNotifier;
    NvBool   bEccEnabled;
} UvmGpuEccInfo;

typedef struct UvmPmaAllocationOptions_tag
{
    NvU32 flags;
    NvU32 minimumSpeed;         // valid if flags & UVM_PMA_ALLOCATE_SPECIFY_MININUM_SPEED
    NvU64 physBegin, physEnd;   // valid if flags & UVM_PMA_ALLOCATE_SPECIFY_ADDRESS_RANGE
    NvU32 regionId;             // valid if flags & UVM_PMA_ALLOCATE_SPECIFY_REGION_ID
    NvU64 alignment;            // valid if flags & UVM_PMA_ALLOCATE_FORCE_ALIGNMENT
    NvLength numPagesAllocated; // valid if flags & UVM_PMA_ALLOCATE_ALLOW_PARTIAL

    NvU32 resultFlags;          // valid if the allocation function returns NV_OK
} UvmPmaAllocationOptions;

//
// Mirrored in PMA (PMA_STATS)
//
typedef struct UvmPmaStatistics_tag
{
    volatile NvU64 numPages2m;      // PMA-wide 2MB pages count
    volatile NvU64 numFreePages64k; // PMA-wide free 64KB page count
    volatile NvU64 numFreePages2m;  // PMA-wide free 2MB pages count
} UvmPmaStatistics;

/*******************************************************************************
    uvmEventSuspend
    This function will be called by the GPU driver to signal to UVM that the
    system is about to enter a sleep state.  When it is called, the
    following assumptions/guarantees are valid/made:

      * User channels have been preempted and disabled
      * UVM channels are still running normally and will continue to do
        so until after this function returns control
      * User threads are still running, but can no longer issue system
        system calls to the GPU driver
      * Until exit from this function, UVM is allowed to make full use of
        the GPUs under its control, as well as of the GPU driver

    Upon return from this function, UVM may not access GPUs under its control
    until the GPU driver calls uvmEventResume().  It may still receive
    calls to uvmEventIsrTopHalf() during this time, which it should return
    NV_ERR_NO_INTR_PENDING from.  It will not receive any other calls.
*/
typedef NV_STATUS (*uvmEventSuspend_t) (void);

/*******************************************************************************
    uvmEventResume
    This function will be called by the GPU driver to signal to UVM that the
    system has exited a previously entered sleep state.  When it is called,
    the following assumptions/guarantees are valid/made:

      * UVM is again allowed to make full use of the GPUs under its
        control, as well as of the GPU driver
      * UVM channels are running normally
      * User channels are still preempted and disabled
      * User threads are again running, but still cannot issue system
        calls to the GPU driver, nor submit new work

    Upon return from this function, UVM is expected to be fully functional.
*/
typedef NV_STATUS (*uvmEventResume_t) (void);

/*******************************************************************************
    uvmEventStartDevice
    This function will be called by the GPU driver once it has finished its
    initialization to tell the UVM driver that this GPU has come up.
*/
typedef NV_STATUS (*uvmEventStartDevice_t) (const NvProcessorUuid *pGpuUuidStruct);

/*******************************************************************************
    uvmEventStopDevice
    This function will be called by the GPU driver to let UVM know that a GPU
    is going down.
*/
typedef NV_STATUS (*uvmEventStopDevice_t) (const NvProcessorUuid *pGpuUuidStruct);

#if defined (_WIN32)
/*******************************************************************************
    uvmEventWddmResetDuringTimeout
    This function will be called by KMD in a TDR servicing path to unmap channel
    resources and to destroy channels. This is a Windows specific event.
*/
typedef NV_STATUS (*uvmEventWddmResetDuringTimeout_t) (const NvProcessorUuid *pGpuUuidStruct);

/*******************************************************************************
    uvmEventWddmRestartAfterTimeout
    This function will be called by KMD in a TDR servicing path to map channel
    resources and to create channels. This is a Windows specific event.
*/
typedef NV_STATUS (*uvmEventWddmRestartAfterTimeout_t) (const NvProcessorUuid *pGpuUuidStruct);

/*******************************************************************************
    uvmEventServiceInterrupt
    This function gets called from RM's intr service routine when an interrupt
    to service a page fault is triggered.
*/
typedef NV_STATUS (*uvmEventServiceInterrupt_t) (void *pDeviceObject,
    NvU32 deviceId, NvU32 subdeviceId);
#endif

/*******************************************************************************
    uvmEventIsrTopHalf_t
    This function will be called by the GPU driver to let UVM know
    that an interrupt has occurred.

    Returns:
        NV_OK if the UVM driver handled the interrupt
        NV_ERR_NO_INTR_PENDING if the interrupt is not for the UVM driver
*/
#if defined (__linux__)
typedef NV_STATUS (*uvmEventIsrTopHalf_t) (const NvProcessorUuid *pGpuUuidStruct);
#else
typedef void (*uvmEventIsrTopHalf_t) (void);
#endif

struct UvmOpsUvmEvents
{
    uvmEventSuspend_t     suspend;
    uvmEventResume_t      resume;
    uvmEventStartDevice_t startDevice;
    uvmEventStopDevice_t  stopDevice;
    uvmEventIsrTopHalf_t  isrTopHalf;
#if defined (_WIN32)
    uvmEventWddmResetDuringTimeout_t wddmResetDuringTimeout;
    uvmEventWddmRestartAfterTimeout_t wddmRestartAfterTimeout;
    uvmEventServiceInterrupt_t serviceInterrupt;
#endif
};

typedef struct UvmGpuFaultInfo_tag
{
    struct
    {
        // Register mappings obtained from RM
        volatile NvU32* pFaultBufferGet;
        volatile NvU32* pFaultBufferPut;
        // Note: this variable is deprecated since buffer overflow is not a separate
        // register from future chips.
        volatile NvU32* pFaultBufferInfo;
        volatile NvU32* pPmcIntr;
        volatile NvU32* pPmcIntrEnSet;
        volatile NvU32* pPmcIntrEnClear;
        volatile NvU32* pPrefetchCtrl;
        NvU32 replayableFaultMask;
        // fault buffer cpu mapping and size
        void* bufferAddress;
        NvU32  bufferSize;
    } replayable;
    struct
    {
        // Shadow buffer for non-replayable faults on cpu memory. Resman copies
        // here the non-replayable faults that need to be handled by UVM
        void* shadowBufferAddress;
        NvU32  bufferSize;

        // Preallocated stack for functions called from the UVM isr top half
        void *isr_sp;

        // Preallocated stack for functions called from the UVM isr bottom half
        void *isr_bh_sp;
    } nonReplayable;
    NvHandle faultBufferHandle;
} UvmGpuFaultInfo;

typedef struct UvmGpuAccessCntrInfo_tag
{
    // Register mappings obtained from RM
    // pointer to the Get register for the access counter buffer
    volatile NvU32* pAccessCntrBufferGet;
    // pointer to the Put register for the access counter buffer
    volatile NvU32* pAccessCntrBufferPut;
    // pointer to the Full register for the access counter buffer
    volatile NvU32* pAccessCntrBufferFull;
    // pointer to the hub interrupt
    volatile NvU32* pHubIntr;
    // pointer to interrupt enable register
    volatile NvU32* pHubIntrEnSet;
    // pointer to interrupt disable register
    volatile NvU32* pHubIntrEnClear;
    // mask for the access counter buffer
    NvU32 accessCounterMask;
    // access counter buffer cpu mapping and size
    void* bufferAddress;
    NvU32  bufferSize;
    NvHandle accessCntrBufferHandle;

    // The Notification address in the access counter notification msg does not
    // contain the correct upper bits 63-47 for GPA-based notifications. RM
    // provides us with the correct offset to be added.
    // See Bug 1803015
    NvU64 baseDmaSysmemAddr;
} UvmGpuAccessCntrInfo;

typedef enum
{
    UVM_ACCESS_COUNTER_GRANULARITY_64K = 1,
    UVM_ACCESS_COUNTER_GRANULARITY_2M  = 2,
    UVM_ACCESS_COUNTER_GRANULARITY_16M = 3,
    UVM_ACCESS_COUNTER_GRANULARITY_16G = 4,
} UVM_ACCESS_COUNTER_GRANULARITY;

typedef enum
{
    UVM_ACCESS_COUNTER_USE_LIMIT_NONE = 1,
    UVM_ACCESS_COUNTER_USE_LIMIT_QTR  = 2,
    UVM_ACCESS_COUNTER_USE_LIMIT_HALF = 3,
    UVM_ACCESS_COUNTER_USE_LIMIT_FULL = 4,
} UVM_ACCESS_COUNTER_USE_LIMIT;

typedef struct UvmGpuAccessCntrConfig_tag
{
    NvU32 mimcGranularity;

    NvU32 momcGranularity;

    NvU32 mimcUseLimit;

    NvU32 momcUseLimit;

    NvU32 threshold;
} UvmGpuAccessCntrConfig;

typedef UvmGpuChannelInfo gpuChannelInfo;
typedef UvmGpuChannelAllocParams gpuChannelAllocParams;
typedef UvmGpuCaps gpuCaps;
typedef UvmGpuCopyEngineCaps gpuCeCaps;
typedef UvmGpuCopyEnginesCaps gpuCesCaps;
typedef UvmGpuP2PCapsParams getP2PCapsParams;
typedef UvmGpuAddressSpaceInfo gpuAddressSpaceInfo;
typedef UvmGpuAllocInfo gpuAllocInfo;
typedef UvmGpuInfo gpuInfo;
typedef UvmGpuClientInfo gpuClientInfo;
typedef UvmGpuAccessCntrInfo gpuAccessCntrInfo;
typedef UvmGpuAccessCntrConfig gpuAccessCntrConfig;
typedef UvmGpuFaultInfo gpuFaultInfo;
typedef UvmGpuMemoryInfo gpuMemoryInfo;
typedef UvmGpuExternalMappingInfo gpuExternalMappingInfo;
typedef UvmGpuChannelResourceInfo gpuChannelResourceInfo;
typedef UvmGpuChannelInstanceInfo gpuChannelInstanceInfo;
typedef UvmGpuChannelResourceBindParams gpuChannelResourceBindParams;
typedef UvmGpuFbInfo gpuFbInfo;
typedef UvmGpuEccInfo gpuEccInfo;

#endif // _NV_UVM_TYPES_H_
