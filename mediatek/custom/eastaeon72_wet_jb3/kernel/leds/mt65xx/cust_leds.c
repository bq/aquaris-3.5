#include <cust_leds.h>
#include <mach/mt_pwm.h>

#include <linux/kernel.h>
#include <mach/pmic_mt6329_hw_bank1.h> 
#include <mach/pmic_mt6329_sw_bank1.h> 
#include <mach/pmic_mt6329_hw.h>
#include <mach/pmic_mt6329_sw.h>
#include <mach/upmu_common_sw.h>
#include <mach/upmu_hw.h>
#include <mach/mt_gpio.h>
#include <linux/delay.h>
#include <mach/irqs.h>

#define LCD_PWM_MOD_VALUE 255

#ifndef GPIO_LCD_PULSE_CONTRL
#define GPIO_LCD_PULSE_CONTRL GPIO134
#endif

//extern int mtkfb_set_backlight_level(unsigned int level);
//extern int mtkfb_set_backlight_pwm(int div);
extern int disp_bls_set_backlight(unsigned int level);
/*
#define ERROR_BL_LEVEL 0xFFFFFFFF

unsigned int brightness_mapping(unsigned int level)
{  
	return ERROR_BL_LEVEL;
}
*/
unsigned int brightness_mapping(unsigned int level)
{
    unsigned int mapped_level;
    
    mapped_level = level;
       
	return mapped_level;
}

unsigned int Cust_SetBacklight(int level, int div)
{
    //mtkfb_set_backlight_pwm(div);
    //mtkfb_set_backlight_level(brightness_mapping(level));
    disp_bls_set_backlight(brightness_mapping(level));
    return 0;
}

extern int g_dispSearchLcm;
static int custom_disp_bls_set_backlight( unsigned int  value)
{
	int err = 0xff;
	unsigned long flags;
	unsigned long duty_mod= 0;
	unsigned long brightness= value;
	unsigned int i= 0;
	unsigned int  brightness_count = 0; 
	static unsigned int s_bBacklightOn = 1; 
	static unsigned int	g_ledCurrPulseCount = 0;

	if(g_dispSearchLcm == 0)
	{
		mt_set_gpio_out(GPIO_LCD_PULSE_CONTRL, GPIO_OUT_ZERO);
		return 0;
	}
	
	if(value > LCD_PWM_MOD_VALUE)
		value = LCD_PWM_MOD_VALUE;

	if(value < 0)
		value = 0;

	duty_mod = (value << 8) | LCD_PWM_MOD_VALUE;
    printk("### kernel leanda value =%d  g_ledCurrPulseCount=%d \r\n",value,g_ledCurrPulseCount);
	
	//gpio_direction_output(143,1);		
	mt_set_gpio_mode(GPIO_LCD_PULSE_CONTRL, GPIO_MODE_00);
	mt_set_gpio_dir(GPIO_LCD_PULSE_CONTRL, GPIO_DIR_OUT);
    if (value == 0)
    {

		mt_set_gpio_out(GPIO_LCD_PULSE_CONTRL, GPIO_OUT_ZERO);

		s_bBacklightOn = 0; 
		g_ledCurrPulseCount = 0;
		mdelay(2);			
    }
    else
    {
		brightness =(255 -brightness) /8;
	
        printk("brightness is %d\n",brightness);

#if 1
        if (s_bBacklightOn)
        { 
			if (g_ledCurrPulseCount > brightness) 
			{ 
			        brightness_count = brightness+32-g_ledCurrPulseCount; 
			}
			else if (g_ledCurrPulseCount < brightness)
			{   
			        brightness_count = brightness - g_ledCurrPulseCount; 
			}
			else
			{  
			        return 0; 
			} 
		
	
			local_irq_save(flags);
    		for(i=0 ; i < brightness_count ; i++)
    		{
    			mt_set_gpio_out(GPIO_LCD_PULSE_CONTRL, GPIO_OUT_ZERO);	
    			udelay(1);
    			mt_set_gpio_out(GPIO_LCD_PULSE_CONTRL, GPIO_OUT_ONE);
    			udelay(1);
    		}	
			local_irq_restore(flags);
		}
        else
        {
            brightness_count = brightness;
			local_irq_save(flags);
			mt_set_gpio_out(GPIO_LCD_PULSE_CONTRL, GPIO_OUT_ONE);
			udelay(40);
        		for(i=0 ; i < brightness_count; i++)
        		{
        			mt_set_gpio_out(GPIO_LCD_PULSE_CONTRL, GPIO_OUT_ZERO);	
        			udelay(1);
        			mt_set_gpio_out(GPIO_LCD_PULSE_CONTRL, GPIO_OUT_ONE);	
        			udelay(1);
        		}	
			local_irq_restore(flags);
        }

        g_ledCurrPulseCount = brightness;
        s_bBacklightOn = 1; 
			
#endif		
    }
	
	return 0;
}


static struct cust_mt65xx_led cust_led_list[MT65XX_LED_TYPE_TOTAL] = {
	{"red",               MT65XX_LED_MODE_NONE, -1, {0}},
	{"green",              MT65XX_LED_MODE_NONE, -1, {0}},
	{"blue",              MT65XX_LED_MODE_NONE, -1, {0}},
	{"jogball-backlight", MT65XX_LED_MODE_NONE, -1, {0}},
	{"keyboard-backlight",MT65XX_LED_MODE_NONE, -1, {0}},
	{"button-backlight",  MT65XX_LED_MODE_NONE, -1,{0}},
	//{"lcd-backlight",     MT65XX_LED_MODE_CUST_BLS_PWM, (int)disp_bls_set_backlight,{0}},
	{"lcd-backlight",     MT65XX_LED_MODE_CUST_BLS_PWM, (int)custom_disp_bls_set_backlight,{0}},
};

struct cust_mt65xx_led *get_cust_led_list(void)
{
	return cust_led_list;
}

