/*
 * Copyright (c) 2021, Jens Kuenzer
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef _VAMPIRE4SA_H_
#define _VAMPIRE4SA_H_
#include "adapter/adapter.h"

void vampire4sa_meta_init(struct generic_ctrl *ctrl_data);
void vampire4sa_init_buffer(int32_t dev_mode, struct wired_data *wired_data);
void vampire4sa_from_generic(int32_t dev_mode, struct generic_ctrl *ctrl_data, struct wired_data *wired_data);

#endif /* _VAMPIRE4SA_H_ */
