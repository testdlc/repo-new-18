#ifndef _HPCTOOLKIT_SANITIZER_CONTEXT_MAP_H_
#define _HPCTOOLKIT_SANITIZER_CONTEXT_MAP_H_

/******************************************************************************
 * system includes
 *****************************************************************************/

#include <stdint.h>

/******************************************************************************
 * local includes
 *****************************************************************************/

#include <cuda.h>

#include "cstack.h"
#include "sanitizer-record.h"

/******************************************************************************
 * type definitions 
 *****************************************************************************/

typedef struct sanitizer_context_map_entry_s sanitizer_context_map_entry_t;

/******************************************************************************
 * interface operations
 *****************************************************************************/

sanitizer_context_map_entry_t *
sanitizer_context_map_lookup
(
 CUcontext context
);


void
sanitizer_context_map_init
(
 CUcontext context,
 CUstream priority_stream
);


void
sanitizer_context_map_insert
(
 CUcontext context,
 CUstream stream,
 cstack_node_t *notification
);


void
sanitizer_context_map_process
(
 sanitizer_record_fn_t fn
);


void
sanitizer_context_map_context_process
(
 CUcontext context,
 sanitizer_record_fn_t fn
);


void
sanitizer_context_map_stream_process
(
 CUcontext context,
 CUstream stream,
 sanitizer_record_fn_t fn
);


void
sanitizer_context_map_stream_lock
(
 CUcontext context,
 CUstream stream
);


void
sanitizer_context_map_stream_unlock
(
 CUcontext context,
 CUstream stream
);


void
sanitizer_context_map_delete
(
 CUcontext context
);


CUstream
sanitizer_context_map_entry_priority_stream_get
(
 sanitizer_context_map_entry_t *entry
);

#endif
