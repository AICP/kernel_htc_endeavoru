/*
 * Description:
 * Helper functions to support the tegra USB controller
 *
 * This program is free software; you can redistribute  it and/or modify it
 * under  the terms of  the GNU General  Public License as published by the
 * Free Software Foundation;  either version 2 of the  License, or (at your
 * option) any later version.
 */
#include <linux/fsl_devices.h>
#include <linux/platform_device.h>
#include <linux/err.h>
#include <linux/io.h>
#include <mach/usb_phy.h>
#include <mach/board_htc.h>
static struct tegra_usb_phy *phy;
static struct clk *udc_clk;
static struct clk *emc_clk;
static struct clk *sclk_clk;
static void *udc_base;

int fsl_udc_clk_init(struct platform_device *pdev)
{
	struct resource *res;
	int err;
	int instance;
	struct fsl_usb2_platform_data *pdata = pdev->dev.platform_data;


	udc_clk = clk_get(&pdev->dev, NULL);
	if (IS_ERR(udc_clk)) {
		dev_err(&pdev->dev, "Can't get udc clock\n");
		return PTR_ERR(udc_clk);
	}

	clk_enable(udc_clk);

	sclk_clk = clk_get(&pdev->dev, "sclk");
	if (IS_ERR(sclk_clk)) {
		dev_err(&pdev->dev, "Can't get sclk clock\n");
		err = PTR_ERR(sclk_clk);
		goto err_sclk;
	}

	//clk_set_rate(sclk_clk, 80000000);
	if(board_mfg_mode() == 0)/* normal mode */
		clk_set_rate(sclk_clk, 240000000);
	else
		clk_set_rate(sclk_clk, 80000000);
	clk_enable(sclk_clk);

	emc_clk = clk_get(&pdev->dev, "emc");
	if (IS_ERR(emc_clk)) {
		dev_err(&pdev->dev, "Can't get emc clock\n");
		err = PTR_ERR(emc_clk);
		goto err_emc;
	}

	clk_enable(emc_clk);
#ifdef CONFIG_ARCH_TEGRA_2x_SOC
	/* Set DDR busy hints to 150MHz. For Tegra 2x SOC, DDR rate is half of EMC rate */
	clk_set_rate(emc_clk, 300000000);
#else
	/* Set DDR busy hints to 100MHz. For Tegra 3x SOC DDR rate equals to EMC rate */
	//clk_set_rate(emc_clk, 100000000);
	if(board_mfg_mode() == 0)/* normal mode */
		clk_set_rate(emc_clk, 533000000);
	else
		clk_set_rate(emc_clk, 100000000);
#endif

	/* we have to remap the registers ourselves as fsl_udc does not
	 * export them for us.
	 */
	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!res) {
		err = -ENXIO;
		goto err0;
	}
	udc_base = ioremap(res->start, resource_size(res));
	if (!udc_base) {
		err = -ENOMEM;
		goto err0;
	}

	instance = pdev->id;
	if (instance == -1)
		instance = 0;

	phy = tegra_usb_phy_open(instance, udc_base, pdata->phy_config,
					TEGRA_USB_PHY_MODE_DEVICE, pdata->usb_phy_type);
	if (IS_ERR(phy)) {
		dev_err(&pdev->dev, "Can't open phy\n");
		err = PTR_ERR(phy);
		goto err1;
	}
	tegra_usb_phy_power_on(phy, true);

	return 0;
err1:
	iounmap(udc_base);
err0:
	clk_disable(emc_clk);
	clk_put(emc_clk);
err_emc:
	clk_disable(sclk_clk);
	clk_put(sclk_clk);
err_sclk:
	clk_disable(udc_clk);
	clk_put(udc_clk);
	return err;
}

void fsl_udc_clk_finalize(struct platform_device *pdev)
{
}

void fsl_udc_clk_release(void)
{
	tegra_usb_phy_close(phy);

	iounmap(udc_base);

	clk_disable(udc_clk);
	clk_put(udc_clk);

	clk_disable(sclk_clk);
	clk_put(sclk_clk);

	clk_disable(emc_clk);
	clk_put(emc_clk);
}

void fsl_udc_clk_suspend(bool is_dpd)
{
	pr_info("[USBUDC] fsl_udc_clk_suspend is_dpd:%d \n",is_dpd);
	tegra_usb_phy_power_off(phy, is_dpd);
	clk_disable(udc_clk);
	clk_disable(sclk_clk);
	clk_disable(emc_clk);
}

void fsl_udc_clk_resume(bool is_dpd)
{
	pr_info("[USBUDC] fsl_udc_clk_resume is_dpd:%d \n",is_dpd);
	clk_enable(emc_clk);
	clk_enable(sclk_clk);
	clk_enable(udc_clk);
	tegra_usb_phy_power_on(phy,  is_dpd);
}

void fsl_udc_clk_enable(void)
{
	clk_enable(udc_clk);
}

void fsl_udc_clk_disable(void)
{
	clk_disable(udc_clk);
}

void fsl_udc_clk_pull_high(bool pull_high)
{
	if (pull_high) {
		clk_set_rate(sclk_clk, 240000000);;
		clk_set_rate(emc_clk, 533000000);
	}
	else {
		clk_set_rate(sclk_clk, 80000000);
		clk_set_rate(emc_clk, 100000000);
	}
}
EXPORT_SYMBOL(fsl_udc_clk_pull_high);

bool fsl_udc_charger_detect(void)
{
	return tegra_usb_phy_charger_detect(phy);
}
