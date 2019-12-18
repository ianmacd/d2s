/* --------------- (C) COPYRIGHT 2012 STMicroelectronics ----------------------
 *
 * File Name	: stm_fsr_functions.c
 * Authors		: AMS(Analog Mems Sensor) Team
 * Description	: Firmware update for Strain gauge sidekey controller (Formosa + D3)
 *
 * -----------------------------------------------------------------------------
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * THE PRESENT SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES
 * OR CONDITIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED, FOR THE SOLE
 * PURPOSE TO SUPPORT YOUR APPLICATION DEVELOPMENT.
 * AS A RESULT, STMICROELECTRONICS SHALL NOT BE HELD LIABLE FOR ANY DIRECT,
 * INDIRECT OR CONSEQUENTIAL DAMAGES WITH RESPECT TO ANY CLAIMS ARISING FROM THE
 * CONTENT OF SUCH SOFTWARE AND/OR THE USE MADE BY CUSTOMERS OF THE CODING
 * INFORMATION CONTAINED HEREIN IN CONNECTION WITH THEIR PRODUCTS.
 *
 * THIS SOFTWARE IS SPECIFICALLY DESIGNED FOR EXCLUSIVE USE WITH ST PARTS.
 * -----------------------------------------------------------------------------
 * REVISON HISTORY
 * DATE		 | DESCRIPTION
 * 01/03/2019| First Release
 * 01/23/2019| Added functions 
 *           | - get_cx_data, run_cx_data_read
 *           | - get_rawcap, run_rawcap_read
 *           | - get_delta, run_delta_read
 *           | - get_reference, run_reference_read
 * -----------------------------------------------------------------------------
 */
 
#include "stm_fsr_sidekey.h"

#define BUFFER_MAX			((256 * 1024) - 16)

enum {
	TYPE_RAW_DATA,
	TYPE_BASELINE_DATA,
	TYPE_STRENGTH_DATA,
};

struct cx1_data {
	u8 data : 7;
	u8 sign : 1;	
} __attribute__((packed));

struct cx2_data {
	u8 data : 6;
	u8 sign : 1;	
	u8 reserved : 1;
} __attribute__((packed));

struct cx_data_raw {
	struct cx1_data cx1;
	struct cx2_data cx2;
} __attribute__((packed));

struct channel_cx_data_raw {
	u8 dummy;
	struct cx_data_raw ch12;
	u16 reserved1;
	struct cx_data_raw ch06;
	u16 reserved2[3];
	struct cx_data_raw ch13;
	u16 reserved3;
	struct cx_data_raw ch09;
	u16 reserved4[3];
	struct cx_data_raw ch14;
	u16 reserved5;
	struct cx_data_raw ch07;
	u16 reserved6[3];
	struct cx_data_raw ch15;
	u16 reserved7;
	struct cx_data_raw ch08;
} __attribute__((packed));

static void fw_update(void *device_data);
static void get_fw_ver_bin(void *device_data);
static void get_fw_ver_ic(void *device_data);
static void get_config_ver(void *device_data);
static void get_cx_data(void *device_data);
static void run_cx_data_read(void *device_data);
static void run_rawcap_read(void *device_data);
static void get_rawcap(void *device_data);
static void run_delta_read(void *device_data);
static void get_delta(void *device_data);
static void run_reference_read(void *device_data);
static void get_reference(void *device_data);
static void reset(void *device_data);

int fsr_read_frame(struct fsr_sidekey_info *info, u16 type, struct fsr_frame_data_info *data);

static struct sec_cmd sec_cmds[] = {
	{SEC_CMD("fw_update", fw_update),},
	{SEC_CMD("get_fw_ver_bin", get_fw_ver_bin),},
	{SEC_CMD("get_fw_ver_ic", get_fw_ver_ic),},
	{SEC_CMD("get_config_ver", get_config_ver),},
	{SEC_CMD("get_cx_data", get_cx_data),},
	{SEC_CMD("run_cx_data_read", run_cx_data_read),},
	{SEC_CMD("run_rawcap_read", run_rawcap_read),},
	{SEC_CMD("get_rawcap", get_rawcap),},
	{SEC_CMD("run_delta_read", run_delta_read),},
	{SEC_CMD("get_delta", get_delta),},
	{SEC_CMD("run_reference_read", run_reference_read),},
	{SEC_CMD("get_reference", get_reference),},
	{SEC_CMD("reset", reset),},
};

int fsr_functions_init(struct fsr_sidekey_info *info)
{
	int retval;
	u8 regAdd[3] = {0xD0, 0x00, 0x00};

	retval = sec_cmd_init(&info->sec, sec_cmds,
			ARRAY_SIZE(sec_cmds), SEC_CLASS_DEVT_TKEY);
	if (retval < 0) {
		dev_err(&info->client->dev,
				"%s: Failed to sec_cmd_init\n", __func__);
		goto exit;
	}
	
	// Read system info from IC
	regAdd[1] = (info->board->system_info_addr >> 8) & 0xff;
	regAdd[2] = (info->board->system_info_addr) & 0xff;
	retval = fsr_read_reg(info, &regAdd[0], 3, (u8*)&info->fsr_sys_info.dummy, sizeof(struct fsr_system_info));
	if (retval < 0) {
		dev_err(&info->client->dev,
				"%s: Failed to read system info from IC\n", __func__);
		goto exit;
	}

	return 0;

exit:
	return retval;
}

void fsr_functions_remove(struct fsr_sidekey_info *info)
{
	dev_err(&info->client->dev, "%s\n", __func__);

	sec_cmd_exit(&info->sec, SEC_CLASS_DEVT_TKEY);

}

static void fw_update(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct fsr_sidekey_info *info = container_of(sec, struct fsr_sidekey_info, sec);
	char buff[64] = { 0 };
	int retval = 0;

	sec_cmd_set_default_result(sec);

	retval = fsr_fw_update_on_hidden_menu(info, sec->cmd_param[0]);

	if (retval < 0) {
		snprintf(buff, sizeof(buff), "%s", "NG");
		sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
		dev_err(&info->client->dev, "%s: failed [%d]\n", __func__, retval);
	} else {
		snprintf(buff, sizeof(buff), "%s", "OK");
		sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
		sec->cmd_state = SEC_CMD_STATUS_OK;
		dev_info(&info->client->dev, "%s: success [%d]\n", __func__, retval);
	}
}

static void get_fw_ver_bin(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct fsr_sidekey_info *info = container_of(sec, struct fsr_sidekey_info, sec);
	char buff[16] = { 0 };

	sec_cmd_set_default_result(sec);

	snprintf(buff, sizeof(buff), "ST%04X",
			info->fw_main_version_of_bin);

	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	if (sec->cmd_all_factory_state == SEC_CMD_STATUS_RUNNING)
		sec_cmd_set_cmd_result_all(sec, buff, strnlen(buff, sizeof(buff)), "FW_VER_BIN");
	sec->cmd_state = SEC_CMD_STATUS_OK;
	dev_info(&info->client->dev, "%s: %s\n", __func__, buff);
}

static void get_fw_ver_ic(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct fsr_sidekey_info *info = container_of(sec, struct fsr_sidekey_info, sec);
	char buff[16] = { 0 };

	sec_cmd_set_default_result(sec);

	fsr_get_version_info(info);

	snprintf(buff, sizeof(buff), "ST%04X",
			info->fw_main_version_of_ic);

	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	if (sec->cmd_all_factory_state == SEC_CMD_STATUS_RUNNING)
		sec_cmd_set_cmd_result_all(sec, buff, strnlen(buff, sizeof(buff)), "FW_VER_IC");
	sec->cmd_state = SEC_CMD_STATUS_OK;
	dev_info(&info->client->dev, "%s: %s\n", __func__, buff);
}

static void get_config_ver(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct fsr_sidekey_info *info = container_of(sec, struct fsr_sidekey_info, sec);
	char buff[20] = { 0 };

	snprintf(buff, sizeof(buff), "%s_ST_%04X",
			info->board->model_name ?: info->board->project_name ?: "STM",
			info->config_version_of_ic);

	sec_cmd_set_default_result(sec);
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec->cmd_state = SEC_CMD_STATUS_OK;
	dev_info(&info->client->dev, "%s: %s\n", __func__, buff);
}

static int fsr_check_index(struct fsr_sidekey_info *info)
{
	struct sec_cmd_data *sec = &info->sec;
	char buff[SEC_CMD_STR_LEN] = { 0 };
	int node;

	if (sec->cmd_param[0] < 0 || sec->cmd_param[0] >= 6) {

		snprintf(buff, sizeof(buff), "%s", "NG");
		sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
		dev_err(&info->client->dev, "%s: parameter error: %u\n",
				__func__, sec->cmd_param[0]);
		node = -1;
		return node;
	}
	node = sec->cmd_param[0];
	dev_info(&info->client->dev, "%s: node = %d\n", __func__, node);
	return node;
}

void fsr_print_cx(struct fsr_sidekey_info *info)
{
	u8 ChannelName[6] = {6,12,7,14,8,15};
	u8 ChannelRemap[6] = {1,0,3,2,5,4};
	u8 pTmp[16] = { 0 };
	u8 *pStr = NULL;
	int i = 0;
	
	pStr = kzalloc(BUFFER_MAX, GFP_KERNEL);
	if (pStr == NULL)
		return;
	
	memset(pStr, 0x0, BUFFER_MAX);
	for (i = 0; i < 7; i++) {
		snprintf(pTmp, 8, "-------");
		strncat(pStr, pTmp, 8);
	}
	dev_info(&info->client->dev, "%s: %s\n", __func__, pStr);
	
	memset(pStr, 0x0, BUFFER_MAX);
	snprintf(pTmp, 8, "       ");
	strncat(pStr, pTmp, 8);
	for (i = 0; i < 6; i++) {
		snprintf(pTmp, 8, "  CH%02d  ", ChannelName[i]);
		strncat(pStr, pTmp, 8);
	}
	dev_info(&info->client->dev, "%s: %s\n", __func__, pStr);
	
	memset(pStr, 0x0, BUFFER_MAX);
	for (i = 0; i < 7; i++) {
		snprintf(pTmp, 8, "-------");
		strncat(pStr, pTmp, 8);
	}
	dev_info(&info->client->dev, "%s: %s\n", __func__, pStr);
	
	memset(pStr, 0x0, BUFFER_MAX);
	snprintf(pTmp, 7, " CX1  ");
	strncat(pStr, pTmp, 7);
	for (i = 0; i < 6; i++) {
		snprintf(pTmp, 8, " %6d", info->ch_cx_data[ChannelRemap[i]].cx1);
		strncat(pStr, pTmp, 8);
	}
	dev_info(&info->client->dev, "%s: %s\n", __func__, pStr);
	
	memset(pStr, 0x0, BUFFER_MAX);
	snprintf(pTmp, 7, " CX2  ");
	strncat(pStr, pTmp, 7);
	for (i = 0; i < 6; i++) {
		snprintf(pTmp, 8, " %6d", info->ch_cx_data[ChannelRemap[i]].cx2);
		strncat(pStr, pTmp, 8);
	}
	dev_info(&info->client->dev, "%s: %s\n", __func__, pStr);
	
	memset(pStr, 0x0, BUFFER_MAX);
	snprintf(pTmp, 7, " Total");
	strncat(pStr, pTmp, 7);
	for (i = 0; i < 6; i++) {
		snprintf(pTmp, 8, " %6d", info->ch_cx_data[ChannelRemap[i]].total);
		strncat(pStr, pTmp, 8);
	}
	dev_info(&info->client->dev, "%s: %s\n", __func__, pStr);
	
	memset(pStr, 0x0, BUFFER_MAX);
	for (i = 0; i < 7; i++) {
		snprintf(pTmp, 8, "-------");
		strncat(pStr, pTmp, 8);
	}
	dev_info(&info->client->dev, "%s: %s\n", __func__, pStr);
	
	kfree(pStr);
}

static void run_cx_data_read(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct fsr_sidekey_info *info = container_of(sec, struct fsr_sidekey_info, sec);
	u8 regAdd[3] = {0xD0, 0x00, 0x00};
	u8 ch_cx1_loc[6] = {0, 2, 12, 14, 18, 20};
	struct channel_cx_data_raw ch_cx_data;
	u16 cxAddr;
	short *data = (short *)&ch_cx_data.ch12;
	struct cx_data_raw *ch_data;
	char buff[64] = { 0 };
	int retval = 0;
	int i =0;

	sec_cmd_set_default_result(sec);

	cxAddr = info->fsr_sys_info.compensationAddr + 12; // Add offset value 12.
	regAdd[1] = (cxAddr >> 8) & 0xff;
	regAdd[2] = (cxAddr) & 0xff;
	retval = fsr_read_reg(info, &regAdd[0], 3, (u8*)&ch_cx_data.dummy, sizeof(struct channel_cx_data_raw));
	if (retval < 0) {
		snprintf(buff, sizeof(buff), "%s", "NG");
		sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
		dev_err(&info->client->dev, "%s: failed [%d]\n", __func__, retval);
	} else {
		for (i=0; i<6; i++)
		{
			ch_data = (struct cx_data_raw *)(data+ch_cx1_loc[i]);
			info->ch_cx_data[i].cx1 = (ch_data->cx1.sign?(-1*ch_data->cx1.data):ch_data->cx1.data);
			info->ch_cx_data[i].cx2 = (ch_data->cx2.sign?(-1*ch_data->cx2.data):ch_data->cx2.data);
			info->ch_cx_data[i].total = (info->ch_cx_data[i].cx1 * 60) + (info->ch_cx_data[i].cx2 * 25);
		}
		
		fsr_print_cx(info);
		
		snprintf(buff, sizeof(buff), "%s", "OK");
		sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
		sec->cmd_state = SEC_CMD_STATUS_OK;						
		dev_info(&info->client->dev, "%s: success [%d]\n", __func__, retval);
	}
}

static void get_cx_data(void *device_data)
{
	u8 ChannelRemap[6] = {1,0,3,2,5,4};
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct fsr_sidekey_info *info = container_of(sec, struct fsr_sidekey_info, sec);
	char buff[SEC_CMD_STR_LEN] = { 0 };
	int node = 0;

	sec_cmd_set_default_result(sec);

	node = fsr_check_index(info);
	if (node < 0)
		return;

	snprintf(buff, sizeof(buff), "%d, %d, %d", 
					info->ch_cx_data[ChannelRemap[node]].cx1,
					info->ch_cx_data[ChannelRemap[node]].cx2,
					info->ch_cx_data[ChannelRemap[node]].total);
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec->cmd_state = SEC_CMD_STATUS_OK;
	input_info(true, &info->client->dev, "%s: %s\n", __func__, buff);
}

void fsr_print_frame(struct fsr_sidekey_info *info, struct fsr_frame_data_info *data)
{
	u8 ChannelName[6] = {6,12,7,14,8,15};
	u8 ChannelRemap[6] = {1,0,3,2,5,4};
	u8 pTmp[16] = { 0 };
	u8 *pStr = NULL;
	short *fdata = (short *)data;
	int i = 0;
	
	pStr = kzalloc(BUFFER_MAX, GFP_KERNEL);
	if (pStr == NULL)
		return;
	
	memset(pStr, 0x0, BUFFER_MAX);
	for (i = 0; i < 6; i++) {
		snprintf(pTmp, 8, "-------");
		strncat(pStr, pTmp, 8);
	}
	dev_info(&info->client->dev, "%s: %s\n", __func__, pStr);
	
	memset(pStr, 0x0, BUFFER_MAX);
	for (i = 0; i < 6; i++) {
		snprintf(pTmp, 8, "  CH%02d ", ChannelName[i]);
		strncat(pStr, pTmp, 8);
	}
	dev_info(&info->client->dev, "%s: %s\n", __func__, pStr);
	
	memset(pStr, 0x0, BUFFER_MAX);
	for (i = 0; i < 6; i++) {
		snprintf(pTmp, 8, "-------");
		strncat(pStr, pTmp, 8);
	}
	dev_info(&info->client->dev, "%s: %s\n", __func__, pStr);
	
	memset(pStr, 0x0, BUFFER_MAX);
	for (i = 0; i < 6; i++) {
		snprintf(pTmp, 8, " %6d", fdata[ChannelRemap[i]]);
		strncat(pStr, pTmp, 8);
	}
	dev_info(&info->client->dev, "%s: %s\n", __func__, pStr);
	
	memset(pStr, 0x0, BUFFER_MAX);
	for (i = 0; i < 6; i++) {
		snprintf(pTmp, 8, "-------");
		strncat(pStr, pTmp, 8);
	}
	dev_info(&info->client->dev, "%s: %s\n", __func__, pStr);
	
	kfree(pStr);
}

int fsr_read_frame(struct fsr_sidekey_info *info, u16 type, struct fsr_frame_data_info *data)
{
	u8 regAdd[3] = {0xD0, 0x00, 0x00};
	u8 regData[6*2+1];
	u16 addr;
	int retval = 0;
	
	
	
	switch (type) {
	case TYPE_RAW_DATA:
		addr = info->fsr_sys_info.rawFrcTouchAddr;
		break;
	case TYPE_STRENGTH_DATA:
		addr = info->fsr_sys_info.normFrcTouchAddr;
		break;
	case TYPE_BASELINE_DATA:
		addr = info->fsr_sys_info.calibFrcTouchAddr;
		break;
	}
	
	regAdd[1] = (addr >> 8) & 0xff;
	regAdd[2] = (addr) & 0xff;
	
	retval = fsr_read_reg(info, &regAdd[0], 3, (u8*)regData, 6*2+1);
	if (retval <= 0) {
		dev_err(&info->client->dev, "%s: read failed at 0x%4x rc = %d\n", __func__, retval, type);
		return retval;
	}
	
	memcpy(data, &regData[1], 6*2);
	
	switch (type) {
	case TYPE_RAW_DATA:
		dev_info(&info->client->dev, "%s: [Raw Data]\n", __func__);
		break;
	case TYPE_STRENGTH_DATA:
		dev_info(&info->client->dev, "%s: [Strength Data]\n", __func__);
		break;
	case TYPE_BASELINE_DATA:
		dev_info(&info->client->dev, "%s: [Baseline Data]\n", __func__);
		break;
	}
	
	fsr_print_frame(info, data);
	
	return 0;
}

static void run_rawcap_read(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct fsr_sidekey_info *info = container_of(sec, struct fsr_sidekey_info, sec);
	char buff[SEC_CMD_STR_LEN] = { 0 };
	struct fsr_frame_data_info *fsr_frame_data = (struct fsr_frame_data_info *)&info->fsr_frame_data_raw;
	
	sec_cmd_set_default_result(sec);

	fsr_read_frame(info, TYPE_RAW_DATA, fsr_frame_data);
	snprintf(buff, sizeof(buff), "%d,%d,%d,%d,%d,%d", 
				(*fsr_frame_data).ch06, (*fsr_frame_data).ch12, 
				(*fsr_frame_data).ch07, (*fsr_frame_data).ch14,
				(*fsr_frame_data).ch08, (*fsr_frame_data).ch15);
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec->cmd_state = SEC_CMD_STATUS_OK;
	dev_info(&info->client->dev, "%s: %s\n", __func__, buff);
}

static void get_rawcap(void *device_data)
{
	u8 ChannelRemap[6] = {1,0,3,2,5,4};
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct fsr_sidekey_info *info = container_of(sec, struct fsr_sidekey_info, sec);
	char buff[SEC_CMD_STR_LEN] = { 0 };
	short *fdata = (short *)&info->fsr_frame_data_raw;
	short val = 0;
	int node = 0;
	
	sec_cmd_set_default_result(sec);
	
	node = fsr_check_index(info);
	if (node < 0)
		return;

	val = fdata[ChannelRemap[node]];
	snprintf(buff, sizeof(buff), "%d", val);
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec->cmd_state = SEC_CMD_STATUS_OK;
	dev_info(&info->client->dev, "%s: %s\n", __func__, buff);
}

static void run_delta_read(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct fsr_sidekey_info *info = container_of(sec, struct fsr_sidekey_info, sec);
	char buff[SEC_CMD_STR_LEN] = { 0 };
	struct fsr_frame_data_info *fsr_frame_data = (struct fsr_frame_data_info *)&info->fsr_frame_data_delta;
	
	sec_cmd_set_default_result(sec);

	fsr_read_frame(info, TYPE_STRENGTH_DATA, fsr_frame_data);
	snprintf(buff, sizeof(buff), "%d,%d,%d,%d,%d,%d", 
				(*fsr_frame_data).ch06, (*fsr_frame_data).ch12, 
				(*fsr_frame_data).ch07, (*fsr_frame_data).ch14,
				(*fsr_frame_data).ch08, (*fsr_frame_data).ch15);
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec->cmd_state = SEC_CMD_STATUS_OK;
	dev_info(&info->client->dev, "%s: %s\n", __func__, buff);
}

static void get_delta(void *device_data)
{
	u8 ChannelRemap[6] = {1,0,3,2,5,4};
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct fsr_sidekey_info *info = container_of(sec, struct fsr_sidekey_info, sec);
	char buff[SEC_CMD_STR_LEN] = { 0 };
	short *fdata = (short *)&info->fsr_frame_data_delta;
	short val = 0;
	int node = 0;
	
	sec_cmd_set_default_result(sec);
	
	node = fsr_check_index(info);
	if (node < 0)
		return;

	val = fdata[ChannelRemap[node]];
	snprintf(buff, sizeof(buff), "%d", val);
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec->cmd_state = SEC_CMD_STATUS_OK;
	dev_info(&info->client->dev, "%s: %s\n", __func__, buff);
}

static void run_reference_read(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct fsr_sidekey_info *info = container_of(sec, struct fsr_sidekey_info, sec);
	char buff[SEC_CMD_STR_LEN] = { 0 };
	struct fsr_frame_data_info *fsr_frame_data = (struct fsr_frame_data_info *)&info->fsr_frame_data_reference;
	
	sec_cmd_set_default_result(sec);

	fsr_read_frame(info, TYPE_BASELINE_DATA, fsr_frame_data);
	snprintf(buff, sizeof(buff), "%d,%d,%d,%d,%d,%d", 
				(*fsr_frame_data).ch06, (*fsr_frame_data).ch12, 
				(*fsr_frame_data).ch07, (*fsr_frame_data).ch14,
				(*fsr_frame_data).ch08, (*fsr_frame_data).ch15);
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec->cmd_state = SEC_CMD_STATUS_OK;
	dev_info(&info->client->dev, "%s: %s\n", __func__, buff);
}
static void get_reference(void *device_data)
{
	u8 ChannelRemap[6] = {1,0,3,2,5,4};
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct fsr_sidekey_info *info = container_of(sec, struct fsr_sidekey_info, sec);
	char buff[SEC_CMD_STR_LEN] = { 0 };
	short *fdata = (short *)&info->fsr_frame_data_reference;
	short val = 0;
	int node = 0;
	
	sec_cmd_set_default_result(sec);
	
	node = fsr_check_index(info);
	if (node < 0)
		return;

	val = fdata[ChannelRemap[node]];
	snprintf(buff, sizeof(buff), "%d", val);
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec->cmd_state = SEC_CMD_STATUS_OK;
	dev_info(&info->client->dev, "%s: %s\n", __func__, buff);
}

static void reset(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct fsr_sidekey_info *info = container_of(sec, struct fsr_sidekey_info, sec);
	char buff[SEC_CMD_STR_LEN] = { 0 };

	if (sec->cmd_param[0] == 0) {
		gpio_set_value(info->board->rst_gpio, 0);
		dev_err(&info->client->dev, "%s: set 0, rst_gpio:%d\n", __func__, gpio_get_value(info->board->rst_gpio));
		msleep(20);
		gpio_set_value(info->board->rst_gpio, 1);
		dev_err(&info->client->dev, "%s: set 1, rst_gpio:%d\n", __func__, gpio_get_value(info->board->rst_gpio));
	} else if (sec->cmd_param[0] == 1) {
		gpio_set_value(info->board->rst_gpio, 0);
		dev_err(&info->client->dev, "%s: set 0, rst_gpio:%d\n", __func__, gpio_get_value(info->board->rst_gpio));
	} else if (sec->cmd_param[0] == 2) {
		gpio_set_value(info->board->rst_gpio, 1);
		dev_err(&info->client->dev, "%s: set 1, rst_gpio:%d\n", __func__, gpio_get_value(info->board->rst_gpio));
	}

	snprintf(buff, sizeof(buff), "%s", "OK");
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec->cmd_state = SEC_CMD_STATUS_OK;
	dev_info(&info->client->dev, "%s: %s\n", __func__, buff);
}

