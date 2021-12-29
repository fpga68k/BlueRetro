/*
 * Copyright (c) 2021, Jens Kuenzer
 * SPDX-License-Identifier: Apache-2.0
 */

#include <string.h>
#include <driver/periph_ctrl.h>
#include <soc/spi_periph.h>
#include <esp32/rom/ets_sys.h>
#include <esp32/rom/gpio.h>
#include "hal/clk_gate_ll.h"
#include "driver/gpio.h"
#include "system/intr.h"
#include "system/gpio.h"
#include "system/delay.h"
#include "zephyr/atomic.h"
#include "zephyr/types.h"
#include "tools/util.h"
#include "adapter/adapter.h"
#include "adapter/config.h"
#include "vampire4sa.h"

#define SPI_LL_UNUSED_INT_MASK  

#define SPI_HW                 SPI2

#define SPI2_HSPI_INTR_NUM     19

#define vampire4sa_cs_pin      15
#define vampire4sa_clk_pin     14
#define vampire4sa_mosi_pin    12
#define vampire4sa_miso_pin    13
#define vampire4sa_cs_sig      HSPICS0_IN_IDX
#define vampire4sa_clk_sig     HSPICLK_IN_IDX
#define vampire4sa_mosi_sig    HSPIQ_OUT_IDX
#define vampire4sa_miso_sig    HSPID_IN_IDX

static inline void load_mouse_axes(uint8_t port, uint8_t *axes) {
    uint8_t *relative = (uint8_t *)(wired_adapter.data[port].output + 6);
    int32_t *raw_axes = (int32_t *)(wired_adapter.data[port].output + 8);
    int32_t val = 0;

    for (uint32_t i = 0; i < 2; i++) {
        if (relative[i]) {
            val = atomic_clear(&raw_axes[i]);
        }
        else {
            val = raw_axes[i];
        }

        if (val > 127) {
            axes[i] = 127;
        }
        else if (val < -128) {
            axes[i] = -128;
        }
        else {
            axes[i] = (uint8_t)val;
        }
    }
}

static uint32_t latch_isr(uint32_t cause) {
    switch (config.out_cfg[0].dev_mode) {
        case DEV_PAD: {
                SPI_HW.addr=0x60 << 24; // cmd 60 = hid joy
              //SPI_HW.mosi_dlen.usr_mosi_dbitlen = 4*8-1;
                memcpy((void *)&SPI_HW.data_buf[0], (void *)&wired_adapter.data[0].output, 4);
            } break;
        //case DEV_MOUSE: {
        //    uint32_t tmp;
        //    load_mouse_axes(0, &tmp);
        //    SPI_HW.addr=0x68 << 24; // cmd 68 = hid mouse
        //    SPI_HW.data_buf[0] = tmp;
        //    } break;
    }
    SPI_HW.cmd.usr = 1;  //start
    return 0;
}

void vampire4sa_init(void) {
    //gpio_config_t io_conf = {0};

    /* CSn */
    //io_conf.mode = GPIO_MODE_OUTPUT;
    //io_conf.intr_type = GPIO_PIN_INTR_DISABLE; // GPIO_PIN_INTR_NEGEDGE;
    //io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
    //io_conf.pull_up_en = GPIO_PULLUP_ENABLE;
    //io_conf.pin_bit_mask = 1ULL << vampire4sa_cs_pin;
    //gpio_config_iram(&io_conf);
    //gpio_matrix_in(vampire4sa_cs_pin, vampire4sa_cs_sig, true); // Invert latch to use as CS
    PIN_FUNC_SELECT(GPIO_PIN_MUX_REG_IRAM[vampire4sa_cs_pin], FUNC_MTDO_HSPICS0);

    /* MOSI */
    //gpio_set_level_iram(vampire4sa_mosi_pin, 1);
    //gpio_set_direction_iram(vampire4sa_mosi_pin, GPIO_MODE_OUTPUT);
    //gpio_matrix_out(vampire4sa_mosi_pin, vampire4sa_mosi_sig, true, false); // data is inverted
    PIN_FUNC_SELECT(GPIO_PIN_MUX_REG_IRAM[vampire4sa_mosi_pin], FUNC_MTDI_HSPIQ);


    /* MISO */
    //gpio_set_level_iram(vampire4sa_miso_pin, 1);
    //gpio_set_direction_iram(vampire4sa_miso_pin, GPIO_MODE_INPUT);
    //gpio_matrix_out(vampire4sa_miso_pin, vampire4sa_miso_sig, true, false); // data is inverted
    PIN_FUNC_SELECT(GPIO_PIN_MUX_REG_IRAM[vampire4sa_miso_pin], FUNC_MTCK_HSPID);

    /* Clock */
    //io_conf.mode = GPIO_MODE_OUTPUT;
    //io_conf.intr_type = GPIO_PIN_INTR_DISABLE;
    //io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
    //io_conf.pull_up_en = GPIO_PULLUP_ENABLE;
    //io_conf.pin_bit_mask = 1ULL << vampire4sa_clk_pin;
    //gpio_config_iram(&io_conf);
    PIN_FUNC_SELECT(GPIO_PIN_MUX_REG_IRAM[vampire4sa_clk_pin], FUNC_MTMS_HSPICLK);
    //gpio_matrix_in(vampire4sa_clk_pin, vampire4sa_clk_sig, false);

    periph_ll_enable_clk_clear_rst(PERIPH_HSPI_MODULE);

    SPI_HW.clock.val = 0;
    SPI_HW.user.val = 0;
    SPI_HW.ctrl.val = 0;

    SPI_HW.slave.slave_mode = 0;   // 0 -> master mode

    const int n=4;
    const int pre=2;
    //const spi_freq = APB_CLK_FREQ/pre/n;

    SPI_HW.clock.clk_equ_sysclk=0;
    SPI_HW.clock.clkdiv_pre=pre-1;
    SPI_HW.clock.clkcnt_n=n-1;
    SPI_HW.clock.clkcnt_h=(n>>1)-1;
    SPI_HW.clock.clkcnt_l=n-1;

    /* 68k is MSB first */
    SPI_HW.ctrl.wr_bit_order = 0;
    SPI_HW.ctrl.rd_bit_order = 0;
    SPI_HW.user.rd_byte_order = 1; //big endian
    SPI_HW.user.wr_byte_order = 1; //big endian

    /* mode2 - see 7.4.1 GP-SPI Clock Polarity (CPOL) and Clock Phase (CPHA) */
    SPI_HW.pin.ck_idle_edge = 0;
    SPI_HW.user.ck_i_edge = 1;
    SPI_HW.user.ck_out_edge = 0;
    SPI_HW.ctrl2.miso_delay_mode = 0;
    SPI_HW.ctrl2.miso_delay_num = 0;
    SPI_HW.ctrl2.mosi_delay_mode = 0;
    SPI_HW.ctrl2.mosi_delay_num = 0;

    SPI_HW.user.doutdin = 0;       // 0 -> disable full-duplex communication
    SPI_HW.user.sio = 0;           // 0 -> disable three-line half-duplex communication
    SPI_HW.user.usr_command = 0;
    SPI_HW.user.usr_addr = 1;
    SPI_HW.user.usr_dummy = 0;
    SPI_HW.user.usr_miso = 0;
    SPI_HW.user.usr_mosi = 1;

    SPI_HW.user1.usr_addr_bitlen=8-1;
    SPI_HW.addr=0x60 << 24; // cmd 60 = hid joy

    SPI_HW.miso_dlen.usr_miso_dbitlen=40-1;
    SPI_HW.mosi_dlen.usr_mosi_dbitlen=32-1;
    SPI_HW.data_buf[0]=0x00000000;

    intexc_alloc_iram(ETS_SPI2_INTR_SOURCE, SPI2_HSPI_INTR_NUM, latch_isr);
    SPI_HW.slave.trans_inten = 1;

    SPI_HW.cmd.usr = 1;

}
