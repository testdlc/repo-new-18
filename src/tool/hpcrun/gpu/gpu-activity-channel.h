#ifndef gpu_activity_channel_h
#define gpu_activity_channel_h

//******************************************************************************
// local includes
//******************************************************************************

#include <lib/prof-lean/bichannel.h>

#include "gpu-activity.h"


//******************************************************************************
// type declarations
//******************************************************************************

typedef struct gpu_activity_channel_t gpu_activity_channel_t; 



//******************************************************************************
// interface operations
//******************************************************************************

gpu_activity_channel_t *
gpu_activity_channel_get
(
 void
);


void
gpu_activity_channel_consume
(
 void
);



#endif
