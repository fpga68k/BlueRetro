/*
 * Copyright (c) 2021, Jens Kuenzer
 * SPDX-License-Identifier: Apache-2.0
 */

#include <string.h>
#include "zephyr/types.h"
#include "tools/util.h"
#include "adapter/config.h"
#include "vampire4sa.h"

static DRAM_ATTR const uint8_t vampire4sa_mouse_axes_idx[ADAPTER_MAX_AXES] =
{
/*  AXIS_LX, AXIS_LY, AXIS_RX, AXIS_RY, TRIG_L, TRIG_R  */
    1,       0,       1,       0,       1,      0
};

static DRAM_ATTR const struct ctrl_meta vampire4sa_mouse_axes_meta[ADAPTER_MAX_AXES] =
{
    {.size_min = -128, .size_max = 127, .neutral = 0x00, .abs_max = 128},
    {.size_min = -128, .size_max = 127, .neutral = 0x00, .abs_max = 128, .polarity = 1},
    {.size_min = -128, .size_max = 127, .neutral = 0x00, .abs_max = 128},
    {.size_min = -128, .size_max = 127, .neutral = 0x00, .abs_max = 128, .polarity = 1},
    {.size_min = -128, .size_max = 127, .neutral = 0x00, .abs_max = 128},
    {.size_min = -128, .size_max = 127, .neutral = 0x00, .abs_max = 128},
};

enum { VAMPIRE4SA_unused0  //bit 0
     , VAMPIRE4SA_fire1    //bit 1
     , VAMPIRE4SA_fire2    //bit 2
     , VAMPIRE4SA_fire3    //bit 3
     , VAMPIRE4SA_fire4    //bit 4
     , VAMPIRE4SA_fire5    //bit 5
     , VAMPIRE4SA_fire6    //bit 6
     , VAMPIRE4SA_fire7    //bit 7
     , VAMPIRE4SA_fire8    //bit 8
     , VAMPIRE4SA_back     //bit 9
     , VAMPIRE4SA_start    //bit 10
     , VAMPIRE4SA_unused1  //bit 11
     , VAMPIRE4SA_U        //bit 12
     , VAMPIRE4SA_D        //bit 13
     , VAMPIRE4SA_L        //bit 14
     , VAMPIRE4SA_R        //bit 15
     };

struct vampire4sa_map {
    union {
        struct {
            uint32_t joy0_R       : 1; //bit 31
            uint32_t joy0_L       : 1; //bit 30
            uint32_t joy0_D       : 1; //bit 29
            uint32_t joy0_U       : 1; //bit 28
            uint32_t joy0_unused1 : 1; //bit 27
            uint32_t joy0_start   : 1; //bit 26
            uint32_t joy0_back    : 1; //bit 25
            uint32_t joy0_fire    : 8; //bit 24-15
            uint32_t joy0_unused0 : 1; //bit 16
            uint32_t joy1_R       : 1; //bit 15
            uint32_t joy1_L       : 1; //bit 14
            uint32_t joy1_D       : 1; //bit 13
            uint32_t joy1_U       : 1; //bit 12
            uint32_t joy1_unused1 : 1; //bit 11
            uint32_t joy1_start   : 1; //bit 10
            uint32_t joy1_back    : 1; //bit 9
            uint32_t joy1_fire    : 8; //bit 8-1
            uint32_t joy1_unused0 : 1; //bit 0
        };
        uint32_t val;
    } data0;
} __packed;

struct vampire4sa_mouse_map {
    uint8_t axes[2];
    uint8_t buttons;
    uint8_t id;
    uint8_t align[2];
    uint8_t relative[2];
    int32_t raw_axes[2];
} __packed;

static const uint32_t vampire4sa_mask[4] = {0x333F0F00, 0x00000000, 0x00000000, 0x00000000};
static const uint32_t vampire4sa_desc[4] = {0x00000000, 0x00000000, 0x00000000, 0x00000000};

static const uint32_t vampire4sa_btns_mask[2][32] = {
    {
        0, 0, 0, 0,
        0, 0, 0, 0,
        BIT(VAMPIRE4SA_L), BIT(VAMPIRE4SA_R), BIT(VAMPIRE4SA_D), BIT(VAMPIRE4SA_U),
        0, 0, 0, 0,
        BIT(VAMPIRE4SA_fire3), BIT(VAMPIRE4SA_fire2), BIT(VAMPIRE4SA_fire1), BIT(VAMPIRE4SA_fire4),
        BIT(VAMPIRE4SA_start), BIT(VAMPIRE4SA_back), 0, 0,
        BIT(VAMPIRE4SA_fire5), BIT(VAMPIRE4SA_fire6), 0, 0,
        BIT(VAMPIRE4SA_fire7), BIT(VAMPIRE4SA_fire8), 0, 0,
    },
    {
        0, 0, 0, 0,
        0, 0, 0, 0,
        BIT(16+VAMPIRE4SA_L), BIT(16+VAMPIRE4SA_R), BIT(16+VAMPIRE4SA_D), BIT(16+VAMPIRE4SA_U),
        0, 0, 0, 0,
        BIT(16+VAMPIRE4SA_fire3), BIT(16+VAMPIRE4SA_fire2), BIT(16+VAMPIRE4SA_fire1), BIT(16+VAMPIRE4SA_fire4),
        BIT(16+VAMPIRE4SA_start), BIT(16+VAMPIRE4SA_back), 0, 0,
        BIT(16+VAMPIRE4SA_fire5), BIT(16+VAMPIRE4SA_fire6), 0, 0,
        BIT(16+VAMPIRE4SA_fire7), BIT(16+VAMPIRE4SA_fire8), 0, 0,
    }
};

static const uint32_t vampire4sa_mouse_mask[4] = {0x110000F0, 0x00000000, 0x00000000, 0x00000000};
static const uint32_t vampire4sa_mouse_desc[4] = {0x000000F0, 0x00000000, 0x00000000, 0x00000000};
static const uint32_t vampire4sa_mouse_btns_mask[32] = {
    0, 0, 0, 0,
    0, 0, 0, 0,
    0, 0, 0, 0,
    0, 0, 0, 0,
    0, 0, 0, 0,
    0, 0, 0, 0,
    BIT(VAMPIRE4SA_fire1), 0, 0, 0,
    BIT(VAMPIRE4SA_fire2), 0, 0, 0,
};

void IRAM_ATTR vampire4sa_init_buffer(int32_t dev_mode, struct wired_data *wired_data) {
    switch (dev_mode) {
        case DEV_MOUSE:
        {
            struct vampire4sa_mouse_map *map = (struct vampire4sa_mouse_map *)wired_data->output;
            map->id = 0xD0;
            map->buttons = 0x00;
            for (uint32_t i = 0; i < 2; i++) {
                map->raw_axes[i] = 0;
                map->relative[i] = 1;
            }
            break;
        }
        default:
        {
            struct vampire4sa_map *map = (struct vampire4sa_map *)wired_data->output;
            map->data0.val = 0;
            break;
        }
    }
}

void vampire4sa_meta_init(struct generic_ctrl *ctrl_data) {
    memset((void *)ctrl_data, 0, sizeof(*ctrl_data)*WIRED_MAX_DEV);

    for (uint32_t i = 0; i < WIRED_MAX_DEV; i++) {
        for (uint32_t j = 0; j < ADAPTER_MAX_AXES; j++) {
            switch (config.out_cfg[i].dev_mode) {
                case DEV_MOUSE:
                    ctrl_data[i].mask = vampire4sa_mouse_mask;
                    ctrl_data[i].desc = vampire4sa_mouse_desc;
                    ctrl_data[i].axes[j].meta = &vampire4sa_mouse_axes_meta[j];
                    break;
                case DEV_PAD:
                    ctrl_data[i].mask = vampire4sa_mask;
                    ctrl_data[i].desc = vampire4sa_desc;
                    break;
                case DEV_PAD_ALT:
                    break;
                case DEV_KB:
                    break;
                default:
                    break;
            }
        }
    }
}

void vampire4sa_ctrl_from_generic(struct generic_ctrl *ctrl_data, struct wired_data *wired_data) {
    struct vampire4sa_map map_tmp;
    memcpy((void *)&map_tmp, wired_data->output, sizeof(map_tmp));

    if(ctrl_data->index<2) {
        for (uint32_t i = 0; i < ARRAY_SIZE(generic_btns_mask); i++) {
            if (ctrl_data->map_mask[0] & BIT(i)) {
                if (ctrl_data->btns[0].value & generic_btns_mask[i]) {
                    map_tmp.data0.val |= vampire4sa_btns_mask[ctrl_data->index][i];
                }
                else {
                    map_tmp.data0.val &= ~vampire4sa_btns_mask[ctrl_data->index][i];
                }
            }
        }
    }

    memcpy(wired_data->output, (void *)&map_tmp, sizeof(map_tmp));
}

static void vampire4sa_mouse_from_generic(struct generic_ctrl *ctrl_data, struct wired_data *wired_data) {
    struct vampire4sa_mouse_map map_tmp;
    int32_t *raw_axes = (int32_t *)(wired_data->output + 8);

    memcpy((void *)&map_tmp, wired_data->output, sizeof(map_tmp));

    for (uint32_t i = 0; i < ARRAY_SIZE(generic_btns_mask); i++) {
        if (ctrl_data->map_mask[0] & BIT(i)) {
            if (ctrl_data->btns[0].value & generic_btns_mask[i]) {
                map_tmp.buttons |= vampire4sa_mouse_btns_mask[i];
            }
            else {
                map_tmp.buttons &= ~vampire4sa_mouse_btns_mask[i];
            }
        }
    }

    for (uint32_t i = 2; i < 4; i++) {
        if (ctrl_data->map_mask[0] & (axis_to_btn_mask(i) & vampire4sa_mouse_desc[0])) {
            if (ctrl_data->axes[i].relative) {
                map_tmp.relative[vampire4sa_mouse_axes_idx[i]] = 1;
                atomic_add(&raw_axes[vampire4sa_mouse_axes_idx[i]], ctrl_data->axes[i].value);
            }
            else {
                map_tmp.relative[vampire4sa_mouse_axes_idx[i]] = 0;
                raw_axes[vampire4sa_mouse_axes_idx[i]] = ctrl_data->axes[i].value;
            }
        }
    }

    memcpy(wired_data->output, (void *)&map_tmp, sizeof(map_tmp) - 8);
}

void vampire4sa_from_generic(int32_t dev_mode, struct generic_ctrl *ctrl_data, struct wired_data *wired_data) {
    switch (dev_mode) {
        case DEV_MOUSE:
            vampire4sa_mouse_from_generic(ctrl_data, wired_data);
            break;
        case DEV_PAD:
            vampire4sa_ctrl_from_generic(ctrl_data, wired_data);
            break;
        case DEV_PAD_ALT:
            break;
        case DEV_KB:
            break;
        default:
            break;
    }
}
