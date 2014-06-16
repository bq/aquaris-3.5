/* Copyright Statement:
 *
 * This software/firmware and related documentation ("MediaTek Software") are
 * protected under relevant copyright laws. The information contained herein
 * is confidential and proprietary to MediaTek Inc. and/or its licensors.
 * Without the prior written permission of MediaTek inc. and/or its licensors,
 * any reproduction, modification, use or disclosure of MediaTek Software,
 * and information contained herein, in whole or in part, shall be strictly prohibited.
 */
/* MediaTek Inc. (C) 2010. All rights reserved.
 *
 * BY OPENING THIS FILE, RECEIVER HEREBY UNEQUIVOCALLY ACKNOWLEDGES AND AGREES
 * THAT THE SOFTWARE/FIRMWARE AND ITS DOCUMENTATIONS ("MEDIATEK SOFTWARE")
 * RECEIVED FROM MEDIATEK AND/OR ITS REPRESENTATIVES ARE PROVIDED TO RECEIVER ON
 * AN "AS-IS" BASIS ONLY. MEDIATEK EXPRESSLY DISCLAIMS ANY AND ALL WARRANTIES,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE OR NONINFRINGEMENT.
 * NEITHER DOES MEDIATEK PROVIDE ANY WARRANTY WHATSOEVER WITH RESPECT TO THE
 * SOFTWARE OF ANY THIRD PARTY WHICH MAY BE USED BY, INCORPORATED IN, OR
 * SUPPLIED WITH THE MEDIATEK SOFTWARE, AND RECEIVER AGREES TO LOOK ONLY TO SUCH
 * THIRD PARTY FOR ANY WARRANTY CLAIM RELATING THERETO. RECEIVER EXPRESSLY ACKNOWLEDGES
 * THAT IT IS RECEIVER'S SOLE RESPONSIBILITY TO OBTAIN FROM ANY THIRD PARTY ALL PROPER LICENSES
 * CONTAINED IN MEDIATEK SOFTWARE. MEDIATEK SHALL ALSO NOT BE RESPONSIBLE FOR ANY MEDIATEK
 * SOFTWARE RELEASES MADE TO RECEIVER'S SPECIFICATION OR TO CONFORM TO A PARTICULAR
 * STANDARD OR OPEN FORUM. RECEIVER'S SOLE AND EXCLUSIVE REMEDY AND MEDIATEK'S ENTIRE AND
 * CUMULATIVE LIABILITY WITH RESPECT TO THE MEDIATEK SOFTWARE RELEASED HEREUNDER WILL BE,
 * AT MEDIATEK'S OPTION, TO REVISE OR REPLACE THE MEDIATEK SOFTWARE AT ISSUE,
 * OR REFUND ANY SOFTWARE LICENSE FEES OR SERVICE CHARGE PAID BY RECEIVER TO
 * MEDIATEK FOR SUCH MEDIATEK SOFTWARE AT ISSUE.
 *
 * The following software/firmware and/or related documentation ("MediaTek Software")
 * have been modified by MediaTek Inc. All revisions are subject to any receiver's
 * applicable license agreements with MediaTek Inc.
 */

/*****************************************************************************
*  Copyright Statement:
*  --------------------
*  This software is protected by Copyright and the information contained
*  herein is confidential. The software may not be copied and the information
*  contained herein may not be used or disclosed except with the written
*  permission of MediaTek Inc. (C) 2008
*
*  BY OPENING THIS FILE, BUYER HEREBY UNEQUIVOCALLY ACKNOWLEDGES AND AGREES
*  THAT THE SOFTWARE/FIRMWARE AND ITS DOCUMENTATIONS ("MEDIATEK SOFTWARE")
*  RECEIVED FROM MEDIATEK AND/OR ITS REPRESENTATIVES ARE PROVIDED TO BUYER ON
*  AN "AS-IS" BASIS ONLY. MEDIATEK EXPRESSLY DISCLAIMS ANY AND ALL WARRANTIES,
*  EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE IMPLIED WARRANTIES OF
*  MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE OR NONINFRINGEMENT.
*  NEITHER DOES MEDIATEK PROVIDE ANY WARRANTY WHATSOEVER WITH RESPECT TO THE
*  SOFTWARE OF ANY THIRD PARTY WHICH MAY BE USED BY, INCORPORATED IN, OR
*  SUPPLIED WITH THE MEDIATEK SOFTWARE, AND BUYER AGREES TO LOOK ONLY TO SUCH
*  THIRD PARTY FOR ANY WARRANTY CLAIM RELATING THERETO. MEDIATEK SHALL ALSO
*  NOT BE RESPONSIBLE FOR ANY MEDIATEK SOFTWARE RELEASES MADE TO BUYER'S
*  SPECIFICATION OR TO CONFORM TO A PARTICULAR STANDARD OR OPEN FORUM.
*
*  BUYER'S SOLE AND EXCLUSIVE REMEDY AND MEDIATEK'S ENTIRE AND CUMULATIVE
*  LIABILITY WITH RESPECT TO THE MEDIATEK SOFTWARE RELEASED HEREUNDER WILL BE,
*  AT MEDIATEK'S OPTION, TO REVISE OR REPLACE THE MEDIATEK SOFTWARE AT ISSUE,
*  OR REFUND ANY SOFTWARE LICENSE FEES OR SERVICE CHARGE PAID BY BUYER TO
*  MEDIATEK FOR SUCH MEDIATEK SOFTWARE AT ISSUE.
*
*  THE TRANSACTION CONTEMPLATED HEREUNDER SHALL BE CONSTRUED IN ACCORDANCE
*  WITH THE LAWS OF THE STATE OF CALIFORNIA, USA, EXCLUDING ITS CONFLICT OF
*  LAWS PRINCIPLES.  ANY DISPUTES, CONTROVERSIES OR CLAIMS ARISING THEREOF AND
*  RELATED THERETO SHALL BE SETTLED BY ARBITRATION IN SAN FRANCISCO, CA, UNDER
*  THE RULES OF THE INTERNATIONAL CHAMBER OF COMMERCE (ICC).
*
*****************************************************************************/

//#include <linux/string.h>

#include "lcm_drv.h"

#if 0
#include <linux/types.h>
#include <linux/mtd/mtd.h>	
#include <linux/mtd/partitions.h>
#include <linux/mtd/concat.h>
#include <asm/io.h>
#endif

//zhaoshaopeng add fn00410 lcd_id det
#ifdef BUILD_LK
#include <mt_gpio.h>
#else
#include <mach/mt_gpio.h>
#endif

#ifndef BUILD_LK
#include <linux/kernel.h>       ///for printk
#endif
#ifdef BUILD_LK
#undef printk
#define printk printf
#endif


// ---------------------------------------------------------------------------
//  Local Constants
// ---------------------------------------------------------------------------

#define FRAME_WIDTH  										(320)
#define FRAME_HEIGHT 										(480)
#define LCM_ID       (0x90)
#define REGFLAG_DELAY             							0XFE
#define REGFLAG_END_OF_TABLE      							0xFF   // END OF REGISTERS MARKER


#ifndef TRUE
    #define TRUE 1
#endif

#ifndef FALSE
    #define FALSE 0
#endif

static unsigned int lcm_esd_test = FALSE;      ///only for ESD test
#define LCM_DSI_CMD_MODE									1

int lcd_apex_stable = 1;

// ---------------------------------------------------------------------------
//  Local Variables
// ---------------------------------------------------------------------------

static LCM_UTIL_FUNCS lcm_util = {0};

#define SET_RESET_PIN(v)    								(lcm_util.set_reset_pin((v)))

#define UDELAY(n) 											(lcm_util.udelay(n))
#define MDELAY(n) 											(lcm_util.mdelay(n))


// ---------------------------------------------------------------------------
//  Local Functions
// ---------------------------------------------------------------------------

#define dsi_set_cmdq_V2(cmd, count, ppara, force_update)	lcm_util.dsi_set_cmdq_V2(cmd, count, ppara, force_update)
#define dsi_set_cmdq(pdata, queue_size, force_update)		lcm_util.dsi_set_cmdq(pdata, queue_size, force_update)
#define wrtie_cmd(cmd)										lcm_util.dsi_write_cmd(cmd)
#define write_regs(addr, pdata, byte_nums)					lcm_util.dsi_write_regs(addr, pdata, byte_nums)
#define read_reg(cmd)											lcm_util.dsi_dcs_read_lcm_reg(cmd)
#define read_reg_v2(cmd, buffer, buffer_size)   				lcm_util.dsi_dcs_read_lcm_reg_v2(cmd, buffer, buffer_size)    

struct LCM_setting_table {
    unsigned char cmd;
    unsigned char count;
    unsigned char para_list[64];
};

//static unsigned char ii = 0 ;
static struct LCM_setting_table lcm_initialization_setting[] = {
	
	/*
	Note :

	Data ID will depends on the following rule.
	
		count of parameters > 1	=> Data ID = 0x39
		count of parameters = 1	=> Data ID = 0x15
		count of parameters = 0	=> Data ID = 0x05

	Structure Format :

	{DCS command, count of parameters, {parameter list}}
	{REGFLAG_DELAY, milliseconds of time, {}},

	...

	Setting ending by predefined flag
	
	{REGFLAG_END_OF_TABLE, 0x00, {}}
	*/
#if 0  //chuanma
	{0xB9,	3,	{0xFF, 0x83, 0x57}},
	{REGFLAG_DELAY, 10, {}},

	{0xB6, 	1,	{0x25}},  //0x2e //0x80  leanda
   // {REGFLAG_DELAY, 10, {}},
	{0xE3,	2,	{0x0f, 0x0f}},

	{0x11, 0, {}},
    {REGFLAG_DELAY, 150, {}},

	{0x3A,	1,	{0x66}},//0x77
//	{REGFLAG_DELAY, 10, {}},

    {0x35, 	1,	{0x00}}, // TE ON
	{0x36,	1,	{0xc0}},
  //   {REGFLAG_DELAY, 10, {}},
	{0xCC, 	1,	{0x05}},  //0x05 
//	{REGFLAG_DELAY, 10, {}},

//	{0xB3,	4, {0x43,0x00,0x06,0x06}},        

	{0xB0,	2,	{0x0a, 0x01}},//80hz
//	{REGFLAG_DELAY, 10, {}},
	
    {0xC2,	3,	{0x00, 0x08,0x04}},
	{0xB1,	6,	{0x00,//DP
    	 0x12,//BT0x12
    	 0x1d,//VSPR0x1d
    	 0x1d,//VSNR
    	 0xc3,//AP
    	 0xaa}},    
   // {REGFLAG_DELAY, 10, {}},

	{0xC0,	6,	{0x70,//OPON
    	 0x70,//OPON
    	 0x01,//STBA
    	 0x3C,//STBA
    	 0xc8,//STBA
    	 0x08}},    
   // 	 {REGFLAG_DELAY, 10, {}},

	{0xB4, 	7,	{0x00,//Nw  //0x01 leanda 
    	 0x40,//RTN
    	 0x00,//DIV
    	 0x2A,//DUM
    	 0x2A,//DUM
    	 0x20,//GDON
    	 0x91//GDOFF
    	 }},
    //	 {REGFLAG_DELAY, 10, {}},

  //  {0xE0, 	34,	{0x00, 0x03, 0x08, 0x0C, 0x0E, 0x24, 0x37, 0x48, 0x46, 0x38, 0x2B, 0x1D, 0x19, 0x12, 0x0E, 0x00,0xE0, 0x00, 0x03, 0x08, 0x0C, 0x0E, 0x24, 0x37, 0x48, 0x46, 0x38, 0x2B, 0x1D, 0x19, 0x12, 0x0E, 0x00,0x00,0x01}},
	{0xe0,	67, {0x00,0x00,0x03,0x00,0x04,0x00,0x05,0x00,0x07,0x00,0x1a,0x00,0x33,0x00,0x41,0x00,0x49,0x00,0x50,0x00,0x49,0x00,0x42,0x00,0x39,0x00,0x36,0x00,0x31,0x00,0x2d,0x00,0x21,0x01,0x03,0x00,0x04,0x00,0x05,0x00,0x07,0x00,0x1a,0x00,0x33,0x00,0x41,0x00,0x49,0x00,0x50,0x00,0x49,0x00,0x42,0x00,0x39,0x00,0x36,0x00,0x31,0x00,0x2d,0x00,0x21,0x00,0x01}},
	//{REGFLAG_DELAY, 10, {}},

	//{0x44,	2,	{((FRAME_HEIGHT/2)>>8), ((FRAME_HEIGHT/2)&0xFF)}},

	{0x2c, 0, {}},
	{REGFLAG_DELAY, 20, {}},

    {0x11, 0, {}},
        {REGFLAG_DELAY, 20, {}},

    // Display ON
	{0x29, 0, {}},
	{REGFLAG_DELAY, 20, {}},

#else

#if 1
	#if 0
	{0xe0, 	15,	{0x0F,0x1B,0x18,0x08,0x0B,0x05,0x4A,0x98,0x3C,0x0A,0x15,0x08,0x0E,0x07,0x00}},
	{0xe1, 	15,	{0x0F,0x37,0x34,0x0B,0x0C,0x03,0x4B,0x52,0x38,0x04,0x03,0x02,0x22,0x20,0x00}},
	#else
	//{0xE0,	15,	{0x00,0x0d,0x14, 0x06,0x14,0x08, 0x3c,0x37,0x45, 0x08,0x0d,0x09,0x17, 0x19,0x0F}},
	//{0xE1,	15,	{0x00,0x1a,0x1f, 0x03,0x0f,0x05, 0x33,0x23,0x48, 0x02,0x0a,0x08,0x31, 0x39,0x0f}},
	#endif
	
	{0x36,	1,	{0x08}},
//	{0x35,	1,	{0x00}},
	{0x3A,	1,	{0x66}},
	{0xC0,	2,	{0x0F, 0x0A}},

	{0xC1,	1,	{0x41}},
	{0xC5,	2,	{0x01,0x3F}},
	{0xB1,	2,	{0xB0,0x11}},
	{0xB4,	1,	{0x02}},

	{0xB6,	3,	{0x02,0x22,0x3B}},
	{0xF2,	5,	{0x58,0x10,0x12,0x02,0x92}},
	{0xF7,	4,	{0xA9,0x51,0x2C,0x8A}},
	{0xFC,	2,	{0x00,0x09}},

	{0xD4,	2,	{0x97,0x40}},
	{0xD5,	11,	{0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0xCF,0x01}},


	{0x11, 1, {0x00}},
	{REGFLAG_DELAY, 200, {}},
    // Display ON
	{0x29, 1, {0x00}},
//	{REGFLAG_DELAY, 100, {}},
#else
	{0xF7,	4,	{ 0xa9,0x51,0x2c, 0x8a}},

	{0xc0,	2,	{0x10,0x10}},
	
	{0xC1,	1,	{0x41}},

	{0xC2,	1,	{0x33}},

	{0xC5,	3,	{0x00,0x23,0x80}},
	
	{0x36,	1,	{0x08}},

	{0x3A,   1, {0x66}},

	{0xB1,   2, {0xa0,0x11}},

	{0x35,	1, {0x00}},

	{0x44,	2 ,{0x01,0x22}},
		
	{0xB4,   1,      {0x02}},

	{0xB6,	3,	{0x00, 0x42,0x3B}},
	
	{0xBe,	 2, 	  {0x00,0x04}},

	{0xE0,	15,	{0x00,0x0d,0x14, 0x06,0x14,0x08, 0x3c,0x37,0x45, 0x08,0x0d,0x09,0x17, 0x19,0x0F}},
	//{REGFLAG_DELAY, 10, {}},	

	{0xE1,	15,	{0x00,0x1a,0x1f, 0x03,0x0f,0x05, 0x33,0x23,0x48, 0x02,0x0a,0x08,0x31, 0x39,0x0f}},
	//{REGFLAG_DELAY, 10, {}},

	 

	{0x11,	1,	{0x00}},
	{REGFLAG_DELAY, 150, {}},	
	

	{0x29,	1,	{0x00}},
	//{REGFLAG_DELAY, 60, {}},		
#endif

#endif
	// Setting ending by predefined flag
	{REGFLAG_END_OF_TABLE, 0x00, {}}
};

static volatile unsigned int g_c5Value = 0x30;
static void lcm_init_setting(void)
{

	unsigned int data_array[16];

	data_array[0]= 0x08361500;
	dsi_set_cmdq(&data_array, 1, 1);
	//MDELAY(1);
	data_array[0]= 0x663A1500;
	dsi_set_cmdq(&data_array, 1, 1);
	
//	{0xC0,	2,	{0x0F, 0x0A}},
	data_array[0]= 0x00033902;
	data_array[1]= 0x000A0FC0;
	dsi_set_cmdq(&data_array, 2, 1);
	
	//{0xC1,	1,	{0x41}},
	data_array[0]= 0x41C11500;
	dsi_set_cmdq(&data_array, 1, 1);
	
	//{0xC5,	2,	{0x01,0x30}},
	data_array[0]= 0x00033902;
	//data_array[1]= 0x003001C5;
	data_array[1]= 0x000001C5;
	data_array[1] |= (g_c5Value++)<<16;
	dsi_set_cmdq(&data_array, 2, 1);
	//{0xB1,	2,	{0xB0,0x11}},
	data_array[0]= 0x00033902;
	data_array[1]= 0x0011B0B1;
	dsi_set_cmdq(&data_array, 2, 1);
	//{0xB4,	1,	{0x02}},
	data_array[0]= 0x02B41500;
	dsi_set_cmdq(&data_array, 1, 1);

	//{0xB6,	3,	{0x02,0x22,0x3B}},

	data_array[0]= 0x00043902;
	data_array[1]= 0x3B2202B6;
	dsi_set_cmdq(&data_array, 2, 1);
	//{0xF2,	5,	{0x58,0x10,0x12,0x02,0x92}},
	data_array[0]= 0x00063902;
	data_array[1]= 0x121058F2;
	data_array[2]= 0x00009202;
	dsi_set_cmdq(&data_array, 3, 1);
	//{0xF7,	4,	{0xA9,0x51,0x2C,0x8A}},
	data_array[0]= 0x00053902;
	data_array[1]= 0x2C51A9F7;
	data_array[2]= 0x0000008A;
	dsi_set_cmdq(&data_array, 3, 1);
	//{0xFC,	2,	{0x00,0x09}},
	data_array[0]= 0x00033902;
	data_array[1]= 0x000900FC;
	dsi_set_cmdq(&data_array, 2, 1);

	//{0xD4,	2,	{0x97,0x40}},
	data_array[0]= 0x00033902;
	data_array[1]= 0x004097D4;
	dsi_set_cmdq(&data_array, 2, 1);
//{0xD5,	11,	{0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0xCF,0x01}},
	data_array[0]= 0x000C3902;
	data_array[1]= 0x000000D5;
	data_array[2]= 0x00000000;
	data_array[3]= 0x01CF0000;
	dsi_set_cmdq(&data_array, 4, 1);

	data_array[0]= 0x00111500;
	dsi_set_cmdq(&data_array, 1, 1);
	//{0x11, 1, {0x00}},
	MDELAY(200);
    // Display ON
    data_array[0]= 0x00291500;
	dsi_set_cmdq(&data_array, 1, 1);
	//{0x29, 1, {0x00}},
	//MDELAY(1);

}


#if 0
static struct LCM_setting_table lcm_set_window[] = {
	{0x2A,	4,	{0x00, 0x00, (FRAME_WIDTH>>8), (FRAME_WIDTH&0xFF)}},
	{0x2B,	4,	{0x00, 0x00, (FRAME_HEIGHT>>8), (FRAME_HEIGHT&0xFF)}},
	{REGFLAG_END_OF_TABLE, 0x00, {}}
};
#endif

static struct LCM_setting_table lcm_id_setting[] = {
    {0xB9,	3,	{0xFF, 0x83, 0x57}},
    {REGFLAG_DELAY, 5, {}},
    {REGFLAG_END_OF_TABLE, 0x00, {}}
};

static struct LCM_setting_table lcm_sleep_out_setting[] = {
    // Sleep Out
	{0x11, 1, {0x00}},
       {REGFLAG_DELAY, 120, {}},

    // Display ON
	{0x29, 1, {0x00}},
	{REGFLAG_DELAY, 50, {}},
	{REGFLAG_END_OF_TABLE, 0x00, {}}
};


static struct LCM_setting_table lcm_deep_sleep_mode_in_setting[] = {
	// Display off sequence
	{0x28, 1, {0x00}},
       {REGFLAG_DELAY, 20, {}},
    // Sleep Mode On
	{0x10, 1, {0x00}},
       {REGFLAG_DELAY, 120, {}},
       //{0x78, 1, {0x00}},//zhaoshaopeng add from baolongda for 1ma leackage
	{REGFLAG_END_OF_TABLE, 0x00, {}}
};

static struct LCM_setting_table lcm_compare_id_setting[] = {
	// Display off sequence
	{0xB9,	3,	{0xFF, 0x83, 0x57}},
	{REGFLAG_DELAY, 10, {}},

    // Sleep Mode On
//	{0xC3, 1, {0xFF}},

	{REGFLAG_END_OF_TABLE, 0x00, {}}
};

static struct LCM_setting_table lcm_backlight_level_setting[] = {
	{0x51, 1, {0xFF}},
	{REGFLAG_END_OF_TABLE, 0x00, {}}
};


static void push_table(struct LCM_setting_table *table, unsigned int count, unsigned char force_update)
{
	unsigned int i;

    for(i = 0; i < count; i++) {
		
        unsigned cmd;
        cmd = table[i].cmd;
		
        switch (cmd) {
			
            case REGFLAG_DELAY :
                MDELAY(table[i].count);
                break;
				
            case REGFLAG_END_OF_TABLE :
                break;
				
            default:
				dsi_set_cmdq_V2(cmd, table[i].count, table[i].para_list, force_update);
       	}
    }
	
}


// ---------------------------------------------------------------------------
//  LCM Driver Implementations
// ---------------------------------------------------------------------------

static void lcm_set_util_funcs(const LCM_UTIL_FUNCS *util)
{
    memcpy(&lcm_util, util, sizeof(LCM_UTIL_FUNCS));
}


static void lcm_get_params(LCM_PARAMS *params)
{
		memset(params, 0, sizeof(LCM_PARAMS));
	
		params->type   = LCM_TYPE_DSI;

		params->width  = FRAME_WIDTH;
		params->height = FRAME_HEIGHT;

		// enable tearing-free
		//params->dbi.te_mode 				= LCM_DBI_TE_MODE_VSYNC_ONLY;
		//params->dbi.te_edge_polarity		= LCM_POLARITY_RISING;

		#if (LCM_DSI_CMD_MODE)
		params->dsi.mode   = CMD_MODE;
		#else
		params->dsi.mode   = SYNC_PULSE_VDO_MODE;
		#endif

		// DSI
		/* Command mode setting */
		params->dsi.LANE_NUM				= LCM_ONE_LANE;
		//The following defined the fomat for data coming from LCD engine.
		params->dsi.data_format.color_order = LCM_COLOR_ORDER_RGB;
		params->dsi.data_format.trans_seq   = LCM_DSI_TRANS_SEQ_MSB_FIRST;
		params->dsi.data_format.padding     = LCM_DSI_PADDING_ON_LSB;
		params->dsi.data_format.format      = LCM_DSI_FORMAT_RGB666; //

		// Highly depends on LCD driver capability.
		params->dsi.packet_size=256;

		// Video mode setting		
		params->dsi.PS=LCM_PACKED_PS_18BIT_RGB666;

		#if (LCM_DSI_CMD_MODE)
		params->dsi.word_count=480*3;	

		params->dsi.vertical_sync_active=3;
		params->dsi.vertical_backporch=12;
		params->dsi.vertical_frontporch=2;
		params->dsi.vertical_active_line=800;
	
		params->dsi.line_byte=2048;		// 2256 = 752*3
		params->dsi.horizontal_sync_active_byte=26;
		params->dsi.horizontal_backporch_byte=146;
		params->dsi.horizontal_frontporch_byte=146;	
		params->dsi.rgb_byte=(480*3+6);	
		#endif
	//	params->dsi.horizontal_sync_active_word_count=20;	
	//	params->dsi.horizontal_backporch_word_count=140;
	//	params->dsi.horizontal_frontporch_word_count=140;
		
		// Bit rate calculation
		params->dsi.pll_div1=1;		// 
		params->dsi.pll_div2=2;			// 
		params->dsi.fbk_div =0x1E; // 19;    // fref=26MHz, fvco=fref*(fbk_div+1)*2/(div1_real*div2_real)	
}

static unsigned int lcm_compare_id(void)
{
#if 0
	unsigned int id = 0;
	unsigned int id1 = 0;
	
	unsigned char buffer[2];
	unsigned int array[16];
	return 1;
	//zhaoshaopeng for tianma/baolongda lcd_id_det  hx8369 
	unsigned char lcd_id = 0;
        SET_RESET_PIN(1);  //NOTE:should reset LCM firstly
    	SET_RESET_PIN(0);
    	MDELAY(150);
    	SET_RESET_PIN(1);
    	MDELAY(50);

	push_table(lcm_compare_id_setting, sizeof(lcm_compare_id_setting) / sizeof(struct LCM_setting_table), 1);

	array[0] = 0x00023700;// read id return two byte,version and id
	dsi_set_cmdq(array, 1, 1);
//	id = read_reg(0xF4);
	read_reg_v2(0xd0, buffer, 2);
	id = buffer[0]; //we only need ID
	id1 = buffer[1]; //we only need ID
	printk("zhuoshineng hx8357c id  = %x\n", id);
	printk("zhuoshineng hx8357c id1 = %x\n", id1);
	
	//zhaoshaopeng for tianma/baolongda lcd_id_det  hx8369 
	//lcd_id =  mt_get_gpio_in(GPIO_LCD_ID);

        if((id == LCM_ID))//&&(lcd_id ==0))
        return 1;
        else
        return 0;
		#else
		unsigned char lcd_id = 2;
		unsigned char lcd_id1 = 2;
		unsigned int id = 0;
		unsigned char buffer[5];
		unsigned int array[16];
		unsigned int retry_count = 5;

	while(retry_count){
		
		SET_RESET_PIN(1);
		MDELAY(10);
		SET_RESET_PIN(0);
		MDELAY(25);
		SET_RESET_PIN(1);
		MDELAY(120);

		//push_table(lcm_id_setting, sizeof(lcm_id_setting) / sizeof(struct LCM_setting_table), 1);

		array[0] = 0x00043700;// read id return two byte,version and id
		dsi_set_cmdq(array, 1, 1);

		memset(buffer,0x0,4);
		read_reg_v2(0xD3, buffer, 4);
		id = (buffer[1]<<8)|buffer[2]; //we only need ID
		// id = (buffer[1]<<8)|buffer[2]; //we only need ID
		printk("buffer is 0x%x,0x%x,0x%x,0x%x\n",buffer[0],buffer[1],buffer[2],buffer[3]);

		   if(id == 0x9487)
		   	break ;
		   retry_count --;
		}


		mt_set_gpio_pull_enable(GPIO_LCD_ID0,GPIO_PULL_DISABLE);
		mt_set_gpio_pull_enable(GPIO_LCD_ID1,GPIO_PULL_DISABLE);
		mt_set_gpio_pull_select(GPIO_LCD_ID0,GPIO_PULL_DOWN);
		mt_set_gpio_pull_select(GPIO_LCD_ID1,GPIO_PULL_DOWN);
		lcd_id1 = mt_get_gpio_in(GPIO_LCD_ID1);
		lcd_id = mt_get_gpio_in(GPIO_LCD_ID0);
		printk("### harrison aizhuoertai 9487 lcd_id = %d ,lcd_id1=%d\r\n",lcd_id,lcd_id);

		//if((1 == lcd_id)&&(lcd_id ==1))
		if(id == 0x9487)
		 	return 1;

		return 0;
#endif
}

static int g_esdCheckCount = 0;
static unsigned int lcm_esd_check(void)
	{
	
	//#ifndef BUILD_UBOOT
	//		  if(lcm_esd_test)
	//{
	//			  lcm_esd_test = FALSE;
	//			  return TRUE;
	//}
	
			/// please notice: the max return packet size is 1
			/// if you want to change it, you can refer to the following marked code
			/// but read_reg currently only support read no more than 4 bytes....
			/// if you need to read more, please let BinHan knows.
			/*
					unsigned int data_array[16];
					unsigned int max_return_size = 1;
					
					data_array[0]= 0x00003700 | (max_return_size << 16);	
					
					dsi_set_cmdq(&data_array, 1, 1);
			*/
#if 0
			if(read_reg(0xB6) == 0x25)
	{
				return FALSE;
			}
			else
			{			 
				return TRUE;
			}
	//#endif
#endif
#ifndef BUILD_LK

			//char  buffer[3];
		char  buffer[5];

		int   array[4];

		if(g_esdCheckCount < 1)
		{
			g_esdCheckCount++;
			return FALSE;
		}
		//array[0] = 0x00023700;
		//dsi_set_cmdq(array, 1, 1);
	
		//read_reg_v2(0xB6, buffer, 2);
		//printk("### leanda 1	test lcm_esd_check buffer[0]=0x%x\n",buffer[0]);

		array[0] = 0x00053700;
		dsi_set_cmdq(array, 1, 1);
		memset(buffer,0x0,5);
		read_reg_v2(0x09, buffer, 5);
		printk("### harrison 9487 aizhuoertai buffer=0x%x,0x%x,0x%x,0x%x,0x%x\r\n",
						buffer[0],buffer[1],buffer[2],buffer[3],buffer[4]);
		if((buffer[1]==0x63)
		 	&&(buffer[2]==0x04))
		{
			return FALSE;
		} 	
		return TRUE; 
#else
		return FALSE;
#endif
	
	}


static void lcm_init(void)
{
	//printk("leanda chuanma ########hx8357c lcm_init\n");

    unsigned char lcd_id =0;
    SET_RESET_PIN(1);
    MDELAY(10);
    SET_RESET_PIN(0);
    MDELAY(25);
    SET_RESET_PIN(1);
    MDELAY(120);
//lcm_compare_id();
	push_table(lcm_initialization_setting, sizeof(lcm_initialization_setting) / sizeof(struct LCM_setting_table), 1);

}


static void lcm_suspend(void)
{
       #if 0
       SET_RESET_PIN(0);
	push_table(lcm_deep_sleep_mode_in_setting, sizeof(lcm_deep_sleep_mode_in_setting) / sizeof(struct LCM_setting_table), 1);
       MDELAY(1);	   
       SET_RESET_PIN(1);	
	#else
	char null = 0;
		SET_RESET_PIN(0);
		MDELAY(25);
		SET_RESET_PIN(1);
		MDELAY(120);
	dsi_set_cmdq_V2(0x28, 1, &null, 1);
		MDELAY(20);
       dsi_set_cmdq_V2(0x10, 1, &null, 1);
       MDELAY(120);	   
       //SET_RESET_PIN(0);	   
       //dsi_set_cmdq_V2(0x78, 1, &null, 1);
     //  MDELAY(1);	   
      // SET_RESET_PIN(1);
      lcd_apex_stable = 0;
       #endif
}


static void lcm_resume(void)
{
	#if 1
	lcm_init();
	lcd_apex_stable = 1;
	//push_table(lcm_sleep_out_setting, sizeof(lcm_sleep_out_setting) / sizeof(struct LCM_setting_table), 1);
	#else
	char null = 0;


	//	MDELAY(20);

        int i=0;
      for(i=0;i<2;i++)
         	{
	dsi_set_cmdq_V2(0x11, 1, &null, 1);
         	}

     //data_array=0x00110500;
	//data_array[0]=0x00013902;
       // data_array[1]=0x11;
       // dsi_set_cmdq(&data_array, 2, 1);
	MDELAY(10);

	dsi_set_cmdq_V2(0x29, 1, &null, 1);
	#endif
}   


static void lcm_update(unsigned int x, unsigned int y,
                       unsigned int width, unsigned int height)
{
	unsigned int x0 = x;
	unsigned int y0 = y;
	unsigned int x1 = x0 + width - 1;
	unsigned int y1 = y0 + height - 1;

	unsigned char x0_MSB = ((x0>>8)&0xFF);
	unsigned char x0_LSB = (x0&0xFF);
	unsigned char x1_MSB = ((x1>>8)&0xFF);
	unsigned char x1_LSB = (x1&0xFF);
	unsigned char y0_MSB = ((y0>>8)&0xFF);
	unsigned char y0_LSB = (y0&0xFF);
	unsigned char y1_MSB = ((y1>>8)&0xFF);
	unsigned char y1_LSB = (y1&0xFF);

	unsigned int data_array[16];

	data_array[0]= 0x00053902;
	data_array[1]= (x1_MSB<<24)|(x0_LSB<<16)|(x0_MSB<<8)|0x2a;
	data_array[2]= (x1_LSB);
	dsi_set_cmdq(&data_array, 3, 1);
	//MDELAY(1);
	
	data_array[0]= 0x00053902;
	data_array[1]= (y1_MSB<<24)|(y0_LSB<<16)|(y0_MSB<<8)|0x2b;
	data_array[2]= (y1_LSB);
	dsi_set_cmdq(&data_array, 3, 1);
	//MDELAY(1);
	
//	data_array[0]= 0x00290508;
//	dsi_set_cmdq(&data_array, 1, 1);
	//MDELAY(1);
	
	data_array[0]= 0x002c3909;
	dsi_set_cmdq(&data_array, 1, 0);
	//MDELAY(1);

}


static void lcm_setbacklight(unsigned int level)
{
	unsigned int default_level = 145;
	unsigned int mapped_level = 0;

	//for LGE backlight IC mapping table
	if(level > 255) 
			level = 255;

	if(level >0) 
			mapped_level = default_level+(level)*(255-default_level)/(255);
	else
			mapped_level=0;

	// Refresh value of backlight level.
	lcm_backlight_level_setting[0].para_list[0] = mapped_level;

	push_table(lcm_backlight_level_setting, sizeof(lcm_backlight_level_setting) / sizeof(struct LCM_setting_table), 1);
}



static unsigned int lcm_esd_recover(void)
{
/*
    unsigned char para = 0;

    SET_RESET_PIN(1);
    SET_RESET_PIN(0);
    MDELAY(1);
    SET_RESET_PIN(1);
    MDELAY(120);
	  push_table(lcm_initialization_setting, sizeof(lcm_initialization_setting) / sizeof(struct LCM_setting_table), 1);
    MDELAY(10);
	  push_table(lcm_sleep_out_setting, sizeof(lcm_sleep_out_setting) / sizeof(struct LCM_setting_table), 1);
    MDELAY(10);
    dsi_set_cmdq_V2(0x35, 1, &para, 1);     ///enable TE
    MDELAY(10);

    return TRUE;
	*/
	#ifndef BUILD_LK	
	printk("###harrison 9487 test lcm_esd_recover\n");
	lcm_init();
    #endif
	return TRUE;
}
#if 0
static unsigned int lcm_compare_id(void)
{
	unsigned int id = 0;
	unsigned int id1 = 0;
	
	unsigned char buffer[2];
	unsigned int array[16];
	//zhaoshaopeng for tianma/baolongda lcd_id_det  hx8369 
	unsigned char lcd_id = 0;
        SET_RESET_PIN(1);  //NOTE:should reset LCM firstly
    	SET_RESET_PIN(0);
    	MDELAY(150);
    	SET_RESET_PIN(1);
    	MDELAY(50);

	push_table(lcm_compare_id_setting, sizeof(lcm_compare_id_setting) / sizeof(struct LCM_setting_table), 1);

	array[0] = 0x00023700;// read id return two byte,version and id
	dsi_set_cmdq(array, 1, 1);
//	id = read_reg(0xF4);
	read_reg_v2(0xd0, buffer, 2);
	id = buffer[0]; //we only need ID
	id1 = buffer[1]; //we only need ID
	printk("zhuoshineng hx8357c id  = %x\r\n", id);
	printk("zhuoshineng hx8357c id1 = %x\r\n", id1);
	
	//zhaoshaopeng for tianma/baolongda lcd_id_det  hx8369 
	//lcd_id =  mt_get_gpio_in(GPIO_LCD_ID);

        if((id != LCM_ID))//&&(lcd_id ==0))
        return 1;
        else
        return 0;
}
#endif

static void lcm_read_fb(unsigned char *buffer)
{
	  unsigned int array[2];

   array[0] = 0x000A3700;// read size
   dsi_set_cmdq(array, 1, 1);
   
   read_reg_v2(0x2E,buffer,10);
   read_reg_v2(0x3E,buffer+10,10);
   read_reg_v2(0x3E,buffer+10*2,10);
   read_reg_v2(0x3E,buffer+10*3,10);
   read_reg_v2(0x3E,buffer+10*4,10);
   read_reg_v2(0x3E,buffer+10*5,10);
}


// ---------------------------------------------------------------------------
//  Get LCM Driver Hooks
// ---------------------------------------------------------------------------
LCM_DRIVER ili9487_dsi_hvga_aizhuoertai_lcm_drv = 
{
        .name			= "ili9487_dsi_hvga_aizhuoertai",
	.set_util_funcs       = lcm_set_util_funcs,
	.get_params          = lcm_get_params,
	.init                       = lcm_init,
	.suspend               = lcm_suspend,
	.resume                = lcm_resume,
	#if (LCM_DSI_CMD_MODE)
	.update         = lcm_update,
	#endif
	//.set_backlight	= lcm_setbacklight,
	//.set_pwm        = lcm_setpwm,
	//.get_pwm        = lcm_getpwm,
	.compare_id    = lcm_compare_id,
	//.esd_check = lcm_esd_check,
	//.esd_recover = lcm_esd_recover,
	.read_fb				= lcm_read_fb,
};

