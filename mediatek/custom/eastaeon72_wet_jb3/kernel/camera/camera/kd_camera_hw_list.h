#ifndef _KD_CAMERA_HW_LIST_H_
#define _KD_CAMERA_HW_LIST_H_
//#include <mach/mt_pm_ldo.h>
//#include <mach/mt6577_gpio.h>
#include "kd_camera_hw.h"

#define CAM_ON	0
#define CAM_OFF     1

typedef enum cam_power_index_e{
	VCAMD_ID,
	VCAMD2_ID,
	VCAMA_ID,
	VCAMA2_ID,
	CAM_RST_TYPE,
	CAM_PDN_TYPE,
	CAM_INDEX_MAX
}CAM_POWER_INDEX;
#define	CAM_PIN_MAX 2

#define	CAM_POWER_INDEX_TYPE  0x1
#define CAM_PIN_INDEX_TYPE	(0x1<<1)
#define CAM_POWER_OR_PIN_MAX_ARRAY	20

typedef enum cam_type_e{
	CAM_MAIN_E,
	CAM_SUB_E,
	CAM_MAIN2_E,
	CAM_TYPE_MAX,
}CAM_TYPE_E;
#define     CAM_MAX_SUPPORT_TYPE        CAM_MAIN2_E         //支持前后摄像头或者后摄像头2

typedef struct cam_power_info_s{
	CAM_POWER_INDEX cam_index;
	unsigned long data;
	unsigned long data1;
	unsigned int delay;
	unsigned int reserve;
}CAM_POWER_INFO;

typedef struct cam_power_str{
//	unsigned int cam_seq;
	char deviceName[32];
	CAM_TYPE_E cam_type;
	CAM_POWER_INFO cam_info[2][CAM_POWER_OR_PIN_MAX_ARRAY];
}CAM_POWER_STR;

typedef struct cam_power_list_str{
	CAM_POWER_STR *powerStr;
        CAM_TYPE_E cam_type;
	char defaultSet; 
}CAM_POWER_List_STR;

typedef struct cam_single_pin_info{
	u32 cam_pin;
	u32 cam_pin_mode;
}CAM_SINGLE_PIN_INFO;

typedef struct cam_pin_info{
	CAM_SINGLE_PIN_INFO pingInfo[CAM_PIN_MAX];
	u32 pinLevel[CAM_PIN_MAX*2];
}CAM_PIN_INFO;

typedef struct cam_curr_info{
	char deviceName[32];
	CAM_TYPE_E cam_type;
}CAM_CURR_INFO;

//	RST_ON	     		RST_OFF         PDN_ON            PDN_OFF
#define CAM_PIN_LEVEL1	\
	{GPIO_OUT_ONE,GPIO_OUT_ZERO,GPIO_OUT_ZERO,GPIO_OUT_ONE}

#define CAM_PIN_LEVEL2	\
	{GPIO_OUT_ONE,GPIO_OUT_ZERO,GPIO_OUT_ONE,GPIO_OUT_ZERO}


static MT65XX_POWER g_camPowerVolt[][4]={
	//main camer
	{
		CAMERA_POWER_VCAM_D,
		CAMERA_POWER_VCAM_D2,
		CAMERA_POWER_VCAM_A,
		CAMERA_POWER_VCAM_A2
	},
	//sub camer
	{
		CAMERA_POWER_VCAM_D,
		CAMERA_POWER_VCAM_D2,
		CAMERA_POWER_VCAM_A,
		MT65XX_POWER_NONE,//CAMERA_POWER_VCAM_A2
	},
	///2 main camer
	{
		MT65XX_POWER_NONE,//CAMERA_POWER_VCAM_D,
		MT65XX_POWER_NONE,//CAMERA_POWER_VCAM_D2,
		MT65XX_POWER_NONE,//CAMERA_POWER_VCAM_A,
		MT65XX_POWER_NONE,//CAMERA_POWER_VCAM_A2
	},
};

//顺序必须与CAM_TYPE_E一致
//修改时要注意修改g_camPinInfo中默认的pinLevel
static CAM_PIN_INFO g_camPinInfo[]= {
//main sensor
	{
		{{GPIO_CAMERA_CMRST1_PIN,GPIO_CAMERA_CMRST1_PIN_M_GPIO},{GPIO_CAMERA_CMPDN1_PIN,GPIO_CAMERA_CMPDN1_PIN_M_GPIO}},
		CAM_PIN_LEVEL1
	},
//sub sensor
	{
		{{GPIO_CAMERA_CMRST_PIN,GPIO_CAMERA_CMRST_PIN_M_GPIO},{GPIO_CAMERA_CMPDN_PIN,GPIO_CAMERA_CMPDN_PIN_M_GPIO}},
		CAM_PIN_LEVEL2
	},
//main 2 sensor
	{
		{{GPIO_CAMERA_2_CMRST_PIN,GPIO_CAMERA_2_CMRST_PIN_M_GPIO},{GPIO_CAMERA_2_CMPDN_PIN,GPIO_CAMERA_2_CMPDN_PIN_M_GPIO}},
		CAM_PIN_LEVEL1
	},
};
#define	CAMPININFO_NUM	(sizeof(g_camPinInfo)/sizeof(g_camPinInfo[0]))

#if defined(OV9740_MIPI_YUV)
static CAM_POWER_STR g_camPowerInfo_ov9740 ={
 	//Device Name		 				Cam_type
	SENSOR_DRVNAME_OV9740_MIPI_YUV,		CAM_SUB_E,
	{
		//on
		{
			//   cam_index		data		data1,	delay, reserve	
			{
				CAM_RST_TYPE,	0,		0,		5,		0
			},
			{
				CAM_PDN_TYPE,	0,		0,		1,		0
			},
			{
				VCAMD2_ID,	VOL_2800,	0,		5,		0
			},
			{
				VCAMA_ID,	VOL_2800,	0,		5,		0
			},
			{
				VCAMD_ID,	VOL_1800,	0,		5,		0
			},
			{
				CAM_RST_TYPE,	1,		0,		0,		0
			},
			{
				CAM_INDEX_MAX,	1,		0,		10,		0		//结束
			}
		},
		//off
		{
			{
				CAM_PDN_TYPE,	0,		0,		0,		0
			},
			{
				CAM_RST_TYPE,	0,		0,		5,		0
			},
			{
				VCAMD_ID,	VOL_1800,	0,		1,		0
			},
			{
				VCAMA_ID,	VOL_2800,	0,		1,		0
			},
			{
				VCAMD2_ID,	VOL_1800,	0,		0,		0
			},
			{
				CAM_INDEX_MAX,	1,		0,		10,		0		//结束
			}
		}
	}
};
#endif


#if defined(OV8825_MIPI_RAW)
static CAM_POWER_STR g_camPowerInfo_ov8825 ={
 	//Device Name		 				Cam_type
	SENSOR_DRVNAME_OV8825_MIPI_RAW,		CAM_MAIN_E,
	{
		//on
		{
			//   cam_index		data		data1,	delay, reserve			
			{
				VCAMD2_ID,	VOL_1800,	0,		1,		0
			},
			{
				VCAMA_ID,	VOL_2800,	0,		1,		0
			},
			{
				VCAMD_ID,	VOL_1500,	0,		1,		0
			},
			{
				VCAMA2_ID,	VOL_2800,	0,		5,		0
			},
			{
				CAM_PDN_TYPE,	0,		0,		10,		0
			},
			{
				CAM_PDN_TYPE,	1,		0,		1,		0
			},
			{
				CAM_RST_TYPE,	0,		0,		10,		0
			},
			{
				CAM_RST_TYPE,	1,		0,		1,		0
			},
			{
				CAM_INDEX_MAX,	1,		0,		10,		0		//结束
			}
		},
		//off
		{
			{
				CAM_RST_TYPE,	0,		0,		1,		0
			},
			{
				VCAMD_ID,	VOL_1500,	0,		1,		0
			},
			{
				VCAMA_ID,	VOL_2800,	0,		1,		0
			},
			{
				VCAMD2_ID,	VOL_1800,	0,		1,		0
			},
			{
				VCAMA2_ID,	VOL_2800,	0,		1,		0
			},
			{
				CAM_PDN_TYPE,	0,		0,		1,		0
			},
			{
				CAM_INDEX_MAX,	1,		0,		10,		0		//结束
			}
		}
	}
};
#endif


#if defined(S5K4E1FX_MIPI_RAW)
static CAM_POWER_STR g_camPowerInfo_s5k4e1fx ={
 	//Device Name		 				Cam_type
	SENSOR_DRVNAME_S5K4E1FX_MIPI_RAW,	CAM_MAIN_E,
	{
		//on
		{
			//   cam_index		data		data1,	delay, reserve
			{
				CAM_RST_TYPE,	0,		0,		1,		0
			},
			{
				CAM_PDN_TYPE,	0,		0,		0,		0
			},
			{
				VCAMD_ID,	VOL_1800,	0,		10,		0
			},
			{
				VCAMA_ID,	VOL_2800,	0,		10,		0
			},
			{
				VCAMA2_ID,	VOL_2800,	0,		5,		0
			},
			{
				VCAMD2_ID,	VOL_1800,	0,		10,		0
			},
			{
				CAM_PDN_TYPE,	1,		0,		5,		0
			},
			{
				CAM_RST_TYPE,	1,		0,		10,		0
			},
			{
				CAM_PDN_TYPE,	1,		0,		5,		0
			},
			{
				CAM_INDEX_MAX,	1,		0,		10,		0		//结束
			}
		},
		//off
		{
			{
				CAM_PDN_TYPE,	0,		0,		1,		0
			},
			{
				CAM_RST_TYPE,	0,		0,		1,		0
			},
			{
				CAM_PDN_TYPE,	0,		0,		1,		0
			},
			{
				VCAMA_ID,	VOL_2800,	0,		10,		0
			},
			{
				VCAMA2_ID,	VOL_2800,	0,		10,		0
			},
			{
				VCAMD_ID,	VOL_1800,	0,		5,		0
			},
			{
				VCAMD2_ID,	VOL_1800,	0,		5,		0
			},
			{
				CAM_INDEX_MAX,	1,		0,		10,		0		//结束
			}
		}
	}
};
#endif

#if defined(HI542_MIPI_RAW)
static CAM_POWER_STR g_camPowerInfo_hi542 ={
 	//Device Name		 		Cam_type
	SENSOR_DRVNAME_HI542MIPI_RAW,CAM_MAIN_E,
	{
		//on
		{
			//cam_index			data		data1,	delay,	reserve
			{
				CAM_RST_TYPE,	0,		0,		1,		0
			},
			{
				CAM_PDN_TYPE,	0,		0,		0,		0
			},
			{
				VCAMD2_ID,	VOL_1800,	0,		1,		0
			},
			{
				CAM_RST_TYPE,	1,		0,		10,		0
			},
			{
				VCAMA_ID,	VOL_2800,	0,		1,		0
			},
			{
				VCAMD_ID,	VOL_1500,	0,		5,		0
			},
			{
				VCAMA2_ID,	VOL_2800,	0,		5,		0
			},
//			{
//				CAM_PDN_TYPE,CAM_ON,0,5,0
//			},
//			{
//				CAM_RST_TYPE,CAM_ON,0,10,0
//			},
			{
				CAM_PDN_TYPE,	1,		0,		0,		0
			},
			{
				CAM_INDEX_MAX,	1,		0,		10,		0		//结束
			}
		},
		//off
		{
			{
				CAM_PDN_TYPE,	0,		0,		1,		0
			},
//			{
//				CAM_RST_TYPE,CAM_OFF,0,24,0
//			},
//			{
//				CAM_PDN_TYPE,CAM_OFF,0,0,0
//			},
			{
				VCAMD_ID,	VOL_1500,	0,		1,		0
			},
			{
				VCAMA_ID,	VOL_2800,	0,		1,		0
			},
			{
				VCAMA2_ID,	VOL_2800,	0,		5,		0
			},
			{
				CAM_RST_TYPE,	1,		0,		24,		0
			},
			{
				VCAMD2_ID,	VOL_1800,	0,		5,		0
			},
			{
				CAM_INDEX_MAX,	0,		0,		10,		0		//结束
			}
		}
	}
};
#endif

#if defined(HI253_YUV)
static CAM_POWER_STR g_camPowerInfo_hi253 ={
 	//Device Name		 		Cam_type
	SENSOR_DRVNAME_HI253_YUV,	CAM_MAIN_E,
	{
		//on
		{
			//cam_index			data		data1,	delay,	reserve
			{
				CAM_RST_TYPE,	0,		0,		1,		0
			},
			{
				CAM_PDN_TYPE,	1,		0,		1,		0
			},
			{
				VCAMD2_ID,	VOL_2800,	0,		1,		0
			},
//			{
//				CAM_RST_TYPE,	1,		0,		10,		0
//			},
			{
				VCAMA_ID,	VOL_2800,	0,		1,		0
			},
			{
				VCAMD_ID,	VOL_1800,	0,		5,		0
			},
			{
				VCAMA2_ID,	VOL_2800,	0,		5,		0
			},
			{
				CAM_PDN_TYPE,	0,		0,		110,	0
			},
			{
				CAM_RST_TYPE,	1,		0,		10,		0
			},
//			{
//				CAM_PDN_TYPE,	1,		0,		0,		0
//			},
			{
				CAM_INDEX_MAX,	1,		0,		10,		0		//结束
			}
		},
		//off
		{
//			{
//				CAM_PDN_TYPE,	0,		0,		1,		0
//			},
			{
				CAM_RST_TYPE,	0,		0,		100,		0
			},
			{
				CAM_PDN_TYPE,	1,		0,		1,		0
			},
			{
				VCAMD_ID,	VOL_1800,	0,		1,		0
			},
			{
				VCAMA_ID,	VOL_2800,	0,		1,		0
			},
			{
				VCAMD2_ID,	VOL_1800,	0,		1,		0
			},
			{
				VCAMA2_ID,	VOL_2800,	0,		1,		0
			},
//			{
//				CAM_RST_TYPE,	1,		0,		24,		0
//			},
			{
				CAM_INDEX_MAX,	0,		0,		10,		0		//结束
			}
		}
	}
};
#endif

#if defined(SP2518_YUV)
static CAM_POWER_STR g_camPowerInfo_sp2518 ={
 	//Device Name		 		Cam_type
	SENSOR_DRVNAME_SP2518_YUV,	CAM_MAIN_E,
	{
		//on
		{
			//cam_index			data		data1,	delay,	reserve
//			{
//				CAM_RST_TYPE,	0,		0,		1,		0
//			},
//			{
//				CAM_PDN_TYPE,	1,		0,		1,		0
//			},
			{
				VCAMA_ID,	VOL_2800,	0,		0,		0
			},
			{
				VCAMD2_ID,	VOL_2800,	0,		1,		0
			},
			{
				VCAMD_ID,	VOL_1800,	0,		0,		0
			},
			{
				VCAMA2_ID,	VOL_2800,	0,		10,		0
			},
			{
				CAM_PDN_TYPE,	0,		0,		5,	0
			},
			{
				CAM_PDN_TYPE,	1,		0,		5,	0
			},
			{
				CAM_PDN_TYPE,	0,		0,		5,	0
			},
			{
				CAM_RST_TYPE,	1,		0,		2,		0
			},
			{
				CAM_RST_TYPE,	0,		0,		10,		0
			},
			{
				CAM_RST_TYPE,	1,		0,		1,		0
			},
			{
				CAM_INDEX_MAX,	1,		0,		10,		0		//结束
			}
		},
		//off
		{
//			{
//				CAM_PDN_TYPE,	0,		0,		1,		0
//			},
			{
				CAM_RST_TYPE,	1,		0,		0,		0
			},
			{
				CAM_PDN_TYPE,	0,		0,		1,		0
			},
			{
				VCAMA2_ID,	VOL_2800,	0,		1,		0
			},
			{
				VCAMD_ID,	VOL_1800,	0,		1,		0
			},
			{
				VCAMA_ID,	VOL_2800,	0,		1,		0
			},
			{
				VCAMD2_ID,	VOL_2800,	0,		1,		0
			},
//			{
//				CAM_RST_TYPE,	1,		0,		24,		0
//			},
			{
				CAM_INDEX_MAX,	0,		0,		10,		0		//结束
			}
		}
	}
};
#endif


#if defined(OV7690_YUV)
static CAM_POWER_STR g_camPowerInfo_ov7690 ={
 	//Device Name		 		Cam_type
	SENSOR_DRVNAME_OV7690_YUV,	CAM_SUB_E,
	{
		//on
		{
				//cam_index	  	data		data1,	delay, 	reserve
			{
				CAM_RST_TYPE,	0,		0,		1,		0
			},
			{
				CAM_PDN_TYPE,	1,		0,		0,		0
			},
			{
				VCAMD2_ID,	VOL_1800,	0,		3,		0
			},
			{
				VCAMA_ID,	VOL_2800,	0,		50,		0
			},
			{
				VCAMA2_ID,	VOL_2800,	0,		5,		0
			},
			{
				VCAMD_ID,	VOL_1800,	0,		5,		0
			},
			{
				CAM_RST_TYPE,	0,		0,		10,		0
			},
			{
				CAM_RST_TYPE,	1,		0,		1,		0
			},
			{
				CAM_PDN_TYPE,	0,		0,		5,		0
			},
			{
				CAM_INDEX_MAX,	1,		0,		10,		0		//结束
			}
		},
		//off
		{
			{
				CAM_PDN_TYPE,	1,		0,		1,		0
			},
			{
				CAM_RST_TYPE,	0,		0,		1,		0
			},
			{
				CAM_PDN_TYPE,	1,		0,		5,		0
			},
			{
				VCAMA_ID,	VOL_1800,	0,		3,		0
			},
			{
				VCAMA2_ID,	VOL_2800,	0,		5,		0
			},
			{
				VCAMD_ID,	VOL_1800,	0,		1,		0
			},
			{
				VCAMD2_ID,	VOL_2800,	0,		5,		0
			},
			{
				CAM_INDEX_MAX,	1,		0,		10,		0		//结束
			}
		}
	}
};
#endif

#if defined(S5K4ECGX_MIPI_YUV)
static CAM_POWER_STR g_camPowerInfo_s5k4ecgx ={
 	//Device Name		 		Cam_type
	SENSOR_DRVNAME_S5K4ECGX_MIPI_YUV,	CAM_MAIN_E,
	{
		//on
		{
				//cam_index	  	data		data1,	delay, 	reserve
			{
				VCAMD2_ID,	VOL_1800,	0,		0,		0
			},
			{
				VCAMA_ID,	VOL_2800,	0,		0,		0
			},
			{
				VCAMD_ID,	VOL_1200,	0,		0,		0
			},
			{
				VCAMA2_ID,	VOL_1800,	0,		5,		0
			},
			{
				CAM_RST_TYPE,	0,		0,		1,		0
			},
			{
				CAM_PDN_TYPE,	0,		0,		1,		0
			},
			{
				CAM_PDN_TYPE,	1,		0,		3,		0
			},
			{
				CAM_RST_TYPE,	1,		0,		3,		0
			},
			{
				CAM_INDEX_MAX,	1,		0,		10,		0		//结束
			}
		},
		//off
		{
		        {
				CAM_RST_TYPE,	0,		0,		10,		0
			},
			{
				CAM_PDN_TYPE,	0,		0,		5,		0
			},
			{
				VCAMD_ID,	VOL_1800,	0,		0,		0
			},
			{
				VCAMA_ID,	VOL_2800,	0,		0,		0
			},
			{
				VCAMD2_ID,	VOL_1800,	0,		0,		0
			},
			{
				VCAMA2_ID,	VOL_2800,	0,		0,		0
			},
			{
				CAM_INDEX_MAX,	1,		0,		10,		0		//结束
			}
		}
	}
};
#endif

#if defined(S5K8AAYX_YUV)
static CAM_POWER_STR g_camPowerInfo_s5k8aayx_yuv ={
 	//Device Name		 		Cam_type
	SENSOR_DRVNAME_S5K8AAYX_YUV,	CAM_MAIN_E,
	{
		//on
		{
				//cam_index	  	data		data1,	delay, 	reserve
			{
				VCAMD2_ID,	VOL_1800,	0,		0,		0
			},
			{
				VCAMA_ID,	VOL_2800,	0,		0,		0
			},
			{
				VCAMD_ID,	VOL_1200,	0,		0,		0
			},
			{
				CAM_PDN_TYPE,	0,		0,		10,		0
			},
			{
				CAM_PDN_TYPE,	1,		0,		5,		0
			},
			{
				CAM_RST_TYPE,	0,		0,		10,		0
			},
			{
				CAM_RST_TYPE,	1,		0,		5,		0
			},
			{
				CAM_INDEX_MAX,	1,		0,		10,		0		//结束
			}
		},
		//off
		{
		        {
				CAM_RST_TYPE,	0,		0,		10,		0
			},
			{
				CAM_PDN_TYPE,	0,		0,		5,		0
			},
			{
				VCAMD_ID,	VOL_1800,	0,		0,		0
			},
			{
				VCAMA_ID,	VOL_2800,	0,		0,		0
			},
			{
				VCAMD2_ID,	VOL_1800,	0,		0,		0
			},
			{
				VCAMA2_ID,	VOL_2800,	0,		0,		0
			},
			{
				CAM_INDEX_MAX,	1,		0,		10,		0		//结束
			}
		}
	}
};
#endif

#if defined(GC0309_YUV)
static CAM_POWER_STR g_camPowerInfo_gc0309yuv ={
 	//Device Name		 		Cam_type
	SENSOR_DRVNAME_GC0309_YUV,	CAM_SUB_E,
	{
		//on
		{
				//cam_index	  	data		data1,	delay, 	reserve
			{
				VCAMD2_ID,	VOL_1800,	0,		0,		0
			},
			{
				VCAMA_ID,	VOL_2800,	0,		0,		0
			},
			{
				VCAMD_ID,	VOL_1800,	0,		0,		0
			},
			{
				CAM_RST_TYPE,	0,		0,		10,		0
			},
			{
				CAM_RST_TYPE,	1,		0,		1,		0
			},
			{
				CAM_PDN_TYPE,	0,		0,		0,		0
			},
			{
				CAM_INDEX_MAX,	1,		0,		10,		0		//结束
			}
		},
		//off
		{
		        {
				CAM_RST_TYPE,	0,		0,		1,		0
			},
			{
				CAM_PDN_TYPE,	1,		0,		1,		0
			},
			{
				VCAMA_ID,	VOL_2800,	0,		0,		0
			},
			{
				VCAMD_ID,	VOL_1800,	0,		0,		0
			},
			{
				VCAMD2_ID,	VOL_1800,	0,		0,		0
			},
			{
				CAM_INDEX_MAX,	1,		0,		10,		0		//结束
			}
		}
	}
};
#endif

#if defined(GC0329_YUV)
static CAM_POWER_STR g_camPowerInfo_gc0329yuv ={
 	//Device Name		 		Cam_type
	SENSOR_DRVNAME_GC0329_YUV,	CAM_SUB_E,
	{
		//on
		{
				//cam_index	  	data		data1,	delay, 	reserve
			{
				CAM_RST_TYPE,	0,		0,		0,		0
			},
			{
				CAM_PDN_TYPE,	1,		0,		0,		0
			},
			{
				VCAMD2_ID,	VOL_1800,	0,		1,		0
			},
			{
				VCAMA_ID,	VOL_2800,	0,		1,		0
			},
			{
				VCAMD_ID,	VOL_1800,	0,		10,		0
			},
			{
				CAM_PDN_TYPE,	0,		0,		10,		0
			},
			{
				CAM_RST_TYPE,	1,		0,		20,		0
			},
			{
				CAM_INDEX_MAX,	1,		0,		10,		0		//结束
			}
		},
		//off
		{
		        {
				CAM_PDN_TYPE,	1,		0,		5,		0
			},
		        {
				CAM_RST_TYPE,	0,		0,		5,		0
			},
			{
				VCAMD_ID,	VOL_1800,	0,		5,		0
			},
			{
				VCAMA_ID,	VOL_1800,	0,		5,		0
			},
			{
				VCAMD2_ID,	VOL_2800,	0,		10,		0
			},
			{
				CAM_INDEX_MAX,	1,		0,		10,		0		//结束
			}
		}
	}
};
#endif

#if defined(SOC5140_MIPI_YUV)
static CAM_POWER_STR g_camPowerInfo_soc5140mipi={
 	//Device Name		 		Cam_type
	SENSOR_DRVNAME_SOC5140_MIPI_YUV,	CAM_MAIN_E,
	{
		//on
		{
				//cam_index	  	data		data1,	delay, 	reserve
			{
				CAM_PDN_TYPE,	1,		0,		0,		0
			},
			{
				CAM_RST_TYPE,	0,		0,		10,		0
			},
			{
				VCAMD_ID,	VOL_1800,	0,		1,		0
			},
			{
				VCAMD2_ID,	VOL_1800,	0,		1,		0
			},
			{
				VCAMA_ID,	VOL_2800,	0,		1,		0
			},
			{
				VCAMA2_ID,	VOL_2800,	0,		20,		0
			},
			{
				CAM_RST_TYPE,	1,		0,		2,		0
			},
			{
				CAM_PDN_TYPE,	0,		0,		20,		0
			},
			{
				CAM_INDEX_MAX,	1,		0,		10,		0		//结束
			}
		},
		//off
		{
		        {
				CAM_PDN_TYPE,	1,		0,		10,		0
			},
		        {
				CAM_RST_TYPE,	0,		0,		10,		0
			},
			{
				VCAMA2_ID,	VOL_1800,	0,		0,		0
			},
			{
				VCAMD_ID,	VOL_1800,	0,		0,		0
			},
			{
				VCAMA_ID,	VOL_2800,	0,		0,		0
			},
			{
				VCAMD2_ID,	VOL_1800,	0,		20,		0
			},
			{
				CAM_PDN_TYPE,	0,		0,		0,		0
			},
			{
				CAM_INDEX_MAX,	1,		0,		10,		0		//结束
			}
		}
	}
};
#endif


#if defined(GC2035_YUV)
static CAM_POWER_STR g_camPowerInfo_gc2035yuv ={
 	//Device Name		 		Cam_type
	SENSOR_DRVNAME_GC2035_YUV,	CAM_MAIN_E,
	{
		//on
		{
				//cam_index	  	data		data1,	delay, 	reserve
		        {
				CAM_PDN_TYPE,	0,		0,		0,		0
			},
			{
				CAM_RST_TYPE,	0,		0,		0,		0
			},
			{
				VCAMD2_ID,	VOL_1800,	0,		1,		0
			},
			{
				VCAMA_ID,	VOL_2800,	0,		1,		0
			},
			{
				VCAMD_ID,	VOL_1800,	0,		10,		0
			},
			{
				CAM_PDN_TYPE,	0,		0,		20,		0
			},
			{
				CAM_RST_TYPE,	1,		0,		20,		0
			},
			{
				CAM_INDEX_MAX,	1,		0,		10,		0		//结束
			}
		},
		//off
		{
		        {
				CAM_PDN_TYPE,	1,		0,		5,		0
			},
		        {
				CAM_RST_TYPE,	0,		0,		5,		0
			},
			{
				VCAMD_ID,	VOL_1800,	0,		5,		0
			},
			{
				VCAMA_ID,	VOL_1800,	0,		5,		0
			},
			{
				VCAMD2_ID,	VOL_2800,	0,		10,		0
			},
			{
				CAM_INDEX_MAX,	1,		0,		10,		0		//结束
			}
		}
	}
};
#endif


static CAM_POWER_List_STR g_camPowerInfo_list[] = {
	#if defined(OV8825_MIPI_RAW)
	{&g_camPowerInfo_ov8825,	CAM_MAIN_E,     0	},
	#endif
	#if defined(S5K4E1FX_MIPI_RAW)
	{&g_camPowerInfo_s5k4e1fx,	CAM_MAIN_E,     0	},
	#endif
	#if defined(HI542_MIPI_RAW)
	{&g_camPowerInfo_hi542,	 CAM_MAIN_E,    0	},
	#endif
	#if defined(HI253_YUV)
	{&g_camPowerInfo_hi253,	CAM_MAIN_E,     0	},
	#endif
	#if defined(OV9740_MIPI_YUV)
	{&g_camPowerInfo_ov9740,	CAM_SUB_E,          0	},
	#endif
	#if defined(SP2518_YUV)
	{&g_camPowerInfo_sp2518,	CAM_MAIN_E,     0	},
	#endif
	#if defined(OV7690_YUV)
	{&g_camPowerInfo_ov7690,	CAM_SUB_E,          0	},
	#endif
        #if defined(S5K4ECGX_MIPI_YUV)
        {&g_camPowerInfo_s5k4ecgx,  CAM_MAIN_E ,    0     },
        #endif
        #if defined(S5K8AAYX_YUV)
        {&g_camPowerInfo_s5k8aayx_yuv, CAM_MAIN_E,      0     },
        #endif
        #if defined(GC0309_YUV)
    {&g_camPowerInfo_gc0309yuv,         CAM_SUB_E,      0},
        #endif
         #if defined(GC0329_YUV)
    {&g_camPowerInfo_gc0329yuv,         CAM_SUB_E,      1},
        #endif
        #if defined(GC2035_YUV)
        {&g_camPowerInfo_gc2035yuv,         CAM_MAIN_E,   1},
        #endif
        #if defined(SOC5140_MIPI_YUV)
        {&g_camPowerInfo_soc5140mipi,       CAM_MAIN_E,     0},
        #endif
};
#define CAM_POWER_INFO_LIST_NUM		(sizeof(g_camPowerInfo_list)/sizeof(g_camPowerInfo_list[0]))	

#endif
