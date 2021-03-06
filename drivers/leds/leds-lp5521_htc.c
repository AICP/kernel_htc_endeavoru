/* driver/leds/leds-lp5521_htc.c
 *
 * Copyright (C) 2010 HTC Corporation.
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

#include <linux/kernel.h>
#include <linux/i2c.h>
#include <linux/gpio.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/input.h>
#include <linux/platform_device.h>
#include <linux/hrtimer.h>
#include <linux/interrupt.h>
#include <linux/android_alarm.h>
#include <linux/earlysuspend.h>
#include <linux/leds.h>
#include <linux/leds-lp5521_htc.h>
#include <linux/regulator/consumer.h>

#define LP5521_MAX_LEDS			3	/* Maximum number of LEDs */
#define LED_DEBUG				0
#if LED_DEBUG
	#define D(x...) pr_info("[LED]" x)
#else
	#define D(x...)
#endif

#define I(x...) pr_info("[LED]" x)
#define E(x...) pr_err("[LED]" x)

static int led_rw_delay;
static int current_state, current_blink, current_time;
static int current_currents = 0, current_lut_coefficient, current_pwm_coefficient;
static int current_mode, backlight_mode, suspend_mode, offtimer_mode;
static int amber_mode, button_brightness, slow_blink_brightness, slow_blink_brightness_limit = 1;
static int button_brightness_board;
static struct regulator *regulator;
static struct i2c_client *private_lp5521_client;
static struct mutex	led_mutex;
static struct workqueue_struct *g_led_work_queue;
static struct work_struct led_powerkey_work;
static struct workqueue_struct *led_powerkey_work_queue;

typedef struct {
	struct work_struct fade_work;
	int fade_mode;
	struct i2c_client *client;
} button_fade_work_t;
static button_fade_work_t button_fade_work;


struct lp5521_led {
	int			id;
	u8			chan_nr;
	u8			led_current;
	u8			max_current;
	struct led_classdev	cdev;
	struct work_struct	brightness_work;
	struct mutex led_data_mutex;
	u8			brightness;
	uint8_t 	blink;
	struct alarm led_alarm;
	struct work_struct led_work;
};

struct lp5521_chip {
	struct led_i2c_platform_data *pdata;
	struct mutex		led_i2c_rw_mutex; /* Serialize control */
	struct i2c_client	*client;
	struct lp5521_led	leds[LP5521_MAX_LEDS];
	struct early_suspend early_suspend_led;
};

static long unsigned int lp5521_led_tag_status = 0;
static int __init lp5521_led_tag(char *tag)
{
    int rc = 0;
	if (strlen(tag))
		rc = strict_strtoul(tag, 16, &lp5521_led_tag_status);
	/* mapping */
	if (lp5521_led_tag_status == 2)
		lp5521_led_tag_status = DUAL_COLOR_BLINK;
	else if(lp5521_led_tag_status == 3)
		lp5521_led_tag_status = GREEN_ON;
	else if(lp5521_led_tag_status == 4)
		lp5521_led_tag_status = AMBER_ON;
	else if(lp5521_led_tag_status == 5)
		lp5521_led_tag_status = AMBER_BLINK;
	else if(lp5521_led_tag_status == 6)
		lp5521_led_tag_status = AMBER_LOW_BLINK;
	else
		lp5521_led_tag_status = 0;

	return 1;
}
 __setup("led=", lp5521_led_tag);

char *hex2string(uint8_t *data, int len)
{
	static char buf[LED_I2C_WRITE_BLOCK_SIZE*4];
	int i;

	i = (sizeof(buf) - 1) / 4;
	if (len > i)
		len = i;

	for (i = 0; i < len; i++)
		sprintf(buf + i * 4, "[%02X]", data[i]);

	return buf;
}

static int i2c_write_block(struct i2c_client *client, uint8_t addr,
	uint8_t *data, int length)
{
	int retry;
	uint8_t buf[LED_I2C_WRITE_BLOCK_SIZE];
	int i;
	struct lp5521_chip *cdata;
	struct i2c_msg msg[] = {
		{
			.addr = client->addr,
			.flags = 0,
			.len = length + 1,
			.buf = buf,
		}
	};

	D("W [%02X] = %s\n",
			addr, hex2string(data, length));

	cdata = i2c_get_clientdata(client);
	if (length + 1 > LED_I2C_WRITE_BLOCK_SIZE) {
		E("i2c_write_block length too long\n");
		return -E2BIG;
	}

	buf[0] = addr;
	for (i = 0; i < length; i++)
		buf[i+1] = data[i];

	mutex_lock(&cdata->led_i2c_rw_mutex);
	msleep(1);
	for (retry = 0; retry < I2C_WRITE_RETRY_TIMES; retry++) {
		if (i2c_transfer(client->adapter, msg, 1) == 1)
			break;
		msleep(led_rw_delay);
	}
	if (retry >= I2C_WRITE_RETRY_TIMES) {
		E("i2c_write_block retry over %d times\n",
			I2C_WRITE_RETRY_TIMES);
		mutex_unlock(&cdata->led_i2c_rw_mutex);
		return -EIO;
	}
	mutex_unlock(&cdata->led_i2c_rw_mutex);

	return 0;
}

static void lp5521_led_enable(struct i2c_client *client)
{
	int ret = 0;
	uint8_t data;
	struct led_i2c_platform_data *pdata;

	D("%s\n" , __func__);

	pdata = client->dev.platform_data;
	/* === led pin enable === */
	ret = gpio_direction_output(pdata->ena_gpio, 1);
	if (ret < 0) {
		E("%s: gpio_direction_output high failed %d\n", __func__, ret);
		gpio_free(pdata->ena_gpio);
	}
	mutex_lock(&led_mutex);
	/* === enable CHIP_EN === */
	data = 0x40;
	ret = i2c_write_block(client, 0x00, &data, 1);
	udelay(550);
	/* === configuration control in power save mode=== */
	data = 0x29;
	ret = i2c_write_block(client, 0x08, &data, 1);
	/* === set red current mA === */
	data = (u8)pdata->led_config[0].led_cur;
	ret = i2c_write_block(client, 0x05, &data, 1);
	/* === set green current mA === */
	data = (u8)pdata->led_config[1].led_cur;
	ret = i2c_write_block(client, 0x06, &data, 1);
	/* === set blue current mA === */
	data = (u8)pdata->led_config[2].led_cur;
	ret = i2c_write_block(client, 0x07, &data, 1);
	/* === set blue channel to direct PWM control mode === */
	data = 0x03;
	ret = i2c_write_block(client, 0x01, &data, 1);
	mutex_unlock(&led_mutex);
}

static void lp5521_green_on(struct i2c_client *client)
{
	uint8_t data = 0x00;
	int ret;
	struct led_i2c_platform_data *pdata;

	D("%s\n" , __func__);
	pdata = client->dev.platform_data;
	if( current_mode == 0 && backlight_mode == 0 )
		lp5521_led_enable(client);
	current_mode = 1;
	mutex_lock(&led_mutex);
	/* === set green pwm === */
	data = (u8)pdata->led_config[1].led_lux * 255 / 100;
	ret = i2c_write_block(client, 0x03, &data, 1);

	/* === run program with green direct control and blue direct program === */
	if ( backlight_mode >= 2 )
		data = 0x0e;
	else
		data = 0x0f;
	ret = i2c_write_block(client, 0x01, &data, 1);
	udelay(200);
	if ( backlight_mode >= 2 )
		data = 0x42;
	else
		data = 0x40;
	ret = i2c_write_block(client, 0x00, &data, 1);
	udelay(500);
	mutex_unlock(&led_mutex);
}

static void lp5521_green_blink(struct i2c_client *client)
{
	uint8_t data = 0x00;
	int ret;
	struct led_i2c_platform_data *pdata;

	D("%s\n" , __func__);
	pdata = client->dev.platform_data;
	if( current_mode == 0 && backlight_mode == 0 )
		lp5521_led_enable(client);
	current_mode = 2;
	mutex_lock(&led_mutex);
	/* === load program with green load program and blue direct program === */
	if ( backlight_mode >= 2 )
		data = 0x06;
	else
		data = 0x07;
	ret = i2c_write_block(client, 0x01, &data, 1);
	udelay(200);

	/* === function blink === */
	/* === set pwm === */
	data = 0x40;
	ret = i2c_write_block(client, 0x30, &data, 1);
	data = (u8)pdata->led_config[1].led_lux * 255 / 100;
	ret = i2c_write_block(client, 0x31, &data, 1);
	/* === wait 0.064s === */
	data = 0x44;
	ret = i2c_write_block(client, 0x32, &data, 1);
	data = 0x00;
	ret = i2c_write_block(client, 0x33, &data, 1);
	/* === set pwm to 0 === */
	data = 0x40;
	ret = i2c_write_block(client, 0x34, &data, 1);
	data = 0x00;
	ret = i2c_write_block(client, 0x35, &data, 1);
	/* === wait 1.936s === */
	data = 0x7c;
	ret = i2c_write_block(client, 0x36, &data, 1);
	data = 0x00;
	ret = i2c_write_block(client, 0x37, &data, 1);
	data = 0x7f;
	ret = i2c_write_block(client, 0x38, &data, 1);
	data = 0x00;
	ret = i2c_write_block(client, 0x39, &data, 1);
	/* === clear register === */
	data = 0x00;
	ret = i2c_write_block(client, 0x3a, &data, 1);
	data = 0x00;
	ret = i2c_write_block(client, 0x3b, &data, 1);
	data = 0x00;
	ret = i2c_write_block(client, 0x3c, &data, 1);
	data = 0x00;
	ret = i2c_write_block(client, 0x3d, &data, 1);

	/* === run program === */
	if ( backlight_mode >= 2 )
		data = 0x0a;
	else
		data = 0x0b;
	ret = i2c_write_block(client, 0x01, &data, 1);
	udelay(200);
	if ( backlight_mode >= 2 )
		data = 0x4a;
	else
		data = 0x48;
	ret = i2c_write_block(client, 0x00, &data, 1);
	udelay(500);
	mutex_unlock(&led_mutex);
}

static void lp5521_amber_on(struct i2c_client *client)
{
	uint8_t data = 0x00;
	int ret;
	struct led_i2c_platform_data *pdata;

	D("%s\n" , __func__);
	pdata = client->dev.platform_data;
	if( current_mode == 0 && backlight_mode == 0 )
		lp5521_led_enable(client);
	current_mode = 3;
	mutex_lock(&led_mutex);
	/* === set amber pwm === */
	data = (u8)pdata->led_config[0].led_lux * 255 / 100;
	ret = i2c_write_block(client, 0x02, &data, 1);

	/* === run program with amber direct control and blue direct program === */
	if ( backlight_mode >= 2 )
		data = 0x32;
	else
		data = 0x33;
	ret = i2c_write_block(client, 0x01, &data, 1);
	udelay(200);
	if ( backlight_mode >= 2 )
		data = 0x42;
	else
		data = 0x40;
	ret = i2c_write_block(client, 0x00, &data, 1);
	udelay(500);
	mutex_unlock(&led_mutex);
}

static void lp5521_amber_blink(struct i2c_client *client)
{
	uint8_t data = 0x00;
	int ret;
	struct led_i2c_platform_data *pdata;

	D("%s\n" , __func__);
	pdata = client->dev.platform_data;
	if( current_mode == 0 && backlight_mode == 0 )
		lp5521_led_enable(client);
	current_mode = 4;
	amber_mode = 2;
	mutex_lock(&led_mutex);
	/* === load program with amber load program and blue direct program === */
	if ( backlight_mode >= 2 )
		data = 0x12;
	else
		data = 0x13;
	ret = i2c_write_block(client, 0x01, &data, 1);
	udelay(200);

	/* === function blink === */
	/* === wait 0.999s === */
	data = 0x7f;
	ret = i2c_write_block(client, 0x10, &data, 1);
	data = 0x00;
	ret = i2c_write_block(client, 0x11, &data, 1);
	/* === set pwm === */
	data = 0x40;
	ret = i2c_write_block(client, 0x12, &data, 1);
	data = (u8)pdata->led_config[0].led_lux * 255 / 100;
	ret = i2c_write_block(client, 0x13, &data, 1);
	/* === wait 0.064s === */
	data = 0x44;
	ret = i2c_write_block(client, 0x14, &data, 1);
	data = 0x00;
	ret = i2c_write_block(client, 0x15, &data, 1);
	/* === set pwm to 0 === */
	data = 0x40;
	ret = i2c_write_block(client, 0x16, &data, 1);
	data = 0x00;
	ret = i2c_write_block(client, 0x17, &data, 1);
	/* === wait 0.935s === */
	data = 0x7c;
	ret = i2c_write_block(client, 0x18, &data, 1);
	data = 0x00;
	ret = i2c_write_block(client, 0x19, &data, 1);

	/* === run program === */
	if ( backlight_mode >= 2 )
		data = 0x22;
	else
		data = 0x23;
	ret = i2c_write_block(client, 0x01, &data, 1);
	udelay(200);
	if ( backlight_mode >= 2 )
		data = 0x62;
	else
		data = 0x60;
	ret = i2c_write_block(client, 0x00, &data, 1);
	udelay(550);
	mutex_unlock(&led_mutex);
}

static void lp5521_amber_low_blink(struct i2c_client *client)
{
	uint8_t data = 0x00;
	int ret;
	struct led_i2c_platform_data *pdata;

	D("%s\n" , __func__);
	pdata = client->dev.platform_data;
	if( current_mode == 0 && backlight_mode == 0 )
		lp5521_led_enable(client);
	current_mode = 5;
	mutex_lock(&led_mutex);
	/* === load program with amber load program and blue direct program === */
	if ( backlight_mode >= 2 )
		data = 0x12;
	else
		data = 0x13;
	ret = i2c_write_block(client, 0x01, &data, 1);
	udelay(200);

	/* === function low blink === */
	/* === set pwm === */
	data = 0x40;
	ret = i2c_write_block(client, 0x10, &data, 1);
	data = (u8)pdata->led_config[0].led_lux * 255 / 100;
	ret = i2c_write_block(client, 0x11, &data, 1);
	/* === wait 0.999s === */
	data = 0x7f;
	ret = i2c_write_block(client, 0x12, &data, 1);
	data = 0x00;
	ret = i2c_write_block(client, 0x13, &data, 1);
	/* === set pwm to 0 === */
	data = 0x40;
	ret = i2c_write_block(client, 0x14, &data, 1);
	data = 0x00;
	ret = i2c_write_block(client, 0x15, &data, 1);
	/* === wait 0.999s === */
	data = 0x7f;
	ret = i2c_write_block(client, 0x16, &data, 1);
	/* === clear register === */
	data = 0x00;
	ret = i2c_write_block(client, 0x17, &data, 1);
	data = 0x00;
	ret = i2c_write_block(client, 0x18, &data, 1);
	data = 0x00;
	ret = i2c_write_block(client, 0x19, &data, 1);

	/* === run program === */
	if ( backlight_mode >= 2 )
		data = 0x22;
	else
		data = 0x23;
	ret = i2c_write_block(client, 0x01, &data, 1);
	udelay(200);
	if ( backlight_mode >= 2 )
		data = 0x62;
	else
		data = 0x60;
	ret = i2c_write_block(client, 0x00, &data, 1);
	udelay(550);
	mutex_unlock(&led_mutex);
}

static void lp5521_dual_color_blink(struct i2c_client *client)
{
	uint8_t data = 0x00;
	int ret;
	struct led_i2c_platform_data *pdata;

	D("%s\n" , __func__);
	pdata = client->dev.platform_data;
	if( current_mode == 0 && backlight_mode == 0 )
		lp5521_led_enable(client);
	current_mode = 6;
	mutex_lock(&led_mutex);
	/* === load program to with amber/green load program and blue direct program === */
	if ( backlight_mode >= 2 )
		data = 0x16;
	else
		data = 0x17;
	ret = i2c_write_block(client, 0x01, &data, 1);
	udelay(200);


	/* === function fast blink === */
	/* === set pwm === */
	data = 0x40;
	ret = i2c_write_block(client, 0x10, &data, 1);
	data = (u8)pdata->led_config[0].led_lux * 255 / 100;
	ret = i2c_write_block(client, 0x11, &data, 1);
	/* === wait 0.064s === */
	data = 0x44;
	ret = i2c_write_block(client, 0x12, &data, 1);
	data = 0x00;
	ret = i2c_write_block(client, 0x13, &data, 1);
	/* === set pwm to 0 === */
	data = 0x40;
	ret = i2c_write_block(client, 0x14, &data, 1);
	data = 0x00;
	ret = i2c_write_block(client, 0x15, &data, 1);
	/* === wait 0.25s === */
	data = 0x50;
	ret = i2c_write_block(client, 0x16, &data, 1);
	data = 0x00;
	ret = i2c_write_block(client, 0x17, &data, 1);
	/* === trigger sg, wg === */
	data = 0xe1;
	ret = i2c_write_block(client, 0x18, &data, 1);
	data = 0x04;
	ret = i2c_write_block(client, 0x19, &data, 1);
	udelay(550);

	/* === trigger wr === */
	data = 0xe0;
	ret = i2c_write_block(client, 0x30, &data, 1);
	data = 0x80;
	ret = i2c_write_block(client, 0x31, &data, 1);
	udelay(550);
	/* === set pwm === */
	data = 0x40;
	ret = i2c_write_block(client, 0x32, &data, 1);
	data = (u8)pdata->led_config[1].led_lux * 255 / 100;
	ret = i2c_write_block(client, 0x33, &data, 1);
	/* === wait 0.064s === */
	data = 0x44;
	ret = i2c_write_block(client, 0x34, &data, 1);
	data = 0x00;
	ret = i2c_write_block(client, 0x35, &data, 1);
	/* === set pwm to 0 === */
	data = 0x40;
	ret = i2c_write_block(client, 0x36, &data, 1);
	data = 0x00;
	ret = i2c_write_block(client, 0x37, &data, 1);
	/* === wait 0.999s === */
	data = 0x7f;
	ret = i2c_write_block(client, 0x38, &data, 1);
	data = 0x00;
	ret = i2c_write_block(client, 0x39, &data, 1);
	/* === wait 0.622s === */
	data = 0x68;
	ret = i2c_write_block(client, 0x3a, &data, 1);
	data = 0x00;
	ret = i2c_write_block(client, 0x3b, &data, 1);
	/* === trigger sr === */
	data = 0xe0;
	ret = i2c_write_block(client, 0x3c, &data, 1);
	data = 0x02;
	ret = i2c_write_block(client, 0x3d, &data, 1);
	udelay(550);

	/* === run program === */
	if ( backlight_mode >= 2 )
		data = 0x2a;
	else
		data = 0x2b;
	ret = i2c_write_block(client, 0x01, &data, 1);
	udelay(200);
	if ( backlight_mode >= 2 )
		data = 0x6a;
	else
		data = 0x68;
	ret = i2c_write_block(client, 0x00, &data, 1);
	udelay(550);
	mutex_unlock(&led_mutex);
}

static inline int button_brightness_adjust(struct i2c_client *client) {
	uint8_t data = 0x00;
	int ret = 0, brightness;
	D("%s, current_mode: %d, backlight_mode: %d\n", __func__, current_mode, backlight_mode);

	// if buttons are not on do nothing
	if (current_mode == 0 && backlight_mode == 0)
		return ret;
		
	brightness = button_brightness;
	mutex_lock(&led_mutex);

	data = (u8)brightness;
	ret = i2c_write_block(client, 0x04, &data, 1);

	mutex_unlock(&led_mutex);
	return ret;
}

static inline int button_fade_in(struct i2c_client *client) {
	uint8_t data = 0x00;
	int i, ret, brightness;
	D("%s, current_mode: %d, backlight_mode: %d\n", __func__, current_mode, backlight_mode);

	if (current_mode == 0 && backlight_mode == 0)
		lp5521_led_enable(client);
	brightness = button_brightness/5;
	backlight_mode = 1;
	mutex_lock(&led_mutex);

	/* === set blue pwm to 255 === */
	for (i=1;i<=5;i++) {
		data = (u8)i*brightness;
		ret = i2c_write_block(client, 0x04, &data, 1);
		if (ret < 0)
			break;
		msleep(25);
	}
	mutex_unlock(&led_mutex);
	return ret;
}

static inline int button_fade_out(struct i2c_client *client) {
	uint8_t data = 0x00;
	int i, ret, brightness;
	struct led_i2c_platform_data *pdata;

	D("%s, current_mode: %d, backlight_mode: %d\n", __func__, current_mode, backlight_mode);
	pdata = client->dev.platform_data;
	brightness = button_brightness/5;
	backlight_mode = 0;
	mutex_lock(&led_mutex);

	/* === set blue pwm to 0 === */
	for (i=4;i>=0;i--) {
		data = (u8)i*brightness;
		ret = i2c_write_block(client, 0x04, &data, 1);
		if (ret < 0)
			break;
		msleep(25);
	}

	if( current_mode == 0 ) {
		if( suspend_mode == 1 ) {
			/* === reset register === */
			data = 0xff;
			ret = i2c_write_block(client, 0x0d, &data, 1);
			udelay(550);
			gpio_direction_output(pdata->ena_gpio, 0);
			D("no LED command now in suspend, reset chip & gpio, no ack in i2c 0x32 is correct in LED chip lp5521.\n");
		} else {
			/* === disable CHIP_EN === */
			data = 0x00;
			ret = i2c_write_block(client, 0x00, &data, 1);
			udelay(550);
			gpio_direction_output(pdata->ena_gpio, 0);
			D("no LED command now, disable chip & gpio.\n");
		}
	}
	mutex_unlock(&led_mutex);
	return ret;
}

static void button_fade_work_func(struct work_struct *work)
{
	button_fade_work_t *fade_work;
	int ret;
	struct i2c_client *client;

	D(" %s\n", __func__);

	fade_work = (button_fade_work_t *) work;
	client = fade_work->client;

	if (fade_work->fade_mode) {
		ret = button_fade_in(client);
	} else {
		ret = button_fade_out(client);
	}

	if (ret < 0)
		I(" %s: mode=%d, ret=%d\n", __func__, fade_work->fade_mode, ret);
}

static void lp5521_backlight_on(struct i2c_client *client)
{
	D("%s\n" , __func__);
	button_fade_work.fade_mode = 1;
	button_fade_work.client = client;
	queue_work(led_powerkey_work_queue, (struct work_struct *) &button_fade_work);
}

static void lp5521_backlight_off(struct i2c_client *client)
{
	D("%s\n" , __func__);
	button_fade_work.fade_mode = 0;
	button_fade_work.client = client;
	queue_work(led_powerkey_work_queue, (struct work_struct *) &button_fade_work);
}

static void lp5521_dual_off(struct i2c_client *client)
{
	uint8_t data = 0x00;
	int ret;
	struct led_i2c_platform_data *pdata;

	D("%s\n" , __func__);
	pdata = client->dev.platform_data;
	mutex_lock(&led_mutex);
	/* === set green pwm to 0 === */
	data = 0x00;
	if( current_mode == 1 )
		ret = i2c_write_block(client, 0x03, &data, 1);
	else if( current_mode == 3 )
		ret = i2c_write_block(client, 0x02, &data, 1);
	current_mode = 0;
	if( backlight_mode == 1 ) {
		data = 0x03;
		ret = i2c_write_block(client, 0x01, &data, 1);
		udelay(200);
	} else if ( backlight_mode >= 2 )  {
		data = 0x02;
		ret = i2c_write_block(client, 0x01, &data, 1);
		udelay(200);
	} else {
		data = 0x00;
		ret = i2c_write_block(client, 0x01, &data, 1);
		udelay(200);
		if( suspend_mode == 1 ) {
			/* === reset register === */
			data = 0xff;
			ret = i2c_write_block(client, 0x0d, &data, 1);
			udelay(550);
			gpio_direction_output(pdata->ena_gpio, 0);
			D("no LED command now in suspend, reset chip & gpio, no ack in i2c 0x32 is correct in LED chip lp5521.\n");
		} else {
			/* === disable CHIP_EN === */
			data = 0x00;
			ret = i2c_write_block(client, 0x00, &data, 1);
			udelay(550);
			gpio_direction_output(pdata->ena_gpio, 0);
			D("no LED command now in idle, disable chip & gpio.\n");
		}
	}
	mutex_unlock(&led_mutex);
}

void lp5521_led_current_set_for_key(int brightness_key)
{
	D("%s\n" , __func__);
	if (brightness_key)
		backlight_mode = brightness_key + 1;
	else
		backlight_mode = 0;
	queue_work(led_powerkey_work_queue, &led_powerkey_work);
}

void led_behavior(struct i2c_client *client, int val)
{
	switch(val)
	{
		case LED_RESET:
			lp5521_led_enable(client);
			break;
		case GREEN_ON:
			lp5521_green_on(client);
			break;
		case GREEN_BLINK:
			lp5521_green_blink(client);
			break;
		case AMBER_ON:
			lp5521_amber_on(client);
			break;
		case AMBER_BLINK:
			lp5521_amber_blink(client);
			break;
		case AMBER_LOW_BLINK:
			lp5521_amber_low_blink(client);
			break;
		case DUAL_COLOR_BLINK:
			lp5521_dual_color_blink(client);
			break;
		case BACKLIGHT_ON:
			lp5521_backlight_on(client);
			break;
		case BACKLIGHT_BLINK:
			lp5521_led_current_set_for_key(1);
			break;
		default:
			break;
	}
}
EXPORT_SYMBOL(led_behavior);

static ssize_t led_behavior_set(struct device *dev,
				   struct device_attribute *attr,
				   const char *buf, size_t count)
{
	struct i2c_client *client;
	int val;

	val = -1;
	sscanf(buf, "%d", &val);
	if (val < 0 && val > 8)
		return -EINVAL;

	client = to_i2c_client(dev);
	led_behavior(client, val);
	current_state = val;

	return count;
}

static ssize_t led_behavior_show(struct device *dev,
				   struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%d\n", current_state);
}

static DEVICE_ATTR(behavior, 0644, led_behavior_show, led_behavior_set);

static void lp5521_led_birghtness_set(struct led_classdev *led_cdev,
				   enum led_brightness brightness)
{
	struct i2c_client *client = private_lp5521_client;
	struct lp5521_led *ldata;

	if (brightness < 0)
		brightness = 0;
	else if (brightness > 255)
		brightness = 255;
	ldata = container_of(led_cdev, struct lp5521_led, cdev);
	if(brightness) {
		if(!strcmp(ldata->cdev.name, "green"))	 {
			lp5521_green_on(client);
		} else if (!strcmp(ldata->cdev.name, "amber")) {
			lp5521_amber_on(client);
		} else if (!strcmp(ldata->cdev.name, "button-backlight")) {
			if ( backlight_mode < 2 )
				lp5521_backlight_on(client);
		}
	} else {
		if (!strcmp(ldata->cdev.name, "button-backlight")) {
			if( backlight_mode == 1 )
				lp5521_backlight_off(client);
		}else if(!strcmp(ldata->cdev.name, "amber"))	 {
			if( current_mode == 3 )
				lp5521_dual_off(client);
		}else if(!strcmp(ldata->cdev.name, "green"))	 {
			if( current_mode == 1)
				lp5521_dual_off(client);
		}
	}
}

static void led_powerkey_work_func(struct work_struct *work)
{
	struct i2c_client *client = private_lp5521_client;
	struct led_i2c_platform_data *pdata;
	uint8_t data;
	int ret;
	int address, i, fade_in_steps, fade_out_steps;

	fade_in_steps = fade_out_steps = 3;

	D("%s, current_mode: %d, backlight_mode: %d\n", __func__, current_mode, backlight_mode);
	pdata = client->dev.platform_data;
	if(current_mode == 0 )
		lp5521_led_enable(client);
	mutex_lock(&led_mutex);
	if (backlight_mode >= 2) {
		if(current_mode == 1) {
			/* === load program with green direct and blue load program === */
			data = 0x0d;
			ret = i2c_write_block(client, 0x01, &data, 1);
			udelay(200);
		}else if( current_mode == 3 ) {
			/* === load program with red direct and blue load program === */
			data = 0x31;
			ret = i2c_write_block(client, 0x01, &data, 1);
			udelay(200);
		}else if( current_mode == 2 ) {
			/* === load program with green run and blue load program === */
			data = 0x09;
			ret = i2c_write_block(client, 0x01, &data, 1);
			udelay(200);
		}else if( current_mode == 4 || current_mode == 5 ) {
			/* === load program with red run and blue load program === */
			data = 0x21;
			ret = i2c_write_block(client, 0x01, &data, 1);
			udelay(200);
		}else if( current_mode == 6 ) {
			/* === load program with red and green run and blue load program === */
			data = 0x29;
			ret = i2c_write_block(client, 0x01, &data, 1);
			udelay(200);
		}else {
			/* === load program with blue load program === */
			data = 0x01;
			ret = i2c_write_block(client, 0x01, &data, 1);
			udelay(200);
		}
		if (backlight_mode == 2) {
			/* === function virtual key blink === */
			/* === set pwm to 255 === */
			data = 0x40;
			ret = i2c_write_block(client, 0x50, &data, 1);
			data = (u8)button_brightness;
			ret = i2c_write_block(client, 0x51, &data, 1);
			/* === wait 0.2s === */
			data = 0x4d;
			ret = i2c_write_block(client, 0x52, &data, 1);
			data = 0x00;
			ret = i2c_write_block(client, 0x53, &data, 1);
			/* === set pwm to 0 === */
			data = 0x40;
			ret = i2c_write_block(client, 0x54, &data, 1);
			data = 0x00;
			ret = i2c_write_block(client, 0x55, &data, 1);
			/* === wait 0.2s === */
			data = 0x4d;
			ret = i2c_write_block(client, 0x56, &data, 1);
			data = 0x00;
			ret = i2c_write_block(client, 0x57, &data, 1);

			// Clear stuff from slow blink mode
			data = 0x00;
			for(i=0x58; i<0x70; i++) {
			ret = i2c_write_block(client, i, &data, 1);
			}
		} else {
			/* slow blinking */
			// Start address for the button programm. We have 32 instructions.
			address = 0x50;

			for(i=1; i<=fade_in_steps; i++) {
				/* === set pwm to (255/10)*i === */
				data = 0x40;
				ret = i2c_write_block(client, address++, &data, 1);
				data = (u8)((slow_blink_brightness/fade_in_steps)*i);
				ret = i2c_write_block(client, address++, &data, 1);
				/* === wait 0.064s < ?s < 0.2s === */
				data = 0x48;
				ret = i2c_write_block(client, address++, &data, 1);
				data = 0x00;
				ret = i2c_write_block(client, address++, &data, 1);
			}

			/* === wait 0.999s === */
			data = 0x7f;
			ret = i2c_write_block(client, address++, &data, 1);
			data = 0x00;
			ret = i2c_write_block(client, address++, &data, 1);

			for(i=fade_out_steps-1; i>=0; i--) {
				/* === set pwm to (255/10)*i === */
				data = 0x40;
				ret = i2c_write_block(client, address++, &data, 1);
				data = (u8)((slow_blink_brightness/fade_out_steps)*i);
				ret = i2c_write_block(client, address++, &data, 1);
				/* === wait 0.064s < ?s < 0.2s === */
				data = 0x48;
				ret = i2c_write_block(client, address++, &data, 1);
				data = 0x00;
				ret = i2c_write_block(client, address++, &data, 1);
			}

			/* === wait 0.999s === */
			data = 0x7f;
			ret = i2c_write_block(client, address++, &data, 1);
			data = 0x00;
			ret = i2c_write_block(client, address++, &data, 1);
			/* === wait 0.999s === */
			data = 0x7f;
			ret = i2c_write_block(client, address++, &data, 1);
			data = 0x00;
			ret = i2c_write_block(client, address++, &data, 1);
			/* === wait 0.999s === */
			data = 0x7f;
			ret = i2c_write_block(client, address++, &data, 1);
			data = 0x00;
			ret = i2c_write_block(client, address++, &data, 1);

			D("Last address was: 0x%x\n", address-1);
			if(address > 0x70) {
				E("Too many instructions for backlight programm!");
			}
		}
		if(current_mode == 1) {
			/* === run program with green direct and blue run program === */
			data = 0x0e;
			ret = i2c_write_block(client, 0x01, &data, 1);
			udelay(200);
			data = 0x42;
			ret = i2c_write_block(client, 0x00, &data, 1);
			udelay(550);
		}else if( current_mode == 3 ) {
			/* === run program with red direct and blue run program === */
			data = 0x32;
			ret = i2c_write_block(client, 0x01, &data, 1);
			udelay(200);
			data = 0x42;
			ret = i2c_write_block(client, 0x00, &data, 1);
			udelay(550);
		}else if( current_mode == 2 ) {
			/* === run program with green run and blue run program === */
			data = 0x0a;
			ret = i2c_write_block(client, 0x01, &data, 1);
			udelay(200);
			data = 0x4a;
			ret = i2c_write_block(client, 0x00, &data, 1);
			udelay(550);
		}else if( current_mode == 4 || current_mode == 5 ) {
			/* === run program with red run and blue run program === */
			data = 0x22;
			ret = i2c_write_block(client, 0x01, &data, 1);
			udelay(200);
			data = 0x62;
			ret = i2c_write_block(client, 0x00, &data, 1);
			udelay(550);
		}else if( current_mode == 6 ) {
			/* === run program with red and green run and blue run program === */
			data = 0x2a;
			ret = i2c_write_block(client, 0x01, &data, 1);
			udelay(200);
			data = 0x6a;
			ret = i2c_write_block(client, 0x00, &data, 1);
			udelay(550);
		}else {
			/* === run program with blue load program === */
			data = 0x02;
			ret = i2c_write_block(client, 0x01, &data, 1);
			udelay(200);
			data = 0x42;
			ret = i2c_write_block(client, 0x00, &data, 1);
			udelay(550);
		}
	}else if (backlight_mode == 0) {
		/* === set blue channel to direct PWM control mode === */
		if(current_mode == 1) {
			/* === run program with green direct and blue direct program === */
			data = 0x0f;
			ret = i2c_write_block(client, 0x01, &data, 1);
			usleep_range(1000, 2000);
			data = 0x40;
			ret = i2c_write_block(client, 0x00, &data, 1);
			usleep_range(1000, 2000);
			data = 0x00;
			ret = i2c_write_block(client, 0x04, &data, 1);
		}else if( current_mode == 3 ) {
			/* === run program with red direct and blue direct program === */
			data = 0x33;
			ret = i2c_write_block(client, 0x01, &data, 1);
			usleep_range(1000, 2000);
			data = 0x40;
			ret = i2c_write_block(client, 0x00, &data, 1);
			usleep_range(1000, 2000);
			data = 0x00;
			ret = i2c_write_block(client, 0x04, &data, 1);
		}else if( current_mode == 2 ) {
			/* === run program with green run and blue direct program === */
			data = 0x0b;
			ret = i2c_write_block(client, 0x01, &data, 1);
			usleep_range(1000, 2000);
			data = 0x48;
			ret = i2c_write_block(client, 0x00, &data, 1);
			usleep_range(1000, 2000);
			data = 0x00;
			ret = i2c_write_block(client, 0x04, &data, 1);
		}else if( current_mode == 4 || current_mode == 5 ) {
			/* === run program with red run and blue direct program === */
			data = 0x23;
			ret = i2c_write_block(client, 0x01, &data, 1);
			usleep_range(1000, 2000);
			data = 0x60;
			ret = i2c_write_block(client, 0x00, &data, 1);
			usleep_range(1000, 2000);
			data = 0x00;
			ret = i2c_write_block(client, 0x04, &data, 1);
		}else if( current_mode == 6 ) {
			/* === run program with red and green run and blue direct program === */
			data = 0x2b;
			ret = i2c_write_block(client, 0x01, &data, 1);
			usleep_range(1000, 2000);
			data = 0x68;
			ret = i2c_write_block(client, 0x00, &data, 1);
			usleep_range(1000, 2000);
			data = 0x00;
			ret = i2c_write_block(client, 0x04, &data, 1);
		}else {
			/* === run program with blue direct program === */
			data = 0x03;
			ret = i2c_write_block(client, 0x01, &data, 1);
			usleep_range(1000, 2000);
			data = 0x40;
			ret = i2c_write_block(client, 0x00, &data, 1);
			usleep_range(1000, 2000);
			data = 0x00;
			ret = i2c_write_block(client, 0x04, &data, 1);
			/* === disable CHIP_EN === */
			data = 0x00;
			ret = i2c_write_block(client, 0x00, &data, 1);
			udelay(550);
			gpio_direction_output(pdata->ena_gpio, 0);
			D("no LED command now, disable chip & gpio.\n");
		}

	}
	mutex_unlock(&led_mutex);
}

static void led_work_func(struct work_struct *work)
{
	struct i2c_client *client = private_lp5521_client;
	struct lp5521_led *ldata;

	D("%s\n", __func__);
	ldata = container_of(work, struct lp5521_led, led_work);
	if ( offtimer_mode == current_mode )
		lp5521_dual_off(client);
	offtimer_mode = 0;
}

static void led_alarm_handler(struct alarm *alarm)
{
	struct lp5521_led *ldata;

	D("%s\n", __func__);
	ldata = container_of(alarm, struct lp5521_led, led_alarm);
	queue_work(g_led_work_queue, &ldata->led_work);
}

static ssize_t lp5521_led_off_timer_show(struct device *dev,
				  struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%d\n", current_time);;
}

static ssize_t lp5521_led_off_timer_store(struct device *dev,
				   struct device_attribute *attr,
				   const char *buf, size_t count)
{
	struct led_classdev *led_cdev;
	struct lp5521_led *ldata;
	int min, sec;
	uint16_t off_timer;
	ktime_t interval;
	ktime_t next_alarm;
	int ret;

	min = -1;
	sec = -1;
	led_cdev = (struct led_classdev *)dev_get_drvdata(dev);
	ldata = container_of(led_cdev, struct lp5521_led, cdev);

	ret = sscanf(buf, "%d %d", &min, &sec);
	if (ret!=2)
		return -EINVAL;

	D("%s , min = %d, sec = %d, current_mode=%d dev_name=%s\n" , __func__, min, sec, current_mode, ldata->cdev.name);
	if (min < 0 || min > 255)
		return -EINVAL;
	if (sec < 0 || sec > 255)
		return -EINVAL;


	off_timer = min * 60 + sec;

	if(off_timer) {
		if(!strcmp(ldata->cdev.name, "green"))	 {
			if( current_mode != 1 &&  current_mode != 2 && current_mode != 6 )
				return count;
		} else if (!strcmp(ldata->cdev.name, "amber")) {
			if( current_mode != 3 &&  current_mode != 4 && current_mode != 5 && current_mode != 6)
				return count;
		} else if (!strcmp(ldata->cdev.name, "button-backlight")) {
			if( backlight_mode != 1 &&  backlight_mode != 2 )
				return count;
		}
	}
	current_time = off_timer;
	alarm_cancel(&ldata->led_alarm);
	cancel_work_sync(&ldata->led_work);
	offtimer_mode = current_mode;
	D("%s , off_timer = %d\n" , __func__, off_timer);
	if (off_timer) {
		interval = ktime_set(off_timer, 0);
		next_alarm = ktime_add(alarm_get_elapsed_realtime(), interval);
		alarm_start_range(&ldata->led_alarm, next_alarm, next_alarm);
	}

	return count;
}

static DEVICE_ATTR(off_timer, 0644, lp5521_led_off_timer_show,
					lp5521_led_off_timer_store);

static ssize_t lp5521_led_blink_show(struct device *dev,
				  struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%d\n", current_blink);
}

static ssize_t lp5521_led_blink_store(struct device *dev,
				   struct device_attribute *attr,
				   const char *buf, size_t count)
{
	struct i2c_client *client = private_lp5521_client;
	struct led_classdev *led_cdev;
	struct lp5521_led *ldata;
	int val, ret;

	val = -1;
	ret = sscanf(buf, "%d", &val);
	if (ret!=1)
		return -EINVAL;

	D("%s , val = %d\n" , __func__, val);
	if (val < 0 )
		val = 0;
	else if (val > 255)
		val = 255;

	led_cdev = (struct led_classdev *)dev_get_drvdata(dev);
	ldata = container_of(led_cdev, struct lp5521_led, cdev);

	if(val) {
		if(!strcmp(ldata->cdev.name, "green"))	 {
			if( (current_mode == 4 || amber_mode == 2) && val == 3 )
				lp5521_dual_color_blink(client);
			else
				lp5521_green_blink(client);
		} else if (!strcmp(ldata->cdev.name, "amber")) {
			if( val == 4 )
				lp5521_amber_low_blink(client);
			else
				lp5521_amber_blink(client);
		} else if (!strcmp(ldata->cdev.name, "button-backlight")) {
			if ( backlight_mode != 2 )
				lp5521_led_current_set_for_key(1);
		}
	} else {
		if(!strcmp(ldata->cdev.name, "amber"))	 {
			if( amber_mode == 2 )
				amber_mode = 0;
			if( current_mode == 4 || current_mode == 5 || current_mode == 6 )
				lp5521_dual_off(client);
		}else if(!strcmp(ldata->cdev.name, "green")) {
			if( current_mode == 2 || current_mode == 6 )
				lp5521_dual_off(client);
		}else if (!strcmp(ldata->cdev.name, "button-backlight")) {
			if ( backlight_mode == 2 )
				lp5521_led_current_set_for_key(0);
		}
	}
	current_blink= val;

	return count;
}

static DEVICE_ATTR(blink, 0644, lp5521_led_blink_show,
					lp5521_led_blink_store);

static ssize_t lp5521_led_slow_blink_show(struct device *dev,
					  struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%d\n", slow_blink_brightness);
}

static ssize_t lp5521_led_slow_blink_store(struct device *dev,
					   struct device_attribute *attr,
					   const char *buf, size_t count)
{
	struct led_classdev *led_cdev;
	struct lp5521_led *ldata;
	int val, ret;

	led_cdev = (struct led_classdev *)dev_get_drvdata(dev);
	ldata = container_of(led_cdev, struct lp5521_led, cdev);

	val = 0;
	ret = sscanf(buf, "%d", &val);
	if (ret!=1)
		return -EINVAL;
		
	D("%s , val = %d\n" , __func__, val);

	if (val < 0 )
		val = 0;
	else if (val > 255)
		val = 255;

	if(val) {
		if(!strcmp(ldata->cdev.name, "button-backlight")) {
			if(backlight_mode != 3 || val != slow_blink_brightness) {
				// limit it to button brightness value
				if(slow_blink_brightness_limit)
					slow_blink_brightness = button_brightness;
				else
					slow_blink_brightness = val;				
				lp5521_led_current_set_for_key(2);
			}
		}
	} else {
		if(!strcmp(ldata->cdev.name, "button-backlight")) {
			if (backlight_mode == 3) {
				slow_blink_brightness = 0;
				lp5521_led_current_set_for_key(0);
			}
		}
	}

	return count;
}

static DEVICE_ATTR(slow_blink, 0644, lp5521_led_slow_blink_show,
		   lp5521_led_slow_blink_store);

static ssize_t lp5521_led_currents_show(struct device *dev,
				  struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%d\n", current_currents);
}

static ssize_t lp5521_led_currents_store(struct device *dev,
				   struct device_attribute *attr,
				   const char *buf, size_t count)
{
	struct i2c_client *client = private_lp5521_client;
	struct led_classdev *led_cdev;
	struct lp5521_led *ldata;
	uint8_t data = 0x00;
	int val, ret;

	ret = sscanf(buf, "%d", &val);

	if (ret!=1 || val < 0)
		return -EINVAL;

	D("%s , val = %d\n" , __func__, val);

	current_currents = val;
	led_cdev = (struct led_classdev *)dev_get_drvdata(dev);
	ldata = container_of(led_cdev, struct lp5521_led, cdev);

	mutex_lock(&led_mutex);
	/* === run program with all direct program === */
	data = 0x3f;
	ret = i2c_write_block(client, 0x01, &data, 1);
	udelay(200);
	data = 0x40;
	ret = i2c_write_block(client, 0x00, &data, 1);
	udelay(500);
	/* === set pwm to all === */
	data = (u8)val;
	if(!strcmp(ldata->cdev.name, "green"))	 {
		ret = i2c_write_block(client, 0x06, &data, 1);
	} else if (!strcmp(ldata->cdev.name, "amber")) {
		ret = i2c_write_block(client, 0x05, &data, 1);
	} else if (!strcmp(ldata->cdev.name, "button-backlight")) {
		ret = i2c_write_block(client, 0x07, &data, 1);
	}
	mutex_unlock(&led_mutex);

	return count;
}

static DEVICE_ATTR(currents, 0644, lp5521_led_currents_show,
					lp5521_led_currents_store);

static ssize_t lp5521_led_pwm_coefficient_show(struct device *dev,
				  struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%d\n", current_pwm_coefficient);
}

static ssize_t lp5521_led_pwm_coefficient_store(struct device *dev,
				   struct device_attribute *attr,
				   const char *buf, size_t count)
{
	struct i2c_client *client = private_lp5521_client;
	struct led_classdev *led_cdev;
	struct lp5521_led *ldata;
	uint8_t data = 0x00;
	int val, ret;

	ret = sscanf(buf, "%d", &val);
	if (ret!=1 || val < 0 || val > 100)
		return -EINVAL;
		
	D("%s , val = %d\n" , __func__, val);
	
	current_pwm_coefficient = val;
	led_cdev = (struct led_classdev *)dev_get_drvdata(dev);
	ldata = container_of(led_cdev, struct lp5521_led, cdev);

	mutex_lock(&led_mutex);
	/* === run program with all direct program === */
	data = 0x3f;
	ret = i2c_write_block(client, 0x01, &data, 1);
	udelay(200);
	data = 0x40;
	ret = i2c_write_block(client, 0x00, &data, 1);
	udelay(500);
	/* === set current to amber & green === */
	data = (u8)val*255/100;
	if(!strcmp(ldata->cdev.name, "green"))	 {
		ret = i2c_write_block(client, 0x03, &data, 1);
	} else if (!strcmp(ldata->cdev.name, "amber")) {
		ret = i2c_write_block(client, 0x02, &data, 1);
	}
	mutex_unlock(&led_mutex);

	return count;
}

static DEVICE_ATTR(pwm_coefficient, 0644, lp5521_led_pwm_coefficient_show,
					lp5521_led_pwm_coefficient_store);

static ssize_t lp5521_led_lut_coefficient_show(struct device *dev,
				  struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%d\n", current_lut_coefficient);
}

static ssize_t lp5521_led_lut_coefficient_store(struct device *dev,
				   struct device_attribute *attr,
				   const char *buf, size_t count)
{
	struct i2c_client *client = private_lp5521_client;
	struct led_classdev *led_cdev;
	struct lp5521_led *ldata;
	uint8_t data = 0x00;
	int val, ret;

	ret = sscanf(buf, "%d", &val);
	if (ret!=1 || val < 0 || val > 100)
		return -EINVAL;

	D("%s , val = %d\n" , __func__, val);
	
	current_lut_coefficient = val;
	led_cdev = (struct led_classdev *)dev_get_drvdata(dev);
	ldata = container_of(led_cdev, struct lp5521_led, cdev);

	mutex_lock(&led_mutex);
	/* === run program with all direct program === */
	data = 0x3f;
	ret = i2c_write_block(client, 0x01, &data, 1);
	udelay(200);
	data = 0x40;
	ret = i2c_write_block(client, 0x00, &data, 1);
	udelay(500);
	/* === set current to blue(button) === */
	data = (u8)val*255/100;
	if(!strcmp(ldata->cdev.name, "button-backlight")) {
		ret = i2c_write_block(client, 0x04, &data, 1);
	}
	mutex_unlock(&led_mutex);

	return count;
}

static DEVICE_ATTR(lut_coefficient, 0644, lp5521_led_lut_coefficient_show,
					lp5521_led_lut_coefficient_store);

static ssize_t lp5521_led_button_brightness_show(struct device *dev,
				  struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%d\n", button_brightness);
}

static ssize_t lp5521_led_button_brightness_store(struct device *dev,
				   struct device_attribute *attr,
				   const char *buf, size_t count)
{
	int ret = 0;
	unsigned int temp;
	struct i2c_client *client = private_lp5521_client;

	ret = sscanf(buf, "%d", &temp);
	if (ret!=1 || temp < 0 || temp > 255)
		return -EINVAL;

	D("%s , val = %d\n" , __func__, temp);
	
	// 0 will reset it to the boards default value
	if (temp == 0)
		button_brightness = button_brightness_board;
	else
		button_brightness = temp;
	
	button_brightness_adjust(client);

	return count;
}

static DEVICE_ATTR(button_brightness, 0644, lp5521_led_button_brightness_show,
					lp5521_led_button_brightness_store);


static ssize_t lp5521_slow_blink_brightness_limit_show(struct device *dev,
				  struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%d\n", slow_blink_brightness_limit);
}

static ssize_t lp5521_slow_blink_brightness_limit_store(struct device *dev,
				   struct device_attribute *attr,
				   const char *buf, size_t count)
{
	int ret = 0;
	unsigned int temp;

	ret = sscanf(buf, "%d", &temp);
	if (ret!=1 || temp < 0 || temp > 1)
		return -EINVAL;

	D("%s , val = %d\n" , __func__, temp);
	
	slow_blink_brightness_limit = temp;

	return count;
}

static DEVICE_ATTR(slow_blink_brightness_limit, 0644, lp5521_slow_blink_brightness_limit_show,
					lp5521_slow_blink_brightness_limit_store);

static void lp5521_led_early_suspend(struct early_suspend *handler)
{
	struct i2c_client *client = private_lp5521_client;

	I("%s start\n", __func__);
	suspend_mode = 1;

	if( backlight_mode == 1 )
		lp5521_backlight_off(client);
	else if ( backlight_mode == 2 )
		lp5521_led_current_set_for_key(0);
	I("%s end\n", __func__);
}

static void lp5521_led_late_resume(struct early_suspend *handler)
{
	I("%s start\n", __func__);
	suspend_mode = 0;
	I("%s end\n", __func__);
}

static int lp5521_led_probe(struct i2c_client *client
	, const struct i2c_device_id *id)
{
	struct device *dev = &client->dev;
	struct lp5521_chip		*cdata;
	struct led_i2c_platform_data *pdata;
	int ret, i;

	I("%s probe start\n", __func__);

	/* === init platform and client data === */
	cdata = kzalloc(sizeof(struct lp5521_chip), GFP_KERNEL);
	if (!cdata) {
		ret = -ENOMEM;
		E("failed on allocat cdata\n");
		goto err_cdata;
	}
	i2c_set_clientdata(client, cdata);
	cdata->client = client;

	pdata = client->dev.platform_data;
	if (!pdata) {
		ret = -EBUSY;
		E("failed on get pdata\n");
		goto err_exit;
	}
	led_rw_delay = 5;

	/* === led enable pin === */
	ret = gpio_request(pdata->ena_gpio, "led_enable");
	if (ret < 0) {
		E("%s: gpio_request failed %d\n", __func__, ret);
		return ret;
	}
	ret = gpio_direction_output(pdata->ena_gpio, lp5521_led_tag_status ? 1 : 0);
	if (ret < 0) {
		E("%s: gpio_direction_output failed %d\n", __func__, ret);
		gpio_free(pdata->ena_gpio);
		return ret;
	}
	
	I("led_config default %d %d %d\n", pdata->led_config[0].led_lux, 
		pdata->led_config[1].led_lux, pdata->led_config[2].led_lux);
	
   	tegra_gpio_enable(pdata->ena_gpio);
	button_brightness = pdata->led_config[2].led_lux * 255 / 100;
	button_brightness_board = button_brightness;
	slow_blink_brightness = 0;

	private_lp5521_client = client;
	g_led_work_queue = create_workqueue("led");
	if (!g_led_work_queue)
		goto err_create_work_queue;
	led_powerkey_work_queue = create_workqueue("led_powerkey");
	if (!led_powerkey_work_queue)
		goto err_create_work_queue;

	/* intail LED config */
	for (i = 0; i < pdata->num_leds; i++) {
		cdata->leds[i].cdev.name = pdata->led_config[i].name;
		cdata->leds[i].cdev.brightness_set = lp5521_led_birghtness_set;
		ret = led_classdev_register(dev, &cdata->leds[i].cdev);
		if (ret < 0) {
			E("couldn't register led[%d]\n", i);
			return ret;
		}
		ret = device_create_file(cdata->leds[i].cdev.dev, &dev_attr_blink);
		if (ret < 0) {
			E("%s: failed on create attr blink [%d]\n", __func__, i);
			goto err_register_attr_blink;
		}
		ret = device_create_file(cdata->leds[i].cdev.dev, &dev_attr_slow_blink);
		if (ret < 0) {
			E("%s: failed on create attr slow_blink [%d]\n", __func__, i);
			goto err_register_attr_slow_blink;
		}
		ret = device_create_file(cdata->leds[i].cdev.dev, &dev_attr_off_timer);
		if (ret < 0) {
			E("%s: failed on create attr off_timer [%d]\n", __func__, i);
			goto err_register_attr_off_timer;
		}
		ret = device_create_file(cdata->leds[i].cdev.dev, &dev_attr_currents);
		if (ret < 0) {
			E("%s: failed on create attr currents [%d]\n", __func__, i);
			goto err_register_attr_currents;
		}
		ret = device_create_file(cdata->leds[i].cdev.dev, &dev_attr_pwm_coefficient);
		if (ret < 0) {
			E("%s: failed on create attr pwm_coefficient [%d]\n", __func__, i);
			goto err_register_attr_pwm_coefficient;
		}
		ret = device_create_file(cdata->leds[i].cdev.dev, &dev_attr_lut_coefficient);
		if (ret < 0) {
			E("%s: failed on create attr lut_coefficient [%d]\n", __func__, i);
			goto err_register_attr_lut_coefficient;
		}
		ret = device_create_file(cdata->leds[i].cdev.dev, &dev_attr_button_brightness);
		if (ret < 0) {
			E("%s: failed on create attr button_brightness [%d]\n", __func__, i);
			goto err_register_attr_button_brightness;
		}
		ret = device_create_file(cdata->leds[i].cdev.dev, &dev_attr_slow_blink_brightness_limit);
		if (ret < 0) {
			E("%s: failed on create attr slow_blink_brightness_limit [%d]\n", __func__, i);
			goto err_register_attr_slow_blink_brightness_limit;
		}
		INIT_WORK(&cdata->leds[i].led_work, led_work_func);
		alarm_init(&cdata->leds[i].led_alarm,
				   ANDROID_ALARM_ELAPSED_REALTIME_WAKEUP,
				   led_alarm_handler);
	}
	INIT_WORK(&led_powerkey_work, led_powerkey_work_func);
	INIT_WORK((struct work_struct *) &button_fade_work, button_fade_work_func);

	/* === create device node === */
	ret = device_create_file(&client->dev, &dev_attr_behavior);
	if (ret) {
		E( "device_create_file failed\n");
		goto err_fun_init;
	}

	regulator = regulator_get(NULL, "v_led_3v3");
	if( (regulator==NULL) | (IS_ERR(regulator)))
		E("Fail to get regulator: v_led_3v3");
	regulator_enable(regulator);

	mutex_init(&cdata->led_i2c_rw_mutex);
	mutex_init(&led_mutex);
	current_mode = backlight_mode = suspend_mode = offtimer_mode = amber_mode = 0;

#ifdef CONFIG_HAS_EARLYSUSPEND
	cdata->early_suspend_led.suspend = lp5521_led_early_suspend;
	cdata->early_suspend_led.resume = lp5521_led_late_resume;
	register_early_suspend(&cdata->early_suspend_led);
#endif
	if (lp5521_led_tag_status) {
		led_behavior(client, lp5521_led_tag_status);
	}

	I("%s success!\n", __func__);
	return 0;

err_fun_init:
	device_remove_file(&client->dev, &dev_attr_behavior);
	kfree(cdata);
	
err_register_attr_slow_blink_brightness_limit:
	for (i = 0; i < pdata->num_leds; i++) {
		device_remove_file(cdata->leds[i].cdev.dev,&dev_attr_slow_blink_brightness_limit);
	}
err_register_attr_button_brightness:
	for (i = 0; i < pdata->num_leds; i++) {
		device_remove_file(cdata->leds[i].cdev.dev,&dev_attr_button_brightness);
	}
err_register_attr_lut_coefficient:
	for (i = 0; i < pdata->num_leds; i++) {
		device_remove_file(cdata->leds[i].cdev.dev,&dev_attr_lut_coefficient);
	}
err_register_attr_pwm_coefficient:
	for (i = 0; i < pdata->num_leds; i++) {
		device_remove_file(cdata->leds[i].cdev.dev,&dev_attr_pwm_coefficient);
	}
err_register_attr_currents:
	for (i = 0; i < pdata->num_leds; i++) {
		device_remove_file(cdata->leds[i].cdev.dev,&dev_attr_currents);
	}
err_register_attr_off_timer:
	for (i = 0; i < pdata->num_leds; i++) {
		device_remove_file(cdata->leds[i].cdev.dev,&dev_attr_off_timer);
	}
err_register_attr_slow_blink:
	for (i = 0; i < pdata->num_leds; i++) {
		device_remove_file(cdata->leds[i].cdev.dev,&dev_attr_slow_blink);
	}
err_register_attr_blink:
	for (i = 0; i < pdata->num_leds; i++) {
		device_remove_file(cdata->leds[i].cdev.dev,&dev_attr_blink);
	}
err_create_work_queue:
	kfree(pdata);
err_exit:
	kfree(cdata);
err_cdata:
	return ret;
}

static int __devexit lp5521_led_remove(struct i2c_client *client)
{
	struct led_i2c_platform_data *pdata;
	struct lp5521_chip *cdata;
	int i;

	cdata = i2c_get_clientdata(client);
	cdata = kzalloc(sizeof(struct lp5521_chip), GFP_KERNEL);
	i2c_set_clientdata(client, cdata);
	cdata->client = client;
	pdata = client->dev.platform_data;
	gpio_direction_output(pdata->ena_gpio, 0);
	if ( (regulator_is_enabled(regulator)) > 0)
		regulator_disable(regulator);
	regulator_put(regulator);
	device_remove_file(&client->dev, &dev_attr_behavior);
	unregister_early_suspend(&cdata->early_suspend_led);
	for (i = 0; i < pdata->num_leds; i++) {
		device_remove_file(cdata->leds[i].cdev.dev,&dev_attr_blink);
		device_remove_file(cdata->leds[i].cdev.dev,&dev_attr_slow_blink);
		device_remove_file(cdata->leds[i].cdev.dev,&dev_attr_off_timer);
		device_remove_file(cdata->leds[i].cdev.dev,&dev_attr_currents);
		device_remove_file(cdata->leds[i].cdev.dev,&dev_attr_pwm_coefficient);
		device_remove_file(cdata->leds[i].cdev.dev,&dev_attr_lut_coefficient);
		device_remove_file(cdata->leds[i].cdev.dev,&dev_attr_button_brightness);
		device_remove_file(cdata->leds[i].cdev.dev,&dev_attr_slow_blink_brightness_limit);
		led_classdev_unregister(&cdata->leds[i].cdev);
	}
	destroy_workqueue(g_led_work_queue);
	destroy_workqueue(led_powerkey_work_queue);
	kfree(cdata);

	return 0;
}

/* === LED driver info === */
static const struct i2c_device_id led_i2c_id[] = {
	{ LED_I2C_NAME, 0 },
	{}
};


static struct i2c_driver led_i2c_driver = {
	.driver = {
		   .name = LED_I2C_NAME,
		   },
	.id_table = led_i2c_id,
	.probe = lp5521_led_probe,
	.remove = __devexit_p(lp5521_led_remove),
};

static int __init lp5521_led_init(void)
{
	int ret;

	ret = i2c_add_driver(&led_i2c_driver);
	if (ret)
		return ret;
	return 0;
}

static void __exit lp5521_led_exit(void)
{
	i2c_del_driver(&led_i2c_driver);
}

module_init(lp5521_led_init);
module_exit(lp5521_led_exit);

MODULE_AUTHOR("<ShihHao_Shiung@htc.com>, <Dirk_Chang@htc.com>");
MODULE_DESCRIPTION("LP5521 LED driver");

