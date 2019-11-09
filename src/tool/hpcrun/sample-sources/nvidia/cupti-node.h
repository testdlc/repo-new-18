#ifndef _HPCTOOLKIT_CUPTI_NODE_H_
#define _HPCTOOLKIT_CUPTI_NODE_H_

#include <cupti_activity.h>
#include <hpcrun/cct2metrics.h>
#include <hpcrun/gpu/gpu-activity.h>


void
cupti_activity_entry_set
(
 gpu_activity_t *entry,
 CUpti_Activity *activity,
 cct_node_t *cct_node
);

#if 0
typedef enum {
  CUPTI_ENTRY_TYPE_ACTIVITY = 1,
  CUPTI_ENTRY_TYPE_NOTIFICATION = 2,
  CUPTI_ENTRY_TYPE_BUFFER = 3,
  CUPTI_ENTRY_TYPE_TRACE = 4,
  CUPTI_ENTRY_TYPE_COUNT = 5
} cupti_entry_type_t;

// pc sampling
typedef struct cupti_pc_sampling {
  uint32_t samples;
  uint32_t latencySamples;
  CUpti_ActivityPCSamplingStallReason stallReason;
} cupti_pc_sampling_t;

typedef struct cupti_pc_sampling_record_info {
  uint64_t droppedSamples;
  uint64_t samplingPeriodInCycles;
  uint64_t totalSamples;
  uint64_t fullSMSamples;
} cupti_pc_sampling_record_info_t;

// memory
typedef struct cupti_memcpy {
  uint64_t start;
  uint64_t end;
  uint64_t bytes;
  uint32_t copyKind;
} cupti_memcpy_t;

typedef struct cupti_memory {
  uint64_t start;
  uint64_t end;
  uint64_t bytes;
  uint32_t memKind;
} cupti_memory_t;

typedef struct cupti_memset {
  uint64_t start;
  uint64_t end;
  uint64_t bytes;
  uint32_t memKind;
} cupti_memset_t;

// kernel
typedef struct cupti_kernel {
  uint64_t start;
  uint64_t end;
  int32_t dynamicSharedMemory;
  int32_t staticSharedMemory;
  int32_t localMemoryTotal;
  uint32_t activeWarpsPerSM;
  uint32_t maxActiveWarpsPerSM;
  uint32_t threadRegisters;
  uint32_t blockThreads;
  uint32_t blockSharedMemory;
} cupti_kernel_t;

typedef enum {
  CUPTI_GLOBAL_ACCESS_LOAD_CACHED,
  CUPTI_GLOBAL_ACCESS_LOAD_UNCACHED,
  CUPTI_GLOBAL_ACCESS_STORE,
  CUPTI_GLOBAL_ACCESS_COUNT,
} cupti_global_access_type_t;

typedef struct cupti_global_access {
  uint64_t l2_transactions;
  uint64_t theoreticalL2Transactions;
  uint64_t bytes;
  cupti_global_access_type_t type;
} cupti_global_access_t;

typedef enum {
  CUPTI_SHARED_ACCESS_LOAD,
  CUPTI_SHARED_ACCESS_STORE,
  CUPTI_SHARED_ACCESS_COUNT,
} cupti_shared_access_type_t;

typedef struct cupti_shared_access {
  uint64_t sharedTransactions;
  uint64_t theoreticalSharedTransactions;
  uint64_t bytes;
  cupti_shared_access_type_t type;
} cupti_shared_access_t;

typedef struct cupti_branch {
  uint32_t diverged;
  uint32_t executed;
} cupti_branch_t;

// synchronization
typedef struct cupti_synchronization {
  uint64_t start;
  uint64_t end;
  uint32_t syncKind;
} cupti_synchronization_t;

// generic activity entry
typedef struct cupti_activity {
  CUpti_ActivityKind kind;
  union {
    cupti_pc_sampling_t pc_sampling;
    cupti_pc_sampling_record_info_t pc_sampling_record_info;
    cupti_memcpy_t memcpy;
    cupti_memory_t memory;
    cupti_memset_t memset;
    cupti_kernel_t kernel;
    cupti_global_access_t global_access;
    cupti_shared_access_t shared_access;
    cupti_branch_t branch;
    cupti_synchronization_t synchronization;
  } data;
} cupti_activity_t;

// activity entry
typedef struct cupti_entry_activity {
  cupti_activity_t activity;
  cct_node_t *cct_node;
} cupti_entry_activity_t;

// notification entry
typedef struct cupti_entry_correlation {
  uint64_t host_op_id;
  void *activity_channel;
  cct_node_t *api_node;
  cct_node_t *func_node;
} cupti_entry_correlation_t;

// trace entry
typedef struct cupti_entry_trace {
  uint64_t start;
  uint64_t end;
  cct_node_t *node;
} cupti_entry_trace_t;


void
cupti_entry_activity_set
(
 cupti_entry_activity_t *entry,
 CUpti_Activity *activity,
 cct_node_t *cct_node
);


void
cupti_entry_correlation_set
(
 cupti_entry_correlation_t *entry,
 uint64_t host_op_id,
 void *activity_channel,
 cct_node_t *cct_node,
 cct_node_t *func_node
);


void
cupti_entry_trace_set
(
 cupti_entry_trace_t *entry,
 uint64_t start,
 uint64_t end,
 cct_node_t *node
);

#endif


#endif
