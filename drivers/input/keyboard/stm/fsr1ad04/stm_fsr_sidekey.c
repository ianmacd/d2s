/* --------------- (C) COPYRIGHT 2012 STMicroelectronics ----------------------
 *
 * File Name	: stm_fsr_sidekey.c
 * Authors		: AMS(Analog Mems Sensor) Team
 * Description	: Strain gauge sidekey controller (Formosa + D3)
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
 * 01/23/2019| Changed release information to little endian.
 *           | Get system_info_addr from device tree
 *           | Enabled fsr_functions_init function
 * 01/28/2019| Add Reset in fsr_init function
 * -----------------------------------------------------------------------------
 */

#include "stm_fsr_sidekey.h"

int fsr_read_reg(struct fsr_sidekey_info *info, unsigned char *reg, int cnum,
		 unsigned char *buf, int num)
{
	struct i2c_msg xfer_msg[2];
	int ret;

	mutex_lock(&info->i2c_mutex);

	xfer_msg[0].addr = info->client->addr;
	xfer_msg[0].len = cnum;
	xfer_msg[0].flags = 0;
	xfer_msg[0].buf = reg;

	xfer_msg[1].addr = info->client->addr;
	xfer_msg[1].len = num;
	xfer_msg[1].flags = I2C_M_RD;
	xfer_msg[1].buf = buf;

	ret = i2c_transfer(info->client->adapter, xfer_msg, 2);

	mutex_unlock(&info->i2c_mutex);
	return ret;
}

int fsr_write_reg(struct fsr_sidekey_info *info,
		  unsigned char *reg, unsigned short num_com)
{
	struct i2c_msg xfer_msg[2];
	int ret;

	mutex_lock(&info->i2c_mutex);

	xfer_msg[0].addr = info->client->addr;
	xfer_msg[0].len = num_com;
	xfer_msg[0].flags = 0;
	xfer_msg[0].buf = reg;

	ret = i2c_transfer(info->client->adapter, xfer_msg, 1);

	mutex_unlock(&info->i2c_mutex);
	return ret;
}

void fsr_delay(unsigned int ms)
{
	if (ms < 20)
		usleep_range(ms * 1000, ms * 1000);
	else
		msleep(ms);
}

void fsr_command(struct fsr_sidekey_info *info, unsigned char cmd)
{
	unsigned char regAdd = 0;
	int ret = 0;

	regAdd = cmd;
	ret = fsr_write_reg(info, &regAdd, 1);
	dev_info(&info->client->dev, "FSR Command (%02X) , ret = %d \n", cmd, ret);
}

void fsr_interrupt_set(struct fsr_sidekey_info *info, int enable)
{
	unsigned char regAdd[4] = { 0xB6, 0x00, 0x2C, enable };

	if (enable) {
		regAdd[3] = INT_ENABLE_D3;
		dev_info(&info->client->dev, "%s: Enable\n", __func__);
	} else {
		regAdd[3] = INT_DISABLE_D3;
		dev_info(&info->client->dev, "%s: Disable\n", __func__);
	}

	fsr_write_reg(info, &regAdd[0], 4);
}

static int fsr_check_chip_id(struct fsr_sidekey_info *info) {

	unsigned char regAdd[3] = {0xB6, 0x00, 0x04};
	unsigned char val[7] = {0};
	int ret;

	ret = fsr_read_reg(info, regAdd, 3, (unsigned char *)val, 7);
	if (ret < 0) {
		dev_err(&info->client->dev, "%s failed. ret: %d\n",
			__func__, ret);
		return ret;
	}

	dev_info(&info->client->dev, "FTS %02X%02X%02X =  %02X %02X %02X %02X %02X %02X\n",
	       regAdd[0], regAdd[1], regAdd[2], val[1], val[2], val[3], val[4], val[5], val[6]);

	if(val[1] == FSR_ID0 && val[2] == FSR_ID1)
	{
		dev_info(&info->client->dev,"FTS Chip ID : %02X %02X\n", val[1], val[2]);

	}
	else
		return -FSR_ERROR_INVALID_CHIP_ID;

	return ret;
}

int fsr_wait_for_ready(struct fsr_sidekey_info *info)
{
	int rc;
	unsigned char regAdd;
	unsigned char data[FSR_EVENT_SIZE];
	int retry = 0;
	int err_cnt = 0;

	memset(data, 0x0, FSR_EVENT_SIZE);

	regAdd = CMD_READ_EVENT;
	rc = -1;
	while (fsr_read_reg(info, &regAdd, 1, (unsigned char *)data, FSR_EVENT_SIZE)) {
		if (data[0] == EID_CONTROLLER_READY) {
			rc = 0;
			break;
		}

		if (data[0] == EID_ERROR) {
			if (data[1] == EID_ERROR_FLASH_CORRUPTION) {
				rc = -FSR_ERROR_EVENT_ID;
				dev_err(&info->client->dev, "%s: flash corruption:%02X,%02X,%02X\n",
						__func__, data[0], data[1], data[2]);
				break;
			}

			if (err_cnt++ > 32) {
				rc = -FSR_ERROR_EVENT_ID;
				break;
			}
			continue;
		}

		if (retry++ > FSR_RETRY_COUNT) {
			rc = -FSR_ERROR_TIMEOUT;
			dev_err(&info->client->dev, "%s: Time Over\n", __func__);

			break;
		}
		fsr_delay(20);
	}

	dev_info(&info->client->dev,
		"%s: %02X, %02X, %02X, %02X, %02X, %02X, %02X, %02X\n",
		__func__, data[0], data[1], data[2], data[3],
		data[4], data[5], data[6], data[7]);

	return rc;
}

void fsr_reset(struct fsr_sidekey_info *info)
{
	dev_err(&info->client->dev, "%s: rst_gpio:%d\n", __func__, gpio_get_value(info->board->rst_gpio));
	gpio_set_value(info->board->rst_gpio, 0);
	dev_err(&info->client->dev, "%s: set 0, rst_gpio:%d\n", __func__, gpio_get_value(info->board->rst_gpio));
	msleep(20);
	gpio_set_value(info->board->rst_gpio, 1);
	dev_err(&info->client->dev, "%s: set 1, rst_gpio:%d\n", __func__, gpio_get_value(info->board->rst_gpio));
	msleep(20);
}

int fsr_systemreset(struct fsr_sidekey_info *info, unsigned int delay)
{
	unsigned char regAdd[4] = { 0xB6, 0x00, 0x28, 0x80 };
	int rc;

	dev_err(&info->client->dev, "%s\n", __func__);

//	fsr_reset(info);

	fsr_write_reg(info, &regAdd[0], 4);
	fsr_delay(delay);
	rc = fsr_wait_for_ready(info);

	return rc;
}

int fsr_get_version_info(struct fsr_sidekey_info *info)
{
	int rc = 0;
	unsigned char regAdd[3];
	unsigned char data[FSR_EVENT_SIZE];
	int retry = 0;

	fsr_command(info, CMD_VERSION_INFO);

	memset(data, 0x0, FSR_EVENT_SIZE);

	regAdd[0] = CMD_READ_EVENT;
	while (fsr_read_reg(info, &regAdd[0], 1, (unsigned char *)data, FSR_EVENT_SIZE)) {
		if (data[0] == EID_INT_REL_INFO) {
			// Internal release Information
			info->fw_version_of_ic = (data[3] << 8) + data[4];
			info->config_version_of_ic = (data[6] << 8) + data[5];
			info->product_id_of_ic = data[2];
			continue;
		} else if (data[0] == EID_EXT_REL_INFO) {
			// External release Information
			info->fw_main_version_of_ic = (data[2] << 8) + data[1];
			break;
		}

		if (retry++ > FSR_RETRY_COUNT) {
			rc = -FSR_ERROR_TIMEOUT;
			dev_err(&info->client->dev, "%s: Time Over\n", __func__);
			break;
		}
	}

	dev_info(&info->client->dev,
			"IC product id : 0x%02X "
			"IC Firmware Version : 0x%04X "
			"IC Config Version : 0x%04X "
			"IC Main Version : 0x%04X\n",
			info->product_id_of_ic,
			info->fw_version_of_ic,
			info->config_version_of_ic,
			info->fw_main_version_of_ic);

	return rc;
}

#ifdef ENABLE_POWER_CONTROL
static int fsr_power_ctrl(void *data, bool on)
{
	struct fsr_sidekey_info *info = (struct fsr_sidekey_info *)data;
	const struct fsr_sidekey_plat_data *pdata = info->board;
	struct device *dev = &info->client->dev;
	struct regulator *regulator_dvdd = NULL;
	struct regulator *regulator_avdd = NULL;
	static bool enabled;
	int retval = 0;

	if (enabled == on)
		return retval;

	regulator_dvdd = regulator_get(NULL, pdata->regulator_dvdd);
	if (IS_ERR_OR_NULL(regulator_dvdd)) {
		dev_err(dev, "%s: Failed to get %s regulator\n",
			 __func__, pdata->regulator_dvdd);
		goto out;
	}

	regulator_avdd = regulator_get(NULL, pdata->regulator_avdd);
	if (IS_ERR_OR_NULL(regulator_avdd)) {
		dev_err(dev, "%s: Failed to get %s regulator\n",
			 __func__, pdata->regulator_avdd);
		goto out;
	}

	if (on) {
		retval = regulator_enable(regulator_avdd);
		if (retval) {
			dev_err(dev, "%s: Failed to enable avdd: %d\n", __func__, retval);
			regulator_disable(regulator_avdd);
			goto out;
		}

		fsr_delay(1);

		retval = regulator_enable(regulator_dvdd);
		if (retval) {
			dev_err(dev, "%s: Failed to enable vdd: %d\n", __func__, retval);
			regulator_disable(regulator_dvdd);
			regulator_disable(regulator_avdd);
			goto out;
		}

		fsr_delay(5);
	} else {
		regulator_disable(regulator_dvdd);
		regulator_disable(regulator_avdd);
	}

	enabled = on;

	dev_err(dev, "%s: %s: avdd:%s, dvdd:%s\n", __func__, on ? "on" : "off",
		regulator_is_enabled(regulator_avdd) ? "on" : "off",
		regulator_is_enabled(regulator_dvdd) ? "on" : "off");

out:
	regulator_put(regulator_dvdd);
	regulator_put(regulator_avdd);

	return retval;
}
#endif /* ENABLE_POWER_CONTROL */

static int fsr_parse_dt(struct i2c_client *client)
{	
	int retval = 0;
	struct device *dev = &client->dev;
	struct fsr_sidekey_plat_data *pdata = dev->platform_data;
	struct device_node *np = dev->of_node;
	
	pdata->irq_gpio = of_get_named_gpio(np, "stm,irq_gpio", 0);
	if (gpio_is_valid(pdata->irq_gpio)) {
		retval = gpio_request(pdata->irq_gpio, "stm,sidekey_int");
		if (retval) {
			dev_err(dev, "Unable to request tsp_int [%d]\n", pdata->irq_gpio);
		//	return -EINVAL;
		}
	} else {
		dev_err(dev, "Failed to get irq gpio\n");
	//	return -EINVAL;
	}
	client->irq = gpio_to_irq(pdata->irq_gpio);
	dev_err(dev, "%s: irq_gpio:%d, irq:%d\n", __func__, pdata->irq_gpio, client->irq);

	if (of_property_read_u32(np, "stm,irq_type", &pdata->irq_type)) {
		dev_err(dev, "Failed to get irq_type property\n");
	//	return -EINVAL;
	}

	pdata->rst_gpio = of_get_named_gpio(np, "stm,rst_gpio", 0);
	if (gpio_is_valid(pdata->rst_gpio)) {
		dev_err(dev, "%s: reset before1 %d\n", __func__, gpio_get_value(pdata->rst_gpio));
		gpio_direction_output(pdata->rst_gpio, 1);
		dev_err(dev, "%s: reset after1 %d\n", __func__, gpio_get_value(pdata->rst_gpio));
	}
	dev_err(dev, "%s: rst_gpio:%d\n", __func__, pdata->rst_gpio);
#ifdef ENABLE_POWER_CONTROL	
	if (of_property_read_string(np, "stm,regulator_dvdd", &pdata->regulator_dvdd)) {
		dev_err(dev,  "%s: Failed to get regulator_dvdd name property\n", __func__);
		return -EINVAL;
	}

	if (of_property_read_string(np, "stm,regulator_avdd", &pdata->regulator_avdd)) {
		dev_err(dev,  "%s: Failed to get regulator_avdd name property\n", __func__);
		return -EINVAL;
	}
	pdata->power = fsr_power_ctrl;
#endif /* ENABLE_POWER_CONTROL */	

	if (of_property_read_string_index(np, "stm,firmware_name", 0, &pdata->firmware_name)) 
		dev_err(dev, "%s: skipped to get firmware_name property\n", __func__);
	dev_err(dev, "%s: firmware_name: %s\n", __func__, pdata->firmware_name);
	if (of_property_read_string_index(np, "stm,project_name", 0, &pdata->project_name))
		dev_err(dev, "%s: skipped to get project_name property\n", __func__);
	if (of_property_read_string_index(np, "stm,project_name", 1, &pdata->model_name))
		dev_err(dev, "%s: skipped to get model_name property\n", __func__);

	return 0;	
//	return retval;
}

static int fsr_setup_drv_data(struct i2c_client *client) 
{
	int retval = 0;
	struct fsr_sidekey_plat_data *pdata;
	struct fsr_sidekey_info *info;
	
	/* parse dt */
	if (client->dev.of_node) {
		pdata = devm_kzalloc(&client->dev,
			sizeof(struct fsr_sidekey_plat_data), GFP_KERNEL);

		if (!pdata) {
			dev_err(&client->dev, "%s: Failed to allocate platform data\n", __func__);
			return -ENOMEM;
		}

		client->dev.platform_data = pdata;
		retval = fsr_parse_dt(client);
		if (retval) {
			dev_err(&client->dev, "%s: Failed to parse dt\n", __func__);
			return retval;
		}
	} else {
		pdata = client->dev.platform_data;
	}

	if (!pdata) {
		dev_err(&client->dev, "%s: No platform data found\n", __func__);
		return -EINVAL;
	}
	
	info = kzalloc(sizeof(struct fsr_sidekey_info), GFP_KERNEL);
	if (!info)
		return -ENOMEM;

	info->client = client;
	info->board = pdata;
	info->irq = client->irq;
	info->irq_type = info->board->irq_type;
	info->irq_enabled = false;
	
	i2c_set_clientdata(client, info);
	
	return retval;
}

/*static irqreturn_t fsr_interrupt_handler(int irq, void *handle)
{
	// Please implement here to handle interrupt.
	
	return IRQ_HANDLED;
}*/

int fsr_irq_enable(struct fsr_sidekey_info *info,
		bool enable)
{
	int retval = 0;

	if (enable) {
		if (info->irq_enabled)
			return retval;

	/*	retval = request_threaded_irq(info->irq, NULL,
				fsr_interrupt_handler, info->irq_type,
				STM_FSR_DRV_NAME, info);
		if (retval < 0) {
			dev_err(&info->client->dev,
					"%s: Failed to create irq thread %d\n",
					__func__, retval);
			return retval;
		}*/

		info->irq_enabled = true;
	} else {
		if (info->irq_enabled) {
			disable_irq(info->irq);
			free_irq(info->irq, info);
			info->irq_enabled = false;
		}
	}

	return retval;
}


static int fsr_init(struct fsr_sidekey_info *info)
{
	int rc;

	fsr_systemreset(info, 10);

	rc = fsr_check_chip_id(info);
	if (rc < 0)
	{
		dev_err(&info->client->dev, "Please check the IC for sidekey!!!");
		return FSR_ERROR_INVALID_CHIP_ID;
	}
	
	info->product_id_of_ic = 0;
	info->fw_version_of_ic = 0;
	info->config_version_of_ic = 0;
	info->fw_main_version_of_ic = 0;

	msleep(200);

	rc = fsr_get_version_info(info);
	if (rc < 0)
	{
		dev_err(&info->client->dev, "Fail to get version information!!!");
	}
	
	rc  = fsr_fw_update_on_probe(info);
	if (rc  < 0)
		dev_err(&info->client->dev, "%s: Failed to firmware update\n",
				__func__);
	
	return FSR_NOT_ERROR;
}

static int fsr_probe(struct i2c_client *client, const struct i2c_device_id *idp)
{
	int retval;
	struct fsr_sidekey_info *info = NULL;
	
	dev_info(&client->dev, "%s: STM Sidekey Driver [%s]\n", __func__,
			STM_FSR_DRV_VERSION);
			
	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		dev_err(&client->dev, "%s: FSR err = EIO!\n", __func__);
		return -EIO;
	}
	
	/* Build up driver data */
	retval = fsr_setup_drv_data(client);
	if (retval < 0) {
		dev_err(&client->dev, "%s: Failed to set up driver data\n", __func__);
		goto err_setup_drv_data;
	}
	
	info = (struct fsr_sidekey_info *)i2c_get_clientdata(client);
	if (!info) {
		dev_err(&client->dev, "%s: Failed to get driver data\n", __func__);
		retval = -ENODEV;
		goto err_get_drv_data;
	}
	
	info->probe_done = false;
	
#ifdef ENABLE_POWER_CONTROL	
	if (info->board->power)
		info->board->power(info, true);
#endif /* ENABLE_POWER_CONTROL */
	
	mutex_init(&info->i2c_mutex);
	
	retval = fsr_init(info);
	if (retval) {
		dev_err(&info->client->dev, "%s: fsr_init fail!\n", __func__);
		goto err_fsr_init;
	}

	retval = fsr_irq_enable(info, true);
	if (retval < 0) {
		dev_err(&info->client->dev,
				"%s: Failed to enable attention interrupt\n",
				__func__);
		goto err_enable_irq;
	}
	
	fsr_functions_init(info);
	
	info->probe_done = true;
	
	dev_err(&info->client->dev, "%s: done\n", __func__);
	input_log_fix();

	dev_info(&client->dev, "%s: Init Finished.\n", __func__);

	return 0;
	
err_enable_irq:

	kfree(info);
err_fsr_init:
	mutex_destroy(&info->i2c_mutex);

#ifdef ENABLE_POWER_CONTROL	
	if (info->board->power)
		info->board->power(info, false);
#endif /* ENABLE_POWER_CONTROL */
	
err_get_drv_data:
err_setup_drv_data:
	dev_err(&client->dev, "%s: failed(%d)\n", __func__, retval);
	input_log_fix();
	return retval;
}

static int fsr_remove(struct i2c_client *client)
{
	struct fsr_sidekey_info *info = i2c_get_clientdata(client);
	
	dev_info(&info->client->dev, "%s\n", __func__);
	
	fsr_functions_remove(info);
	
/*	info->input_dev = info->input_dev_sidekey;
	input_unregister_device(info->input_dev);
	info->input_dev = NULL;
	info->input_dev_sidekey = NULL;*/
		
	kfree(info);

	return 0;
}

static void fsr_shutdown(struct i2c_client *client)
{
	struct fsr_sidekey_info *info = i2c_get_clientdata(client);

	dev_info(&info->client->dev, "%s\n", __func__);

	fsr_remove(client);
}

#ifdef CONFIG_PM
static int fsr_pm_suspend(struct device *dev)
{
	struct fsr_sidekey_info *info = dev_get_drvdata(dev);

	dev_dbg(&info->client->dev, "%s\n", __func__);

/*#ifdef USE_OPEN_CLOSE
	if (info->input_dev) {
		int retval = mutex_lock_interruptible(&info->input_dev->mutex);

		if (retval) {
			dev_err(true, &info->client->dev,
					"%s : mutex error\n", __func__);
			goto out;
		}

		if (!info->input_dev->disabled) {
			info->input_dev->disabled = true;
			if (info->input_dev->users && info->input_dev->close) {
				dev_err(true, &info->client->dev,
						"%s called without input_close\n",
						__func__);
				info->input_dev->close(info->input_dev);
			}
			info->input_dev->users = 0;
		}

		mutex_unlock(&info->input_dev->mutex);
	}
out:
#endif*/
	//if (info->fts_power_state > FSR_POWER_STATE_POWERDOWN)
	//	reinit_completion(&info->resume_done);

	return 0;
}

static int fsr_pm_resume(struct device *dev)
{
	struct fsr_sidekey_info *info = dev_get_drvdata(dev);

	dev_dbg(&info->client->dev, "%s\n", __func__);

	//if (info->fts_power_state > FSR_POWER_STATE_POWERDOWN)
	//	complete_all(&info->resume_done);

	return 0;
}
#endif

#if (!defined(CONFIG_PM)) && !defined(USE_OPEN_CLOSE)
static int fsr_suspend(struct i2c_client *client, pm_message_t mesg)
{
	struct fsr_sidekey_info *info = i2c_get_clientdata(client);

	dev_dbg(&info->client->dev, "%s\n", __func__);

	//fsr_stop_device(info, info->lowpower_flag);

	return 0;
}

static int fsr_resume(struct i2c_client *client)
{

	struct fsr_sidekey_info *info = i2c_get_clientdata(client);

	dev_dbg(&info->client->dev, "%s\n", __func__);

	//fsr_start_device(info);

	return 0;
}
#endif

static const struct i2c_device_id fsr_device_id[] = {
	{STM_FSR_DRV_NAME, 0},
	{}
};

#ifdef CONFIG_PM
static const struct dev_pm_ops fsr_dev_pm_ops = {
	.suspend = fsr_pm_suspend,
	.resume = fsr_pm_resume,
};
#endif

#ifdef CONFIG_OF
static const struct of_device_id fsr_match_table[] = {
	{.compatible = "stm,fsr_sidekey",},
	{},
}; 
#else
#define fsr_match_table NULL
#endif

static struct i2c_driver fsr_i2c_driver = {
	.driver = {
		   .name = STM_FSR_DRV_NAME,
		   .owner = THIS_MODULE,
#ifdef CONFIG_OF
		   .of_match_table = fsr_match_table,
#endif
#ifdef CONFIG_PM
		   .pm = &fsr_dev_pm_ops,
#endif
		   },
	.probe = fsr_probe,
	.remove = fsr_remove,
	.shutdown = fsr_shutdown,
#if (!defined(CONFIG_PM)) && !defined(USE_OPEN_CLOSE)
	.suspend = fsr_suspend,
	.resume = fsr_resume,
#endif
	.id_table = fsr_device_id,
};
 
static int __init fsr_driver_init(void)
{
	pr_err("%s %s\n", SECLOG, __func__);
	return i2c_add_driver(&fsr_i2c_driver);
}

static void __exit fsr_driver_exit(void)
{
	i2c_del_driver(&fsr_i2c_driver);
}

MODULE_DESCRIPTION("STMicroelectronics Sidekey Driver");
MODULE_AUTHOR("STMicroelectronics, Inc.");
MODULE_LICENSE("GPL v2");

module_init(fsr_driver_init);
module_exit(fsr_driver_exit);
