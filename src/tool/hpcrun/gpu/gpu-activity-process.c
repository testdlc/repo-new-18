//******************************************************************************
// local includes
//******************************************************************************

#include <hpcrun/cct/cct.h>
#include <hpcrun/gpu/gpu-activity.h>
#include <hpcrun/gpu/gpu-activity-channel.h>
#include <hpcrun/gpu/gpu-trace-item.h>
#include <hpcrun/gpu/gpu-correlation-id-map.h>
#include <hpcrun/gpu/nvidia/cuda-device-map.h>
#include <hpcrun/gpu/gpu-host-correlation-map.h>
#include <hpcrun/hpcrun_stats.h>



//******************************************************************************
// macros
//******************************************************************************

#define UNIT_TEST 0

#define DEBUG 0

#if DEBUG
#define PRINT(...) fprintf(stderr, __VA_ARGS__)
#else
#define PRINT(...)
#endif



//******************************************************************************
// private operations
//******************************************************************************

static void
trace_item_set
(
 gpu_trace_item_t *ti,
 gpu_activity_t *ga,
 cct_node_t *call_path_leaf
 )
{
  ti->start = ga->details.interval.start;
  ti->end = ga->details.interval.end;
  ti->call_path_leaf = call_path_leaf;
}


static void
gpu_unknown_process
(
 gpu_activity_t *activity
)
{
  PRINT("Unknown activity kind %d\n", activity->kind);
}

static void
gpu_sample_process
(
 gpu_activity_t* sample
)
{
  gpu_correlation_id_map_entry_t *cid_map_entry =
    gpu_correlation_id_map_lookup(sample->correlationId);

  if (cid_map_entry != NULL) {
    uint64_t external_id =
      gpu_correlation_id_map_entry_external_id_get(cid_map_entry);

    ip_normalized_t ip = sample->details.pc_sampling.pc;

    gpu_host_correlation_map_entry_t *host_op_entry =
      gpu_host_correlation_map_lookup(external_id);

    if (host_op_entry != NULL) {
      PRINT("external_id %d\n", external_id);

      bool more_samples = 
	gpu_host_correlation_map_samples_increase
	(external_id, sample->details.pc_sampling.samples);

      if (!more_samples) {
	gpu_correlation_id_map_delete(sample->correlationId);
      }

      cct_node_t *host_op_node =
	gpu_host_correlation_map_entry_op_cct_get(host_op_entry, 
						  gpu_placeholder_type_kernel);

      cct_node_t *cct_child = hpcrun_cct_insert_ip_norm(host_op_node, ip);
      if (cct_child) {
	PRINT("frm %d\n", ip);
	gpu_activity_channel_t *channel =
	  gpu_host_correlation_map_entry_channel_get(host_op_entry);
	gpu_activity_channel_produce(channel, sample, cct_child);
      }
    } else {
      PRINT("host_map_entry %d not found\n", external_id);
    }
  } else {
    PRINT("host_correlation_map_entry %lu not found\n", external_id);
  }
}


static void
gpu_sampling_record_info_process
(
 gpu_activity_t *sri
)
{
  gpu_correlation_id_map_entry_t *cid_map_entry =
    gpu_correlation_id_map_lookup(sri->correlationId);
  if (cid_map_entry != NULL) {
    uint64_t external_id =
      gpu_correlation_id_map_entry_external_id_get(cid_map_entry);
    gpu_host_correlation_map_entry_t *host_op_entry =
      gpu_host_correlation_map_lookup(external_id);
    if (host_op_entry != NULL) {
      gpu_activity_channel_t *channel =
        gpu_host_correlation_map_entry_record_get(host_op_entry);
      cct_node_t *host_op_node =
	gpu_host_correlation_map_entry_op_cct_get(host_op_entry, 
						  gpu_placeholder_type_kernel);
      gpu_activity_channel_produce(channel, sri, host_op_node);
    }
    // sample record info is the last record for a given correlation id
    bool more_samples = 
      gpu_host_correlation_map_total_samples_update
      (external_id, sri->details.pc_sampling_record_info.totalSamples - 
       sri->details.pc_sampling_record_info.droppedSamples);
    if (!more_samples) {
      gpu_correlation_id_map_delete(sri->correlationId);
    }
  }
  hpcrun_stats_acc_samples_add(sri->details.pc_sampling_record_info.totalSamples);
  hpcrun_stats_acc_samples_dropped_add(sri->details.pc_sampling_record_info.droppedSamples);
}


static void
gpu_correlation_process
(
 gpu_activity_t *ec
)
{
  uint32_t correlation_id = ec->correlationId;
  uint64_t external_id = ec->details.correlation.externalId;
  if (gpu_correlation_id_map_lookup(correlation_id) == NULL) {
    gpu_correlation_id_map_insert(correlation_id, external_id);
  } else {
    PRINT("External CorrelationId Replace %lu)\n", external_id);
    gpu_correlation_id_map_external_id_replace(correlation_id, external_id);
  }
}


static void
gpu_memcpy_process
(
 gpu_activity_t *activity
)
{
  gpu_correlation_id_map_entry_t *cid_map_entry =
    gpu_correlation_id_map_lookup(activity->correlationId);
  if (cid_map_entry != NULL) {
    uint64_t external_id =
      gpu_correlation_id_map_entry_external_id_get(cid_map_entry);
    gpu_host_correlation_map_entry_t *host_op_entry =
      gpu_host_correlation_map_lookup(external_id);
    if (host_op_entry != NULL) {
      cct_node_t *host_op_node =
	gpu_host_correlation_map_entry_op_cct_get(host_op_entry, 
						  gpu_placeholder_type_memcpy);

      gpu_trace_item_t entry_trace;
      trace_item_set(&entry_trace, activity, host_op_node);

      cupti_context_id_map_stream_process
	(activity->details.memcpy.context_id,
	 activity->details.memcpy.stream_id,
	 cupti_trace_append, &entry_trace);

      gpu_activity_channel_t *channel =
        gpu_host_correlation_map_entry_channel_get(host_op_entry);
      gpu_activity_channel_produce(channel, activity, host_op_node);
      //FIXME(keren): In OpenMP, an external_id may maps to multiple cct_nodes
      //gpu_host_correlation_map_delete(external_id);
    }
    gpu_correlation_id_map_delete(activity->correlationId);
  } else {
    PRINT("Memcpy copy CorrelationId %u cannot be found\n",
      activity->correlationId);
  }
}

static void
gpu_memset_process
(
 gpu_activity_t *activity
)
{
  gpu_correlation_id_map_entry_t *cid_map_entry = gpu_correlation_id_map_lookup(activity->correlationId);
  if (cid_map_entry != NULL) {
    uint64_t external_id = gpu_correlation_id_map_entry_external_id_get(cid_map_entry);
    gpu_host_correlation_map_entry_t *host_op_entry = gpu_host_correlation_map_lookup(external_id);
    if (host_op_entry != NULL) {
      cct_node_t *host_op_node =
        gpu_host_correlation_map_entry_host_op_node_get(host_op_entry);

      gpu_trace_item_t entry_trace;
      trace_item_set(&entry_trace, activity, host_op_node);

      cupti_context_id_map_stream_process
	( activity->details.memset.context_id,
	  activity->details.memset.stream_id,
	  cupti_trace_append, &entry_trace);

      gpu_activity_channel_t *channel =
        gpu_host_correlation_map_entry_channel_get(host_op_entry);
      gpu_activity_channel_produce(channel, activity, host_op_node);
      //FIXME(keren): In OpenMP, an external_id may maps to multiple cct_nodes
      //gpu_host_correlation_map_delete(external_id);
    }
    gpu_correlation_id_map_delete(activity->correlationId);
  }
  PRINT("Memset CorrelationId %u\n", activity->correlationId);
  PRINT("Memset kind %u\n", activity->memoryKind);
  PRINT("Memset bytes %lu\n", activity->bytes);
}


static void
gpu_kernel_process
(
 gpu_activity_t *activity
)
{
  gpu_correlation_id_map_entry_t *cid_map_entry =
    gpu_correlation_id_map_lookup(activity->correlationId);
  // TODO: how to refactor this device map
  if (cid_map_entry != NULL) {
#if 0
    if (cuda_device_map_lookup(activity->deviceId) == NULL) {
      cuda_device_map_insert(activity->deviceId);
    }
#endif
    gpu_correlation_id_map_kernel_update
      (activity->correlationId, activity->details.kernel.deviceId, 
       activity->details.interval.start, activity->details.interval.end);

    uint64_t external_id =
      gpu_correlation_id_map_entry_external_id_get(cid_map_entry);
    gpu_host_correlation_map_entry_t *host_op_entry =
      gpu_host_correlation_map_lookup(external_id);
    if (host_op_entry != NULL) {
      cct_node_t *func_node = 
        gpu_host_correlation_map_entry_func_node_get(host_op_entry);
      // do not delete it because it shares external_id with activity samples

      gpu_trace_item_t entry_trace;
      trace_item_set(&entry_trace, activity, func_node);

      cupti_context_id_map_stream_process
	(activity->details.kernel.context_id,
	 activity->details.kernel.stream_id,
        cupti_trace_append, &entry_trace);

      gpu_activity_channel_t *channel = 
        gpu_host_correlation_map_entry_channel_get(host_op_entry);
      gpu_activity_channel_produce(channel, activity, func_node);
    }
  }

  PRINT("Kernel execution deviceId %u\n", activity->deviceId);
  PRINT("Kernel execution CorrelationId %u\n", activity->correlationId);
}


static void
gpu_synchronization_process
(
 gpu_activity_t *activity
)
{
  gpu_correlation_id_map_entry_t *cid_map_entry =
    gpu_correlation_id_map_lookup(activity->correlationId);
  if (cid_map_entry != NULL) {
    uint64_t external_id =
      gpu_correlation_id_map_entry_external_id_get(cid_map_entry);
    gpu_host_correlation_map_entry_t *host_op_entry =
      gpu_host_correlation_map_lookup(external_id);
    if (host_op_entry != NULL) {
      cct_node_t *host_op_node =
        gpu_host_correlation_map_entry_host_op_node_get(host_op_entry);
      gpu_activity_channel_t *channel =
        gpu_host_correlation_map_entry_channel_get(host_op_entry);
      gpu_activity_channel_produce(channel, activity, host_op_node);

      gpu_trace_item_t entry_trace;
      trace_item_set(&entry_trace, activity, host_op_node);

      if (activity->kind == GPU_ACTIVITY_KIND_SYNCHRONIZATION) {
	switch (activity->details.synchronization.syncKind) {
	case GPU_SYNCHRONIZATION_STREAM:
	  // Insert a event for a specific stream
	  PRINT("Add context %u stream %u sync\n", activity->contextId, activity->streamId);
	  cupti_context_id_map_stream_process(activity->contextId, activity->streamId,
					      cupti_trace_append, &entry_trace); 
	  break;
	case GPU_SYNCHRONIZATION_CONTEXT:
	  // Insert events for all current active streams
	  // TODO(Keren): What if the stream is created
	  PRINT("Add context %u sync\n", activity->contextId);
	  cupti_context_id_map_context_process(activity->contextId, 
					       cupti_trace_append, &entry_trace);
	  break;
	default:
	  // unimplemented
	  assert(0);
	}
      }
      // TODO(Keren): handle event synchronization
      //FIXME(keren): In OpenMP, an external_id may maps to multiple cct_nodes
      //gpu_host_correlation_map_delete(external_id);
    }
    gpu_correlation_id_map_delete(activity->correlationId);
  }
  PRINT("Synchronization CorrelationId %u\n", activity->correlationId);
}


static void
gpu_cdpkernel_process
(
 gpu_activity_t *activity
)
{
  gpu_correlation_id_map_entry_t *cid_map_entry =
    gpu_correlation_id_map_lookup(activity->correlationId);
  if (cid_map_entry != NULL) {
    uint64_t external_id =
      gpu_correlation_id_map_entry_external_id_get(cid_map_entry);
    gpu_host_correlation_map_entry_t *host_op_entry =
      gpu_host_correlation_map_lookup(external_id);
    if (host_op_entry != NULL) {
      cct_node_t *host_op_node =
        gpu_host_correlation_map_entry_host_op_node_get(host_op_entry);

      gpu_trace_item_t entry_trace;
      trace_item_set(&entry_trace, activity, host_op_node);

      cupti_context_id_map_stream_process
	(activity->details.cdpkernel.context_id,
	 activity->details.cdpkernel.stream_id,
	 cupti_trace_append, &entry_trace);
    }
    gpu_correlation_id_map_delete(activity->correlationId);
  }
  PRINT("Cdp Kernel CorrelationId %u\n", activity->correlationId);
}


static void
gpu_memory_process
(
 gpu_activity_t *activity
)
{
  PRINT("Memory process not implemented\n");
}


static void
gpu_instruction_process
(
 gpu_activity_t *activity
)
{
  uint32_t correlation_id = activity->correlationId;
  uint32_t pc_offset = activity->details.instruction.pcOffset;    
  gpu_correlation_id_map_entry_t *cid_map_entry =
    gpu_correlation_id_map_lookup(correlation_id);
  if (cid_map_entry != NULL) {
    uint64_t external_id =
      gpu_correlation_id_map_entry_external_id_get(cid_map_entry);
    PRINT("external_id %lu\n", external_id);
    gpu_host_correlation_map_entry_t *host_op_entry = 
      gpu_host_correlation_map_lookup(external_id);
    if (host_op_entry != NULL) {
      // Function node has the start pc of the function
      cct_node_t *func_node = 
        gpu_host_correlation_map_entry_func_node_get(host_op_entry);
      cct_addr_t *func_addr = hpcrun_cct_addr(func_node);
      ip_normalized_t ip_norm = {
        .lm_id = func_addr->ip_norm.lm_id,
        .lm_ip = func_addr->ip_norm.lm_ip + pc_offset
      };
      cct_addr_t frm = { .ip_norm = ip_norm };
      cct_node_t *cct_child = NULL;
      if ((cct_child = hpcrun_cct_insert_addr(func_node, &frm)) != NULL) {
        gpu_activity_channel_t *channel = 
          gpu_host_correlation_map_entry_channel_get(host_op_entry);
        gpu_activity_channel_produce(channel, activity, cct_child);
      }
    }
  }
  PRINT("Instruction correlationId %u\n", correlation_id);
}


//******************************************************************************
// interface operations
//******************************************************************************

void
gpu_activity_process
(
 gpu_activity_t *ga
)
{
  switch (ga->kind) {

  case GPU_ACTIVITY_KIND_PC_SAMPLING:
    gpu_sample_process(ga);
    break;

  case GPU_ACTIVITY_KIND_PC_SAMPLING_RECORD_INFO:
    gpu_sampling_record_info_process(ga);
    break;

  case GPU_ACTIVITY_KIND_EXTERNAL_CORRELATION: 
    gpu_correlation_process(ga);
    break;

  case GPU_ACTIVITY_KIND_MEMCPY:
    gpu_memcpy_process(ga);
    break;
/*
  case GPU_ACTIVITY_KIND_MEMCPY2:
    gpu_memcpy2_process(ga);
    break;
*/
  case GPU_ACTIVITY_KIND_MEMSET:
    gpu_memset_process(ga);

  case GPU_ACTIVITY_KIND_KERNEL:
    gpu_kernel_process(ga);
    break;

  case GPU_ACTIVITY_KIND_SYNCHRONIZATION:
    gpu_synchronization_process(ga);
    break;

  case GPU_ACTIVITY_KIND_MEMORY:
    gpu_memory_process(ga);
    break;

  case GPU_ACTIVITY_KIND_SHARED_ACCESS:
  case GPU_ACTIVITY_KIND_GLOBAL_ACCESS:
  case GPU_ACTIVITY_KIND_BRANCH:
    gpu_instruction_process(ga);
    break;

  case GPU_ACTIVITY_KIND_CDP_KERNEL:
     gpu_cdpkernel_process(ga);
     break;

  default:
    gpu_unknown_process(ga);
    break;
  }
}

