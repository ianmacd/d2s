#ifndef _LINUX_STM_FSR_SIDEKEY_H_
#define _LINUX_STM_FSR_SIDEKEY_H_

#include <linux/init.h>
#include <linux/errno.h>
#include <linux/platform_device.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/input.h>
#include <linux/interrupt.h>
#include <linux/serio.h>
#include <linux/init.h>
#include <linux/pm.h>
#include <linux/delay.h>
#include <linux/ctype.h>
#include <linux/gpio.h>
#include <linux/i2c.h>
#include <linux/i2c-dev.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/power_supply.h>
#include <linux/firmware.h>
#include <linux/regulator/consumer.h>
#include <linux/of_gpio.h>
#include <linux/input/mt.h>
#ifdef CONFIG_SEC_SYSFS
#include <linux/sec_sysfs.h>
#endif

#include <linux/device.h>
#include <linux/input/sec_cmd.h>
#include <linux/proc_fs.h>

//#define ENABLE_POWER_CONTROL

#define STM_FSR_DRV_NAME	"fsr_sidekey"
#define STM_FSR_DRV_VERSION	"201901023"

#define FSR_ID0							0x36
#define FSR_ID1							0x70

#define FSR_EVENT_SIZE			8

#define FSR_RETRY_COUNT			10

#define CMD_READ_EVENT			0x85
#define CMD_SENSEON				0x93
#define CMD_TRIM_LOW_POWER_OSC	0x97
#define CMD_VERSION_INFO		0xAA

#define EID_ERROR				0x0F
#define EID_CONTROLLER_READY	0x10
#define EID_STATUS_EVENT		0x16
#define EID_INT_REL_INFO		0x14
#define EID_EXT_REL_INFO		0x15

#define EID_ERROR_FLASH_CORRUPTION		0x03

#define STATUS_EVENT_FORCE_CAL_DONE_D3			0x06

#define INT_ENABLE_D3					0x48
#define INT_DISABLE_D3					0x08

#define INT_ENABLE					0x01
#define INT_DISABLE					0x00

enum fsr_error_return {
	FSR_NOT_ERROR = 0,
	FSR_ERROR_INVALID_CHIP_ID,
	FSR_ERROR_INVALID_SW_VERSION,
	FSR_ERROR_EVENT_ID,
	FSR_ERROR_TIMEOUT,
	FSR_ERROR_FW_UPDATE_FAIL,
};

struct fsr_sidekey_plat_data {
	const char *firmware_name;
	const char *project_name;
	const char *model_name;

	u16 system_info_addr;

	int irq_gpio;	/* Interrupt GPIO */
	int rst_gpio;
	unsigned int irq_type;
	
#ifdef ENABLE_POWER_CONTROL	
	struct pinctrl *pinctrl;
	
	const char *regulator_dvdd;
	const char *regulator_avdd;
	
	int (*power)(void *data, bool on);
#endif
};

struct cx_data {
	short cx1;
	short cx2;
	int total;
};

struct fsr_system_info {
	u8 dummy;
	//Formosa&D3 Signature
    u16 D3Fmsasig;			///< 00 - D3&Formosa signature
    //System information
    u16 fwVer;				///< 02 - firmware version
    u8 cfgid0;				///< 04 - cfg id0
    u8 cfgid1;				///< 05 - cfg id 1
    u16 chipid;				///< 06 - D3 chip id
    u16 fmsaid;				///< 08 - Formosa chip id
    u16 reservedbuffer[9];	///< 0A - Reserved

    //Afe setting
    u8 linelen;				///< 1C - line length
    u8 afelen;				///< 1D - afe length

    //Force Touch Frames
    u16 rawFrcTouchAddr;	///< 1E - Touch raw frame address
    u16 filterFrcTouchAddr;	///< 20 - Touch filtered frame address
    u16 normFrcTouchAddr;	///< 22 - Touch normalized frame address
    u16 calibFrcTouchAddr;	///< 24 - Touch calibrated frame address

    //Compensation data
    u16 compensationAddr;  	///< 26 - Compensation frame address

    //
    u16 gpiostatusAddr;		///< 28 - GPIO status
} __attribute__((packed));

struct fsr_frame_data_info {
	short ch12;
	short ch06;
	short ch14;
	short ch07;
	short ch15;
	short ch08;
} __attribute__((packed));

struct fsr_sidekey_info {
	struct i2c_client *client;
	struct input_dev *input_dev;
	struct input_dev *input_dev_sidekey;
	
	int irq;
	int irq_type;
	bool irq_enabled;
	struct fsr_sidekey_plat_data *board;
	
	bool probe_done;
	
#ifdef ENABLE_POWER_CONTROL	
	int (*power)(void *data, bool on);
#endif	
	
	struct mutex i2c_mutex;
	
	const char *firmware_name;
	
	int product_id_of_ic;			/* product id of ic */
	int fw_version_of_ic;			/* firmware version of IC */
	int config_version_of_ic;		/* Config release data from IC */
	u16 fw_main_version_of_ic;		/* firmware main version of IC */
	
	int fw_version_of_bin;			/* firmware version of binary */
	int config_version_of_bin;		/* Config release data from IC */
	u16 fw_main_version_of_bin;		/* firmware main version of binary */
	
	struct sec_cmd_data sec;
	struct fsr_system_info fsr_sys_info;
	struct cx_data ch_cx_data[6];
	
	struct fsr_frame_data_info fsr_frame_data_raw;
	struct fsr_frame_data_info fsr_frame_data_delta;
	struct fsr_frame_data_info fsr_frame_data_reference;
};

int fsr_read_reg(struct fsr_sidekey_info *info, unsigned char *reg, int cnum,
		 unsigned char *buf, int num);
int fsr_write_reg(struct fsr_sidekey_info *info,
		  unsigned char *reg, unsigned short num_com);
void fsr_command(struct fsr_sidekey_info *info, unsigned char cmd);
int fsr_wait_for_ready(struct fsr_sidekey_info *info);
int fsr_get_version_info(struct fsr_sidekey_info *info);
int fsr_systemreset(struct fsr_sidekey_info *info, unsigned int delay);
void fsr_interrupt_set(struct fsr_sidekey_info *info, int enable);
void fsr_delay(unsigned int ms);

int fsr_fw_update_on_probe(struct fsr_sidekey_info *info);
int fsr_fw_update_on_hidden_menu(struct fsr_sidekey_info *info, int update_type);

 int fsr_functions_init(struct fsr_sidekey_info *info);
 void fsr_functions_remove(struct fsr_sidekey_info *info);

#endif /* _LINUX_STM_FSR_SIDEKEY_H_ */
