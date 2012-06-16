/*
 * HTC mode Server Header
 *
 * Copyright (C) 2011 HTC Corporation
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */
#ifndef _HTC_MODE_SERVER_H_
#define _HTC_MODE_SERVER_H_

#include <linux/completion.h>

#define PRODUCT_NAME_MAX			64
#define CAR_MODEL_NAME_MAX			64
#define NONCE_SIZE				5

#define HTC_MODE_CONTROL_REQ		0x12

#define CLIENT_INFO_SERVER_ROTATE_USED	(1 << 1)

#define PIXEL_FORMAT_RGB565					(1 << 0)

#define CTRL_CONF_TOUCH_EVENT_SUPPORTED		(1 << 0)
#define CTRL_CONF_NUM_SIMULTANEOUS_TOUCH	(1 << 1)

enum {
	CLIENT_INFO_MESGID = 0,
	SERVER_INFO_MESGID,
	TOUCH_EVENT_MESGID,
	AUTH_SERVER_NONCE_MESGID,
	AUTH_CLIENT_NONCE_MESGID,
	AUTH_RESPONSE_MESGID,
	OBU_INFO_MESGID
};

struct tegra_client_info {
	u8 mesg_id;
	u16 width;
	u16 height;
	u32 display_conf;
	u32 pixel_format;
	u32 ctrl_conf;
} __attribute__ ((__packed__));

struct tegra_server_info {
	u8 mesg_id;
	u16 width;
	u16 height;
	u32 pixel_format;
	u32 ctrl_conf;
} __attribute__ ((__packed__));


struct htcmode_protocol {
	u16 version;
	struct tegra_client_info client_info;
	struct tegra_server_info server_info;
	u8 product_name[PRODUCT_NAME_MAX];
	u8 car_model[CAR_MODEL_NAME_MAX];
};


extern int tegrafb_get_var(struct tegra_fb_info *tmp);
extern void tegrafb_set_var(unsigned char *addr, int area);
extern int tegrafb_get_fb_area(void);

#endif /* _HTC_MODE_SERVER_H_ */
