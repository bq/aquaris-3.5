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
#ifdef BUILD_LK
#else
#include <linux/string.h>
    #if defined(BUILD_UBOOT)
        #include <asm/arch/mt_gpio.h>
    #else
        #include <mach/mt_gpio.h>
    #endif
#endif
#include "lcm_drv.h"


// ---------------------------------------------------------------------------
//  Local Constants
// ---------------------------------------------------------------------------

#define FRAME_WIDTH  										(480)
#define FRAME_HEIGHT 										(800)

#define REGFLAG_DELAY             							0XFE
#define REGFLAG_END_OF_TABLE      							0xFF   // END OF REGISTERS MARKER

#define LCM_DSI_CMD_MODE									0

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
#define read_reg											lcm_util.dsi_read_reg()
#define read_reg_v2(cmd, buffer, buffer_size)   			lcm_util.dsi_dcs_read_lcm_reg_v2(cmd, buffer, buffer_size)        

#define write_1byte(x) 	data_array[0] = (x<<16) | 0x00000500;\
						dsi_set_cmdq(&data_array, 1, 1);\
						MDELAY(3);

#define write_2byte(x1,x2)  data_array[0] = (x2<<24) |(x1<<16) |0x00001500;\
						dsi_set_cmdq(&data_array, 1, 1);\
						MDELAY(3);
#define write_nbyte(n)      data_array[0] = (n<<16) | 0x00003902;
#define get_4byte(x1,x2,x3,x4)  ((x4<<24) |(x3<<16) |(x2<<8) |(x1) )


static unsigned int lcm_read(void);

static struct LCM_setting_table {
    unsigned cmd;
    unsigned char count;
    unsigned char para_list[64];
};


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

	{0x03,	1,	{0x00}},
	{REGFLAG_DELAY, 10, {}},

   #if 1

	{0x20,	0,	{}},
	{0x35,	0,	{}},
	{0x36,	1,	{0x00}},
	{0x3A,	1,	{0x77}},
	
	//{0x51,	1,	{0xFF}},
	//{0x53,	1,	{0x24}},
	//{0x55,	1,	{0x00}},
	//{0x5E,	1,	{0x55}},

	{0xB1,	3,	{0x06, 0x43, 0x0A}},
	{0xB2,	2,	{0x00, 0xC8}},
	{REGFLAG_DELAY, 5, {}},

	{0xB3,	1,	{0x00}},
	{0xB4,	1,    {0x04}},
	{0xB5,	5,	{0x42, 0x10, 0x10, 0x00, 0x20}},
	{0xB6,	6,	{0x0B, 0x0F, 0x3c, 0x13, 0x13, 0xE8}},
	{0xb7,	5,	{0x46, 0x06, 0x0c, 0x00, 0x00}},

	{0xc0,	2,	{0x01, 0x15}},
	{0xC3,	5,	{0x07,0x03,0x04,0x04,0x04}},
	{0xC4,	6,	{0x12,0x24,0x13,0x13,0x02,0x49}},
	{0xC5,	1,	{0x67}},
	{0xC6,	2,	{0x41,0x63}},

	{0xD0,	9, {0x00,     0x07,     0x60,     0x25,    0x07,   0x02,    0x50,     0x26,    0x05}},
	{0xD1,	9, {0x00,     0x07,     0x60,     0x25,    0x07,   0x02,    0x50,     0x26,    0x05}},
	{0xD2,	9, {0x00,     0x07,     0x60,     0x25,    0x07,   0x02,    0x50,     0x26,    0x05 }},
	{0xD3,	9, {0x00,     0x07,     0x60,     0x25,    0x07,   0x02,    0x50,     0x26,    0x05}},
	{0xD4,	9, {0x00,     0x07,     0x60,     0x25,    0x07,   0x02,    0x50,     0x26,    0x05 }},
	{0xD5,	9, {0x00,     0x07,     0x60,     0x25,    0x07,   0x02,    0x50,     0x26,    0x05}},

//	{REGFLAG_DELAY, 10, {}},
//	{0x35,	1,	{0x00}},//soso for test
{0x11, 0, {0x00}},

{REGFLAG_DELAY, 150, {}},

    // Display ON

{0x29, 0, {0x00}},

{REGFLAG_DELAY, 20, {}},
	
#endif


	#if 0

	{0x20,	0,	{}},
	
	{0x36,	1,	{0x00}},
	{0x3A,	1,	{0x70}},
	
	{0x51,	1,	{0xFF}},
	{0x53,	1,	{0x24}},
	{0x55,	1,	{0x00}},
	{0x5E,	1,	{0x55}},

	{0xB1,	3,	{0x06, 0x43, 0x0A}},
	{0xB2,	2,	{0x00, 0xC8}},
	{0xB3,	1,	{0x02}},
	{0xB4,	1, {0x04}},
	{0xB5,	5,	{0x40, 0x10, 0x10, 0x00, 0x00}},
	{0xB6,	6,	{0x0B, 0x0F, 0x02, 0x40, 0x10, 0xE8}},

	{0xC3,	5,	{0x07,0x0A,0x0A,0x0A,0x02}},
	{0xC4,	6,	{0x12,0x24,0x18,0x18,0x02,0x49}},
	{0xC5,	1,	{0x6D}},
	{0xC6,	3,	{0x42,0x63,0x03}},

	{0xD0,	9, {0x22,0x05,0x65,0x03,0x00,0x04,0x21,0x00,0x02}},
	{0xD1,	9, {0x22,0x05,0x65,0x03,0x00,0x04,0x21,0x00,0x02}},
	{0xD2,	9, {0x22,0x05,0x65,0x03,0x00,0x04,0x21,0x00,0x02}},
	{0xD3,	9, {0x22,0x05,0x65,0x03,0x00,0x04,0x21,0x00,0x02}},
	{0xD4,	9, {0x22,0x05,0x65,0x03,0x00,0x04,0x21,0x00,0x02}},
	{0xD5,	9, {0x22,0x05,0x65,0x03,0x00,0x04,0x21,0x00,0x02}},


	 #endif





//	{REGFLAG_DELAY, 10, {}},
//	{0x35,	1,	{0x00}},//soso for test

	
	// Setting ending by predefined flag
	{REGFLAG_END_OF_TABLE, 0x00, {}}

};

/*
static struct LCM_setting_table lcm_set_window[] = {
	{0x2A,	4,	{0x00, 0x00, (FRAME_WIDTH>>8), (FRAME_WIDTH&0xFF)}},
	{0x2B,	4,	{0x00, 0x00, (FRAME_HEIGHT>>8), (FRAME_HEIGHT&0xFF)}},
	{REGFLAG_END_OF_TABLE, 0x00, {}}
};
*/

static struct LCM_setting_table lcm_sleep_out_setting[] = {
    // Sleep Out
	{0x11, 0, {0x00}},
       {REGFLAG_DELAY, 125, {}},
    // Display ON
	{0x29, 0, {0x00}},
	{REGFLAG_DELAY, 20, {}},
	{REGFLAG_END_OF_TABLE, 0x00, {}}

};


static struct LCM_setting_table lcm_deep_sleep_mode_in_setting[] = {
	// Display off sequence
	{0x28, 0, {0x00}},
       {REGFLAG_DELAY, 200, {}},
    // Sleep Mode On
	{0x10, 0, {0x00}},
       {REGFLAG_DELAY, 200, {}},    
	{REGFLAG_END_OF_TABLE, 0x00, {}}
};


static struct LCM_setting_table lcm_backlight_level_setting[] = {
	{0x51, 1, {0xFF}},
	{REGFLAG_END_OF_TABLE, 0x00, {}}
};

static struct LCM_setting_table lcm_backlight_mode_setting[] = {
	{0x55, 1, {0x1}},
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
		params->dbi.te_mode 				= LCM_DBI_TE_MODE_DISABLED;
		params->dbi.te_edge_polarity		= LCM_POLARITY_RISING;

#if (LCM_DSI_CMD_MODE)
		params->dsi.mode   = CMD_MODE;
#else
		params->dsi.mode   = SYNC_PULSE_VDO_MODE;
#endif
	
		// DSI
		/* Command mode setting */
		params->dsi.LANE_NUM				= LCM_TWO_LANE;
    
		//The following defined the fomat for data coming from LCD engine.
		params->dsi.data_format.color_order = LCM_COLOR_ORDER_RGB;
		params->dsi.data_format.trans_seq   = LCM_DSI_TRANS_SEQ_MSB_FIRST;
		params->dsi.data_format.padding     = LCM_DSI_PADDING_ON_LSB;
		params->dsi.data_format.format      = LCM_DSI_FORMAT_RGB888;

		// Video mode setting		
		params->dsi.intermediat_buffer_num = 2;

		params->dsi.PS=LCM_PACKED_PS_24BIT_RGB888;
    
    params->dsi.word_count=480*3;	//DSI CMD mode need set these two bellow params, different to 6577
    params->dsi.vertical_active_line=800;
		params->dsi.vertical_sync_active				= 3;
		params->dsi.vertical_backporch					= 12;
		params->dsi.vertical_frontporch					= 2;
		params->dsi.vertical_active_line				= FRAME_HEIGHT; 

		params->dsi.horizontal_sync_active				= 10;
		params->dsi.horizontal_backporch				= 50;
		params->dsi.horizontal_frontporch				= 50;
		params->dsi.horizontal_active_pixel				= FRAME_WIDTH;

		// Bit rate calculation
#ifdef CONFIG_MT6589_FPGA
    params->dsi.pll_div1=2;		// div1=0,1,2,3;div1_real=1,2,4,4
    params->dsi.pll_div2=2;		// div2=0,1,2,3;div1_real=1,2,4,4
    params->dsi.fbk_div =8;		// fref=26MHz, fvco=fref*(fbk_div+1)*2/(div1_real*div2_real)
#else//zhaoshaopeng for ili9806c 390Mhz
    params->dsi.pll_div1=2;		// div1=0,1,2,3;div1_real=1,2,4,4
    params->dsi.pll_div2=0;		// div2=0,1,2,3;div2_real=1,2,4,4
    params->dsi.fbk_div =0x1d;		// fref=26MHz, fvco=fref*(fbk_div+1)*2/(div1_real*div2_real)		
#endif
}

static void lcm_init(void)
{
	unsigned int data_array[16];

    SET_RESET_PIN(1);
    MDELAY(1 );
	SET_RESET_PIN(0);
    MDELAY(20);
	SET_RESET_PIN(1);
    MDELAY(120);
#if 1

	/*For DI Issue, Use dsi_set_cmdq() instead of dsi_set_cmdq_v2()*/
	data_array[0] = 0x00032300;//MIPI config
	dsi_set_cmdq(&data_array, 1, 1);

	//for MIPI Video mode
	data_array[0] = 0x00023902;
    data_array[1] = 0x00004701;                 
    dsi_set_cmdq(&data_array, 2, 1); 
	
	write_1byte(0x20);
	write_2byte(0x35,0x00);

 	write_2byte(0x36,0x00);
  	write_2byte(0x3a,0x77);

	write_nbyte(4);
	data_array[1]=get_4byte(0xb1,0x06,0x43,0x0a);
	dsi_set_cmdq(&data_array, 2, 1);
	MDELAY(3);	

	write_nbyte(3);
	data_array[1]=get_4byte(0xb2,0x00,0xc8,00);
	dsi_set_cmdq(&data_array, 2, 1);
	MDELAY(10);

  	write_2byte(0xb3,0x00);
  
  	write_2byte(0xb4,0x04);
  
 	write_nbyte(6);
	data_array[1]=get_4byte(0xb5,0x42,0x10,0x10);
	data_array[2]=get_4byte(0x00,0x20,0x00,00);
	dsi_set_cmdq(&data_array, 3, 1);
	MDELAY(3);
  
  	write_nbyte(7);
	data_array[1]=get_4byte(0xb6,0x0b,0x0f,0x3c);
	data_array[2]=get_4byte(0x13,0x13,0xe8,00);
	dsi_set_cmdq(&data_array, 3, 1);
	MDELAY(3);

  	write_nbyte(6);
	data_array[1]=get_4byte(0xb7,0x46,0x06,0x0c);
	data_array[2]=get_4byte(0x00,0x00,0x00,00);
	dsi_set_cmdq(&data_array, 3, 1);
	MDELAY(3);

	write_nbyte(3);
	data_array[1]=get_4byte(0xc0,0x01,0x15,00);
	dsi_set_cmdq(&data_array, 2, 1);
	MDELAY(10);
	
  	write_nbyte(6);
	data_array[1]=get_4byte(0xc3,0x07,0x03,0x04);
	data_array[2]=get_4byte(0x04,0x04,0x00,00);
	dsi_set_cmdq(&data_array, 3, 1);
	MDELAY(3);

    write_nbyte(7);
	data_array[1]=get_4byte(0xc4,0x12,0x24,0x13);
	data_array[2]=get_4byte(0x13,0x02,0x49,00);
	dsi_set_cmdq(&data_array, 3, 1);
	MDELAY(3);

    write_2byte(0xc5,0x67);

	write_nbyte(3);
	data_array[1]=get_4byte(0xc6,0x41,0x63,00);
	dsi_set_cmdq(&data_array, 2, 1);
	MDELAY(10);

	write_nbyte(10);
	data_array[1]=get_4byte(0xd0,0x00,0x07,0x60);
	data_array[2]=get_4byte(0x25,0x07,0x02,0x50);
	data_array[3]=get_4byte(0x26,0x05,0x00,0x00);
	dsi_set_cmdq(&data_array, 4, 1);
	MDELAY(10);

	write_nbyte(10);
	data_array[1]=get_4byte(0xd1,0x00,0x07,0x60);
	data_array[2]=get_4byte(0x25,0x07,0x02,0x50);
	data_array[3]=get_4byte(0x26,0x05,0x00,0x00);
	dsi_set_cmdq(&data_array, 4, 1);
	MDELAY(10);

	write_nbyte(10);
	data_array[1]=get_4byte(0xd2,0x00,0x07,0x60);
	data_array[2]=get_4byte(0x25,0x07,0x02,0x50);
	data_array[3]=get_4byte(0x26,0x05,0x00,0x00);
	dsi_set_cmdq(&data_array, 4, 1);
	MDELAY(10);

	write_nbyte(10);
	data_array[1]=get_4byte(0xd3,0x00,0x07,0x60);
	data_array[2]=get_4byte(0x25,0x07,0x02,0x50);
	data_array[3]=get_4byte(0x26,0x05,0x00,0x00);
	dsi_set_cmdq(&data_array, 4, 1);
	MDELAY(10);

	write_nbyte(10);
	data_array[1]=get_4byte(0xd4,0x00,0x07,0x60);
	data_array[2]=get_4byte(0x25,0x07,0x02,0x50);
	data_array[3]=get_4byte(0x26,0x05,0x00,0x00);
	dsi_set_cmdq(&data_array, 4, 1);
	MDELAY(10);

	write_nbyte(10);
	data_array[1]=get_4byte(0xd5,0x00,0x07,0x60);
	data_array[2]=get_4byte(0x25,0x07,0x02,0x50);
	data_array[3]=get_4byte(0x26,0x05,0x00,0x00);
	dsi_set_cmdq(&data_array, 4, 1);
	MDELAY(10);

	data_array[0] = 0x00110500;	// Sleep Out
	dsi_set_cmdq(&data_array, 1, 1);
	MDELAY(200);

    data_array[0] = 0x00290500; // Display On
    dsi_set_cmdq(&data_array, 1, 1);
    MDELAY(20);



#else
	push_table(lcm_initialization_setting, sizeof(lcm_initialization_setting) / sizeof(struct LCM_setting_table), 1);
#endif
}

static void lcm_suspend(void)
{
#if 0
	unsigned int data_array[16];
	data_array[0] = 0x00010500;//SW reset
	dsi_set_cmdq(&data_array, 1, 1);
	MDELAY(10);//Must > 5ms
#endif

#if 1
	SET_RESET_PIN(0);
	MDELAY(10);
	SET_RESET_PIN(1);
	MDELAY(20);

#endif
	push_table(lcm_deep_sleep_mode_in_setting, sizeof(lcm_deep_sleep_mode_in_setting) / sizeof(struct LCM_setting_table), 1);

}


static void lcm_resume(void)
{
	lcm_init();

	push_table(lcm_sleep_out_setting, sizeof(lcm_sleep_out_setting) / sizeof(struct LCM_setting_table), 1);

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
    dsi_set_cmdq(data_array, 3, 1);

    data_array[0]= 0x00053902;
    data_array[1]= (y1_MSB<<24)|(y0_LSB<<16)|(y0_MSB<<8)|0x2b;
    data_array[2]= (y1_LSB);
    dsi_set_cmdq(data_array, 3, 1);

    data_array[0]= 0x002c3909;
    dsi_set_cmdq(data_array, 7, 0);
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


static void lcm_setpwm(unsigned int divider)
{
	// TBD
}


static unsigned int lcm_getpwm(unsigned int divider)
{
	// ref freq = 15MHz, B0h setting 0x80, so 80.6% * freq is pwm_clk;
	// pwm_clk / 255 / 2(lcm_setpwm() 6th params) = pwm_duration = 23706
	unsigned int pwm_clk = 23706 / (1<<divider);	
	return pwm_clk;
}

static unsigned int lcm_read(void)
{
	unsigned int id = 0;
	unsigned char buffer[2]={0x88};
	unsigned int array[16];

	array[0] = 0x00013700;// read id return two byte,version and id
	dsi_set_cmdq(array, 1, 1);
//	id = read_reg(0xF4);
	read_reg_v2(0x52, buffer, 1);
	id = buffer[0]; //we only need ID
#ifndef BUILD_UBOOT
	printk("\n\n\n\n\n\n\n\n\n\n[soso]%s, lcm_read = 0x%08x\n", __func__, id);
#endif
    //return (LCM_ID == id)?1:0;
}


LCM_DRIVER lg4573b_wvga_dsi_vdo_s1282_linlong_lcm_drv = 
{
    .name			= "lg4573b_wvga_dsi_vdo_s1282_linlong",
	.set_util_funcs = lcm_set_util_funcs,
	.get_params     = lcm_get_params,
	.init           = lcm_init,
	.suspend        = lcm_suspend,
	.resume         = lcm_resume,
#if (LCM_DSI_CMD_MODE)
	.set_backlight	= lcm_setbacklight,
    .update         = lcm_update,
#endif
};

