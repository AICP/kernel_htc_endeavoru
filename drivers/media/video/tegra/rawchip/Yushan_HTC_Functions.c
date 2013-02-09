#include <media/rawchip/Yushan_API.h>
#include <media/rawchip/Yushan_HTC_Functions.h>
#include <media/rawchip/Yushan_Platform_Specific.h>
#include <mach/board_htc.h>
extern bool_t	gPllLocked;


extern atomic_t interrupt, interrupt2;
extern struct yushan_int_t yushan_int;

Yushan_DXO_DPP_Tuning_t sDxoDppTuning;
Yushan_DXO_PDP_Tuning_t sDxoPdpTuning;
Yushan_DXO_DOP_Tuning_t sDxoDopTuning;
Yushan_New_Context_Config_t 	sYushanVideoContextConfig;
Yushan_New_Context_Config_t 	sYushanFullContextConfig;
Yushan_New_Context_Config_t 	sYushanVideoFastContextConfig;
Yushan_New_Context_Config_t 	sYushanQTRContextConfig;

Yushan_ImageChar_t	sImageChar_context;
static struct yushan_reg_t *yushan_regs = NULL;

/* Each Block Enable Flag*/
#define PDP_enable 0x01
#define black_level_enable 0x01
#define dead_pixel_enable 0x01
#define DOP_enable 0x01
#define denoise_enable 0x01
#define DPP_enable 0x01

uint8_t bPdpMode = PDP_enable ? (0x01|(((~black_level_enable)&0x01)<<3)|(((~dead_pixel_enable)&0x01)<<4)) : 0;
uint8_t bDppMode = DPP_enable ? 0x03 : 0;
uint8_t bDopMode = DOP_enable ? (0x01|(((~denoise_enable)&0x01)<<4)) : 0;

void Reset_Yushan(void)
{
	uint8_t bStatus;
	uint8_t	bSpiData;
	Yushan_Init_Dxo_Struct_t	sDxoStruct;
    Yushan_GainsExpTime_t sGainsExpTime;

	if (yushan_regs == NULL) {
		pr_err("[CAM] the sendor id is not defined, no calibration data\n");
		return;
	}

	sDxoStruct.pDxoPdpRamImage[0] = (uint8_t *)yushan_regs->pdpcode;
	sDxoStruct.pDxoDppRamImage[0] = (uint8_t *)yushan_regs->dppcode;
	sDxoStruct.pDxoDopRamImage[0] = (uint8_t *)yushan_regs->dopcode;
	sDxoStruct.pDxoPdpRamImage[1] = (uint8_t *)yushan_regs->pdpclib;
	sDxoStruct.pDxoDppRamImage[1] = (uint8_t *)yushan_regs->dppclib;
	sDxoStruct.pDxoDopRamImage[1] = (uint8_t *)yushan_regs->dopclib;

	sDxoStruct.uwDxoPdpRamImageSize[0] = yushan_regs->pdpcode_size;
	sDxoStruct.uwDxoDppRamImageSize[0] = yushan_regs->dppcode_size;
	sDxoStruct.uwDxoDopRamImageSize[0] = yushan_regs->dopcode_size;
	sDxoStruct.uwDxoPdpRamImageSize[1] = yushan_regs->pdpclib_size;
	sDxoStruct.uwDxoDppRamImageSize[1] = yushan_regs->dppclib_size;
	sDxoStruct.uwDxoDopRamImageSize[1] = yushan_regs->dopclib_size;

	sDxoStruct.uwBaseAddrPdpMicroCode[0] = yushan_regs->pdpcode_first_addr;
	sDxoStruct.uwBaseAddrDppMicroCode[0] = yushan_regs->dppcode_first_addr;
	sDxoStruct.uwBaseAddrDopMicroCode[0] = yushan_regs->dopcode_first_addr;
	sDxoStruct.uwBaseAddrPdpMicroCode[1] = yushan_regs->pdpclib_first_addr;
	sDxoStruct.uwBaseAddrDppMicroCode[1] = yushan_regs->dppclib_first_addr;
	sDxoStruct.uwBaseAddrDopMicroCode[1] = yushan_regs->dopclib_first_addr;

	sDxoStruct.uwDxoPdpBootAddr = yushan_regs->pdpBootAddr;
	sDxoStruct.uwDxoDppBootAddr = yushan_regs->dppBootAddr;
	sDxoStruct.uwDxoDopBootAddr = yushan_regs->dopBootAddr;

	sDxoStruct.uwDxoPdpStartAddr = yushan_regs->pdpStartAddr;
	sDxoStruct.uwDxoDppStartAddr = yushan_regs->dppStartAddr;
	sDxoStruct.uwDxoDopStartAddr = yushan_regs->dopStartAddr;
	
	sGainsExpTime.uwAnalogGainCodeGR = 0x20; /* 0x0 10x=>140; 1x=>20 */
	sGainsExpTime.uwAnalogGainCodeR = 0x20;
	sGainsExpTime.uwAnalogGainCodeB = 0x20;
	sGainsExpTime.uwPreDigGainGR = 0x100;
	sGainsExpTime.uwPreDigGainR = 0x100;
	sGainsExpTime.uwPreDigGainB = 0x100;
	sGainsExpTime.uwExposureTime = 0x20;
	sGainsExpTime.bRedGreenRatio = 0x40;
	sGainsExpTime.bBlueGreenRatio = 0x40;

	pr_info("[CAM] %s\n",__func__);

	Yushan_Assert_Reset(0x001F0F10, RESET_MODULE);
	bSpiData =1;
	Yushan_DXO_Sync_Reset_Dereset(bSpiData);
	Yushan_Assert_Reset(0x001F0F10, DERESET_MODULE);
	bSpiData = 0;
	Yushan_DXO_Sync_Reset_Dereset(bSpiData);
	Yushan_Init_Dxo(&sDxoStruct, 1);
	mdelay(10);

	Yushan_Update_ImageChar(&sImageChar_context);
	Yushan_Update_SensorParameters(&sGainsExpTime);
	Yushan_Update_DxoDpp_TuningParameters(&sDxoDppTuning);
	Yushan_Update_DxoDop_TuningParameters(&sDxoDopTuning);
	Yushan_Update_DxoPdp_TuningParameters(&sDxoPdpTuning);
	bStatus = Yushan_Update_Commit(bPdpMode, bDppMode, bDopMode);
	if (bStatus == 1)
		pr_info("[CAM] DXO Commit Done\n");
	else {
		pr_err("[CAM] DXO Commit FAILED\n");
	}

	/*Yushan_sensor_open_init();*/
	/*Yushan_ContextUpdate_Wrapper(&sYushanFullContextConfig);*/
	/*sensor_streaming_on();*/
}

void ASIC_Test(void)
{
	pr_info("[CAM] ASIC_Test E\n");
	mdelay(10);
	// rawchip_spi_write_2B1B(0x0008, 0x6f); /* CLKMGR - CLK_CTRL */
	mdelay(10);
	rawchip_spi_write_2B1B(0x000c, 0x00); /* CLKMGR - RESET_CTRL */
	mdelay(10);
	rawchip_spi_write_2B1B(0x000d, 0x00); /* CLKMGR - RESET_CTRL */
	mdelay(10);
	rawchip_spi_write_2B1B(0x000c, 0x3f); /* CLKMGR - RESET_CTRL */
	mdelay(10);
	rawchip_spi_write_2B1B(0x000d, 0x07); /* CLKMGR - RESET_CTRL */
	mdelay(10);
	rawchip_spi_write_2B1B(0x000f, 0x00); /* Unreferenced register - Warning */
	mdelay(10);

	/* LDO enable sequence */
	rawchip_spi_write_2B1B(0x1405, 0x03);
	mdelay(10);
	rawchip_spi_write_2B1B(0x1405, 0x02);
	mdelay(10);
	rawchip_spi_write_2B1B(0x1405, 0x00);

	/* HD */
	//rawchip_spi_write_2B1B(0x0015, 0x14); /* Unreferenced register - Warning */
	rawchip_spi_write_2B1B(0x0015, 0x19); /* CLKMGR - PLL_LOOP_OUT_DF */
	rawchip_spi_write_2B1B(0x0014, 0x03); /* CLKMGR - PLL_LOOP_OUT_DF */

	mdelay(10);
	rawchip_spi_write_2B1B(0x0000, 0x0a); /* CLKMGR - CLK_DIV_FACTOR */
	mdelay(10);
	rawchip_spi_write_2B1B(0x0001, 0x0a); /* Unreferenced register - Warning */
	mdelay(10);
	rawchip_spi_write_2B1B(0x0002, 0x14); /* Unreferenced register - Warning */
	mdelay(10);
	rawchip_spi_write_2B1B(0x0010, 0x18); /* CLKMGR - PLL_CTRL_MAIN */
	mdelay(10);
	rawchip_spi_write_2B1B(0x0009, 0x01); /* Unreferenced register - Warning */
	mdelay(10);
	rawchip_spi_write_2B1B(0x1000, 0x01); /* NVM - IOR_NVM_CTRL */
	mdelay(10);
	rawchip_spi_write_2B1B(0x2000, 0xff); /* MIPIRX - DPHY_SC_4SF_ENABLE */
	mdelay(10);
	rawchip_spi_write_2B1B(0x2004, 0x06); /* MIPIRX - DPHY_SC_4SF_UIX4 */
	mdelay(10);
	//rawchip_spi_write_2B1B(0x5000, 0xff); /* MIPITX - DPHY_MC_4MF_ENABLE */
	mdelay(10);
	//rawchip_spi_write_2B1B(0x5004, 0x06); /* MIPITX - DPHY_MC_4MF_UIX4 */
	rawchip_spi_write_2B1B(0x5004, 0x14); /* MIPITX - DPHY_MC_4MF_UIX4 */
	mdelay(10);
	rawchip_spi_write_2B1B(0x2408, 0x04); /* CSI2RX - CSI2_RX_NB_DATA_LANES */
	mdelay(10);
	rawchip_spi_write_2B1B(0x240c, 0x0a); /* CSI2RX - CSI2_RX_IMG_UNPACKING_FORMAT */
	mdelay(10);
	rawchip_spi_write_2B1B(0x2420, 0x01); /* CSI2RX - CSI2_RX_BYTE2PIXEL_READ_TH */
	mdelay(10);
	rawchip_spi_write_2B1B(0x2428, 0x2b); /* CSI2RX - CSI2_RX_DATA_TYPE */
	mdelay(10);
	rawchip_spi_write_2B1B(0x4400, 0x01); /* SMIAF - SMIAF_CTRL */
	mdelay(10);
	rawchip_spi_write_2B1B(0x4404, 0x0a); /* SMIAF - SMIAF_PIX_WIDTH */
	mdelay(10);

	/* HD */
	rawchip_spi_write_2B1B(0x4a05, 0x04); /* PW2_FIFO THRESHOLD */

	//rawchip_spi_write_2B1B(0x2c09, 0x08); /* Unreferenced register - Warning */
	rawchip_spi_write_2B1B(0x2c09, 0x10); /* Unreferenced register - Warning */

	mdelay(10);
	rawchip_spi_write_2B1B(0x2c0c, 0xd0); /* IDP_GEN - IDP_GEN_LINE_LENGTH */
	mdelay(10);
	rawchip_spi_write_2B1B(0x2c0d, 0x0c); /* IDP_GEN - IDP_GEN_LINE_LENGTH */
	mdelay(10);
	rawchip_spi_write_2B1B(0x2c0e, 0xa0); /* IDP_GEN - IDP_GEN_LINE_LENGTH */
	mdelay(10);
	rawchip_spi_write_2B1B(0x2c0f, 0x0f); /* IDP_GEN - IDP_GEN_LINE_LENGTH */
	mdelay(10);
	rawchip_spi_write_2B1B(0x2c10, 0xa0); /* IDP_GEN - IDP_GEN_FRAME_LENGTH */
	mdelay(10);
	rawchip_spi_write_2B1B(0x2c11, 0x09); /* IDP_GEN - IDP_GEN_FRAME_LENGTH */
	mdelay(10);
	rawchip_spi_write_2B1B(0x2c12, 0xf0); /* IDP_GEN - IDP_GEN_FRAME_LENGTH */
	mdelay(10);
	rawchip_spi_write_2B1B(0x2c13, 0xff); /* IDP_GEN - IDP_GEN_FRAME_LENGTH */
	mdelay(10);
	rawchip_spi_write_2B1B(0x3400, 0x01); /* PAT_GEN - PATTERN_GEN_ENABLE */
	mdelay(10);
	rawchip_spi_write_2B1B(0x3401, 0x00); /* PAT_GEN - PATTERN_GEN_ENABLE */
	mdelay(10);
	rawchip_spi_write_2B1B(0x3402, 0x00); /* PAT_GEN - PATTERN_GEN_ENABLE */
	mdelay(10);
	rawchip_spi_write_2B1B(0x3403, 0x00); /* PAT_GEN - PATTERN_GEN_ENABLE */
	mdelay(10);
	rawchip_spi_write_2B1B(0x3408, 0x02); /* PAT_GEN - PATTERN_GEN_PATTERN_TYPE_REQ */
	mdelay(10);
	rawchip_spi_write_2B1B(0x3409, 0x00); /* PAT_GEN - PATTERN_GEN_PATTERN_TYPE_REQ */
	mdelay(10);
	rawchip_spi_write_2B1B(0x340a, 0x00); /* PAT_GEN - PATTERN_GEN_PATTERN_TYPE_REQ */
	mdelay(10);
	rawchip_spi_write_2B1B(0x340b, 0x00); /* PAT_GEN - PATTERN_GEN_PATTERN_TYPE_REQ */
	mdelay(10);
	rawchip_spi_write_2B1B(0x5880, 0x01); /* EOFREPRE - EOFRESIZE_ENABLE */
	mdelay(10);
	rawchip_spi_write_2B1B(0x5888, 0x01); /* EOFREPRE - EOFRESIZE_AUTOMATIC_CONTROL */
	mdelay(10);
	rawchip_spi_write_2B1B(0x4400, 0x11); /* SMIAF - SMIAF_CTRL */
	mdelay(10);
	rawchip_spi_write_2B1B(0x4408, 0x01); /* SMIAF - SMIAF_GROUPED_PARAMETER_HOLD */
	mdelay(10);
	rawchip_spi_write_2B1B(0x440c, 0x03); /* SMIAF - SMIAF_EOF_INT_EN */
	mdelay(10);
	rawchip_spi_write_2B1B(0x4c00, 0x01); /* CSI2TX - CSI2_TX_ENABLE */
	mdelay(10);
	rawchip_spi_write_2B1B(0x4c08, 0x01); /* CSI2TX - CSI2_TX_NUMBER_OF_LANES */
	mdelay(10);
	rawchip_spi_write_2B1B(0x4c10, 0x01); /* CSI2TX - CSI2_TX_PACKET_CONTROL */
	mdelay(10);
	rawchip_spi_write_2B1B(0x4c4c, 0x14); /* CSI2TX - CSI2_TX_PACKET_SIZE_0 */
	mdelay(10);
	rawchip_spi_write_2B1B(0x4c4d, 0x00); /* CSI2TX - CSI2_TX_PACKET_SIZE_0 */
	mdelay(10);
	rawchip_spi_write_2B1B(0x4c50, 0x2b); /* CSI2TX - CSI2_TX_DI_INDEX_CTRL_0 */
	mdelay(10);
	rawchip_spi_write_2B1B(0x4c51, 0x00); /* CSI2TX - CSI2_TX_DI_INDEX_CTRL_0 */
	mdelay(10);
	rawchip_spi_write_2B1B(0x4c5c, 0x2b); /* CSI2TX - CSI2_TX_DI_INDEX_CTRL_1 */
	mdelay(10);
	rawchip_spi_write_2B1B(0x4c5d, 0x00); /* CSI2TX - CSI2_TX_DI_INDEX_CTRL_1 */
	mdelay(10);
	rawchip_spi_write_2B1B(0x4c58, 0x04); /* CSI2TX - CSI2_TX_PACKET_SIZE_1 */
	mdelay(10);
	rawchip_spi_write_2B1B(0x4c59, 0x10); /* CSI2TX - CSI2_TX_PACKET_SIZE_1 */
	mdelay(10);
	rawchip_spi_write_2B1B(0x5828, 0x01); /* DTFILTER0 - DTFILTER_MATCH0 */
	mdelay(10);
	rawchip_spi_write_2B1B(0x582c, 0x02); /* DTFILTER0 - DTFILTER_MATCH1 */
	mdelay(10);
	rawchip_spi_write_2B1B(0x5830, 0x0d); /* DTFILTER0 - DTFILTER_MATCH2 */
	mdelay(10);
	rawchip_spi_write_2B1B(0x5834, 0x03); /* DTFILTER0 - DTFILTER_MATCH3 */
	mdelay(10);
	rawchip_spi_write_2B1B(0x5820, 0x01); /* DTFILTER0 - DTFILTER_ENABLE */
	mdelay(10);
	rawchip_spi_write_2B1B(0x5868, 0xff); /* DTFILTER1 - DTFILTER_MATCH0 */
	mdelay(10);
	rawchip_spi_write_2B1B(0x586c, 0xff); /* DTFILTER1 - DTFILTER_MATCH1 */
	mdelay(10);
	rawchip_spi_write_2B1B(0x5870, 0xff); /* DTFILTER1 - DTFILTER_MATCH2 */
	mdelay(10);
	rawchip_spi_write_2B1B(0x5874, 0xff); /* DTFILTER1 - DTFILTER_MATCH3 */
	mdelay(10);
	rawchip_spi_write_2B1B(0x5860, 0x01); /* DTFILTER1 - DTFILTER_ENABLE */
	mdelay(10);
	rawchip_spi_write_2B1B(0x5c08, 0x94); /* LECCI - LECCI_MIN_INTERLINE */
	mdelay(10);
	rawchip_spi_write_2B1B(0x5c09, 0x02); /* LECCI - LECCI_MIN_INTERLINE */
	mdelay(10);
	rawchip_spi_write_2B1B(0x5c0c, 0xfc); /* LECCI - LECCI_OUT_BURST_CTRL */
	mdelay(10);
	rawchip_spi_write_2B1B(0x5c10, 0x90); /* LECCI - LECCI_LINE_SIZE */
	mdelay(10);
	rawchip_spi_write_2B1B(0x5c11, 0x01); /* LECCI - LECCI_LINE_SIZE */
	mdelay(10);
	rawchip_spi_write_2B1B(0x5c14, 0x01); /* LECCI - LECCI_BYPASS_CTRL */
	mdelay(10);
	rawchip_spi_write_2B1B(0x5c00, 0x01); /* LECCI - LECCI_ENABLE */
	mdelay(10);

	/* HD */
	rawchip_spi_write_2B1B(0x5000, 0x33); /* LECCI - LECCI_ENABLE */
	mdelay(100);

	rawchip_spi_write_2B1B(0x2c00, 0x01); /* IDP_GEN - IDP_GEN_AUTO_RUN */
	mdelay(10);
	rawchip_spi_write_2B1B(0x2c01, 0x00); /* IDP_GEN - IDP_GEN_AUTO_RUN */
	mdelay(10);
	rawchip_spi_write_2B1B(0x2c02, 0x00); /* IDP_GEN - IDP_GEN_AUTO_RUN */
	mdelay(10);
	rawchip_spi_write_2B1B(0x2c03, 0x00); /* IDP_GEN - IDP_GEN_AUTO_RUN */
	msleep(2000);

	pr_info("[CAM] ASIC_Test X\n");
}

/* #define	COLOR_BAR */
int Yushan_sensor_open_init(struct rawchip_sensor_init_data data, bool *clock_init_done)
{
	/*color bar test */
#ifdef COLOR_BAR
	int32_t rc = 0;
	ASIC_Test();
	/* frame_counter(); */
#else
	Yushan_Version_Info_t sYushanVersionInfo;
	bool_t	bBypassDxoUpload = 0;


	uint8_t			bPixelFormat = RAW10;/*, bPatternReq=2;*/
	/*uint8_t 		bSpiFreq = 0x80; *//*SPI_CLK_8MHZ;*/
	/* uint32_t		udwSpiFreq = 0; */
	Yushan_Init_Struct_t	sInitStruct;
	Yushan_Init_Dxo_Struct_t	sDxoStruct;
//	Yushan_ImageChar_t sImageChar;
	Yushan_GainsExpTime_t sGainsExpTime;
	Yushan_SystemStatus_t			sSystemStatus;
	uint32_t		udwIntrMask[] = {0x01E38E3B, 0xCC7C7C00, 0x001B7FFB};	// DXO_DOP_NEW_FR_PR enabled
	uint16_t		uwAssignITRGrpToPad1 = 0x008; // Send only DOP interrupts to PAD1

#if 0
	Yushan_AF_ROI_t					sYushanAfRoi[5];
	Yushan_DXO_ROI_Active_Number_t	sYushanDxoRoiActiveNumber;
	Yushan_AF_Stats_t				sYushanAFStats[5];
	Yushan_New_Context_Config_t		sYushanNewContextConfig;
#endif
	uint8_t bSpiData;
	uint8_t bStatus;
	/* uint32_t spiData; */

	uint16_t uwHSize = data.width;
	uint16_t uwVSize = data.height;
	uint16_t uwBlkPixels = data.blk_pixels;
	uint16_t uwBlkLines = data.blk_lines;

#endif


	#if 0/* move to global */
	Yushan_DXO_DPP_Tuning_t sDxoDppTuning;
	Yushan_DXO_PDP_Tuning_t sDxoPdpTuning;
	Yushan_DXO_DOP_Tuning_t sDxoDopTuning;
#endif

	CDBG("[CAM] Yushan API Version : %d.%d \n", API_MAJOR_VERSION, API_MINOR_VERSION);

#ifndef COLOR_BAR
	/* Default Values */
	sInitStruct.bNumberOfLanes		=	data.lane_cnt;
	sInitStruct.fpExternalClock		=	data.ext_clk;
	sInitStruct.uwBitRate			=	data.bitrate;
	sInitStruct.uwPixelFormat = 0x0A0A;//chengyang
	sInitStruct.bDxoSettingCmdPerFrame	=	1;

	if ((sInitStruct.uwPixelFormat&0x0F) == 0x0A)
		bPixelFormat = RAW10;
	else if ((sInitStruct.uwPixelFormat&0x0F) == 0x08) {
		if (((sInitStruct.uwPixelFormat>>8)&0x0F) == 0x08)
			bPixelFormat = RAW8;
		else /* 10 to 8 case */
			bPixelFormat = RAW10_8;
	}
#endif

#ifndef COLOR_BAR
	if (strcmp(data.sensor_name, "s5k3h2y") == 0) {
		yushan_regs = &yushan_regs_s5k3h2yx;
	}
	else if (strcmp(data.sensor_name, "ov8838") == 0) {
		yushan_regs = &yushan_regs_ov8838;
	}
	else if (strcmp(data.sensor_name, "s5k6a2gx") == 0) {
		yushan_regs = &yushan_regs_s5k6a2gx;
	} else {
		pr_err("[CAM] sensor doesn't exist\n");
	}

	sDxoStruct.pDxoPdpRamImage[0] = (uint8_t *)yushan_regs->pdpcode;
	sDxoStruct.pDxoDppRamImage[0] = (uint8_t *)yushan_regs->dppcode;
	sDxoStruct.pDxoDopRamImage[0] = (uint8_t *)yushan_regs->dopcode;
	sDxoStruct.pDxoPdpRamImage[1] = (uint8_t *)yushan_regs->pdpclib;
	sDxoStruct.pDxoDppRamImage[1] = (uint8_t *)yushan_regs->dppclib;
	sDxoStruct.pDxoDopRamImage[1] = (uint8_t *)yushan_regs->dopclib;

	sDxoStruct.uwDxoPdpRamImageSize[0] = yushan_regs->pdpcode_size;
	sDxoStruct.uwDxoDppRamImageSize[0] = yushan_regs->dppcode_size;
	sDxoStruct.uwDxoDopRamImageSize[0] = yushan_regs->dopcode_size;
	sDxoStruct.uwDxoPdpRamImageSize[1] = yushan_regs->pdpclib_size;
	sDxoStruct.uwDxoDppRamImageSize[1] = yushan_regs->dppclib_size;
	sDxoStruct.uwDxoDopRamImageSize[1] = yushan_regs->dopclib_size;

	sDxoStruct.uwBaseAddrPdpMicroCode[0] = yushan_regs->pdpcode_first_addr;
	sDxoStruct.uwBaseAddrDppMicroCode[0] = yushan_regs->dppcode_first_addr;
	sDxoStruct.uwBaseAddrDopMicroCode[0] = yushan_regs->dopcode_first_addr;
	sDxoStruct.uwBaseAddrPdpMicroCode[1] = yushan_regs->pdpclib_first_addr;
	sDxoStruct.uwBaseAddrDppMicroCode[1] = yushan_regs->dppclib_first_addr;
	sDxoStruct.uwBaseAddrDopMicroCode[1] = yushan_regs->dopclib_first_addr;

	sDxoStruct.uwDxoPdpBootAddr = yushan_regs->pdpBootAddr;
	sDxoStruct.uwDxoDppBootAddr = yushan_regs->dppBootAddr;
	sDxoStruct.uwDxoDopBootAddr = yushan_regs->dopBootAddr;

	sDxoStruct.uwDxoPdpStartAddr = yushan_regs->pdpStartAddr;
	sDxoStruct.uwDxoDppStartAddr = yushan_regs->dppStartAddr;
	sDxoStruct.uwDxoDopStartAddr = yushan_regs->dopStartAddr;

#if 0
	pr_info("/*---------------------------------------*\\");
	pr_info("array base ADDRs %d %d %d %d %d %d",
		sDxoStruct.uwBaseAddrPdpMicroCode[0], sDxoStruct.uwBaseAddrDppMicroCode[0], sDxoStruct.uwBaseAddrDopMicroCode[0],
		sDxoStruct.uwBaseAddrPdpMicroCode[1], sDxoStruct.uwBaseAddrDppMicroCode[1], sDxoStruct.uwBaseAddrDopMicroCode[1]);
	pr_info("array 1st values %d %d %d %d %d %d",
		*sDxoStruct.pDxoPdpRamImage[0], *sDxoStruct.pDxoDppRamImage[0], *sDxoStruct.pDxoDopRamImage[0],
		*sDxoStruct.pDxoPdpRamImage[1], *sDxoStruct.pDxoDppRamImage[1], *sDxoStruct.pDxoDopRamImage[1]);
	pr_info("array sizes %d %d %d %d %d %d",
		sDxoStruct.uwDxoPdpRamImageSize[0], sDxoStruct.uwDxoDppRamImageSize[0], sDxoStruct.uwDxoDopRamImageSize[0],
		sDxoStruct.uwDxoPdpRamImageSize[1], sDxoStruct.uwDxoDppRamImageSize[1], sDxoStruct.uwDxoDopRamImageSize[1]);
	pr_info("Boot Addr %d %d %d",
		sDxoStruct.uwDxoPdpBootAddr, sDxoStruct.uwDxoDppBootAddr, sDxoStruct.uwDxoDopBootAddr);
	pr_info("\\*---------------------------------------*/");
#endif

	/* Configuring the SPI Freq: Dimax */
	/* Yushan_SPIConfigure(bSpiFreq); */
#if 0
	/* Converting for Yushan */
	switch (bSpiFreq) {
	case 0x80:	/* 8Mhz */
		udwSpiFreq = 8<<16;
		break;
	case 0x00:	/* 4Mhz */
		udwSpiFreq = 4<<16;
		break;
	case 0x81:	/* 2Mhz */
		udwSpiFreq = 2<<16;
		break;
	case 0x01:	/* 1Mhz */
		udwSpiFreq = 1<<16;
		break;
	case 0x82:	/* 500Khz = 1Mhz/2 */
		udwSpiFreq = 1<<8;
		break;
	case 0x02:	/* 250Khz = 1Mhz/4 */
		udwSpiFreq = 1<<4;
		break;
	case 0x03:	/* 125Khz = 1Mhz/8 */
		udwSpiFreq = 1<<2;
		break;
	}
#endif

	sInitStruct.fpSpiClock			=	data.spi_clk*(1<<16); /* 0x80000; */ /* udwSpiFreq; */
    sInitStruct.fpExternalClock  = sInitStruct.fpExternalClock << 16; /* 0x180000 for 24Mbps */

	sInitStruct.uwActivePixels = uwHSize;
	sInitStruct.uwLineBlankStill = uwBlkPixels;
	sInitStruct.uwLineBlankVf = uwBlkPixels;
	sInitStruct.uwLines = uwVSize;
	sInitStruct.uwFrameBlank = uwBlkLines;
	sInitStruct.bUseExternalLDO = data.use_ext_1v2;
	sImageChar_context.bImageOrientation = data.orientation;
	sImageChar_context.uwXAddrStart = data.x_addr_start;
	sImageChar_context.uwYAddrStart = data.y_addr_start;
	sImageChar_context.uwXAddrEnd = data.x_addr_end;
	sImageChar_context.uwYAddrEnd = data.y_addr_end;
	sImageChar_context.uwXEvenInc = data.x_even_inc;
	sImageChar_context.uwXOddInc = data.x_odd_inc;
	sImageChar_context.uwYEvenInc = data.y_even_inc;
	sImageChar_context.uwYOddInc = data.y_odd_inc;
	sImageChar_context.bBinning = data.binning_rawchip;
/*
	sImageChar.bImageOrientation = data.orientation;
	sImageChar.uwXAddrStart = data.x_addr_start;
	sImageChar.uwYAddrStart = data.y_addr_start;
	sImageChar.uwXAddrEnd = data.x_addr_end;
	sImageChar.uwYAddrEnd = data.y_addr_end;
	sImageChar.uwXEvenInc = data.x_even_inc;
	sImageChar.uwXOddInc = data.x_odd_inc;
	sImageChar.uwYEvenInc = data.y_even_inc;
	sImageChar.uwYOddInc = data.y_odd_inc;
	sImageChar.bBinning = data.binning_rawchip;
*/

	memset(sInitStruct.sFrameFormat, 0, sizeof(Yushan_Frame_Format_t)*15);
	if ((bPixelFormat == RAW8) || (bPixelFormat == RAW10_8)) {
		CDBG("[CAM] bPixelFormat==RAW8");
		sInitStruct.sFrameFormat[0].uwWordcount = (uwHSize);	/* For RAW10 this value should be uwHSize*10/8 */
		sInitStruct.sFrameFormat[0].bDatatype = 0x2a;			/* For RAW10 this value should be 0x2b */
	} else { /* if(bPixelFormat==RAW10) */
		CDBG("[CAM] bPixelFormat==RAW10");
		sInitStruct.sFrameFormat[0].uwWordcount = (uwHSize*10)/8;	/* For RAW10 this value should be uwHSize*10/8 */
		sInitStruct.sFrameFormat[0].bDatatype = 0x2b;				/* For RAW10 this value should be 0x2b */
	}
	/* Overwritting Data Type for 10 to 8 Pixel format */
	if (bPixelFormat == RAW10_8) {
		sInitStruct.sFrameFormat[0].bDatatype = 0x30;
	}
	sInitStruct.sFrameFormat[0].bActiveDatatype = 1;
	sInitStruct.sFrameFormat[0].bSelectStillVfMode = YUSHAN_FRAME_FORMAT_STILL_MODE;

	sInitStruct.bValidWCEntries = 1;

	sGainsExpTime.uwAnalogGainCodeGR = 0x20; /* 0x0 10x=>140; 1x=>20 */
	sGainsExpTime.uwAnalogGainCodeR = 0x20;
	sGainsExpTime.uwAnalogGainCodeB = 0x20;
	sGainsExpTime.uwPreDigGainGR = 0x100;
	sGainsExpTime.uwPreDigGainR = 0x100;
	sGainsExpTime.uwPreDigGainB = 0x100;
	sGainsExpTime.uwExposureTime = 0x20;
	sGainsExpTime.bRedGreenRatio = 0x40;
	sGainsExpTime.bBlueGreenRatio = 0x40;

	sDxoDppTuning.bTemporalSmoothing = 0x63; /*0x80;*/
	sDxoDppTuning.uwFlashPreflashRating = 0;
	sDxoDppTuning.bFocalInfo = 0;

	sDxoPdpTuning.bDeadPixelCorrectionLowGain = 0x80;
	sDxoPdpTuning.bDeadPixelCorrectionMedGain = 0x80;
	sDxoPdpTuning.bDeadPixelCorrectionHiGain = 0x80;

#if 0
	sDxoDopTuning.uwForceClosestDistance = 0;	/* Removed follwing DXO recommendation */
	sDxoDopTuning.uwForceFarthestDistance = 0;
#endif

	if (strcmp(data.sensor_name, "ov8838") == 0) {
		/* Raw-Chip setting for senser OV8838 */
		//pr_info("[CAM] Raw-chip setting for OV8838 \n");
		sDxoDopTuning.bEstimationMode = 1;
		sDxoDopTuning.bSharpness = 0x80; /*0x60;*/
		sDxoDopTuning.bDenoisingLowGain = 0x80; //60/* 0xFF for de-noise verify, original:0x1 */
		sDxoDopTuning.bDenoisingMedGain = 0x80; //80
		sDxoDopTuning.bDenoisingHiGain = 0x80; //50/*0x80;*/
		sDxoDopTuning.bNoiseVsDetailsLowGain = 0x80;
		sDxoDopTuning.bNoiseVsDetailsMedGain = 0x80;
		sDxoDopTuning.bNoiseVsDetailsHiGain = 0x80;
		sDxoDopTuning.bTemporalSmoothing = 0x26; /*0x80;*/
	} else if (strcmp(data.sensor_name, "s5k3h2y") == 0) {
		/* Raw-Chip setting for senser S5K3H2Y */
		//pr_info("[CAM] Raw-chip setting for S5K3H2Y \n");
		sDxoDopTuning.bEstimationMode = 1;
		sDxoDopTuning.bSharpness = 0x01;
		sDxoDopTuning.bDenoisingLowGain = 0x01;
		sDxoDopTuning.bDenoisingMedGain = 0x70;
		sDxoDopTuning.bDenoisingHiGain = 0x45;
		sDxoDopTuning.bNoiseVsDetailsLowGain = 0xA0;
		sDxoDopTuning.bNoiseVsDetailsMedGain = 0x80;
		sDxoDopTuning.bNoiseVsDetailsHiGain = 0x80;
		sDxoDopTuning.bTemporalSmoothing = 0x26;
	} else if (strcmp(data.sensor_name, "s5k6a2gx") == 0) {
		/* Raw-Chip setting for senser Samsung S5K6A2GX */
		//pr_info("[CAM] Raw-chip setting for Samsung S5K6A2GX \n");
		sDxoDopTuning.bEstimationMode = 1;
		sDxoDopTuning.bSharpness = 0x01;
		sDxoDopTuning.bDenoisingLowGain = 0x40;
		sDxoDopTuning.bDenoisingMedGain = 0x60;
		sDxoDopTuning.bDenoisingHiGain = 0x70;
		sDxoDopTuning.bNoiseVsDetailsLowGain = 0xA0;
		sDxoDopTuning.bNoiseVsDetailsMedGain = 0x80;
		sDxoDopTuning.bNoiseVsDetailsHiGain = 0x80;
		sDxoDopTuning.bTemporalSmoothing = 0x26;
	} else {
		/* Raw-Chip setting for else senser */
		//pr_info("[CAM] Raw-chip setting for else senser \n");
		sDxoDopTuning.bEstimationMode = 1;
		sDxoDopTuning.bSharpness = 0x01;
		sDxoDopTuning.bDenoisingLowGain = 0x60;
		sDxoDopTuning.bDenoisingMedGain = 0x80;
		sDxoDopTuning.bDenoisingHiGain = 0x50;
		sDxoDopTuning.bNoiseVsDetailsLowGain = 0xA0;
		sDxoDopTuning.bNoiseVsDetailsMedGain = 0x80;
		sDxoDopTuning.bNoiseVsDetailsHiGain = 0x80;
		sDxoDopTuning.bTemporalSmoothing = 0x26;
    }

	gPllLocked = 0;
	CDBG("[CAM] Yushan_common_init Yushan_Init_Clocks\n");
	bStatus = Yushan_Init_Clocks(&sInitStruct, &sSystemStatus, udwIntrMask) ;
	if (bStatus != 1) {
		pr_err("[CAM] Clock Init FAILED\n");
		pr_err("[CAM] Yushan_common_init Yushan_Init_Clocks=%d\n", bStatus);
		pr_err("[CAM] Min Value Required %d\n", sSystemStatus.udwDxoConstraintsMinValue);
		pr_err("[CAM] Error Code : %d\n", sSystemStatus.bDxoConstraints);
		return -1;
	} else
		pr_info("[CAM] Clock Init Done \n");
	/* Pll Locked. Done to allow auto incremental SPI transactions. */
	gPllLocked = 1;
	*clock_init_done = 1;

	/* Setup interrupt PAD assignment */
	Yushan_AssignInterruptGroupsToPad1    (uwAssignITRGrpToPad1);

	CDBG("[CAM] Yushan_common_init Yushan_Init\n");
	bStatus = Yushan_Init(&sInitStruct) ;
	CDBG("[CAM] Yushan_common_init Yushan_Init=%d\n", bStatus);

	/* Initialize DCPX and CPX of Yushan */
	if (bPixelFormat == RAW10_8)
		Yushan_DCPX_CPX_Enable();

	if (bStatus == 0) {
		pr_err("[CAM] Yushan Init FAILED\n");
		return -1;
	}
	/* The initialization is done */

/* HTC_START_Simon.Ti_Liu_20120702_Enhance_bypass */
	if (data.use_rawchip == RAWCHIP_DXO_BYPASS) {
		/* LECCI Bypass */
		Yushan_DXO_Lecci_Bypass();
	}

	if (data.use_rawchip == RAWCHIP_MIPI_BYPASS) {
		/* DTFliter Bypass */
		Yushan_DXO_DTFilter_Bypass();
	}

	if (data.use_rawchip == RAWCHIP_ENABLE) {
		CDBG("[CAM] Yushan_common_init Yushan_Init_Dxo\n");
		/* bBypassDxoUpload = 1; */
		bStatus = Yushan_Init_Dxo(&sDxoStruct, bBypassDxoUpload);
		CDBG("[CAM] Yushan_common_init Yushan_Init_Dxo=%d\n", bStatus);
		if (bStatus == 1) {
			CDBG("[CAM] DXO Upload and Init Done\n");
		} else {
			pr_err("[CAM] DXO Upload and Init FAILED\n");
			return -1;
		}
		CDBG("[CAM] Yushan_common_init Yushan_Get_Version_Information\n");

		bStatus = Yushan_Get_Version_Information(&sYushanVersionInfo);
#if 0
		pr_info("Yushan_common_init Yushan_Get_Version_Information=%d\n", bStatus);

		pr_info("API Version : %d.%d \n", sYushanVersionInfo.bApiMajorVersion, sYushanVersionInfo.bApiMinorVersion);
		pr_info("DxO Pdp Version : %x \n", sYushanVersionInfo.udwPdpVersion);
		pr_info("DxO Dpp Version : %x \n", sYushanVersionInfo.udwDppVersion);
		pr_info("DxO Dop Version : %x \n", sYushanVersionInfo.udwDopVersion);
		pr_info("DxO Pdp Calibration Version : %x \n", sYushanVersionInfo.udwPdpCalibrationVersion);
		pr_info("DxO Dpp Calibration Version : %x \n", sYushanVersionInfo.udwDppCalibrationVersion);
		pr_info("DxO Dop Calibration Version : %x \n", sYushanVersionInfo.udwDopCalibrationVersion);
#endif
/* #endif */

#if 0
		/* For Test Pattern */
		if (bTestPatternMode == 1) {
			/* Pattern Gen */
			Yushan_PatternGenerator(&sInitStruct, bPatternReq, bDxoBypassForTestPattern);
			/* frame_counter(); */
			return 0;
		}
#endif

		/* Updating DXO */
		Yushan_Update_ImageChar(&sImageChar_context);
		Yushan_Update_SensorParameters(&sGainsExpTime);
		Yushan_Update_DxoDpp_TuningParameters(&sDxoDppTuning);
		Yushan_Update_DxoDop_TuningParameters(&sDxoDopTuning);
		Yushan_Update_DxoPdp_TuningParameters(&sDxoPdpTuning);
		bStatus = Yushan_Update_Commit(bPdpMode, bDppMode, bDopMode);
		CDBG("[CAM] Yushan_common_init Yushan_Update_Commit=%d\n", bStatus);
		/* pr_info("[CAM] Yushan_common_init Yushan_Update_Commit=%d\n", bStatus); */
		if (bStatus == 1)
			CDBG("[CAM] DXO Commit Done\n");
		else {
			pr_err("[CAM] DXO Commit FAILED\n");
			return -1;
		}
	}

	/* disable extra INT */
	bSpiData = 0;
	SPI_Write(YUSHAN_SMIA_FM_EOF_INT_EN, 1,  &bSpiData);

	/* select_mode(0x18); */

	return (bStatus == SUCCESS) ? 0 : -1;
#endif
	return 0;
}
/*********************************************************************
Function to programm AF_ROI and Check the stats too
**********************************************************************/
bool_t Yushan_Dxo_Dop_Af_Run(Yushan_AF_ROI_t	*sYushanAfRoi, uint32_t *pAfStatsGreen, uint8_t	bRoiActiveNumber)
{

	uint8_t		bStatus = SUCCESS;
	/* uint16_t	uwSpiData = 0; */
#if 1
	uint32_t		enableIntrMask[] = {0x00338E30, 0x00000000, 0x00018000};
	uint32_t		disableIntrMask[] = {0x00238E30, 0x00000000, 0x00018000};
#endif

	/* Recommended by DXO */

	/* pr_info("[CAM] Active Number of ROI is more than 0.\nUpdating the ROIs post streaming\n\n"); */

	if (bRoiActiveNumber)
	{
		Yushan_Intr_Enable((uint8_t*)enableIntrMask);
		/* Program the AF_ROI */
		bStatus = Yushan_AF_ROI_Update(&sYushanAfRoi[0], bRoiActiveNumber);
		bStatus &= Yushan_Update_Commit(bPdpMode,bDppMode,bDopMode);

		/* Reading the AF Statistics */
		bStatus &= Yushan_WaitForInterruptEvent2(EVENT_DXODOP7_NEWFRAMEPROC_ACK, TIME_20MS);
	}
	else
		Yushan_Intr_Enable((uint8_t*)disableIntrMask);

#if 0
	if (bStatus) {
		/* pr_info("[CAM] Successufully DXO Commited and received EVENT_DXODOP7_NEWFRAMEPROC_ACK \n"); */
		#if 0
		yushan_go_to_position(0, 0);
		for(i=1; i<=42; i++)
		{
			s5k3h2yx_move_focus( 1, i);
			bStatus = Yushan_Read_AF_Statistics(pAfStatsGreen, bRoiActiveNumber);
		}
		#endif
		bStatus = Yushan_Read_AF_Statistics(pAfStatsGreen, bRoiActiveNumber);
	}

	if (!bStatus) {
		pr_err("ROI AF Statistics read failed\n");
		return FAILURE;
	} else
		pr_err("Read ROI AF Statistics successfully\n");
#endif

	return SUCCESS;

}

int Yushan_get_AFSU(rawchip_af_stats* af_stats)
{

	uint8_t		bStatus = SUCCESS;

	bStatus = Yushan_Read_AF_Statistics(af_stats->udwAfStats, 1, &af_stats->frameIdx);
	if (bStatus == FAILURE) {
		pr_err("[CAM] Get AFSU statistic data fail\n");
		return -1;
	}
	CDBG("[CAM] GET_AFSU:G:%d, R:%d, B:%d, confi:%d, frmIdx:%d\n",
		af_stats->udwAfStats[0].udwAfStatsGreen,
		af_stats->udwAfStats[0].udwAfStatsRed,
		af_stats->udwAfStats[0].udwAfStatsBlue,
		af_stats->udwAfStats[0].udwAfStatsConfidence,
		af_stats->frameIdx);
	return 0;
}



/*********************************************************************
Function: Context Update Wrapper
**********************************************************************/
bool_t	Yushan_ContextUpdate_Wrapper(Yushan_New_Context_Config_t	sYushanNewContextConfig, Yushan_ImageChar_t	sImageNewChar_context)
{

	bool_t	bStatus = SUCCESS;
	//Yushan_ImageChar_t	sImageChar_context;

		pr_info("[CAM] Reconfiguration starts:%d,%d,%d\n",
			sYushanNewContextConfig.uwActiveFrameLength,
			sYushanNewContextConfig.uwActivePixels,
			sYushanNewContextConfig.uwLineBlank);
		bStatus = Yushan_Context_Config_Update(&sYushanNewContextConfig);
	/* sYushanAEContextConfig = (Yushan_New_Context_Config_t*)&sYushanNewContextConfig; */

	memcpy(&sImageChar_context, &sImageNewChar_context, sizeof(Yushan_ImageChar_t));

	/* default enable DxO */
	Yushan_Update_ImageChar(&sImageNewChar_context);
	Yushan_Update_DxoDpp_TuningParameters(&sDxoDppTuning);
	Yushan_Update_DxoDop_TuningParameters(&sDxoDopTuning);
	Yushan_Update_DxoPdp_TuningParameters(&sDxoPdpTuning);
	bStatus &= Yushan_Update_Commit(bPdpMode,bDppMode,bDopMode);

	if (bStatus) {
		/* pr_info("[CAM] DXO Commit, Post Context Reconfigration, Done\n"); */
	}
	else
		pr_err("[CAM] DXO Commit, Post Context Reconfigration, FAILED\n");

	return bStatus;
}

#if 0
void Yushan_Write_Exp_Time_Gain(uint16_t yushan_line, uint16_t yushan_gain)
{
#if 0
	Yushan_GainsExpTime_t sGainsExpTime;
	uint32_t udwSpiBaseIndex;
	uint32_t spidata;
	float ratio = 0.019;
	pr_info("[CAM] Yushan_Write_Exp_Time_Gain, yushan_gain:%d, yushan_line:%d", yushan_gain, yushan_line);
#endif

	sGainsExpTime.uwAnalogGainCodeGR= yushan_gain;
	sGainsExpTime.uwAnalogGainCodeR=yushan_gain;
	sGainsExpTime.uwAnalogGainCodeB=yushan_gain;
	sGainsExpTime.uwPreDigGainGR= 0x100;
	sGainsExpTime.uwPreDigGainR= 0x100;
	sGainsExpTime.uwPreDigGainB= 0x100;
	sGainsExpTime.uwExposureTime= (uint16_t)((19*yushan_line/1000));//((3280+190)*1000))/(float)182400000);//(sYushanAEContextConfig->uwActivePixels + sYushanAEContextConfig->uwPixelBlank) /182400000;//*200/912; ;

	if (sGainsExpTime.bRedGreenRatio == 0) sGainsExpTime.bRedGreenRatio=0x40;
	if (sGainsExpTime.bBlueGreenRatio == 0) sGainsExpTime.bBlueGreenRatio=0x40;

	pr_err("[CAM] uwExposureTime: %d\n", sGainsExpTime.uwExposureTime);
	Yushan_Update_SensorParameters(&sGainsExpTime);
#if 0
	pr_info("DxO Regiser Dump Start******* \n");
	SPI_Read((0x821E), 4, (uint8_t *)(&spidata));
	pr_info("DxO DOP 0x821E : %x \n",spidata);
	SPI_Read((0x8222), 4, (uint8_t *)(&spidata));
	pr_info("DxO DOP 0x8222 : %x \n",spidata);
	SPI_Read((0x8226), 4, (uint8_t *)(&spidata));
	pr_info("DxO DOP 0x8226 : %x \n",spidata);
	SPI_Read((0x822A), 4, (uint8_t *)(&spidata));
	pr_info("DxO DOP 0x822A : %x \n",spidata);
	SPI_Read((0x822E), 2, (uint8_t *)(&spidata));
	pr_info("DxO DOP 0x822E : %x \n",spidata);
	SPI_Read((0x8204), 4, (uint8_t *)(&spidata));
	pr_info("DxO DOP Cali_ID : %x \n",spidata);

	udwSpiBaseIndex = 0x010000;
	SPI_Write(YUSHAN_HOST_IF_SPI_BASE_ADDRESS, 4, (uint8_t *)(&udwSpiBaseIndex));
	SPI_Read((0x821E), 4, (uint8_t *)(&spidata));
	pr_info("DxO DPP 0x821E : %x \n",spidata);
	SPI_Read((0x8222), 4, (uint8_t *)(&spidata));
	pr_info("DxO DPP 0x8222 : %x \n",spidata);
	SPI_Read((0x8226), 4, (uint8_t *)(&spidata));
	pr_info("DxO DPP 0x8226 : %x \n",spidata);
	SPI_Read((0x822A), 2, (uint8_t *)(&spidata));
	pr_info("DxO DPP 0x822A : %x \n",spidata);
	SPI_Read((0x8204), 4, (uint8_t *)(&spidata));
	pr_info("DxO DPP Cali_ID : %x \n",spidata);

	udwSpiBaseIndex = 0x08000;
	SPI_Write(YUSHAN_HOST_IF_SPI_BASE_ADDRESS, 4, (uint8_t *)(&udwSpiBaseIndex));
	SPI_Read((0x621E), 4, (uint8_t *)(&spidata));
	pr_info("DxO PDP 0x621E : %x \n",spidata);
	SPI_Read((0x6222), 4, (uint8_t *)(&spidata));
	pr_info("DxO PDP 0x6222 : %x \n",spidata);
	SPI_Read((0x6226), 4, (uint8_t *)(&spidata));
	pr_info("DxO PDP 0x6226 : %x \n",spidata);
	SPI_Read((0x622A), 2, (uint8_t *)(&spidata));
	pr_info("DxO PDP 0x622A : %x \n",spidata);
	SPI_Read((0x6204), 4, (uint8_t *)(&spidata));
	pr_info("DxO PDP Cali_ID : %x \n",spidata);
#endif

}
#endif

int Yushan_Update_AEC_AWB_Params(rawchip_update_aec_awb_params_t *update_aec_awb_params)
{
	uint8_t bStatus = SUCCESS;
	Yushan_GainsExpTime_t sGainsExpTime;

	sGainsExpTime.uwAnalogGainCodeGR = update_aec_awb_params->aec_params.gain;
	sGainsExpTime.uwAnalogGainCodeR = update_aec_awb_params->aec_params.gain;
	sGainsExpTime.uwAnalogGainCodeB = update_aec_awb_params->aec_params.gain;
	sGainsExpTime.uwPreDigGainGR = 0x100;
	sGainsExpTime.uwPreDigGainR = 0x100;
	sGainsExpTime.uwPreDigGainB = 0x100;
	sGainsExpTime.uwExposureTime = update_aec_awb_params->aec_params.line;
	sGainsExpTime.bRedGreenRatio = update_aec_awb_params->awb_params.rg_ratio;
	sGainsExpTime.bBlueGreenRatio = update_aec_awb_params->awb_params.bg_ratio;
#if 0
	if (sGainsExpTime.bRedGreenRatio == 0)
		sGainsExpTime.bRedGreenRatio = 0x40;
	if (sGainsExpTime.bBlueGreenRatio == 0)
		sGainsExpTime.bBlueGreenRatio = 0x40;
#endif

	CDBG("[CAM] uwExposureTime: %d\n", sGainsExpTime.uwExposureTime);
	bStatus = Yushan_Update_SensorParameters(&sGainsExpTime);

	return (bStatus == SUCCESS) ? 0 : -1;
}

int Yushan_Update_AF_Params(rawchip_update_af_params_t *update_af_params)
{

	uint8_t bStatus = SUCCESS;
	bStatus = Yushan_AF_ROI_Update(&update_af_params->af_params.sYushanAfRoi[0],
		update_af_params->af_params.active_number);
	return (bStatus == SUCCESS) ? 0 : -1;
}

int Yushan_Update_3A_Params(uint8_t enable_newframe_ack)
{
	uint8_t bStatus = SUCCESS;
	uint32_t		enableIntrMask[] = {0x01F38E3B, 0xCC7C7C00, 0x001B7FFB};
	uint32_t		disableIntrMask[] = {0x01E38E3B, 0xCC7C7C00, 0x001B7FFB};
	if (enable_newframe_ack)
		Yushan_Intr_Enable((uint8_t*)enableIntrMask);
	else
		Yushan_Intr_Enable((uint8_t*)disableIntrMask);
	bStatus = Yushan_Update_Commit(bPdpMode, bDppMode, bDopMode);
	return (bStatus == SUCCESS) ? 0 : -1;
}

void Yushan_dump_register(void)
{
	uint16_t read_data = 0;
	uint8_t i;
	for (i = 0; i < 5; i++) {
		/* Yushan's in counting */
		rawchip_spi_read_2B2B(YUSHAN_CSI2_RX_FRAME_NUMBER, &read_data);
		pr_info("[CAM] Yushan's in counting=%d\n", read_data);

		/* Yushan's out counting */
		rawchip_spi_read_2B2B(YUSHAN_CSI2_TX_FRAME_NO_0, &read_data);
		pr_info("[CAM] Yushan's out counting=%d\n", read_data);

		mdelay(50);
	}
	rawchip_spi_read_2B2B(YUSHAN_ITM_CSI2RX_STATUS, &read_data);
	pr_info("[CAM]YUSHAN_ITM_CSI2RX_STATUS=%x\n", read_data);
	rawchip_spi_read_2B2B(YUSHAN_ITM_CSI2TX_STATUS, &read_data);
	pr_info("[CAM]YUSHAN_ITM_CSI2TX_STATUS=%x\n", read_data);
	rawchip_spi_read_2B2B(YUSHAN_ITM_IDP_STATUS, &read_data);
	pr_info("[CAM]YUSHAN_ITM_IDP_STATUS=%x\n", read_data);
	rawchip_spi_read_2B2B(YUSHAN_ITM_P2W_UFLOW_STATUS, &read_data);
	pr_info("[CAM]YUSHAN_ITM_P2W_UFLOW_STATUS=%x\n", read_data);
}

void Yushan_dump_all_register(void)
{
	uint16_t read_data = 0;
	uint8_t i;
	for (i = 0; i < 50; i++) {
		/* Yushan's in counting */
		rawchip_spi_read_2B2B(YUSHAN_CSI2_RX_FRAME_NUMBER, &read_data);
		pr_info("[CAM] Yushan's in counting=%d\n", read_data);

		/* Yushan's out counting */
		rawchip_spi_read_2B2B(YUSHAN_CSI2_TX_FRAME_NO_0, &read_data);
		pr_info("[CAM] Yushan's out counting=%d\n", read_data);

		mdelay(30);
	}

	rawchip_spi_read_2B2B(YUSHAN_CLK_DIV_FACTOR, &read_data);
	pr_info("[CAM]YUSHAN_CLK_DIV_FACTOR=%x\n", read_data);
	rawchip_spi_read_2B2B(YUSHAN_CLK_DIV_FACTOR_2, &read_data);
	pr_info("[CAM]YUSHAN_CLK_DIV_FACTOR_2=%x\n", read_data);
	rawchip_spi_read_2B2B(YUSHAN_CLK_CTRL, &read_data);
	pr_info("[CAM]YUSHAN_CLK_CTRL=%x\n", read_data);
	rawchip_spi_read_2B2B(YUSHAN_RESET_CTRL, &read_data);
	pr_info("[CAM]YUSHAN_RESET_CTRL=%x\n", read_data);
	rawchip_spi_read_2B2B(YUSHAN_PLL_CTRL_MAIN, &read_data);
	pr_info("[CAM]YUSHAN_PLL_CTRL_MAIN=%x\n", read_data);
	rawchip_spi_read_2B2B(YUSHAN_PLL_LOOP_OUT_DF, &read_data);
	pr_info("[CAM]YUSHAN_PLL_LOOP_OUT_DF=%x\n", read_data);
	rawchip_spi_read_2B2B(YUSHAN_PLL_SSCG_CTRL, &read_data);
	pr_info("[CAM]YUSHAN_PLL_SSCG_CTRL=%x\n", read_data);
	rawchip_spi_read_2B2B(YUSHAN_HOST_IF_SPI_CTRL, &read_data);
	pr_info("[CAM]YUSHAN_HOST_IF_SPI_CTRL=%x\n", read_data);
	rawchip_spi_read_2B2B(YUSHAN_HOST_IF_SPI_DEVADDR, &read_data);
	pr_info("[CAM]YUSHAN_HOST_IF_SPI_DEVADDR=%x\n", read_data);
	rawchip_spi_read_2B2B(YUSHAN_HOST_IF_SPI_BASE_ADDRESS, &read_data);
	pr_info("[CAM]YUSHAN_HOST_IF_SPI_BASE_ADDRESS=%x\n", read_data);
	rawchip_spi_read_2B2B(YUSHAN_ITM_CSI2RX_STATUS, &read_data);
	pr_info("[CAM]YUSHAN_ITM_CSI2RX_STATUS=%x\n", read_data);
	rawchip_spi_read_2B2B(YUSHAN_ITM_CSI2RX_EN_STATUS, &read_data);
	pr_info("[CAM]YUSHAN_ITM_CSI2RX_EN_STATUS=%x\n", read_data);
	rawchip_spi_read_2B2B(YUSHAN_ITM_CSI2RX_STATUS_BCLR, &read_data);
	pr_info("[CAM]YUSHAN_ITM_CSI2RX_STATUS_BCLR=%x\n", read_data);
	rawchip_spi_read_2B2B(YUSHAN_ITM_CSI2RX_STATUS_BSET, &read_data);
	pr_info("[CAM]YUSHAN_ITM_CSI2RX_STATUS_BSET=%x\n", read_data);
	rawchip_spi_read_2B2B(YUSHAN_ITM_CSI2RX_EN_STATUS_BCLR, &read_data);
	pr_info("[CAM]YUSHAN_ITM_CSI2RX_EN_STATUS_BCLR=%x\n", read_data);
	rawchip_spi_read_2B2B(YUSHAN_ITM_CSI2RX_EN_STATUS_BSET, &read_data);
	pr_info("[CAM]YUSHAN_ITM_CSI2RX_EN_STATUS_BSET=%x\n", read_data);
	rawchip_spi_read_2B2B(YUSHAN_ITM_PDP_STATUS, &read_data);
	pr_info("[CAM]YUSHAN_ITM_PDP_STATUS=%x\n", read_data);
	rawchip_spi_read_2B2B(YUSHAN_ITM_PDP_EN_STATUS, &read_data);
	pr_info("[CAM]YUSHAN_ITM_PDP_EN_STATUS=%x\n", read_data);
	rawchip_spi_read_2B2B(YUSHAN_ITM_PDP_STATUS_BCLR, &read_data);
	pr_info("[CAM]YUSHAN_ITM_PDP_STATUS_BCLR=%x\n", read_data);
	rawchip_spi_read_2B2B(YUSHAN_ITM_PDP_STATUS_BSET, &read_data);
	pr_info("[CAM]YUSHAN_ITM_PDP_STATUS_BSET=%x\n", read_data);
	rawchip_spi_read_2B2B(YUSHAN_ITM_PDP_EN_STATUS_BCLR, &read_data);
	pr_info("[CAM]YUSHAN_ITM_PDP_EN_STATUS_BCLR=%x\n", read_data);
	rawchip_spi_read_2B2B(YUSHAN_ITM_PDP_EN_STATUS_BSET, &read_data);
	pr_info("[CAM]YUSHAN_ITM_PDP_EN_STATUS_BSET=%x\n", read_data);
	rawchip_spi_read_2B2B(YUSHAN_ITM_DPP_STATUS, &read_data);
	pr_info("[CAM]YUSHAN_ITM_DPP_STATUS=%x\n", read_data);
	rawchip_spi_read_2B2B(YUSHAN_ITM_DPP_EN_STATUS, &read_data);
	pr_info("[CAM]YUSHAN_ITM_DPP_EN_STATUS=%x\n", read_data);
	rawchip_spi_read_2B2B(YUSHAN_ITM_DPP_STATUS_BCLR, &read_data);
	pr_info("[CAM]YUSHAN_ITM_DPP_STATUS_BCLR=%x\n", read_data);
	rawchip_spi_read_2B2B(YUSHAN_ITM_DPP_STATUS_BSET, &read_data);
	pr_info("[CAM]YUSHAN_ITM_DPP_STATUS_BSET=%x\n", read_data);
	rawchip_spi_read_2B2B(YUSHAN_ITM_DPP_EN_STATUS_BCLR, &read_data);
	pr_info("[CAM]YUSHAN_ITM_DPP_EN_STATUS_BCLR=%x\n", read_data);
	rawchip_spi_read_2B2B(YUSHAN_ITM_DPP_EN_STATUS_BSET, &read_data);
	pr_info("[CAM]YUSHAN_ITM_DPP_EN_STATUS_BSET=%x\n", read_data);
	rawchip_spi_read_2B2B(YUSHAN_ITM_DOP7_STATUS, &read_data);
	pr_info("[CAM]YUSHAN_ITM_DOP7_STATUS=%x\n", read_data);
	rawchip_spi_read_2B2B(YUSHAN_ITM_DOP7_EN_STATUS, &read_data);
	pr_info("[CAM]YUSHAN_ITM_DOP7_EN_STATUS=%x\n", read_data);
	rawchip_spi_read_2B2B(YUSHAN_ITM_DOP7_STATUS_BCLR, &read_data);
	pr_info("[CAM]YUSHAN_ITM_DOP7_STATUS_BCLR=%x\n", read_data);
	rawchip_spi_read_2B2B(YUSHAN_ITM_DOP7_STATUS_BSET, &read_data);
	pr_info("[CAM]YUSHAN_ITM_DOP7_STATUS_BSET=%x\n", read_data);
	rawchip_spi_read_2B2B(YUSHAN_ITM_DOP7_EN_STATUS_BCLR, &read_data);
	pr_info("[CAM]YUSHAN_ITM_DOP7_EN_STATUS_BCLR=%x\n", read_data);
	rawchip_spi_read_2B2B(YUSHAN_ITM_DOP7_EN_STATUS_BSET, &read_data);
	pr_info("[CAM]YUSHAN_ITM_DOP7_EN_STATUS_BSET=%x\n", read_data);
	rawchip_spi_read_2B2B(YUSHAN_ITM_CSI2TX_STATUS, &read_data);
	pr_info("[CAM]YUSHAN_ITM_CSI2TX_STATUS=%x\n", read_data);
	rawchip_spi_read_2B2B(YUSHAN_ITM_CSI2TX_EN_STATUS, &read_data);
	pr_info("[CAM]YUSHAN_ITM_CSI2TX_EN_STATUS=%x\n", read_data);
	rawchip_spi_read_2B2B(YUSHAN_ITM_CSI2TX_STATUS_BCLR, &read_data);
	pr_info("[CAM]YUSHAN_ITM_CSI2TX_STATUS_BCLR=%x\n", read_data);
	rawchip_spi_read_2B2B(YUSHAN_ITM_CSI2TX_STATUS_BSET, &read_data);
	pr_info("[CAM]YUSHAN_ITM_CSI2TX_STATUS_BSET=%x\n", read_data);
	rawchip_spi_read_2B2B(YUSHAN_ITM_CSI2TX_EN_STATUS_BCLR, &read_data);
	pr_info("[CAM]YUSHAN_ITM_CSI2TX_EN_STATUS_BCLR=%x\n", read_data);
	rawchip_spi_read_2B2B(YUSHAN_ITM_CSI2TX_EN_STATUS_BSET, &read_data);
	pr_info("[CAM]YUSHAN_ITM_CSI2TX_EN_STATUS_BSET=%x\n", read_data);
	rawchip_spi_read_2B2B(YUSHAN_ITM_RX_PHY_STATUS, &read_data);
	pr_info("[CAM]YUSHAN_ITM_RX_PHY_STATUS=%x\n", read_data);
	rawchip_spi_read_2B2B(YUSHAN_ITM_RX_PHY_EN_STATUS, &read_data);
	pr_info("[CAM]YUSHAN_ITM_RX_PHY_EN_STATUS=%x\n", read_data);
	rawchip_spi_read_2B2B(YUSHAN_ITM_RX_PHY_STATUS_BCLR, &read_data);
	pr_info("[CAM]YUSHAN_ITM_RX_PHY_STATUS_BCLR=%x\n", read_data);
	rawchip_spi_read_2B2B(YUSHAN_ITM_RX_PHY_STATUS_BSET, &read_data);
	pr_info("[CAM]YUSHAN_ITM_RX_PHY_STATUS_BSET=%x\n", read_data);
	rawchip_spi_read_2B2B(YUSHAN_ITM_RX_PHY_EN_STATUS_BCLR, &read_data);
	pr_info("[CAM]YUSHAN_ITM_RX_PHY_EN_STATUS_BCLR=%x\n", read_data);
	rawchip_spi_read_2B2B(YUSHAN_ITM_RX_PHY_EN_STATUS_BSET, &read_data);
	pr_info("[CAM]YUSHAN_ITM_RX_PHY_EN_STATUS_BSET=%x\n", read_data);
	rawchip_spi_read_2B2B(YUSHAN_ITM_TX_PHY_STATUS, &read_data);
	pr_info("[CAM]YUSHAN_ITM_TX_PHY_STATUS=%x\n", read_data);
	rawchip_spi_read_2B2B(YUSHAN_ITM_TX_PHY_EN_STATUS, &read_data);
	pr_info("[CAM]YUSHAN_ITM_TX_PHY_EN_STATUS=%x\n", read_data);
	rawchip_spi_read_2B2B(YUSHAN_ITM_TX_PHY_STATUS_BCLR, &read_data);
	pr_info("[CAM]YUSHAN_ITM_TX_PHY_STATUS_BCLR=%x\n", read_data);
	rawchip_spi_read_2B2B(YUSHAN_ITM_TX_PHY_STATUS_BSET, &read_data);
	pr_info("[CAM]YUSHAN_ITM_TX_PHY_STATUS_BSET=%x\n", read_data);
	rawchip_spi_read_2B2B(YUSHAN_ITM_TX_PHY_EN_STATUS_BCLR, &read_data);
	pr_info("[CAM]YUSHAN_ITM_TX_PHY_EN_STATUS_BCLR=%x\n", read_data);
	rawchip_spi_read_2B2B(YUSHAN_ITM_TX_PHY_EN_STATUS_BSET, &read_data);
	pr_info("[CAM]YUSHAN_ITM_TX_PHY_EN_STATUS_BSET=%x\n", read_data);
	rawchip_spi_read_2B2B(YUSHAN_ITM_IDP_STATUS, &read_data);
	pr_info("[CAM]YUSHAN_ITM_IDP_STATUS=%x\n", read_data);
	rawchip_spi_read_2B2B(YUSHAN_ITM_IDP_EN_STATUS, &read_data);
	pr_info("[CAM]YUSHAN_ITM_IDP_EN_STATUS=%x\n", read_data);
	rawchip_spi_read_2B2B(YUSHAN_ITM_IDP_STATUS_BCLR, &read_data);
	pr_info("[CAM]YUSHAN_ITM_IDP_STATUS_BCLR=%x\n", read_data);
	rawchip_spi_read_2B2B(YUSHAN_ITM_IDP_STATUS_BSET, &read_data);
	pr_info("[CAM]YUSHAN_ITM_IDP_STATUS_BSET=%x\n", read_data);
	rawchip_spi_read_2B2B(YUSHAN_ITM_IDP_EN_STATUS_BCLR, &read_data);
	pr_info("[CAM]YUSHAN_ITM_IDP_EN_STATUS_BCLR=%x\n", read_data);
	rawchip_spi_read_2B2B(YUSHAN_ITM_IDP_EN_STATUS_BSET, &read_data);
	pr_info("[CAM]YUSHAN_ITM_IDP_EN_STATUS_BSET=%x\n", read_data);
	rawchip_spi_read_2B2B(YUSHAN_ITM_RX_CHAR_STATUS, &read_data);
	pr_info("[CAM]YUSHAN_ITM_RX_CHAR_STATUS=%x\n", read_data);
	rawchip_spi_read_2B2B(YUSHAN_ITM_RX_CHAR_EN_STATUS, &read_data);
	pr_info("[CAM]YUSHAN_ITM_RX_CHAR_EN_STATUS=%x\n", read_data);
	rawchip_spi_read_2B2B(YUSHAN_ITM_RX_CHAR_STATUS_BCLR, &read_data);
	pr_info("[CAM]YUSHAN_ITM_RX_CHAR_STATUS_BCLR=%x\n", read_data);
	rawchip_spi_read_2B2B(YUSHAN_ITM_RX_CHAR_STATUS_BSET, &read_data);
	pr_info("[CAM]YUSHAN_ITM_RX_CHAR_STATUS_BSET=%x\n", read_data);
	rawchip_spi_read_2B2B(YUSHAN_ITM_RX_CHAR_EN_STATUS_BCLR, &read_data);
	pr_info("[CAM]YUSHAN_ITM_RX_CHAR_EN_STATUS_BCLR=%x\n", read_data);
	rawchip_spi_read_2B2B(YUSHAN_ITM_RX_CHAR_EN_STATUS_BSET, &read_data);
	pr_info("[CAM]YUSHAN_ITM_RX_CHAR_EN_STATUS_BSET=%x\n", read_data);
	rawchip_spi_read_2B2B(YUSHAN_ITM_LBE_POST_DXO_STATUS, &read_data);
	pr_info("[CAM]YUSHAN_ITM_LBE_POST_DXO_STATUS=%x\n", read_data);
	rawchip_spi_read_2B2B(YUSHAN_ITM_LBE_POST_DXO_EN_STATUS, &read_data);
	pr_info("[CAM]YUSHAN_ITM_LBE_POST_DXO_EN_STATUS=%x\n", read_data);
	rawchip_spi_read_2B2B(YUSHAN_ITM_LBE_POST_DXO_STATUS_BCLR, &read_data);
	pr_info("[CAM]YUSHAN_ITM_LBE_POST_DXO_STATUS_BCLR=%x\n", read_data);
	rawchip_spi_read_2B2B(YUSHAN_ITM_LBE_POST_DXO_STATUS_BSET, &read_data);
	pr_info("[CAM]YUSHAN_ITM_LBE_POST_DXO_STATUS_BSET=%x\n", read_data);
	rawchip_spi_read_2B2B(YUSHAN_ITM_LBE_POST_DXO_EN_STATUS_BCLR, &read_data);
	pr_info("[CAM]YUSHAN_ITM_LBE_POST_DXO_EN_STATUS_BCLR=%x\n", read_data);
	rawchip_spi_read_2B2B(YUSHAN_ITM_LBE_POST_DXO_EN_STATUS_BSET, &read_data);
	pr_info("[CAM]YUSHAN_ITM_LBE_POST_DXO_EN_STATUS_BSET=%x\n", read_data);
	rawchip_spi_read_2B2B(YUSHAN_ITM_SYS_DOMAIN_STATUS, &read_data);
	pr_info("[CAM]YUSHAN_ITM_SYS_DOMAIN_STATUS=%x\n", read_data);
	rawchip_spi_read_2B2B(YUSHAN_ITM_SYS_DOMAIN_EN_STATUS, &read_data);
	pr_info("[CAM]YUSHAN_ITM_SYS_DOMAIN_EN_STATUS=%x\n", read_data);
	rawchip_spi_read_2B2B(YUSHAN_ITM_SYS_DOMAIN_STATUS_BCLR, &read_data);
	pr_info("[CAM]YUSHAN_ITM_SYS_DOMAIN_STATUS_BCLR=%x\n", read_data);
	rawchip_spi_read_2B2B(YUSHAN_ITM_SYS_DOMAIN_STATUS_BSET, &read_data);
	pr_info("[CAM]YUSHAN_ITM_SYS_DOMAIN_STATUS_BSET=%x\n", read_data);
	rawchip_spi_read_2B2B(YUSHAN_ITM_SYS_DOMAIN_EN_STATUS_BCLR, &read_data);
	pr_info("[CAM]YUSHAN_ITM_SYS_DOMAIN_EN_STATUS_BCLR=%x\n", read_data);
	rawchip_spi_read_2B2B(YUSHAN_ITM_SYS_DOMAIN_EN_STATUS_BSET, &read_data);
	pr_info("[CAM]YUSHAN_ITM_SYS_DOMAIN_EN_STATUS_BSET=%x\n", read_data);
	rawchip_spi_read_2B2B(YUSHAN_ITM_ITPOINT_STATUS, &read_data);
	pr_info("[CAM]YUSHAN_ITM_ITPOINT_STATUS=%x\n", read_data);
	rawchip_spi_read_2B2B(YUSHAN_ITM_ITPOINT_EN_STATUS, &read_data);
	pr_info("[CAM]YUSHAN_ITM_ITPOINT_EN_STATUS=%x\n", read_data);
	rawchip_spi_read_2B2B(YUSHAN_ITM_ITPOINT_STATUS_BCLR, &read_data);
	pr_info("[CAM]YUSHAN_ITM_ITPOINT_STATUS_BCLR=%x\n", read_data);
	rawchip_spi_read_2B2B(YUSHAN_ITM_ITPOINT_STATUS_BSET, &read_data);
	pr_info("[CAM]YUSHAN_ITM_ITPOINT_STATUS_BSET=%x\n", read_data);
	rawchip_spi_read_2B2B(YUSHAN_ITM_ITPOINT_EN_STATUS_BCLR, &read_data);
	pr_info("[CAM]YUSHAN_ITM_ITPOINT_EN_STATUS_BCLR=%x\n", read_data);
	rawchip_spi_read_2B2B(YUSHAN_ITM_ITPOINT_EN_STATUS_BSET, &read_data);
	pr_info("[CAM]YUSHAN_ITM_ITPOINT_EN_STATUS_BSET=%x\n", read_data);
	rawchip_spi_read_2B2B(YUSHAN_ITM_P2W_UFLOW_STATUS, &read_data);
	pr_info("[CAM]YUSHAN_ITM_P2W_UFLOW_STATUS=%x\n", read_data);
	rawchip_spi_read_2B2B(YUSHAN_ITM_P2W_UFLOW_EN_STATUS, &read_data);
	pr_info("[CAM]YUSHAN_ITM_P2W_UFLOW_EN_STATUS=%x\n", read_data);
	rawchip_spi_read_2B2B(YUSHAN_ITM_P2W_UFLOW_STATUS_BCLR, &read_data);
	pr_info("[CAM]YUSHAN_ITM_P2W_UFLOW_STATUS_BCLR=%x\n", read_data);
	rawchip_spi_read_2B2B(YUSHAN_ITM_P2W_UFLOW_STATUS_BSET, &read_data);
	pr_info("[CAM]YUSHAN_ITM_P2W_UFLOW_STATUS_BSET=%x\n", read_data);
	rawchip_spi_read_2B2B(YUSHAN_ITM_P2W_UFLOW_EN_STATUS_BCLR, &read_data);
	pr_info("[CAM]YUSHAN_ITM_P2W_UFLOW_EN_STATUS_BCLR=%x\n", read_data);
	rawchip_spi_read_2B2B(YUSHAN_ITM_P2W_UFLOW_EN_STATUS_BSET, &read_data);
	pr_info("[CAM]YUSHAN_ITM_P2W_UFLOW_EN_STATUS_BSET=%x\n", read_data);
	rawchip_spi_read_2B2B(YUSHAN_IOR_NVM_CTRL, &read_data);
	pr_info("[CAM]YUSHAN_IOR_NVM_CTRL=%x\n", read_data);
	rawchip_spi_read_2B2B(YUSHAN_IOR_NVM_STATUS, &read_data);
	pr_info("[CAM]YUSHAN_IOR_NVM_STATUS=%x\n", read_data);
	rawchip_spi_read_2B2B(YUSHAN_IOR_NVM_DATA_WORD_0, &read_data);
	pr_info("[CAM]YUSHAN_IOR_NVM_DATA_WORD_0=%x\n", read_data);
	rawchip_spi_read_2B2B(YUSHAN_IOR_NVM_DATA_WORD_1, &read_data);
	pr_info("[CAM]YUSHAN_IOR_NVM_DATA_WORD_1=%x\n", read_data);
	rawchip_spi_read_2B2B(YUSHAN_IOR_NVM_DATA_WORD_2, &read_data);
	pr_info("[CAM]YUSHAN_IOR_NVM_DATA_WORD_2=%x\n", read_data);
	rawchip_spi_read_2B2B(YUSHAN_IOR_NVM_DATA_WORD_3, &read_data);
	pr_info("[CAM]YUSHAN_IOR_NVM_DATA_WORD_3=%x\n", read_data);
	rawchip_spi_read_2B2B(YUSHAN_IOR_NVM_HYST, &read_data);
	pr_info("[CAM]YUSHAN_IOR_NVM_HYST=%x\n", read_data);
	rawchip_spi_read_2B2B(YUSHAN_IOR_NVM_PDN, &read_data);
	pr_info("[CAM]YUSHAN_IOR_NVM_PDN=%x\n", read_data);
	rawchip_spi_read_2B2B(YUSHAN_IOR_NVM_PUN, &read_data);
	pr_info("[CAM]YUSHAN_IOR_NVM_PUN=%x\n", read_data);
	rawchip_spi_read_2B2B(YUSHAN_IOR_NVM_LOWEMI, &read_data);
	pr_info("[CAM]YUSHAN_IOR_NVM_LOWEMI=%x\n", read_data);
	rawchip_spi_read_2B2B(YUSHAN_IOR_NVM_PAD_IN, &read_data);
	pr_info("[CAM]YUSHAN_IOR_NVM_PAD_IN=%x\n", read_data);
	rawchip_spi_read_2B2B(YUSHAN_IOR_NVM_RATIO_PAD, &read_data);
	pr_info("[CAM]YUSHAN_IOR_NVM_RATIO_PAD=%x\n", read_data);
	rawchip_spi_read_2B2B(YUSHAN_IOR_NVM_SEND_ITR_PAD1, &read_data);
	pr_info("[CAM]YUSHAN_IOR_NVM_SEND_ITR_PAD1=%x\n", read_data);
	rawchip_spi_read_2B2B(YUSHAN_IOR_NVM_INTR_STATUS, &read_data);
	pr_info("[CAM]YUSHAN_IOR_NVM_INTR_STATUS=%x\n", read_data);
	rawchip_spi_read_2B2B(YUSHAN_IOR_NVM_LDO_STS_REG, &read_data);
	pr_info("[CAM]YUSHAN_IOR_NVM_LDO_STS_REG=%x\n", read_data);
	rawchip_spi_read_2B2B(YUSHAN_T1_DMA_REG_ENABLE, &read_data);
	pr_info("[CAM]YUSHAN_T1_DMA_REG_ENABLE=%x\n", read_data);
	rawchip_spi_read_2B2B(YUSHAN_T1_DMA_REG_VERSION, &read_data);
	pr_info("[CAM]YUSHAN_T1_DMA_REG_VERSION=%x\n", read_data);
	rawchip_spi_read_2B2B(YUSHAN_T1_DMA_REG_STATUS, &read_data);
	pr_info("[CAM]YUSHAN_T1_DMA_REG_STATUS=%x\n", read_data);
	rawchip_spi_read_2B2B(YUSHAN_T1_DMA_REG_REFILL_ELT_NB, &read_data);
	pr_info("[CAM]YUSHAN_T1_DMA_REG_REFILL_ELT_NB=%x\n", read_data);
	rawchip_spi_read_2B2B(YUSHAN_T1_DMA_REG_REFILL_ERROR, &read_data);
	pr_info("[CAM]YUSHAN_T1_DMA_REG_REFILL_ERROR=%x\n", read_data);
	rawchip_spi_read_2B2B(YUSHAN_T1_DMA_REG_DFV_CONTROL, &read_data);
	pr_info("[CAM]YUSHAN_T1_DMA_REG_DFV_CONTROL=%x\n", read_data);
	rawchip_spi_read_2B2B(YUSHAN_T1_DMA_MEM_PAGE, &read_data);
	pr_info("[CAM]YUSHAN_T1_DMA_MEM_PAGE=%x\n", read_data);
	rawchip_spi_read_2B2B(YUSHAN_T1_DMA_MEM_LOWER_ELT, &read_data);
	pr_info("[CAM]YUSHAN_T1_DMA_MEM_LOWER_ELT=%x\n", read_data);
	rawchip_spi_read_2B2B(YUSHAN_T1_DMA_MEM_UPPER_ELT, &read_data);
	pr_info("[CAM]YUSHAN_T1_DMA_MEM_UPPER_ELT=%x\n", read_data);
	rawchip_spi_read_2B2B(YUSHAN_MIPI_RX_ENABLE, &read_data);
	pr_info("[CAM]YUSHAN_MIPI_RX_ENABLE=%x\n", read_data);
	rawchip_spi_read_2B2B(YUSHAN_MIPI_RX_UIX4, &read_data);
	pr_info("[CAM]YUSHAN_MIPI_RX_UIX4=%x\n", read_data);
	rawchip_spi_read_2B2B(YUSHAN_MIPI_RX_SWAP_PINS, &read_data);
	pr_info("[CAM]YUSHAN_MIPI_RX_SWAP_PINS=%x\n", read_data);
	rawchip_spi_read_2B2B(YUSHAN_MIPI_RX_INVERT_HS, &read_data);
	pr_info("[CAM]YUSHAN_MIPI_RX_INVERT_HS=%x\n", read_data);
	rawchip_spi_read_2B2B(YUSHAN_MIPI_RX_STOP_STATE, &read_data);
	pr_info("[CAM]YUSHAN_MIPI_RX_STOP_STATE=%x\n", read_data);
	rawchip_spi_read_2B2B(YUSHAN_MIPI_RX_ULP_STATE, &read_data);
	pr_info("[CAM]YUSHAN_MIPI_RX_ULP_STATE=%x\n", read_data);
	rawchip_spi_read_2B2B(YUSHAN_MIPI_RX_CLK_ACTIVE, &read_data);
	pr_info("[CAM]YUSHAN_MIPI_RX_CLK_ACTIVE=%x\n", read_data);
	rawchip_spi_read_2B2B(YUSHAN_MIPI_RX_FORCE_RX_MODE_DL, &read_data);
	pr_info("[CAM]YUSHAN_MIPI_RX_FORCE_RX_MODE_DL=%x\n", read_data);
	rawchip_spi_read_2B2B(YUSHAN_MIPI_RX_TEST_RESERVED, &read_data);
	pr_info("[CAM]YUSHAN_MIPI_RX_TEST_RESERVED=%x\n", read_data);
	rawchip_spi_read_2B2B(YUSHAN_MIPI_RX_ESC_DL_STS, &read_data);
	pr_info("[CAM]YUSHAN_MIPI_RX_ESC_DL_STS=%x\n", read_data);
	rawchip_spi_read_2B2B(YUSHAN_MIPI_RX_EOT_BYPASS, &read_data);
	pr_info("[CAM]YUSHAN_MIPI_RX_EOT_BYPASS=%x\n", read_data);
	rawchip_spi_read_2B2B(YUSHAN_MIPI_RX_HSRX_SHIFT_CL, &read_data);
	pr_info("[CAM]YUSHAN_MIPI_RX_HSRX_SHIFT_CL=%x\n", read_data);
	rawchip_spi_read_2B2B(YUSHAN_MIPI_RX_HS_RX_SHIFT_DL, &read_data);
	pr_info("[CAM]YUSHAN_MIPI_RX_HS_RX_SHIFT_DL=%x\n", read_data);
	rawchip_spi_read_2B2B(YUSHAN_MIPI_RX_VIL_CL, &read_data);
	pr_info("[CAM]YUSHAN_MIPI_RX_VIL_CL=%x\n", read_data);
	rawchip_spi_read_2B2B(YUSHAN_MIPI_RX_VIL_DL, &read_data);
	pr_info("[CAM]YUSHAN_MIPI_RX_VIL_DL=%x\n", read_data);
	rawchip_spi_read_2B2B(YUSHAN_MIPI_RX_OVERSAMPLE_BYPASS, &read_data);
	pr_info("[CAM]YUSHAN_MIPI_RX_OVERSAMPLE_BYPASS=%x\n", read_data);
	rawchip_spi_read_2B2B(YUSHAN_MIPI_RX_OVERSAMPLE_FLAG1, &read_data);
	pr_info("[CAM]YUSHAN_MIPI_RX_OVERSAMPLE_FLAG1=%x\n", read_data);
	rawchip_spi_read_2B2B(YUSHAN_MIPI_RX_SKEW_OFFSET_1, &read_data);
	pr_info("[CAM]YUSHAN_MIPI_RX_SKEW_OFFSET_1=%x\n", read_data);
	rawchip_spi_read_2B2B(YUSHAN_MIPI_RX_SKEW_OFFSET_2, &read_data);
	pr_info("[CAM]YUSHAN_MIPI_RX_SKEW_OFFSET_2=%x\n", read_data);
	rawchip_spi_read_2B2B(YUSHAN_MIPI_RX_SKEW_OFFSET_3, &read_data);
	pr_info("[CAM]YUSHAN_MIPI_RX_SKEW_OFFSET_3=%x\n", read_data);
	rawchip_spi_read_2B2B(YUSHAN_MIPI_RX_SKEW_OFFSET_4, &read_data);
	pr_info("[CAM]YUSHAN_MIPI_RX_SKEW_OFFSET_4=%x\n", read_data);
	rawchip_spi_read_2B2B(YUSHAN_MIPI_RX_OFFSET_CL, &read_data);
	pr_info("[CAM]YUSHAN_MIPI_RX_OFFSET_CL=%x\n", read_data);
	rawchip_spi_read_2B2B(YUSHAN_MIPI_RX_CALIBRATE, &read_data);
	pr_info("[CAM]YUSHAN_MIPI_RX_CALIBRATE=%x\n", read_data);
	rawchip_spi_read_2B2B(YUSHAN_MIPI_RX_SPECS, &read_data);
	pr_info("[CAM]YUSHAN_MIPI_RX_SPECS=%x\n", read_data);
	rawchip_spi_read_2B2B(YUSHAN_MIPI_RX_COMP, &read_data);
	pr_info("[CAM]YUSHAN_MIPI_RX_COMP=%x\n", read_data);
	rawchip_spi_read_2B2B(YUSHAN_MIPI_RX_MIPI_IN_SHORT, &read_data);
	pr_info("[CAM]YUSHAN_MIPI_RX_MIPI_IN_SHORT=%x\n", read_data);
	rawchip_spi_read_2B2B(YUSHAN_MIPI_RX_LANE_CTRL, &read_data);
	pr_info("[CAM]YUSHAN_MIPI_RX_LANE_CTRL=%x\n", read_data);
	rawchip_spi_read_2B2B(YUSHAN_CSI2_RX_ENABLE, &read_data);
	pr_info("[CAM]YUSHAN_CSI2_RX_ENABLE=%x\n", read_data);
	rawchip_spi_read_2B2B(YUSHAN_CSI2_RX_VER_CTRL, &read_data);
	pr_info("[CAM]YUSHAN_CSI2_RX_VER_CTRL=%x\n", read_data);
	rawchip_spi_read_2B2B(YUSHAN_CSI2_RX_NB_DATA_LANES, &read_data);
	pr_info("[CAM]YUSHAN_CSI2_RX_NB_DATA_LANES=%x\n", read_data);
	rawchip_spi_read_2B2B(YUSHAN_CSI2_RX_IMG_UNPACKING_FORMAT, &read_data);
	pr_info("[CAM]YUSHAN_CSI2_RX_IMG_UNPACKING_FORMAT=%x\n", read_data);
	rawchip_spi_read_2B2B(YUSHAN_CSI2_RX_WAIT_AFTER_PACKET_END, &read_data);
	pr_info("[CAM]YUSHAN_CSI2_RX_WAIT_AFTER_PACKET_END=%x\n", read_data);
	rawchip_spi_read_2B2B(YUSHAN_CSI2_RX_MULTIPLE_OF_5_HSYNC_EXTENSION_ENABLE, &read_data);
	pr_info("[CAM]YUSHAN_CSI2_RX_MULTIPLE_OF_5_HSYNC_EXTENSION_ENABLE=%x\n", read_data);
	rawchip_spi_read_2B2B(YUSHAN_CSI2_RX_MULTIPLE_OF_5_HSYNC_EXTENSION_PADDING_DATA, &read_data);
	pr_info("[CAM]YUSHAN_CSI2_RX_MULTIPLE_OF_5_HSYNC_EXTENSION_PADDING_DATA=%x\n", read_data);
	rawchip_spi_read_2B2B(YUSHAN_CSI2_RX_CHARACTERIZATION_MODE, &read_data);
	pr_info("[CAM]YUSHAN_CSI2_RX_CHARACTERIZATION_MODE=%x\n", read_data);
	rawchip_spi_read_2B2B(YUSHAN_CSI2_RX_BYTE2PIXEL_READ_TH, &read_data);
	pr_info("[CAM]YUSHAN_CSI2_RX_BYTE2PIXEL_READ_TH=%x\n", read_data);
	rawchip_spi_read_2B2B(YUSHAN_CSI2_RX_VIRTUAL_CHANNEL, &read_data);
	pr_info("[CAM]YUSHAN_CSI2_RX_VIRTUAL_CHANNEL=%x\n", read_data);
	rawchip_spi_read_2B2B(YUSHAN_CSI2_RX_DATA_TYPE, &read_data);
	pr_info("[CAM]YUSHAN_CSI2_RX_DATA_TYPE=%x\n", read_data);
	rawchip_spi_read_2B2B(YUSHAN_CSI2_RX_FRAME_NUMBER, &read_data);
	pr_info("[CAM]YUSHAN_CSI2_RX_FRAME_NUMBER=%x\n", read_data);
	rawchip_spi_read_2B2B(YUSHAN_CSI2_RX_LINE_NUMBER, &read_data);
	pr_info("[CAM]YUSHAN_CSI2_RX_LINE_NUMBER=%x\n", read_data);
	rawchip_spi_read_2B2B(YUSHAN_CSI2_RX_DATA_FIELD, &read_data);
	pr_info("[CAM]YUSHAN_CSI2_RX_DATA_FIELD=%x\n", read_data);
	rawchip_spi_read_2B2B(YUSHAN_CSI2_RX_WORD_COUNT, &read_data);
	pr_info("[CAM]YUSHAN_CSI2_RX_WORD_COUNT=%x\n", read_data);
	rawchip_spi_read_2B2B(YUSHAN_CSI2_RX_ECC_ERROR_STATUS, &read_data);
	pr_info("[CAM]YUSHAN_CSI2_RX_ECC_ERROR_STATUS=%x\n", read_data);
	rawchip_spi_read_2B2B(YUSHAN_CSI2_RX_DFV, &read_data);
	pr_info("[CAM]YUSHAN_CSI2_RX_DFV=%x\n", read_data);
	rawchip_spi_read_2B2B(YUSHAN_ITPOINT_ENABLE, &read_data);
	pr_info("[CAM]YUSHAN_ITPOINT_ENABLE=%x\n", read_data);
	rawchip_spi_read_2B2B(YUSHAN_ITPOINT_VERSION, &read_data);
	pr_info("[CAM]YUSHAN_ITPOINT_VERSION=%x\n", read_data);
	rawchip_spi_read_2B2B(YUSHAN_ITPOINT_PIX_POS, &read_data);
	pr_info("[CAM]YUSHAN_ITPOINT_PIX_POS=%x\n", read_data);
	rawchip_spi_read_2B2B(YUSHAN_ITPOINT_LINE_POS, &read_data);
	pr_info("[CAM]YUSHAN_ITPOINT_LINE_POS=%x\n", read_data);
	rawchip_spi_read_2B2B(YUSHAN_ITPOINT_PIX_CNT, &read_data);
	pr_info("[CAM]YUSHAN_ITPOINT_PIX_CNT=%x\n", read_data);
	rawchip_spi_read_2B2B(YUSHAN_ITPOINT_LINE_CNT, &read_data);
	pr_info("[CAM]YUSHAN_ITPOINT_LINE_CNT=%x\n", read_data);
	rawchip_spi_read_2B2B(YUSHAN_ITPOINT_FRAME_CNT, &read_data);
	pr_info("[CAM]YUSHAN_ITPOINT_FRAME_CNT=%x\n", read_data);
	rawchip_spi_read_2B2B(YUSHAN_ITPOINT_DFV, &read_data);
	pr_info("[CAM]YUSHAN_ITPOINT_DFV=%x\n", read_data);
	rawchip_spi_read_2B2B(YUSHAN_IDP_GEN_AUTO_RUN, &read_data);
	pr_info("[CAM]YUSHAN_IDP_GEN_AUTO_RUN=%x\n", read_data);
	rawchip_spi_read_2B2B(YUSHAN_IDP_GEN_VERSION, &read_data);
	pr_info("[CAM]YUSHAN_IDP_GEN_VERSION=%x\n", read_data);
	rawchip_spi_read_2B2B(YUSHAN_IDP_GEN_CONTROL, &read_data);
	pr_info("[CAM]YUSHAN_IDP_GEN_CONTROL=%x\n", read_data);
	rawchip_spi_read_2B2B(YUSHAN_IDP_GEN_LINE_LENGTH, &read_data);
	pr_info("[CAM]YUSHAN_IDP_GEN_LINE_LENGTH=%x\n", read_data);
	rawchip_spi_read_2B2B(YUSHAN_IDP_GEN_FRAME_LENGTH, &read_data);
	pr_info("[CAM]YUSHAN_IDP_GEN_FRAME_LENGTH=%x\n", read_data);
	rawchip_spi_read_2B2B(YUSHAN_IDP_GEN_ERROR_LINES_EOF_GAP, &read_data);
	pr_info("[CAM]YUSHAN_IDP_GEN_ERROR_LINES_EOF_GAP=%x\n", read_data);
	rawchip_spi_read_2B2B(YUSHAN_IDP_GEN_WC_DI_0, &read_data);
	pr_info("[CAM]YUSHAN_IDP_GEN_WC_DI_0=%x\n", read_data);
	rawchip_spi_read_2B2B(YUSHAN_IDP_GEN_WC_DI_1, &read_data);
	pr_info("[CAM]YUSHAN_IDP_GEN_WC_DI_1=%x\n", read_data);
	rawchip_spi_read_2B2B(YUSHAN_IDP_GEN_WC_DI_2, &read_data);
	pr_info("[CAM]YUSHAN_IDP_GEN_WC_DI_2=%x\n", read_data);
	rawchip_spi_read_2B2B(YUSHAN_IDP_GEN_WC_DI_3, &read_data);
	pr_info("[CAM]YUSHAN_IDP_GEN_WC_DI_3=%x\n", read_data);
	rawchip_spi_read_2B2B(YUSHAN_IDP_GEN_WC_DI_4, &read_data);
	pr_info("[CAM]YUSHAN_IDP_GEN_WC_DI_4=%x\n", read_data);
	rawchip_spi_read_2B2B(YUSHAN_IDP_GEN_WC_DI_5, &read_data);
	pr_info("[CAM]YUSHAN_IDP_GEN_WC_DI_5=%x\n", read_data);
	rawchip_spi_read_2B2B(YUSHAN_IDP_GEN_WC_DI_6, &read_data);
	pr_info("[CAM]YUSHAN_IDP_GEN_WC_DI_6=%x\n", read_data);
	rawchip_spi_read_2B2B(YUSHAN_IDP_GEN_WC_DI_7, &read_data);
	pr_info("[CAM]YUSHAN_IDP_GEN_WC_DI_7=%x\n", read_data);
	rawchip_spi_read_2B2B(YUSHAN_IDP_GEN_WC_DI_8, &read_data);
	pr_info("[CAM]YUSHAN_IDP_GEN_WC_DI_8=%x\n", read_data);
	rawchip_spi_read_2B2B(YUSHAN_IDP_GEN_WC_DI_9, &read_data);
	pr_info("[CAM]YUSHAN_IDP_GEN_WC_DI_9=%x\n", read_data);
	rawchip_spi_read_2B2B(YUSHAN_IDP_GEN_WC_DI_10, &read_data);
	pr_info("[CAM]YUSHAN_IDP_GEN_WC_DI_10=%x\n", read_data);
	rawchip_spi_read_2B2B(YUSHAN_IDP_GEN_WC_DI_11, &read_data);
	pr_info("[CAM]YUSHAN_IDP_GEN_WC_DI_11=%x\n", read_data);
	rawchip_spi_read_2B2B(YUSHAN_IDP_GEN_WC_DI_12, &read_data);
	pr_info("[CAM]YUSHAN_IDP_GEN_WC_DI_12=%x\n", read_data);
	rawchip_spi_read_2B2B(YUSHAN_IDP_GEN_WC_DI_13, &read_data);
	pr_info("[CAM]YUSHAN_IDP_GEN_WC_DI_13=%x\n", read_data);
	rawchip_spi_read_2B2B(YUSHAN_IDP_GEN_WC_DI_14, &read_data);
	pr_info("[CAM]YUSHAN_IDP_GEN_WC_DI_14=%x\n", read_data);
	rawchip_spi_read_2B2B(YUSHAN_IDP_GEN_DFV, &read_data);
	pr_info("[CAM]YUSHAN_IDP_GEN_DFV=%x\n", read_data);
	rawchip_spi_read_2B2B(YUSHAN_MIPI_RX_DTCHK_ENABLE, &read_data);
	pr_info("[CAM]YUSHAN_MIPI_RX_DTCHK_ENABLE=%x\n", read_data);
	rawchip_spi_read_2B2B(YUSHAN_MIPI_RX_DTCHK_VERSION_CTRL, &read_data);
	pr_info("[CAM]YUSHAN_MIPI_RX_DTCHK_VERSION_CTRL=%x\n", read_data);
	rawchip_spi_read_2B2B(YUSHAN_MIPI_RX_DTCHK_COLORBAR_WIDTH_BY4_M1, &read_data);
	pr_info("[CAM]YUSHAN_MIPI_RX_DTCHK_COLORBAR_WIDTH_BY4_M1=%x\n", read_data);
	rawchip_spi_read_2B2B(YUSHAN_MIPI_RX_DTCHK_COLOR_BAR_VAL_0, &read_data);
	pr_info("[CAM]YUSHAN_MIPI_RX_DTCHK_COLOR_BAR_VAL_0=%x\n", read_data);
	rawchip_spi_read_2B2B(YUSHAN_MIPI_RX_DTCHK_COLOR_BAR_VAL_1, &read_data);
	pr_info("[CAM]YUSHAN_MIPI_RX_DTCHK_COLOR_BAR_VAL_1=%x\n", read_data);
	rawchip_spi_read_2B2B(YUSHAN_MIPI_RX_DTCHK_COLOR_BAR_VAL_2, &read_data);
	pr_info("[CAM]YUSHAN_MIPI_RX_DTCHK_COLOR_BAR_VAL_2=%x\n", read_data);
	rawchip_spi_read_2B2B(YUSHAN_MIPI_RX_DTCHK_COLOR_BAR_VAL_3, &read_data);
	pr_info("[CAM]YUSHAN_MIPI_RX_DTCHK_COLOR_BAR_VAL_3=%x\n", read_data);
	rawchip_spi_read_2B2B(YUSHAN_MIPI_RX_DTCHK_COLOR_BAR_VAL_4, &read_data);
	pr_info("[CAM]YUSHAN_MIPI_RX_DTCHK_COLOR_BAR_VAL_4=%x\n", read_data);
	rawchip_spi_read_2B2B(YUSHAN_MIPI_RX_DTCHK_COLOR_BAR_VAL_5, &read_data);
	pr_info("[CAM]YUSHAN_MIPI_RX_DTCHK_COLOR_BAR_VAL_5=%x\n", read_data);
	rawchip_spi_read_2B2B(YUSHAN_MIPI_RX_DTCHK_COLOR_BAR_VAL_6, &read_data);
	pr_info("[CAM]YUSHAN_MIPI_RX_DTCHK_COLOR_BAR_VAL_6=%x\n", read_data);
	rawchip_spi_read_2B2B(YUSHAN_MIPI_RX_DTCHK_COLOR_BAR_VAL_7, &read_data);
	pr_info("[CAM]YUSHAN_MIPI_RX_DTCHK_COLOR_BAR_VAL_7=%x\n", read_data);
	rawchip_spi_read_2B2B(YUSHAN_MIPI_RX_DTCHK_COLOR_BAR_IGNORE_ERR_CNT_0, &read_data);
	pr_info("[CAM]YUSHAN_MIPI_RX_DTCHK_COLOR_BAR_IGNORE_ERR_CNT_0=%x\n", read_data);
	rawchip_spi_read_2B2B(YUSHAN_MIPI_RX_DTCHK_COLOR_BAR_IGNORE_ERR_CNT_1, &read_data);
	pr_info("[CAM]YUSHAN_MIPI_RX_DTCHK_COLOR_BAR_IGNORE_ERR_CNT_1=%x\n", read_data);
	rawchip_spi_read_2B2B(YUSHAN_MIPI_RX_DTCHK_COLOR_BAR_IGNORE_ERR_CNT_2, &read_data);
	pr_info("[CAM]YUSHAN_MIPI_RX_DTCHK_COLOR_BAR_IGNORE_ERR_CNT_2=%x\n", read_data);
	rawchip_spi_read_2B2B(YUSHAN_MIPI_RX_DTCHK_COLOR_BAR_IGNORE_ERR_CNT_3, &read_data);
	pr_info("[CAM]YUSHAN_MIPI_RX_DTCHK_COLOR_BAR_IGNORE_ERR_CNT_3=%x\n", read_data);
	rawchip_spi_read_2B2B(YUSHAN_MIPI_RX_DTCHK_COLOR_BAR_IGNORE_ERR_CNT_4, &read_data);
	pr_info("[CAM]YUSHAN_MIPI_RX_DTCHK_COLOR_BAR_IGNORE_ERR_CNT_4=%x\n", read_data);
	rawchip_spi_read_2B2B(YUSHAN_MIPI_RX_DTCHK_COLOR_BAR_IGNORE_ERR_CNT_5, &read_data);
	pr_info("[CAM]YUSHAN_MIPI_RX_DTCHK_COLOR_BAR_IGNORE_ERR_CNT_5=%x\n", read_data);
	rawchip_spi_read_2B2B(YUSHAN_MIPI_RX_DTCHK_COLOR_BAR_IGNORE_ERR_CNT_6, &read_data);
	pr_info("[CAM]YUSHAN_MIPI_RX_DTCHK_COLOR_BAR_IGNORE_ERR_CNT_6=%x\n", read_data);
	rawchip_spi_read_2B2B(YUSHAN_MIPI_RX_DTCHK_COLOR_BAR_IGNORE_ERR_CNT_7, &read_data);
	pr_info("[CAM]YUSHAN_MIPI_RX_DTCHK_COLOR_BAR_IGNORE_ERR_CNT_7=%x\n", read_data);
	rawchip_spi_read_2B2B(YUSHAN_MIPI_RX_DTCHK_COLOR_BAR_ERRVAL_0, &read_data);
	pr_info("[CAM]YUSHAN_MIPI_RX_DTCHK_COLOR_BAR_ERRVAL_0=%x\n", read_data);
	rawchip_spi_read_2B2B(YUSHAN_MIPI_RX_DTCHK_COLOR_BAR_ERRVAL_1, &read_data);
	pr_info("[CAM]YUSHAN_MIPI_RX_DTCHK_COLOR_BAR_ERRVAL_1=%x\n", read_data);
	rawchip_spi_read_2B2B(YUSHAN_MIPI_RX_DTCHK_COLOR_BAR_ERRVAL_2, &read_data);
	pr_info("[CAM]YUSHAN_MIPI_RX_DTCHK_COLOR_BAR_ERRVAL_2=%x\n", read_data);
	rawchip_spi_read_2B2B(YUSHAN_MIPI_RX_DTCHK_COLOR_BAR_ERRVAL_3, &read_data);
	pr_info("[CAM]YUSHAN_MIPI_RX_DTCHK_COLOR_BAR_ERRVAL_3=%x\n", read_data);
	rawchip_spi_read_2B2B(YUSHAN_MIPI_RX_DTCHK_COLOR_BAR_ERRVAL_4, &read_data);
	pr_info("[CAM]YUSHAN_MIPI_RX_DTCHK_COLOR_BAR_ERRVAL_4=%x\n", read_data);
	rawchip_spi_read_2B2B(YUSHAN_MIPI_RX_DTCHK_COLOR_BAR_ERRVAL_5, &read_data);
	pr_info("[CAM]YUSHAN_MIPI_RX_DTCHK_COLOR_BAR_ERRVAL_5=%x\n", read_data);
	rawchip_spi_read_2B2B(YUSHAN_MIPI_RX_DTCHK_COLOR_BAR_ERRVAL_6, &read_data);
	pr_info("[CAM]YUSHAN_MIPI_RX_DTCHK_COLOR_BAR_ERRVAL_6=%x\n", read_data);
	rawchip_spi_read_2B2B(YUSHAN_MIPI_RX_DTCHK_COLOR_BAR_ERRVAL_7, &read_data);
	pr_info("[CAM]YUSHAN_MIPI_RX_DTCHK_COLOR_BAR_ERRVAL_7=%x\n", read_data);
	rawchip_spi_read_2B2B(YUSHAN_MIPI_RX_DTCHK_COLOR_BAR_ERR_POS_0, &read_data);
	pr_info("[CAM]YUSHAN_MIPI_RX_DTCHK_COLOR_BAR_ERR_POS_0=%x\n", read_data);
	rawchip_spi_read_2B2B(YUSHAN_MIPI_RX_DTCHK_COLOR_BAR_ERR_POS_1, &read_data);
	pr_info("[CAM]YUSHAN_MIPI_RX_DTCHK_COLOR_BAR_ERR_POS_1=%x\n", read_data);
	rawchip_spi_read_2B2B(YUSHAN_MIPI_RX_DTCHK_COLOR_BAR_ERR_POS_2, &read_data);
	pr_info("[CAM]YUSHAN_MIPI_RX_DTCHK_COLOR_BAR_ERR_POS_2=%x\n", read_data);
	rawchip_spi_read_2B2B(YUSHAN_MIPI_RX_DTCHK_COLOR_BAR_ERR_POS_3, &read_data);
	pr_info("[CAM]YUSHAN_MIPI_RX_DTCHK_COLOR_BAR_ERR_POS_3=%x\n", read_data);
	rawchip_spi_read_2B2B(YUSHAN_MIPI_RX_DTCHK_COLOR_BAR_ERR_POS_4, &read_data);
	pr_info("[CAM]YUSHAN_MIPI_RX_DTCHK_COLOR_BAR_ERR_POS_4=%x\n", read_data);
	rawchip_spi_read_2B2B(YUSHAN_MIPI_RX_DTCHK_COLOR_BAR_ERR_POS_5, &read_data);
	pr_info("[CAM]YUSHAN_MIPI_RX_DTCHK_COLOR_BAR_ERR_POS_5=%x\n", read_data);
	rawchip_spi_read_2B2B(YUSHAN_MIPI_RX_DTCHK_COLOR_BAR_ERR_POS_6, &read_data);
	pr_info("[CAM]YUSHAN_MIPI_RX_DTCHK_COLOR_BAR_ERR_POS_6=%x\n", read_data);
	rawchip_spi_read_2B2B(YUSHAN_MIPI_RX_DTCHK_COLOR_BAR_ERR_POS_7, &read_data);
	pr_info("[CAM]YUSHAN_MIPI_RX_DTCHK_COLOR_BAR_ERR_POS_7=%x\n", read_data);
	rawchip_spi_read_2B2B(YUSHAN_MIPI_RX_DTCHK_DFV, &read_data);
	pr_info("[CAM]YUSHAN_MIPI_RX_DTCHK_DFV=%x\n", read_data);
	rawchip_spi_read_2B2B(YUSHAN_PATTERN_GEN_ENABLE, &read_data);
	pr_info("[CAM]YUSHAN_PATTERN_GEN_ENABLE=%x\n", read_data);
	rawchip_spi_read_2B2B(YUSHAN_PATTERN_GEN_VERSION, &read_data);
	pr_info("[CAM]YUSHAN_PATTERN_GEN_VERSION=%x\n", read_data);
	rawchip_spi_read_2B2B(YUSHAN_PATTERN_GEN_PATTERN_TYPE_REQ, &read_data);
	pr_info("[CAM]YUSHAN_PATTERN_GEN_PATTERN_TYPE_REQ=%x\n", read_data);
	rawchip_spi_read_2B2B(YUSHAN_PATTERN_GEN_TPAT_DATA_RG, &read_data);
	pr_info("[CAM]YUSHAN_PATTERN_GEN_TPAT_DATA_RG=%x\n", read_data);
	rawchip_spi_read_2B2B(YUSHAN_PATTERN_GEN_TPAT_DATA_BG, &read_data);
	pr_info("[CAM]YUSHAN_PATTERN_GEN_TPAT_DATA_BG=%x\n", read_data);
	rawchip_spi_read_2B2B(YUSHAN_PATTERN_GEN_TPAT_HCUR_WP, &read_data);
	pr_info("[CAM]YUSHAN_PATTERN_GEN_TPAT_HCUR_WP=%x\n", read_data);
	rawchip_spi_read_2B2B(YUSHAN_PATTERN_GEN_TPAT_VCUR_WP, &read_data);
	pr_info("[CAM]YUSHAN_PATTERN_GEN_TPAT_VCUR_WP=%x\n", read_data);
	rawchip_spi_read_2B2B(YUSHAN_PATTERN_GEN_PATTERN_TYPE_STATUS, &read_data);
	pr_info("[CAM]YUSHAN_PATTERN_GEN_PATTERN_TYPE_STATUS=%x\n", read_data);
	rawchip_spi_read_2B2B(YUSHAN_SMIA_DCPX_ENABLE, &read_data);
	pr_info("[CAM]YUSHAN_SMIA_DCPX_ENABLE=%x\n", read_data);
	rawchip_spi_read_2B2B(YUSHAN_SMIA_DCPX_VERSION, &read_data);
	pr_info("[CAM]YUSHAN_SMIA_DCPX_VERSION=%x\n", read_data);
	rawchip_spi_read_2B2B(YUSHAN_SMIA_DCPX_ENABLE_STATUS, &read_data);
	pr_info("[CAM]YUSHAN_SMIA_DCPX_ENABLE_STATUS=%x\n", read_data);
	rawchip_spi_read_2B2B(YUSHAN_SMIA_DCPX_MODE_REQ, &read_data);
	pr_info("[CAM]YUSHAN_SMIA_DCPX_MODE_REQ=%x\n", read_data);
	rawchip_spi_read_2B2B(YUSHAN_SMIA_DCPX_MODE_STATUS, &read_data);
	pr_info("[CAM]YUSHAN_SMIA_DCPX_MODE_STATUS=%x\n", read_data);
	rawchip_spi_read_2B2B(YUSHAN_SMIA_CPX_CTRL_REQ, &read_data);
	pr_info("[CAM]YUSHAN_SMIA_CPX_CTRL_REQ=%x\n", read_data);
	rawchip_spi_read_2B2B(YUSHAN_SMIA_CPX_MODE_REQ, &read_data);
	pr_info("[CAM]YUSHAN_SMIA_CPX_MODE_REQ=%x\n", read_data);
	rawchip_spi_read_2B2B(YUSHAN_SMIA_CPX_CTRL_STATUS, &read_data);
	pr_info("[CAM]YUSHAN_SMIA_CPX_CTRL_STATUS=%x\n", read_data);
	rawchip_spi_read_2B2B(YUSHAN_SMIA_CPX_MODE_STATUS, &read_data);
	pr_info("[CAM]YUSHAN_SMIA_CPX_MODE_STATUS=%x\n", read_data);
	rawchip_spi_read_2B2B(YUSHAN_SMIA_FM_CTRL, &read_data);
	pr_info("[CAM]YUSHAN_SMIA_FM_CTRL=%x\n", read_data);
	rawchip_spi_read_2B2B(YUSHAN_SMIA_FM_PIX_WIDTH, &read_data);
	pr_info("[CAM]YUSHAN_SMIA_FM_PIX_WIDTH=%x\n", read_data);
	rawchip_spi_read_2B2B(YUSHAN_SMIA_FM_GROUPED_PARAMETER_HOLD, &read_data);
	pr_info("[CAM]YUSHAN_SMIA_FM_GROUPED_PARAMETER_HOLD=%x\n", read_data);
	rawchip_spi_read_2B2B(YUSHAN_SMIA_FM_EOF_INT_EN, &read_data);
	pr_info("[CAM]YUSHAN_SMIA_FM_EOF_INT_EN=%x\n", read_data);
	rawchip_spi_read_2B2B(YUSHAN_SMIA_FM_EOF_INT_CTRL, &read_data);
	pr_info("[CAM]YUSHAN_SMIA_FM_EOF_INT_CTRL=%x\n", read_data);
	rawchip_spi_read_2B2B(YUSHAN_P2W_FIFO_WR_CTRL, &read_data);
	pr_info("[CAM]YUSHAN_P2W_FIFO_WR_CTRL=%x\n", read_data);
	rawchip_spi_read_2B2B(YUSHAN_P2W_FIFO_WR_STATUS, &read_data);
	pr_info("[CAM]YUSHAN_P2W_FIFO_WR_STATUS=%x\n", read_data);
	rawchip_spi_read_2B2B(YUSHAN_P2W_FIFO_RD_CTRL, &read_data);
	pr_info("[CAM]YUSHAN_P2W_FIFO_RD_CTRL=%x\n", read_data);
	rawchip_spi_read_2B2B(YUSHAN_P2W_FIFO_RD_STATUS, &read_data);
	pr_info("[CAM]YUSHAN_P2W_FIFO_RD_STATUS=%x\n", read_data);
	rawchip_spi_read_2B2B(YUSHAN_CSI2_TX_WRAPPER_CTRL, &read_data);
	pr_info("[CAM]YUSHAN_CSI2_TX_WRAPPER_CTRL=%x\n", read_data);
	rawchip_spi_read_2B2B(YUSHAN_CSI2_TX_WRAPPER_THRESH, &read_data);
	pr_info("[CAM]YUSHAN_CSI2_TX_WRAPPER_THRESH=%x\n", read_data);
	rawchip_spi_read_2B2B(YUSHAN_CSI2_TX_WRAPPER_CHAR_EN, &read_data);
	pr_info("[CAM]YUSHAN_CSI2_TX_WRAPPER_CHAR_EN=%x\n", read_data);
	rawchip_spi_read_2B2B(YUSHAN_CSI2_TX_ENABLE, &read_data);
	pr_info("[CAM]YUSHAN_CSI2_TX_ENABLE=%x\n", read_data);
	rawchip_spi_read_2B2B(YUSHAN_CSI2_TX_VERSION_CTRL, &read_data);
	pr_info("[CAM]YUSHAN_CSI2_TX_VERSION_CTRL=%x\n", read_data);
	rawchip_spi_read_2B2B(YUSHAN_CSI2_TX_NUMBER_OF_LANES, &read_data);
	pr_info("[CAM]YUSHAN_CSI2_TX_NUMBER_OF_LANES=%x\n", read_data);
	rawchip_spi_read_2B2B(YUSHAN_CSI2_TX_LANE_MAPPING, &read_data);
	pr_info("[CAM]YUSHAN_CSI2_TX_LANE_MAPPING=%x\n", read_data);
	rawchip_spi_read_2B2B(YUSHAN_CSI2_TX_PACKET_CONTROL, &read_data);
	pr_info("[CAM]YUSHAN_CSI2_TX_PACKET_CONTROL=%x\n", read_data);
	rawchip_spi_read_2B2B(YUSHAN_CSI2_TX_INTERPACKET_DELAY, &read_data);
	pr_info("[CAM]YUSHAN_CSI2_TX_INTERPACKET_DELAY=%x\n", read_data);
	rawchip_spi_read_2B2B(YUSHAN_CSI2_TX_STATUS_LINE_SIZE, &read_data);
	pr_info("[CAM]YUSHAN_CSI2_TX_STATUS_LINE_SIZE=%x\n", read_data);
	rawchip_spi_read_2B2B(YUSHAN_CSI2_TX_STATUS_LINE_CTRL, &read_data);
	pr_info("[CAM]YUSHAN_CSI2_TX_STATUS_LINE_CTRL=%x\n", read_data);
	rawchip_spi_read_2B2B(YUSHAN_CSI2_TX_VC_CTRL_0, &read_data);
	pr_info("[CAM]YUSHAN_CSI2_TX_VC_CTRL_0=%x\n", read_data);
	rawchip_spi_read_2B2B(YUSHAN_CSI2_TX_VC_CTRL_1, &read_data);
	pr_info("[CAM]YUSHAN_CSI2_TX_VC_CTRL_1=%x\n", read_data);
	rawchip_spi_read_2B2B(YUSHAN_CSI2_TX_VC_CTRL_2, &read_data);
	pr_info("[CAM]YUSHAN_CSI2_TX_VC_CTRL_2=%x\n", read_data);
	rawchip_spi_read_2B2B(YUSHAN_CSI2_TX_VC_CTRL_3, &read_data);
	pr_info("[CAM]YUSHAN_CSI2_TX_VC_CTRL_3=%x\n", read_data);
	rawchip_spi_read_2B2B(YUSHAN_CSI2_TX_FRAME_NO_0, &read_data);
	pr_info("[CAM]YUSHAN_CSI2_TX_FRAME_NO_0=%x\n", read_data);
	rawchip_spi_read_2B2B(YUSHAN_CSI2_TX_FRAME_NO_1, &read_data);
	pr_info("[CAM]YUSHAN_CSI2_TX_FRAME_NO_1=%x\n", read_data);
	rawchip_spi_read_2B2B(YUSHAN_CSI2_TX_FRAME_NO_2, &read_data);
	pr_info("[CAM]YUSHAN_CSI2_TX_FRAME_NO_2=%x\n", read_data);
	rawchip_spi_read_2B2B(YUSHAN_CSI2_TX_FRAME_NO_3, &read_data);
	pr_info("[CAM]YUSHAN_CSI2_TX_FRAME_NO_3=%x\n", read_data);
	rawchip_spi_read_2B2B(YUSHAN_CSI2_TX_BYTE_COUNT, &read_data);
	pr_info("[CAM]YUSHAN_CSI2_TX_BYTE_COUNT=%x\n", read_data);
	rawchip_spi_read_2B2B(YUSHAN_CSI2_TX_CURRENT_DATA_IDENTIFIER, &read_data);
	pr_info("[CAM]YUSHAN_CSI2_TX_CURRENT_DATA_IDENTIFIER=%x\n", read_data);
	rawchip_spi_read_2B2B(YUSHAN_CSI2_TX_DFV, &read_data);
	pr_info("[CAM]YUSHAN_CSI2_TX_DFV=%x\n", read_data);
	rawchip_spi_read_2B2B(YUSHAN_CSI2_TX_PACKET_SIZE_0, &read_data);
	pr_info("[CAM]YUSHAN_CSI2_TX_PACKET_SIZE_0=%x\n", read_data);
	rawchip_spi_read_2B2B(YUSHAN_CSI2_TX_DI_INDEX_CTRL_0, &read_data);
	pr_info("[CAM]YUSHAN_CSI2_TX_DI_INDEX_CTRL_0=%x\n", read_data);
	rawchip_spi_read_2B2B(YUSHAN_CSI2_TX_LINE_NO_0, &read_data);
	pr_info("[CAM]YUSHAN_CSI2_TX_LINE_NO_0=%x\n", read_data);
	rawchip_spi_read_2B2B(YUSHAN_CSI2_TX_PACKET_SIZE_1, &read_data);
	pr_info("[CAM]YUSHAN_CSI2_TX_PACKET_SIZE_1=%x\n", read_data);
	rawchip_spi_read_2B2B(YUSHAN_CSI2_TX_DI_INDEX_CTRL_1, &read_data);
	pr_info("[CAM]YUSHAN_CSI2_TX_DI_INDEX_CTRL_1=%x\n", read_data);
	rawchip_spi_read_2B2B(YUSHAN_CSI2_TX_LINE_NO_1, &read_data);
	pr_info("[CAM]YUSHAN_CSI2_TX_LINE_NO_1=%x\n", read_data);
	rawchip_spi_read_2B2B(YUSHAN_CSI2_TX_PACKET_SIZE_2, &read_data);
	pr_info("[CAM]YUSHAN_CSI2_TX_PACKET_SIZE_2=%x\n", read_data);
	rawchip_spi_read_2B2B(YUSHAN_CSI2_TX_DI_INDEX_CTRL_2, &read_data);
	pr_info("[CAM]YUSHAN_CSI2_TX_DI_INDEX_CTRL_2=%x\n", read_data);
	rawchip_spi_read_2B2B(YUSHAN_CSI2_TX_LINE_NO_2, &read_data);
	pr_info("[CAM]YUSHAN_CSI2_TX_LINE_NO_2=%x\n", read_data);
	rawchip_spi_read_2B2B(YUSHAN_CSI2_TX_PACKET_SIZE_3, &read_data);
	pr_info("[CAM]YUSHAN_CSI2_TX_PACKET_SIZE_3=%x\n", read_data);
	rawchip_spi_read_2B2B(YUSHAN_CSI2_TX_DI_INDEX_CTRL_3, &read_data);
	pr_info("[CAM]YUSHAN_CSI2_TX_DI_INDEX_CTRL_3=%x\n", read_data);
	rawchip_spi_read_2B2B(YUSHAN_CSI2_TX_LINE_NO_3, &read_data);
	pr_info("[CAM]YUSHAN_CSI2_TX_LINE_NO_3=%x\n", read_data);
	rawchip_spi_read_2B2B(YUSHAN_CSI2_TX_PACKET_SIZE_4, &read_data);
	pr_info("[CAM]YUSHAN_CSI2_TX_PACKET_SIZE_4=%x\n", read_data);
	rawchip_spi_read_2B2B(YUSHAN_CSI2_TX_DI_INDEX_CTRL_4, &read_data);
	pr_info("[CAM]YUSHAN_CSI2_TX_DI_INDEX_CTRL_4=%x\n", read_data);
	rawchip_spi_read_2B2B(YUSHAN_CSI2_TX_LINE_NO_4, &read_data);
	pr_info("[CAM]YUSHAN_CSI2_TX_LINE_NO_4=%x\n", read_data);
	rawchip_spi_read_2B2B(YUSHAN_CSI2_TX_PACKET_SIZE_5, &read_data);
	pr_info("[CAM]YUSHAN_CSI2_TX_PACKET_SIZE_5=%x\n", read_data);
	rawchip_spi_read_2B2B(YUSHAN_CSI2_TX_DI_INDEX_CTRL_5, &read_data);
	pr_info("[CAM]YUSHAN_CSI2_TX_DI_INDEX_CTRL_5=%x\n", read_data);
	rawchip_spi_read_2B2B(YUSHAN_CSI2_TX_LINE_NO_5, &read_data);
	pr_info("[CAM]YUSHAN_CSI2_TX_LINE_NO_5=%x\n", read_data);
	rawchip_spi_read_2B2B(YUSHAN_CSI2_TX_PACKET_SIZE_6, &read_data);
	pr_info("[CAM]YUSHAN_CSI2_TX_PACKET_SIZE_6=%x\n", read_data);
	rawchip_spi_read_2B2B(YUSHAN_CSI2_TX_DI_INDEX_CTRL_6, &read_data);
	pr_info("[CAM]YUSHAN_CSI2_TX_DI_INDEX_CTRL_6=%x\n", read_data);
	rawchip_spi_read_2B2B(YUSHAN_CSI2_TX_LINE_NO_6, &read_data);
	pr_info("[CAM]YUSHAN_CSI2_TX_LINE_NO_6=%x\n", read_data);
	rawchip_spi_read_2B2B(YUSHAN_CSI2_TX_PACKET_SIZE_7, &read_data);
	pr_info("[CAM]YUSHAN_CSI2_TX_PACKET_SIZE_7=%x\n", read_data);
	rawchip_spi_read_2B2B(YUSHAN_CSI2_TX_DI_INDEX_CTRL_7, &read_data);
	pr_info("[CAM]YUSHAN_CSI2_TX_DI_INDEX_CTRL_7=%x\n", read_data);
	rawchip_spi_read_2B2B(YUSHAN_CSI2_TX_LINE_NO_7, &read_data);
	pr_info("[CAM]YUSHAN_CSI2_TX_LINE_NO_7=%x\n", read_data);
	rawchip_spi_read_2B2B(YUSHAN_CSI2_TX_PACKET_SIZE_8, &read_data);
	pr_info("[CAM]YUSHAN_CSI2_TX_PACKET_SIZE_8=%x\n", read_data);
	rawchip_spi_read_2B2B(YUSHAN_CSI2_TX_DI_INDEX_CTRL_8, &read_data);
	pr_info("[CAM]YUSHAN_CSI2_TX_DI_INDEX_CTRL_8=%x\n", read_data);
	rawchip_spi_read_2B2B(YUSHAN_CSI2_TX_LINE_NO_8, &read_data);
	pr_info("[CAM]YUSHAN_CSI2_TX_LINE_NO_8=%x\n", read_data);
	rawchip_spi_read_2B2B(YUSHAN_CSI2_TX_PACKET_SIZE_9, &read_data);
	pr_info("[CAM]YUSHAN_CSI2_TX_PACKET_SIZE_9=%x\n", read_data);
	rawchip_spi_read_2B2B(YUSHAN_CSI2_TX_DI_INDEX_CTRL_9, &read_data);
	pr_info("[CAM]YUSHAN_CSI2_TX_DI_INDEX_CTRL_9=%x\n", read_data);
	rawchip_spi_read_2B2B(YUSHAN_CSI2_TX_LINE_NO_9, &read_data);
	pr_info("[CAM]YUSHAN_CSI2_TX_LINE_NO_9=%x\n", read_data);
	rawchip_spi_read_2B2B(YUSHAN_CSI2_TX_PACKET_SIZE_10, &read_data);
	pr_info("[CAM]YUSHAN_CSI2_TX_PACKET_SIZE_10=%x\n", read_data);
	rawchip_spi_read_2B2B(YUSHAN_CSI2_TX_DI_INDEX_CTRL_10, &read_data);
	pr_info("[CAM]YUSHAN_CSI2_TX_DI_INDEX_CTRL_10=%x\n", read_data);
	rawchip_spi_read_2B2B(YUSHAN_CSI2_TX_LINE_NO_10, &read_data);
	pr_info("[CAM]YUSHAN_CSI2_TX_LINE_NO_10=%x\n", read_data);
	rawchip_spi_read_2B2B(YUSHAN_CSI2_TX_PACKET_SIZE_11, &read_data);
	pr_info("[CAM]YUSHAN_CSI2_TX_PACKET_SIZE_11=%x\n", read_data);
	rawchip_spi_read_2B2B(YUSHAN_CSI2_TX_DI_INDEX_CTRL_11, &read_data);
	pr_info("[CAM]YUSHAN_CSI2_TX_DI_INDEX_CTRL_11=%x\n", read_data);
	rawchip_spi_read_2B2B(YUSHAN_CSI2_TX_LINE_NO_11, &read_data);
	pr_info("[CAM]YUSHAN_CSI2_TX_LINE_NO_11=%x\n", read_data);
	rawchip_spi_read_2B2B(YUSHAN_CSI2_TX_PACKET_SIZE_12, &read_data);
	pr_info("[CAM]YUSHAN_CSI2_TX_PACKET_SIZE_12=%x\n", read_data);
	rawchip_spi_read_2B2B(YUSHAN_CSI2_TX_DI_INDEX_CTRL_12, &read_data);
	pr_info("[CAM]YUSHAN_CSI2_TX_DI_INDEX_CTRL_12=%x\n", read_data);
	rawchip_spi_read_2B2B(YUSHAN_CSI2_TX_LINE_NO_12, &read_data);
	pr_info("[CAM]YUSHAN_CSI2_TX_LINE_NO_12=%x\n", read_data);
	rawchip_spi_read_2B2B(YUSHAN_CSI2_TX_PACKET_SIZE_13, &read_data);
	pr_info("[CAM]YUSHAN_CSI2_TX_PACKET_SIZE_13=%x\n", read_data);
	rawchip_spi_read_2B2B(YUSHAN_CSI2_TX_DI_INDEX_CTRL_13, &read_data);
	pr_info("[CAM]YUSHAN_CSI2_TX_DI_INDEX_CTRL_13=%x\n", read_data);
	rawchip_spi_read_2B2B(YUSHAN_CSI2_TX_LINE_NO_13, &read_data);
	pr_info("[CAM]YUSHAN_CSI2_TX_LINE_NO_13=%x\n", read_data);
	rawchip_spi_read_2B2B(YUSHAN_CSI2_TX_PACKET_SIZE_14, &read_data);
	pr_info("[CAM]YUSHAN_CSI2_TX_PACKET_SIZE_14=%x\n", read_data);
	rawchip_spi_read_2B2B(YUSHAN_CSI2_TX_DI_INDEX_CTRL_14, &read_data);
	pr_info("[CAM]YUSHAN_CSI2_TX_DI_INDEX_CTRL_14=%x\n", read_data);
	rawchip_spi_read_2B2B(YUSHAN_CSI2_TX_LINE_NO_14, &read_data);
	pr_info("[CAM]YUSHAN_CSI2_TX_LINE_NO_14=%x\n", read_data);
	rawchip_spi_read_2B2B(YUSHAN_CSI2_TX_PACKET_SIZE_15, &read_data);
	pr_info("[CAM]YUSHAN_CSI2_TX_PACKET_SIZE_15=%x\n", read_data);
	rawchip_spi_read_2B2B(YUSHAN_CSI2_TX_DI_INDEX_CTRL_15, &read_data);
	pr_info("[CAM]YUSHAN_CSI2_TX_DI_INDEX_CTRL_15=%x\n", read_data);
	rawchip_spi_read_2B2B(YUSHAN_CSI2_TX_LINE_NO_15, &read_data);
	pr_info("[CAM]YUSHAN_CSI2_TX_LINE_NO_15=%x\n", read_data);
	rawchip_spi_read_2B2B(YUSHAN_MIPI_TX_ENABLE, &read_data);
	pr_info("[CAM]YUSHAN_MIPI_TX_ENABLE=%x\n", read_data);
	rawchip_spi_read_2B2B(YUSHAN_MIPI_TX_UIX4, &read_data);
	pr_info("[CAM]YUSHAN_MIPI_TX_UIX4=%x\n", read_data);
	rawchip_spi_read_2B2B(YUSHAN_MIPI_TX_SWAP_PINS, &read_data);
	pr_info("[CAM]YUSHAN_MIPI_TX_SWAP_PINS=%x\n", read_data);
	rawchip_spi_read_2B2B(YUSHAN_MIPI_TX_INVERT_HS, &read_data);
	pr_info("[CAM]YUSHAN_MIPI_TX_INVERT_HS=%x\n", read_data);
	rawchip_spi_read_2B2B(YUSHAN_MIPI_TX_STOP_STATE, &read_data);
	pr_info("[CAM]YUSHAN_MIPI_TX_STOP_STATE=%x\n", read_data);
	rawchip_spi_read_2B2B(YUSHAN_MIPI_TX_FORCE_TX_MODE_DL, &read_data);
	pr_info("[CAM]YUSHAN_MIPI_TX_FORCE_TX_MODE_DL=%x\n", read_data);
	rawchip_spi_read_2B2B(YUSHAN_MIPI_TX_ULP_STATE, &read_data);
	pr_info("[CAM]YUSHAN_MIPI_TX_ULP_STATE=%x\n", read_data);
	rawchip_spi_read_2B2B(YUSHAN_MIPI_TX_ULP_EXIT, &read_data);
	pr_info("[CAM]YUSHAN_MIPI_TX_ULP_EXIT=%x\n", read_data);
	rawchip_spi_read_2B2B(YUSHAN_MIPI_TX_ESC_DL, &read_data);
	pr_info("[CAM]YUSHAN_MIPI_TX_ESC_DL=%x\n", read_data);
	rawchip_spi_read_2B2B(YUSHAN_MIPI_TX_HSTX_SLEW, &read_data);
	pr_info("[CAM]YUSHAN_MIPI_TX_HSTX_SLEW=%x\n", read_data);
	rawchip_spi_read_2B2B(YUSHAN_MIPI_TX_SKEW, &read_data);
	pr_info("[CAM]YUSHAN_MIPI_TX_SKEW=%x\n", read_data);
	rawchip_spi_read_2B2B(YUSHAN_MIPI_TX_GPIO_CL, &read_data);
	pr_info("[CAM]YUSHAN_MIPI_TX_GPIO_CL=%x\n", read_data);
	rawchip_spi_read_2B2B(YUSHAN_MIPI_TX_GPIO_DL1, &read_data);
	pr_info("[CAM]YUSHAN_MIPI_TX_GPIO_DL1=%x\n", read_data);
	rawchip_spi_read_2B2B(YUSHAN_MIPI_TX_GPIO_DL2, &read_data);
	pr_info("[CAM]YUSHAN_MIPI_TX_GPIO_DL2=%x\n", read_data);
	rawchip_spi_read_2B2B(YUSHAN_MIPI_TX_GPIO_DL3, &read_data);
	pr_info("[CAM]YUSHAN_MIPI_TX_GPIO_DL3=%x\n", read_data);
	rawchip_spi_read_2B2B(YUSHAN_MIPI_TX_GPIO_DL4, &read_data);
	pr_info("[CAM]YUSHAN_MIPI_TX_GPIO_DL4=%x\n", read_data);
	rawchip_spi_read_2B2B(YUSHAN_MIPI_TX_SPECS, &read_data);
	pr_info("[CAM]YUSHAN_MIPI_TX_SPECS=%x\n", read_data);
	rawchip_spi_read_2B2B(YUSHAN_MIPI_TX_SLEW_RATE, &read_data);
	pr_info("[CAM]YUSHAN_MIPI_TX_SLEW_RATE=%x\n", read_data);
	rawchip_spi_read_2B2B(YUSHAN_MIPI_TX_TEST_RESERVED, &read_data);
	pr_info("[CAM]YUSHAN_MIPI_TX_TEST_RESERVED=%x\n", read_data);
	rawchip_spi_read_2B2B(YUSHAN_MIPI_TX_TCLK_ENABLE, &read_data);
	pr_info("[CAM]YUSHAN_MIPI_TX_TCLK_ENABLE=%x\n", read_data);
	rawchip_spi_read_2B2B(YUSHAN_MIPI_TX_TCLK_POST_DELAY, &read_data);
	pr_info("[CAM]YUSHAN_MIPI_TX_TCLK_POST_DELAY=%x\n", read_data);
	rawchip_spi_read_2B2B(YUSHAN_LINE_FILTER_BYPASS_ENABLE, &read_data);
	pr_info("[CAM]YUSHAN_LINE_FILTER_BYPASS_ENABLE=%x\n", read_data);
	rawchip_spi_read_2B2B(YUSHAN_LINE_FILTER_BYPASS_VERSION, &read_data);
	pr_info("[CAM]YUSHAN_LINE_FILTER_BYPASS_VERSION=%x\n", read_data);
	rawchip_spi_read_2B2B(YUSHAN_LINE_FILTER_BYPASS_LSTART_LEVEL, &read_data);
	pr_info("[CAM]YUSHAN_LINE_FILTER_BYPASS_LSTART_LEVEL=%x\n", read_data);
	rawchip_spi_read_2B2B(YUSHAN_LINE_FILTER_BYPASS_LSTOP_LEVEL, &read_data);
	pr_info("[CAM]YUSHAN_LINE_FILTER_BYPASS_LSTOP_LEVEL=%x\n", read_data);
	rawchip_spi_read_2B2B(YUSHAN_DTFILTER_BYPASS_ENABLE, &read_data);
	pr_info("[CAM]YUSHAN_DTFILTER_BYPASS_ENABLE=%x\n", read_data);
	rawchip_spi_read_2B2B(YUSHAN_DTFILTER_BYPASS_VERSION, &read_data);
	pr_info("[CAM]YUSHAN_DTFILTER_BYPASS_VERSION=%x\n", read_data);
	rawchip_spi_read_2B2B(YUSHAN_DTFILTER_BYPASS_MATCH0, &read_data);
	pr_info("[CAM]YUSHAN_DTFILTER_BYPASS_MATCH0=%x\n", read_data);
	rawchip_spi_read_2B2B(YUSHAN_DTFILTER_BYPASS_MATCH1, &read_data);
	pr_info("[CAM]YUSHAN_DTFILTER_BYPASS_MATCH1=%x\n", read_data);
	rawchip_spi_read_2B2B(YUSHAN_DTFILTER_BYPASS_MATCH2, &read_data);
	pr_info("[CAM]YUSHAN_DTFILTER_BYPASS_MATCH2=%x\n", read_data);
	rawchip_spi_read_2B2B(YUSHAN_DTFILTER_BYPASS_MATCH3, &read_data);
	pr_info("[CAM]YUSHAN_DTFILTER_BYPASS_MATCH3=%x\n", read_data);
	rawchip_spi_read_2B2B(YUSHAN_LINE_FILTER_DXO_ENABLE, &read_data);
	pr_info("[CAM]YUSHAN_LINE_FILTER_DXO_ENABLE=%x\n", read_data);
	rawchip_spi_read_2B2B(YUSHAN_LINE_FILTER_DXO_VERSION, &read_data);
	pr_info("[CAM]YUSHAN_LINE_FILTER_DXO_VERSION=%x\n", read_data);
	rawchip_spi_read_2B2B(YUSHAN_LINE_FILTER_DXO_LSTART_LEVEL, &read_data);
	pr_info("[CAM]YUSHAN_LINE_FILTER_DXO_LSTART_LEVEL=%x\n", read_data);
	rawchip_spi_read_2B2B(YUSHAN_LINE_FILTER_DXO_LSTOP_LEVEL, &read_data);
	pr_info("[CAM]YUSHAN_LINE_FILTER_DXO_LSTOP_LEVEL=%x\n", read_data);
	rawchip_spi_read_2B2B(YUSHAN_DTFILTER_DXO_ENABLE, &read_data);
	pr_info("[CAM]YUSHAN_DTFILTER_DXO_ENABLE=%x\n", read_data);
	rawchip_spi_read_2B2B(YUSHAN_DTFILTER_DXO_VERSION, &read_data);
	pr_info("[CAM]YUSHAN_DTFILTER_DXO_VERSION=%x\n", read_data);
	rawchip_spi_read_2B2B(YUSHAN_DTFILTER_DXO_MATCH0, &read_data);
	pr_info("[CAM]YUSHAN_DTFILTER_DXO_MATCH0=%x\n", read_data);
	rawchip_spi_read_2B2B(YUSHAN_DTFILTER_DXO_MATCH1, &read_data);
	pr_info("[CAM]YUSHAN_DTFILTER_DXO_MATCH1=%x\n", read_data);
	rawchip_spi_read_2B2B(YUSHAN_DTFILTER_DXO_MATCH2, &read_data);
	pr_info("[CAM]YUSHAN_DTFILTER_DXO_MATCH2=%x\n", read_data);
	rawchip_spi_read_2B2B(YUSHAN_DTFILTER_DXO_MATCH3, &read_data);
	pr_info("[CAM]YUSHAN_DTFILTER_DXO_MATCH3=%x\n", read_data);
	rawchip_spi_read_2B2B(YUSHAN_EOF_RESIZE_PRE_DXO_ENABLE, &read_data);
	pr_info("[CAM]YUSHAN_EOF_RESIZE_PRE_DXO_ENABLE=%x\n", read_data);
	rawchip_spi_read_2B2B(YUSHAN_EOF_RESIZE_PRE_DXO_VERSION, &read_data);
	pr_info("[CAM]YUSHAN_EOF_RESIZE_PRE_DXO_VERSION=%x\n", read_data);
	rawchip_spi_read_2B2B(YUSHAN_EOF_RESIZE_PRE_DXO_AUTOMATIC_CONTROL, &read_data);
	pr_info("[CAM]YUSHAN_EOF_RESIZE_PRE_DXO_AUTOMATIC_CONTROL=%x\n", read_data);
	rawchip_spi_read_2B2B(YUSHAN_EOF_RESIZE_PRE_DXO_H_SIZE, &read_data);
	pr_info("[CAM]YUSHAN_EOF_RESIZE_PRE_DXO_H_SIZE=%x\n", read_data);
	rawchip_spi_read_2B2B(YUSHAN_LBE_PRE_DXO_ENABLE, &read_data);
	pr_info("[CAM]YUSHAN_LBE_PRE_DXO_ENABLE=%x\n", read_data);
	rawchip_spi_read_2B2B(YUSHAN_LBE_PRE_DXO_VERSION, &read_data);
	pr_info("[CAM]YUSHAN_LBE_PRE_DXO_VERSION=%x\n", read_data);
	rawchip_spi_read_2B2B(YUSHAN_LBE_PRE_DXO_DFV, &read_data);
	pr_info("[CAM]YUSHAN_LBE_PRE_DXO_DFV=%x\n", read_data);
	rawchip_spi_read_2B2B(YUSHAN_LBE_PRE_DXO_H_SIZE, &read_data);
	pr_info("[CAM]YUSHAN_LBE_PRE_DXO_H_SIZE=%x\n", read_data);
	rawchip_spi_read_2B2B(YUSHAN_LBE_PRE_DXO_READ_START, &read_data);
	pr_info("[CAM]YUSHAN_LBE_PRE_DXO_READ_START=%x\n", read_data);
	rawchip_spi_read_2B2B(YUSHAN_EOF_RESIZE_POST_DXO_ENABLE, &read_data);
	pr_info("[CAM]YUSHAN_EOF_RESIZE_POST_DXO_ENABLE=%x\n", read_data);
	rawchip_spi_read_2B2B(YUSHAN_EOF_RESIZE_POST_DXO_VERSION, &read_data);
	pr_info("[CAM]YUSHAN_EOF_RESIZE_POST_DXO_VERSION=%x\n", read_data);
	rawchip_spi_read_2B2B(YUSHAN_EOF_RESIZE_POST_DXO_AUTOMATIC_CONTROL, &read_data);
	pr_info("[CAM]YUSHAN_EOF_RESIZE_POST_DXO_AUTOMATIC_CONTROL=%x\n", read_data);
	rawchip_spi_read_2B2B(YUSHAN_EOF_RESIZE_POST_DXO_H_SIZE, &read_data);
	pr_info("[CAM]YUSHAN_EOF_RESIZE_POST_DXO_H_SIZE=%x\n", read_data);
	rawchip_spi_read_2B2B(YUSHAN_LECCI_ENABLE, &read_data);
	pr_info("[CAM]YUSHAN_LECCI_ENABLE=%x\n", read_data);
	rawchip_spi_read_2B2B(YUSHAN_LECCI_VERSION, &read_data);
	pr_info("[CAM]YUSHAN_LECCI_VERSION=%x\n", read_data);
	rawchip_spi_read_2B2B(YUSHAN_LECCI_MIN_INTERLINE, &read_data);
	pr_info("[CAM]YUSHAN_LECCI_MIN_INTERLINE=%x\n", read_data);
	rawchip_spi_read_2B2B(YUSHAN_LECCI_OUT_BURST_CTRL, &read_data);
	pr_info("[CAM]YUSHAN_LECCI_OUT_BURST_CTRL=%x\n", read_data);
	rawchip_spi_read_2B2B(YUSHAN_LECCI_LINE_SIZE, &read_data);
	pr_info("[CAM]YUSHAN_LECCI_LINE_SIZE=%x\n", read_data);
	rawchip_spi_read_2B2B(YUSHAN_LECCI_BYPASS_CTRL, &read_data);
	pr_info("[CAM]YUSHAN_LECCI_BYPASS_CTRL=%x\n", read_data);
	rawchip_spi_read_2B2B(YUSHAN_LBE_POST_DXO_ENABLE, &read_data);
	pr_info("[CAM]YUSHAN_LBE_POST_DXO_ENABLE=%x\n", read_data);
	rawchip_spi_read_2B2B(YUSHAN_LBE_POST_DXO_VERSION, &read_data);
	pr_info("[CAM]YUSHAN_LBE_POST_DXO_VERSION=%x\n", read_data);
	rawchip_spi_read_2B2B(YUSHAN_LBE_POST_DXO_DFV, &read_data);
	pr_info("[CAM]YUSHAN_LBE_POST_DXO_DFV=%x\n", read_data);
	rawchip_spi_read_2B2B(YUSHAN_LBE_POST_DXO_H_SIZE, &read_data);
	pr_info("[CAM]YUSHAN_LBE_POST_DXO_H_SIZE=%x\n", read_data);
	rawchip_spi_read_2B2B(YUSHAN_LBE_POST_DXO_READ_START, &read_data);
	pr_info("[CAM]YUSHAN_LBE_POST_DXO_READ_START=%x\n", read_data);

}
