#include "hw_link_ctrl_protocol.h"

#define HW_LCP_START_FLAG        (0x7c)
#define HW_LCP_END_FLAG          (0x7e)
#define HW_LCP_PADDING           (0xff)
#define MAX_PAYLOAD_LEN          (512)

u8 *hw_frame_assemble(u8 *buff, int *buff_len)
{
    static u8 assemble_buff[MAX_PAYLOAD_LEN];

    if (!buff_len || *buff_len < 1 || !buff)
    {
        ERROR_PRINT("!buff_len || buff_len < 0 || !buff\n");
        return NULL;
    }
    memset(assemble_buff, 0x0, sizeof(assemble_buff));

    /* Check FRAME_START FLAG */
    assemble_buff[HW_LCP_START_FLAG_FIELD]   = HW_LCP_START_FLAG;
    assemble_buff[HW_LCP_PADDING_FIELD]      = HW_LCP_PADDING;
    assemble_buff[PAYLOAD_LEN_FIELD1]        = (u8)(*buff_len & 0xFF);         // Lower byte
    assemble_buff[PAYLOAD_LEN_FIELD2]        = (u8)((*buff_len >> 8) & 0xFF);  // Upper byte
    memcpy(assemble_buff+ PAYLOAD_FIELD, buff, *buff_len);
    assemble_buff[PAYLOAD_FIELD + *buff_len] = HW_LCP_END_FLAG;

    *buff_len = *buff_len + (HW_LCP_OVERHEAD -1);

    return assemble_buff;
}

int is_valid_hw_frame(u8 *buff)
{
    int payload_len = 0;
    int end_flag = 0;

    if (!buff)
    {
        ERROR_PRINT("!buff\n");
        return 0;
    }

    /* Check FRAME_START FLAG */
    if (buff[HW_LCP_START_FLAG_FIELD] != HW_LCP_START_FLAG)
    {
#if (0)
        ERROR_PRINT("buff[%d] != HW_LCP_START_FLAG\n", HW_LCP_START_FLAG_FIELD);
#endif
        return 0;
    }

    /* Check padding */
    if (buff[HW_LCP_PADDING_FIELD] != HW_LCP_PADDING)
    {
        ERROR_PRINT("buff != HW_LCP_PADDING_FIELD\n");
        return 0;
    }

    /* Parse data length as Little Endian */
    payload_len = buff[PAYLOAD_LEN_FIELD1] | (buff[PAYLOAD_LEN_FIELD2] << 8);
    if ((payload_len < 0) || (payload_len > MAX_PAYLOAD_LEN))
    {
        ERROR_PRINT("Invalid payload len [%d]\n", payload_len);
        return 0;
    }
              
    /* Check data end flag */
    end_flag = PAYLOAD_FIELD + payload_len;
    if (buff[end_flag] != HW_LCP_END_FLAG)
    {
        ERROR_PRINT("buff[%d] != HW_LCP_END_FLAG = [%u]\n", end_flag, buff[end_flag]);
        return 0;
    }
    
    return payload_len;
}
