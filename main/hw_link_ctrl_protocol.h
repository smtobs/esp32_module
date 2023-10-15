#ifndef _HW_LINK_CTRL_PROTOCOL_H
#define _HW_LINK_CTRL_PROTOCOL_H

#include "utils.h"

enum hw_link_ctl_protocol_frame
{
    HW_LCP_START_FLAG_FIELD = 0,
    HW_LCP_PADDING_FIELD,
    PAYLOAD_LEN_FIELD1,
    PAYLOAD_LEN_FIELD2,
    PAYLOAD_FIELD, //4
    HW_LCP_NOT_USED, // Not Used
    HW_LCP_OVERHEAD,
};

u8 *hw_frame_assemble(u8 *, int *);
int is_valid_hw_frame(u8 *);

#endif
