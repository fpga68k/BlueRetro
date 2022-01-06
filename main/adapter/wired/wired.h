/*
 * Copyright (c) 2021, Jacques Gagnon
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef _WIRED_H_
#define _WIRED_H_
#include "adapter/adapter.h"

void wired_meta_init(struct generic_ctrl *ctrl_data);
void wired_init_buffer(int32_t dev_mode, struct wired_data *wired_data);
void wired_from_generic(int32_t dev_mode, struct generic_ctrl *ctrl_data, struct wired_data *wired_data);
void wired_fb_to_generic(int32_t dev_mode, struct raw_fb *raw_fb_data, struct generic_fb *fb_data);

#endif /* _WIRED_H_ */
