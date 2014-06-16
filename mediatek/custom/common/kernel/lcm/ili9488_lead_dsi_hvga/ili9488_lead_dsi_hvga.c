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
#define LCM_ID       (0x20)
#define REGFLAG_DELAY             							0xFB//0XFE
#define REGFLAG_END_OF_TABLE      							0XFA//0xFF   // END OF REGISTERS MARKER


#ifndef TRUE
    #define TRUE 1
#endif

#ifndef FALSE
    #define FALSE 0
#endif

static unsigned int lcm_esd_test = FALSE;      ///only for ESD test
#define LCM_DSI_CMD_MODE									1
#define USE_V3_CMD											1
int lcd_lead_stable = 1;

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
#define dsi_set_cmdq_V3(para_tbl, size, force_update)	lcm_util.dsi_set_cmdq_V3(para_tbl, size, force_update)
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

#if (USE_V3_CMD)	

static LCM_setting_table_V3 lcm_initialization_setting_v3[] = {
	
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

	//{0xF1,	6,	{0x36,0x04,0x00, 0x3C,0x0F,0x8F}},
	//{REGFLAG_DELAY, 10, {}},	

	//{0xF7,	4,	{0xA9,0x89,0x2D, 0x8A}},
	//{REGFLAG_DELAY, 10, {}},	

	//{0xF7,	2,	{0xB1,0x91}},
	//{REGFLAG_DELAY, 10, {}},

	//{0xB1,	2,	{0xB0, 0x11}},
	//{REGFLAG_DELAY, 10, {}},	

	//{0xC0,	2,	{0x10, 0x10}},
	//{REGFLAG_DELAY, 10, {}},	

	//{0xC5,	2,	{0x34, 0x34}},
	//{REGFLAG_DELAY, 10, {}},

	//{0xF9,	2,	{0x08,0x08}},
	//{REGFLAG_DELAY, 10, {}},

	//{0xF7,	1,	{0xA9}},
	//{REGFLAG_DELAY, 10, {}},

	//{0x36,	1,	{0x48}},
	//{REGFLAG_DELAY, 10, {}},	

	{0x39,0xF7,	4,	{ 0xa9,0x51,0x2c, 0x8a}},

	{0x39,0xc0,	2,	{0x10,0x10}},
	
	{0x15,0xC1,	1,	{0x41}},

	{0x15,0xC2,	1,	{0x33}},

	{0x39,0xC5,	3,	{0x00,0x38,0x80}},
	
	{0x15,0x36,	1,	{0x08}},//ZHENGBOWEN FORM 

	{0x15,0x3A,   1,      {0x77}},

	{0x39,0xB1,   2,       {0xa0,0x11}},

	{0x15,0x35,1, {0x00}},

	//{0x39,0x44,2 ,{0x01,0xFF}},
		
	{0x15,0xB4,   1,      {0x02}},

	{0x39,0xB6,	3,	{0x02, 0x22,0x3B}},

	{0x15,0xB7,	1, 	{0xC6}},
	
	{0x39,0xBe,	 2, 	  {0x00,0x04}},
	
	{0x15, 0xE9,	1, 	{0x00}},

	{0x39,0xE0,	15,	{0x00, 0x04, 0x0D, 0x07, 0x15, 0x0A, 0x3A, 0x88, 0x48, 0x08, 0x0E, 0x0B, 0x17, 0x1B, 0x0F}},
	//{REGFLAG_DELAY, 10, {}},	

	{0x39,0xE1,	15,	{0x00, 0x1A, 0x1D, 0x03, 0x10, 0x06, 0x31, 0x34, 0x43, 0x02, 0x09, 0x08, 0x30, 0x36, 0x0F}},
	//{REGFLAG_DELAY, 10, {}},

	 

	{0x15,0x11,	1,	{0x00}},
	{REGFLAG_ESCAPE_ID,REGFLAG_DELAY_MS_V3,150,{0x0}},	
	

	{0x15,0x29,	1,	{0x00}},
	//{REGFLAG_ESCAPE_ID,REGFLAG_DELAY_MS_V3,100,{0x0}},
	//{REGFLAG_DELAY, 60, {}},		

};


static LCM_setting_table_V3 lcm_sleep_out_setting_v3[] = {
    // Sleep Out
	{0x15,0x11, 1, {0x00}},
    {REGFLAG_ESCAPE_ID,REGFLAG_DELAY_MS_V3,150,{0x0}},

    // Display ON
	{0x15,0x29, 1, {0x00}},
	//{REGFLAG_DELAY, 1000, {}},
//	{REGFLAG_END_OF_TABLE, 0x00, {}}
};


static LCM_setting_table_V3 lcm_deep_sleep_mode_in_setting_v3[] = {
	// Display off sequence
	{0x15,0x28, 1, {0x00}},
    {REGFLAG_ESCAPE_ID,REGFLAG_DELAY_MS_V3,100,{0x0}},
     //Sleep Mode On
	{0x15,0x10, 1, {0x00}},
    {REGFLAG_ESCAPE_ID,REGFLAG_DELAY_MS_V3,200,{0x0}},

//	{REGFLAG_END_OF_TABLE, 0x00, {}}
};
#else

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

	//{0xF1,	6,	{0x36,0x04,0x00, 0x3C,0x0F,0x8F}},
	//{REGFLAG_DELAY, 10, {}},	

	//{0xF7,	4,	{0xA9,0x89,0x2D, 0x8A}},
	//{REGFLAG_DELAY, 10, {}},	

	//{0xF7,	2,	{0xB1,0x91}},
	//{REGFLAG_DELAY, 10, {}},

	//{0xB1,	2,	{0xB0, 0x11}},
	//{REGFLAG_DELAY, 10, {}},	

	//{0xC0,	2,	{0x10, 0x10}},
	//{REGFLAG_DELAY, 10, {}},	

	//{0xC5,	2,	{0x34, 0x34}},
	//{REGFLAG_DELAY, 10, {}},

	//{0xF9,	2,	{0x08,0x08}},
	//{REGFLAG_DELAY, 10, {}},

	//{0xF7,	1,	{0xA9}},
	//{REGFLAG_DELAY, 10, {}},

	//{0x36,	1,	{0x48}},
	//{REGFLAG_DELAY, 10, {}},	

	{0xF7,	4,	{ 0xa9,0x51,0x2c, 0x8a}},

	{0xc0,	2,	{0x10,0x10}},
	
	{0xC1,	1,	{0x41}},

	{0xC2,	1,	{0x33}},

	{0xC5,	3,	{0x00,0x23,0x80}},
	
	{0x36,	1,	{0xC8}},//ZHENGBOWEN FORM 

	{0x3A,   1,      {0x66}},

	{0xB1,   2,       {0xa0,0x11}},

	{0x35,1, {0x00}},

	{0x44,2 ,{0x01,0x22}},
		
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
	{REGFLAG_END_OF_TABLE, 0x00, {}},
};


#if 0
static struct LCM_setting_table lcm_set_window[] = {
	{0x2A,	4,	{0x00, 0x00, (FRAME_WIDTH>>8), (FRAME_WIDTH&0xFF)}},
	{0x2B,	4,	{0x00, 0x00, (FRAME_HEIGHT>>8), (FRAME_HEIGHT&0xFF)}},
	{REGFLAG_END_OF_TABLE, 0x00, {}}
};
#endif

static struct LCM_setting_table lcm_sleep_out_setting[] = {
    // Sleep Out
	{0x11, 1, {0x00}},
       {REGFLAG_DELAY, 150, {}},

    // Display ON
	{0x29, 1, {0x00}},
	//{REGFLAG_DELAY, 1000, {}},
	{REGFLAG_END_OF_TABLE, 0x00, {}}
};


static struct LCM_setting_table lcm_deep_sleep_mode_in_setting[] = {
	// Display off sequence
	{0x28, 1, {0x00}},
       {REGFLAG_DELAY, 100, {}},
     //Sleep Mode On
	{0x10, 1, {0x00}},
       {REGFLAG_DELAY, 200, {}},

	{REGFLAG_END_OF_TABLE, 0x00, {}}
};

#endif

static struct LCM_setting_table lcm_compare_id_setting[] = {
	// Display off sequence
	{0xFF,	3,	{0xFF, 0x94, 0x86}},
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
                                printk("push_table,cmd=0x%x ,table[%d].count=0x%x\r\n",cmd,i,table[i].count);
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
		params->dbi.te_mode 				= LCM_DBI_TE_MODE_VSYNC_ONLY;
		params->dbi.te_edge_polarity		= LCM_POLARITY_RISING;

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
		params->dsi.data_format.format      = LCM_DSI_FORMAT_RGB888;

		// Highly depends on LCD driver capability.
		params->dsi.packet_size=256;

		// Video mode setting		
		params->dsi.PS=LCM_PACKED_PS_18BIT_RGB666;

		#if (LCM_DSI_CMD_MODE)
		params->dsi.intermediat_buffer_num = 0;

		params->dsi.vertical_sync_active				= 2;
    	params->dsi.vertical_backporch					= 2;
    	params->dsi.vertical_frontporch					= 2;
    	params->dsi.vertical_active_line				= FRAME_HEIGHT; 

    	params->dsi.horizontal_sync_active				= 3;
    	params->dsi.horizontal_backporch				= 26;
    	params->dsi.horizontal_frontporch				= 26;
    	params->dsi.horizontal_active_pixel				= FRAME_WIDTH;
//		params->dsi.rgb_byte=(480*3+6);	
		#endif
//		params->dsi.horizontal_sync_active_word_count=20;	
//		params->dsi.horizontal_backporch_word_count=200;
//		params->dsi.horizontal_frontporch_word_count=200;

		// Bit rate calculation
		//params->dsi.pll_div1=30;		// fref=26MHz, fvco=fref*(div1+1)	(div1=0~63, fvco=500MHZ~1GHz) 
		//params->dsi.pll_div2=1;			// div2=0~15: fout=fvo/(2*div2)  zhengbowenmodify form 1
		// Bit rate calculation
		params->dsi.pll_div1=1;		// 
		params->dsi.pll_div2=1;			// 
		params->dsi.fbk_div =0x20; // 19;    // fref=26MHz, fvco=fref*(fbk_div+1)*2/(div1_real*div2_real)	
}

static unsigned int lcm_compare_id(void)
{
	unsigned int id = 0;
	unsigned char buffer[5];
	unsigned int array[16];
	unsigned char lcd_id0 = 0;
	unsigned char lcd_id1 = 0;	
	SET_RESET_PIN(1);	
	MDELAY(1);
	SET_RESET_PIN(0);
	MDELAY(25);
	SET_RESET_PIN(1);
	MDELAY(100);	// 150

	//push_table(lcm_compare_id_setting, sizeof(lcm_compare_id_setting) / sizeof(struct LCM_setting_table), 1);
	//return 1;
	
	array[0] = 0x00043700;// read id return two byte,version and id
	dsi_set_cmdq(array, 1, 1);

	//array[0] = 0x04B02300;// unlock for reading ID
	//dsi_set_cmdq(array, 1, 1);
	//MDELAY(50);

	//	id = read_reg(0xF4);
	memset(buffer,0x0,4);
	read_reg_v2(0xD3, buffer, 4);
	id = (buffer[1]<<8)|buffer[2]; //we only need ID
	printk("buffer is 0x%x,0x%x,0x%x,0x%x,id=0x%x\n",buffer[0],buffer[1],buffer[2],buffer[3],id);
	mt_set_gpio_pull_enable(GPIO_LCD_ID0,GPIO_PULL_DISABLE);
	mt_set_gpio_pull_enable(GPIO_LCD_ID1,GPIO_PULL_DISABLE);
	mt_set_gpio_pull_select(GPIO_LCD_ID0,GPIO_PULL_DOWN);
	mt_set_gpio_pull_select(GPIO_LCD_ID1,GPIO_PULL_DOWN);
	lcd_id0 =  mt_get_gpio_in(GPIO_LCD_ID0);
	lcd_id1 =  mt_get_gpio_in(GPIO_LCD_ID1);

//	printk("### leanda ili9486_lead buffer[0]  = %x,buffer[1]=%x,buffer[2]=%x,buffer[3]=%x \n", buffer[0],buffer[1],buffer[2],buffer[3]);        
    printk("### harrison ili9488 lead  lcd_id0=%d,lcd_id1=%d \r\n",lcd_id0,lcd_id1);
	//if((lcd_id1 == 0)&&(lcd_id0==0))

	if(id == 0x9488)
		return 1;

	return 0;
}
static void lcm_init(void)
{

    unsigned char lcd_id =0;
	// lcm_compare_id();
    unsigned char buffer[5];
	unsigned int array[16];
    
    SET_RESET_PIN(1);
    MDELAY(1);
    SET_RESET_PIN(0);
    MDELAY(15);
    SET_RESET_PIN(1);
    MDELAY(120);
     printk("### leanda lead  lcm_init \r\n");
	 #if (USE_V3_CMD)
	dsi_set_cmdq_V3(lcm_initialization_setting_v3, sizeof(lcm_initialization_setting_v3) / sizeof(LCM_setting_table_V3), 1);
	#else
    push_table(lcm_initialization_setting, sizeof(lcm_initialization_setting) / sizeof(struct LCM_setting_table), 1);
	#endif
}


static void lcm_suspend(void)
{
       #if 1
  //     SET_RESET_PIN(0);
	#if (USE_V3_CMD)
	dsi_set_cmdq_V3(lcm_deep_sleep_mode_in_setting_v3, sizeof(lcm_deep_sleep_mode_in_setting_v3) / sizeof(LCM_setting_table_V3), 1);
	#else
	push_table(lcm_deep_sleep_mode_in_setting, sizeof(lcm_deep_sleep_mode_in_setting) / sizeof(struct LCM_setting_table), 1);
	#endif  //     MDELAY(1);	   
  //     SET_RESET_PIN(1);	
	#else
	char null = 0;
	dsi_set_cmdq_V2(0x28, 1, &null, 1);
	MDELAY(10);
       dsi_set_cmdq_V2(0x10, 1, &null, 1);
       MDELAY(120);	   
    //   SET_RESET_PIN(0);	   
       //dsi_set_cmdq_V2(0x78, 1, &null, 1);
   //    MDELAY(1);	   
     //  SET_RESET_PIN(1);
       #endif
	   lcd_lead_stable = 0;
}

//int lcd_stable=0;
static void lcm_resume(void)
{

#if 1
//lcm_init();

#if (USE_V3_CMD)
	dsi_set_cmdq_V3(lcm_initialization_setting_v3, sizeof(lcm_initialization_setting_v3) / sizeof(LCM_setting_table_V3), 1);
	#else
    push_table(lcm_initialization_setting, sizeof(lcm_initialization_setting) / sizeof(struct LCM_setting_table), 1);
	#endif

#else
//	unsigned int data_array[2];

	char null = 0;


	//	MDELAY(20);

        int i=0;
      for(i=0;i<2;i++)
         	{
	dsi_set_cmdq_V2(0x11, 1, &null, 1);
         	}
	printk("### leanda lead  lcm_resume, 0x11 cmd \r\n");
     //data_array=0x00110500;
	//data_array[0]=0x00013902;
       // data_array[1]=0x11;
       // dsi_set_cmdq(&data_array, 2, 1);
	MDELAY(150);

	dsi_set_cmdq_V2(0x29, 1, &null, 1);
	printk("### leanda lead  lcm_resume, 0x29 cmd \r\n");
        lcd_stable=1;

#endif

	lcd_lead_stable = 1;

	//push_table(lcm_sleep_out_setting, sizeof(lcm_sleep_out_setting) / sizeof(struct LCM_setting_table), 1);
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
//	dsi_set_cmdq(&data_array, 1, 0);
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

static int g_esdCheckCount = 0;
static unsigned int lcm_esd_check(void)
{
#ifndef BUILD_LK

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
	#if 0
	array[0] = 0x00053700;
	dsi_set_cmdq(array, 1, 1);
	memset(buffer,0x0,5);
	read_reg_v2(0x09, buffer, 5);
	printk("### harrison 9488 aizhuoertai 0x09=0x%x,0x%x,0x%x,0x%x,0x%x\r\n",
					buffer[0],buffer[1],buffer[2],buffer[3],buffer[4]);
	if((buffer[1]==0x63)
		&&(buffer[2]==0x06))
	#else
	array[0] = 0x00023700;
	dsi_set_cmdq(array, 1, 1);
	memset(buffer,0x0,2);
	read_reg_v2(0x0A, buffer, 2);
	printk("### harrison 9488 aizhuoertai 0x0A=0x%x,0x%x\r\n",
					buffer[0],buffer[1]);
	if(buffer[0]==0x9C)
	#endif
	{
		return FALSE;
	}
	return TRUE;
#else
	return FALSE;
#endif
}

static unsigned int lcm_esd_recover(void)
{
/*
    unsigned char para = 0;

    SET_RESET_PIN(1);
    SET_RESET_PIN(0);
    MDELAY(1);
    SET_RESET_PIN(1);
    MDELAY(120);*/
//	  push_table(lcm_initialization_setting, sizeof(lcm_initialization_setting) / sizeof(struct LCM_setting_table), 1);
 //   MDELAY(10);
//	  push_table(lcm_sleep_out_setting, sizeof(lcm_sleep_out_setting) / sizeof(struct LCM_setting_table), 1);
	printk("###harrison 9488 test lcm_esd_recover\n");
	lcm_init();
//    MDELAY(10);
 //   dsi_set_cmdq_V2(0x35, 1, &para, 1);     ///enable TE
//    MDELAY(10);

    return TRUE;
}
#if 0
static unsigned int lcm_compare_id(void)
{
	unsigned int id = 0;
	unsigned char buffer[3];
	unsigned int array[16];

	unsigned char lcd_id = 0;
        SET_RESET_PIN(1);  //NOTE:should reset LCM firstly
    	SET_RESET_PIN(0);
    	MDELAY(150);
    	SET_RESET_PIN(1);
    	MDELAY(50);

	//push_table(lcm_compare_id_setting, sizeof(lcm_compare_id_setting) / sizeof(struct LCM_setting_table), 1);

	array[0] = 0x00023700;// read id return two byte,version and id
	dsi_set_cmdq(array, 1, 1);
//	id = read_reg(0xF4);
	read_reg_v2(0xD3, buffer, 3);
	id = buffer[1]; //we only need ID
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
LCM_DRIVER ili9488_lead_dsi_hvga_lcm_drv = 
{
        .name			= "ili9488_lead_dsi_hvga",
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

