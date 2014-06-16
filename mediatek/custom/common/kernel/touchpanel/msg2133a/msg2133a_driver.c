#include "tpd.h"
#include <linux/interrupt.h>
#include <cust_eint.h>
#include <linux/i2c.h>
#include <linux/sched.h>
#include <linux/kthread.h>
#include <linux/rtpm_prio.h>
#include <linux/wait.h>
#include <linux/time.h>
#include <linux/delay.h>

#include "tpd_custom_msg2133a.h"

#include <mach/mt_pm_ldo.h>
#include <mach/mt_typedefs.h>
#include <mach/mt_boot.h>

#include "cust_gpio_usage.h"

/*Open OR Close Debug Info*/
#define __TPD_DEBUG__ 

/*Ctp Power Off In Sleep ? */


#ifdef TPD_PROXIMITY	// add by gpg

#define CTP_PROXIMITY_ESD_MEASURE  //Mark.li 2013-May-6 12:23:13   

#include <linux/hwmsensor.h>
#include <linux/hwmsen_dev.h>
#include <linux/wakelock.h>

#define APS_TAG                  "[ALS/PS] "
#define APS_FUN(f)               printk(KERN_INFO APS_TAG"%s\n", __FUNCTION__)
#define APS_ERR(fmt, args...)    printk(KERN_ERR  APS_TAG"%s %d : "fmt, __FUNCTION__, __LINE__, ##args)
#define APS_LOG(fmt, args...)    printk(KERN_INFO APS_TAG fmt, ##args)
#define APS_DBG(fmt, args...)    printk(KERN_INFO APS_TAG fmt, ##args)   

static DEFINE_MUTEX(msg2133_sensor_mutex);

static int g_bPsSensorOpen = 0;
static int g_nPsSensorDate = 1;
static int g_bSuspend = 0;
static struct wake_lock ps_lock;

static int msg2133_enable_ps(int enable);
static void tpd_initialize_ps_sensor_function();
#endif

 
extern struct tpd_device *tpd;

/*Use For Get CTP Data By I2C*/ 
static struct i2c_client *i2c_client = NULL;

/*Use For Firmware Update By I2C*/
//static struct i2c_client     *msg21xx_i2c_client = NULL;

//struct task_struct *thread = NULL;
 
static DECLARE_WAIT_QUEUE_HEAD(waiter);
//static DEFINE_MUTEX(i2c_access);

typedef struct
{
    u16 X;
    u16 Y;
} TouchPoint_t;

/*CTP Data Package*/
typedef struct
{
    u8 nTouchKeyMode;
    u8 nTouchKeyCode;
    u8 nFingerNum;
    TouchPoint_t Point[MAX_TOUCH_FINGER];
} TouchScreenInfo_t;

 
static void tpd_eint_interrupt_handler(void);
static struct work_struct    msg21xx_wq;

#ifdef TPD_HAVE_BUTTON 
static int tpd_keys_local[TPD_KEY_COUNT] = TPD_KEYS;
static int tpd_keys_dim_local[TPD_KEY_COUNT][4] = TPD_KEYS_DIM;
#endif
 
static int __devinit tpd_probe(struct i2c_client *client, const struct i2c_device_id *id);
static int tpd_detect (struct i2c_client *client, struct i2c_board_info *info);
static int __devexit tpd_remove(struct i2c_client *client);
static int touch_event_handler(void *unused);
 

static int tpd_flag = 0;
static int tpd_halt=0;
static int point_num = 0;
static int p_point_num = 0;



#define TPD_OK 0

 
 static const struct i2c_device_id msg2133_tpd_id[] = {{"msg2133a",0},{}};

 static struct i2c_board_info __initdata msg2133_i2c_tpd={ I2C_BOARD_INFO("msg2133a", (0x26))};
 
 
 static struct i2c_driver tpd_i2c_driver = {
  .driver = {
	 .name = "msg2133a",//.name = TPD_DEVICE,
//	 .owner = THIS_MODULE,
  },
  .probe = tpd_probe,
  .remove = __devexit_p(tpd_remove),
  .id_table = msg2133_tpd_id,
  .detect = tpd_detect,
//  .address_data = &addr_data,
 };
 //start for update firmware //msz   for update firmware 20121126
 static void msg2133_reset()
{
	mt_set_gpio_mode(GPIO_CTP_RST_PIN, GPIO_CTP_RST_PIN_M_GPIO);
    mt_set_gpio_dir(GPIO_CTP_RST_PIN, GPIO_DIR_OUT);
	mt_set_gpio_out(GPIO_CTP_RST_PIN, GPIO_OUT_ONE);
	msleep(10);
	mt_set_gpio_mode(GPIO_CTP_RST_PIN, GPIO_CTP_RST_PIN_M_GPIO);
    mt_set_gpio_dir(GPIO_CTP_RST_PIN, GPIO_DIR_OUT);
    mt_set_gpio_out(GPIO_CTP_RST_PIN, GPIO_OUT_ZERO);  
	msleep(150);
	TPD_DMESG(" msg2133 reset\n");
	mt_set_gpio_mode(GPIO_CTP_RST_PIN, GPIO_CTP_RST_PIN_M_GPIO);
    mt_set_gpio_dir(GPIO_CTP_RST_PIN, GPIO_DIR_OUT);
    mt_set_gpio_out(GPIO_CTP_RST_PIN, GPIO_OUT_ONE);
	msleep(150);
}
static int update_switch = 0;
#ifdef __FIRMWARE_UPDATE__
static u8 curr_ic_type = 0;
#define	CTP_ID_MSG21XX		1
#define	CTP_ID_MSG21XXA		2
static unsigned short curr_ic_major=0;
static unsigned short curr_ic_minor=0;
//#define ENABLE_AUTO_UPDATA
#ifdef ENABLE_AUTO_UPDATA
static unsigned short update_bin_major=0;
static unsigned short update_bin_minor=0;
#endif

/*adair:0777为打开apk升级功能，0664为关闭apk升级功能，无需将宏__FIRMWARE_UPDATE__关闭*/
#define CTP_AUTHORITY 0777//0664

#if 0
#define TP_DEBUG(format, ...)	printk(KERN_INFO "MSG2133_MSG21XXA_update_INFO ***" format "\n", ## __VA_ARGS__)
#else
#define TP_DEBUG(format, ...)
#endif
#if 1//adair:正式版本关闭
#define TP_DEBUG_ERR(format, ...)	printk(KERN_ERR "MSG2133_MSG21XXA_updating ***" format "\n", ## __VA_ARGS__)
#else
#define TP_DEBUG_ERR(format, ...)
#endif
static  char *fw_version;
static u8 temp[94][1024];
//u8  Fmr_Loader[1024];
u32 crc_tab[256];
static u8 g_dwiic_info_data[1024];   // Buffer for info data

static int FwDataCnt;
struct class *firmware_class;
struct device *firmware_cmd_dev;

#define N_BYTE_PER_TIME (8)//adair:1024的约数,根据平台修改
#define UPDATE_TIMES (1024/N_BYTE_PER_TIME)

#if 0//adair:根据平台不同选择不同位的i2c地址
#define FW_ADDR_MSG21XX   (0xC4)
#define FW_ADDR_MSG21XX_TP   (0x4C)
#define FW_UPDATE_ADDR_MSG21XX   (0x92)
#else
#define FW_ADDR_MSG21XX   (0xC4>>1)
#define FW_ADDR_MSG21XX_TP   (0x4C>>1)
#define FW_UPDATE_ADDR_MSG21XX   (0x92>>1)
#endif

/*adair:以下5个以Hal开头的函数需要根据平台修改*/
/*disable irq*/
static void HalDisableIrq(void)
{
	mt_eint_mask(CUST_EINT_TOUCH_PANEL_NUM);
    mt_eint_registration(CUST_EINT_TOUCH_PANEL_NUM, CUST_EINT_TOUCH_PANEL_POLARITY, NULL, 1);
}
/*enable irq*/
static void HalEnableIrq(void)
{
    mt_eint_registration(CUST_EINT_TOUCH_PANEL_NUM, CUST_EINT_TOUCH_PANEL_POLARITY, tpd_eint_interrupt_handler, 1);
	mt_eint_unmask(CUST_EINT_TOUCH_PANEL_NUM);
}
/*reset the chip*/
static void _HalTscrHWReset(void)
{
	msg2133_reset();
}
static void HalTscrCReadI2CSeq(u8 addr, u8* read_data, u16 size)
{
    int ret;
    i2c_client->addr = addr;
    ret = i2c_master_recv(i2c_client, read_data, size);
    i2c_client->addr = FW_ADDR_MSG21XX_TP;
    
    if(ret <= 0)
    {
		TP_DEBUG_ERR("HalTscrCReadI2CSeq error %d,addr = %d\n", ret,addr);
	}
}

static void HalTscrCDevWriteI2CSeq(u8 addr, u8* data, u16 size)
{
    int ret;
    i2c_client->addr = addr;
    ret = i2c_master_send(i2c_client, data, size);
    i2c_client->addr = FW_ADDR_MSG21XX_TP;

    if(ret <= 0)
    {
		TP_DEBUG_ERR("HalTscrCDevWriteI2CSeq error %d,addr = %d\n", ret,addr);
	}
}
#if 1//adair:以下无需修改
/*
static void Get_Chip_Version(void)
{
    printk("[%s]: Enter!\n", __func__);
    unsigned char dbbus_tx_data[3];
    unsigned char dbbus_rx_data[2];

    dbbus_tx_data[0] = 0x10;
    dbbus_tx_data[1] = 0x1E;
    dbbus_tx_data[2] = 0xCE;
    HalTscrCDevWriteI2CSeq(FW_ADDR_MSG21XX, &dbbus_tx_data[0], 3);
    HalTscrCReadI2CSeq(FW_ADDR_MSG21XX, &dbbus_rx_data[0], 2);
    if (dbbus_rx_data[1] == 0)
    {
        // it is Catch2
        TP_DEBUG(printk("*** Catch2 ***\n");)
        //FwVersion  = 2;// 2 means Catch2
    }
    else
    {
        // it is catch1
        TP_DEBUG(printk("*** Catch1 ***\n");)
        //FwVersion  = 1;// 1 means Catch1
    }

}
*/

static void dbbusDWIICEnterSerialDebugMode(void)
{
    u8 data[5];

    // Enter the Serial Debug Mode
    data[0] = 0x53;
    data[1] = 0x45;
    data[2] = 0x52;
    data[3] = 0x44;
    data[4] = 0x42;
    HalTscrCDevWriteI2CSeq(FW_ADDR_MSG21XX, data, 5);
}

static void dbbusDWIICStopMCU(void)
{
    u8 data[1];

    // Stop the MCU
    data[0] = 0x37;
    HalTscrCDevWriteI2CSeq(FW_ADDR_MSG21XX, data, 1);
}

static void dbbusDWIICIICUseBus(void)
{
    u8 data[1];

    // IIC Use Bus
    data[0] = 0x35;
    HalTscrCDevWriteI2CSeq(FW_ADDR_MSG21XX, data, 1);
}

static void dbbusDWIICIICReshape(void)
{
    u8 data[1];

    // IIC Re-shape
    data[0] = 0x71;
    HalTscrCDevWriteI2CSeq(FW_ADDR_MSG21XX, data, 1);
}

static void dbbusDWIICIICNotUseBus(void)
{
    u8 data[1];

    // IIC Not Use Bus
    data[0] = 0x34;
    HalTscrCDevWriteI2CSeq(FW_ADDR_MSG21XX, data, 1);
}

static void dbbusDWIICNotStopMCU(void)
{
    u8 data[1];

    // Not Stop the MCU
    data[0] = 0x36;
    HalTscrCDevWriteI2CSeq(FW_ADDR_MSG21XX, data, 1);
}

static void dbbusDWIICExitSerialDebugMode(void)
{
    u8 data[1];

    // Exit the Serial Debug Mode
    data[0] = 0x45;
    HalTscrCDevWriteI2CSeq(FW_ADDR_MSG21XX, data, 1);

    // Delay some interval to guard the next transaction
    udelay ( 150);//200 );        // delay about 0.2ms
}

static void drvISP_EntryIspMode(void)
{
    u8 bWriteData[5] =
    {
        0x4D, 0x53, 0x54, 0x41, 0x52
    };
	TP_DEBUG("\n******%s come in*******\n",__FUNCTION__);
    HalTscrCDevWriteI2CSeq(FW_UPDATE_ADDR_MSG21XX, bWriteData, 5);
    udelay ( 150 );//200 );        // delay about 0.1ms
}

static u8 drvISP_Read(u8 n, u8* pDataToRead)    //First it needs send 0x11 to notify we want to get flash data back.
{
    u8 Read_cmd = 0x11;
    unsigned char dbbus_rx_data[2] = {0};
    HalTscrCDevWriteI2CSeq(FW_UPDATE_ADDR_MSG21XX, &Read_cmd, 1);
    //msctpc_LoopDelay ( 1 );        // delay about 100us*****
    udelay( 800 );//200);
    if (n == 1)
    {
        HalTscrCReadI2CSeq(FW_UPDATE_ADDR_MSG21XX, &dbbus_rx_data[0], 2);
        *pDataToRead = dbbus_rx_data[0];
        TP_DEBUG("dbbus=%d,%d===drvISP_Read=====\n",dbbus_rx_data[0],dbbus_rx_data[1]);
  	}
    else
    {
        HalTscrCReadI2CSeq(FW_UPDATE_ADDR_MSG21XX, pDataToRead, n);
    }

    return 0;
}

static void drvISP_WriteEnable(void)
{
    u8 bWriteData[2] =
    {
        0x10, 0x06
    };
    u8 bWriteData1 = 0x12;
    HalTscrCDevWriteI2CSeq(FW_UPDATE_ADDR_MSG21XX, bWriteData, 2);
    udelay(150);//1.16
    HalTscrCDevWriteI2CSeq(FW_UPDATE_ADDR_MSG21XX, &bWriteData1, 1);
}


static void drvISP_ExitIspMode(void)
{
    u8 bWriteData = 0x24;
    HalTscrCDevWriteI2CSeq(FW_UPDATE_ADDR_MSG21XX, &bWriteData, 1);
    udelay( 150 );//200);
}

static u8 drvISP_ReadStatus(void)
{
    u8 bReadData = 0;
    u8 bWriteData[2] =
    {
        0x10, 0x05
    };
    u8 bWriteData1 = 0x12;

    HalTscrCDevWriteI2CSeq(FW_UPDATE_ADDR_MSG21XX, bWriteData, 2);
    //msctpc_LoopDelay ( 1 );        // delay about 100us*****
    udelay(150);//200);
    drvISP_Read(1, &bReadData);
    HalTscrCDevWriteI2CSeq(FW_UPDATE_ADDR_MSG21XX, &bWriteData1, 1);
    return bReadData;
}


static void drvISP_BlockErase(u32 addr)
{
    u8 bWriteData[5] = { 0x00, 0x00, 0x00, 0x00, 0x00 };
    u8 bWriteData1 = 0x12;
	TP_DEBUG("\n******%s come in*******\n",__FUNCTION__);
	u32 timeOutCount=0;
    drvISP_WriteEnable();

    //Enable write status register
    bWriteData[0] = 0x10;
    bWriteData[1] = 0x50;
    HalTscrCDevWriteI2CSeq(FW_UPDATE_ADDR_MSG21XX, bWriteData, 2);
    HalTscrCDevWriteI2CSeq(FW_UPDATE_ADDR_MSG21XX, &bWriteData1, 1);

    //Write Status
    bWriteData[0] = 0x10;
    bWriteData[1] = 0x01;
    bWriteData[2] = 0x00;
    HalTscrCDevWriteI2CSeq(FW_UPDATE_ADDR_MSG21XX, bWriteData, 3);
    HalTscrCDevWriteI2CSeq(FW_UPDATE_ADDR_MSG21XX, &bWriteData1, 1);

    //Write disable
    bWriteData[0] = 0x10;
    bWriteData[1] = 0x04;
    HalTscrCDevWriteI2CSeq(FW_UPDATE_ADDR_MSG21XX, bWriteData, 2);
    HalTscrCDevWriteI2CSeq(FW_UPDATE_ADDR_MSG21XX, &bWriteData1, 1);
	//msctpc_LoopDelay ( 1 );        // delay about 100us*****
	udelay(150);//200);
    timeOutCount=0;
	while ( ( drvISP_ReadStatus() & 0x01 ) == 0x01 )
	{
		timeOutCount++;
		if ( timeOutCount >= 100000 ) break; /* around 1 sec timeout */
	}
    drvISP_WriteEnable();

    bWriteData[0] = 0x10;
    bWriteData[1] = 0xC7;//0xD8;        //Block Erase
    //bWriteData[2] = ((addr >> 16) & 0xFF) ;
    //bWriteData[3] = ((addr >> 8) & 0xFF) ;
    //bWriteData[4] = (addr & 0xFF) ;
	HalTscrCDevWriteI2CSeq(FW_UPDATE_ADDR_MSG21XX, bWriteData, 2);
    //HalTscrCDevWriteI2CSeq(FW_UPDATE_ADDR_MSG21XX, &bWriteData, 5);
    HalTscrCDevWriteI2CSeq(FW_UPDATE_ADDR_MSG21XX, &bWriteData1, 1);
		//msctpc_LoopDelay ( 1 );        // delay about 100us*****
	udelay(150);//200);
	timeOutCount=0;
	while ( ( drvISP_ReadStatus() & 0x01 ) == 0x01 )
	{
		timeOutCount++;
		if ( timeOutCount >= 500000 ) break; /* around 5 sec timeout */
	}
}

static void drvISP_Program(u16 k, u8* pDataToWrite)
{
    u16 i = 0;
    u16 j = 0;
    //u16 n = 0;
    u8 TX_data[133];
    u8 bWriteData1 = 0x12;
    u32 addr = k * 1024;
		u32 timeOutCount=0;
    for (j = 0; j < 8; j++)   //128*8 cycle
    {
        TX_data[0] = 0x10;
        TX_data[1] = 0x02;// Page Program CMD
        TX_data[2] = (addr + 128 * j) >> 16;
        TX_data[3] = (addr + 128 * j) >> 8;
        TX_data[4] = (addr + 128 * j);
        for (i = 0; i < 128; i++)
        {
            TX_data[5 + i] = pDataToWrite[j * 128 + i];
        }
        //msctpc_LoopDelay ( 1 );        // delay about 100us*****
        udelay(150);//200);
       
        timeOutCount=0;
		while ( ( drvISP_ReadStatus() & 0x01 ) == 0x01 )
		{
			timeOutCount++;
			if ( timeOutCount >= 100000 ) break; /* around 1 sec timeout */
		}
  
        drvISP_WriteEnable();
        HalTscrCDevWriteI2CSeq(FW_UPDATE_ADDR_MSG21XX, TX_data, 133);   //write 133 byte per cycle
        HalTscrCDevWriteI2CSeq(FW_UPDATE_ADDR_MSG21XX, &bWriteData1, 1);
    }
}

static ssize_t firmware_update_show ( struct device *dev,
                                      struct device_attribute *attr, char *buf )
{
    return sprintf ( buf, "%s\n", fw_version );
}


static void drvISP_Verify ( u16 k, u8* pDataToVerify )
{
    u16 i = 0, j = 0;
    u8 bWriteData[5] ={ 0x10, 0x03, 0, 0, 0 };
    u8 RX_data[256];
    u8 bWriteData1 = 0x12;
    u32 addr = k * 1024;
    u8 index = 0;
    u32 timeOutCount;
    for ( j = 0; j < 8; j++ ) //128*8 cycle
    {
        bWriteData[2] = ( u8 ) ( ( addr + j * 128 ) >> 16 );
        bWriteData[3] = ( u8 ) ( ( addr + j * 128 ) >> 8 );
        bWriteData[4] = ( u8 ) ( addr + j * 128 );
        udelay ( 100 );        // delay about 100us*****

        timeOutCount = 0;
        while ( ( drvISP_ReadStatus() & 0x01 ) == 0x01 )
        {
            timeOutCount++;
            if ( timeOutCount >= 100000 ) break; /* around 1 sec timeout */
        }

        HalTscrCDevWriteI2CSeq ( FW_UPDATE_ADDR_MSG21XX, bWriteData, 5 ); //write read flash addr
        udelay ( 100 );        // delay about 100us*****
        drvISP_Read ( 128, RX_data );
        HalTscrCDevWriteI2CSeq ( FW_UPDATE_ADDR_MSG21XX, &bWriteData1, 1 ); //cmd end
        for ( i = 0; i < 128; i++ ) //log out if verify error
        {
            if ( ( RX_data[i] != 0 ) && index < 10 )
            {
                //TP_DEBUG("j=%d,RX_data[%d]=0x%x\n",j,i,RX_data[i]);
                index++;
            }
            if ( RX_data[i] != pDataToVerify[128 * j + i] )
            {
                TP_DEBUG ( "k=%d,j=%d,i=%d===============Update Firmware Error================", k, j, i );
            }
        }
    }
}

static void drvISP_ChipErase()
{
    u8 bWriteData[5] = { 0x00, 0x00, 0x00, 0x00, 0x00 };
    u8 bWriteData1 = 0x12;
    u32 timeOutCount = 0;
    drvISP_WriteEnable();

    //Enable write status register
    bWriteData[0] = 0x10;
    bWriteData[1] = 0x50;
    HalTscrCDevWriteI2CSeq ( FW_UPDATE_ADDR_MSG21XX, bWriteData, 2 );
    HalTscrCDevWriteI2CSeq ( FW_UPDATE_ADDR_MSG21XX, &bWriteData1, 1 );

    //Write Status
    bWriteData[0] = 0x10;
    bWriteData[1] = 0x01;
    bWriteData[2] = 0x00;
    HalTscrCDevWriteI2CSeq ( FW_UPDATE_ADDR_MSG21XX, bWriteData, 3 );
    HalTscrCDevWriteI2CSeq ( FW_UPDATE_ADDR_MSG21XX, &bWriteData1, 1 );

    //Write disable
    bWriteData[0] = 0x10;
    bWriteData[1] = 0x04;
    HalTscrCDevWriteI2CSeq ( FW_UPDATE_ADDR_MSG21XX, bWriteData, 2 );
    HalTscrCDevWriteI2CSeq ( FW_UPDATE_ADDR_MSG21XX, &bWriteData1, 1 );
    udelay ( 100 );        // delay about 100us*****
    timeOutCount = 0;
    while ( ( drvISP_ReadStatus() & 0x01 ) == 0x01 )
    {
        timeOutCount++;
        if ( timeOutCount >= 100000 ) break; /* around 1 sec timeout */
    }
    drvISP_WriteEnable();

    bWriteData[0] = 0x10;
    bWriteData[1] = 0xC7;

    HalTscrCDevWriteI2CSeq ( FW_UPDATE_ADDR_MSG21XX, bWriteData, 2 );
    HalTscrCDevWriteI2CSeq ( FW_UPDATE_ADDR_MSG21XX, &bWriteData1, 1 );
    udelay ( 100 );        // delay about 100us*****
    timeOutCount = 0;
    while ( ( drvISP_ReadStatus() & 0x01 ) == 0x01 )
    {
        timeOutCount++;
        if ( timeOutCount >= 500000 ) break; /* around 5 sec timeout */
    }
}

/* update the firmware part, used by apk*/
/*show the fw version*/

static ssize_t firmware_update_c2 ( struct device *dev,
                                    struct device_attribute *attr, const char *buf, size_t size )
{
    u8 i;
    u8 dbbus_tx_data[4];
    unsigned char dbbus_rx_data[2] = {0};

    // set FRO to 50M
    dbbus_tx_data[0] = 0x10;
    dbbus_tx_data[1] = 0x11;
    dbbus_tx_data[2] = 0xE2;
    HalTscrCDevWriteI2CSeq ( FW_ADDR_MSG21XX, dbbus_tx_data, 3 );
    dbbus_rx_data[0] = 0;
    dbbus_rx_data[1] = 0;
    HalTscrCReadI2CSeq ( FW_ADDR_MSG21XX, &dbbus_rx_data[0], 2 );
    TP_DEBUG ( "dbbus_rx_data[0]=0x%x", dbbus_rx_data[0] );
    dbbus_tx_data[3] = dbbus_rx_data[0] & 0xF7;  //Clear Bit 3
    HalTscrCDevWriteI2CSeq ( FW_ADDR_MSG21XX, dbbus_tx_data, 4 );

    // set MCU clock,SPI clock =FRO
    dbbus_tx_data[0] = 0x10;
    dbbus_tx_data[1] = 0x1E;
    dbbus_tx_data[2] = 0x22;
    dbbus_tx_data[3] = 0x00;
    HalTscrCDevWriteI2CSeq ( FW_ADDR_MSG21XX, dbbus_tx_data, 4 );

    dbbus_tx_data[0] = 0x10;
    dbbus_tx_data[1] = 0x1E;
    dbbus_tx_data[2] = 0x23;
    dbbus_tx_data[3] = 0x00;
    HalTscrCDevWriteI2CSeq ( FW_ADDR_MSG21XX, dbbus_tx_data, 4 );

    // Enable slave's ISP ECO mode
    dbbus_tx_data[0] = 0x10;
    dbbus_tx_data[1] = 0x08;
    dbbus_tx_data[2] = 0x0c;
    dbbus_tx_data[3] = 0x08;
    HalTscrCDevWriteI2CSeq ( FW_ADDR_MSG21XX, dbbus_tx_data, 4 );

    // Enable SPI Pad
    dbbus_tx_data[0] = 0x10;
    dbbus_tx_data[1] = 0x1E;
    dbbus_tx_data[2] = 0x02;
    HalTscrCDevWriteI2CSeq ( FW_ADDR_MSG21XX, dbbus_tx_data, 3 );
    HalTscrCReadI2CSeq ( FW_ADDR_MSG21XX, &dbbus_rx_data[0], 2 );
    TP_DEBUG ( "dbbus_rx_data[0]=0x%x", dbbus_rx_data[0] );
    dbbus_tx_data[3] = ( dbbus_rx_data[0] | 0x20 ); //Set Bit 5
    HalTscrCDevWriteI2CSeq ( FW_ADDR_MSG21XX, dbbus_tx_data, 4 );

    // WP overwrite
    dbbus_tx_data[0] = 0x10;
    dbbus_tx_data[1] = 0x1E;
    dbbus_tx_data[2] = 0x0E;
    dbbus_tx_data[3] = 0x02;
    HalTscrCDevWriteI2CSeq ( FW_ADDR_MSG21XX, dbbus_tx_data, 4 );

    // set pin high
    dbbus_tx_data[0] = 0x10;
    dbbus_tx_data[1] = 0x1E;
    dbbus_tx_data[2] = 0x10;
    dbbus_tx_data[3] = 0x08;
    HalTscrCDevWriteI2CSeq ( FW_ADDR_MSG21XX, dbbus_tx_data, 4 );

    dbbusDWIICIICNotUseBus();
    dbbusDWIICNotStopMCU();
    dbbusDWIICExitSerialDebugMode();

    drvISP_EntryIspMode();
    drvISP_ChipErase();
    _HalTscrHWReset();
    mdelay ( 300 );

    // Program and Verify
    dbbusDWIICEnterSerialDebugMode();
    dbbusDWIICStopMCU();
    dbbusDWIICIICUseBus();
    dbbusDWIICIICReshape();

    // Disable the Watchdog
    dbbus_tx_data[0] = 0x10;
    dbbus_tx_data[1] = 0x3C;
    dbbus_tx_data[2] = 0x60;
    dbbus_tx_data[3] = 0x55;
    HalTscrCDevWriteI2CSeq ( FW_ADDR_MSG21XX, dbbus_tx_data, 4 );
    dbbus_tx_data[0] = 0x10;
    dbbus_tx_data[1] = 0x3C;
    dbbus_tx_data[2] = 0x61;
    dbbus_tx_data[3] = 0xAA;
    HalTscrCDevWriteI2CSeq ( FW_ADDR_MSG21XX, dbbus_tx_data, 4 );

    //Stop MCU
    dbbus_tx_data[0] = 0x10;
    dbbus_tx_data[1] = 0x0F;
    dbbus_tx_data[2] = 0xE6;
    dbbus_tx_data[3] = 0x01;
    HalTscrCDevWriteI2CSeq ( FW_ADDR_MSG21XX, dbbus_tx_data, 4 );

    // set FRO to 50M
    dbbus_tx_data[0] = 0x10;
    dbbus_tx_data[1] = 0x11;
    dbbus_tx_data[2] = 0xE2;
    HalTscrCDevWriteI2CSeq ( FW_ADDR_MSG21XX, dbbus_tx_data, 3 );
    dbbus_rx_data[0] = 0;
    dbbus_rx_data[1] = 0;
    HalTscrCReadI2CSeq ( FW_ADDR_MSG21XX, &dbbus_rx_data[0], 2 );
    TP_DEBUG ( "dbbus_rx_data[0]=0x%x", dbbus_rx_data[0] );
    dbbus_tx_data[3] = dbbus_rx_data[0] & 0xF7;  //Clear Bit 3
    HalTscrCDevWriteI2CSeq ( FW_ADDR_MSG21XX, dbbus_tx_data, 4 );

    // set MCU clock,SPI clock =FRO
    dbbus_tx_data[0] = 0x10;
    dbbus_tx_data[1] = 0x1E;
    dbbus_tx_data[2] = 0x22;
    dbbus_tx_data[3] = 0x00;
    HalTscrCDevWriteI2CSeq ( FW_ADDR_MSG21XX, dbbus_tx_data, 4 );

    dbbus_tx_data[0] = 0x10;
    dbbus_tx_data[1] = 0x1E;
    dbbus_tx_data[2] = 0x23;
    dbbus_tx_data[3] = 0x00;
    HalTscrCDevWriteI2CSeq ( FW_ADDR_MSG21XX, dbbus_tx_data, 4 );

    // Enable slave's ISP ECO mode
    dbbus_tx_data[0] = 0x10;
    dbbus_tx_data[1] = 0x08;
    dbbus_tx_data[2] = 0x0c;
    dbbus_tx_data[3] = 0x08;
    HalTscrCDevWriteI2CSeq ( FW_ADDR_MSG21XX, dbbus_tx_data, 4 );

    // Enable SPI Pad
    dbbus_tx_data[0] = 0x10;
    dbbus_tx_data[1] = 0x1E;
    dbbus_tx_data[2] = 0x02;
    HalTscrCDevWriteI2CSeq ( FW_ADDR_MSG21XX, dbbus_tx_data, 3 );
    HalTscrCReadI2CSeq ( FW_ADDR_MSG21XX, &dbbus_rx_data[0], 2 );
    TP_DEBUG ( "dbbus_rx_data[0]=0x%x", dbbus_rx_data[0] );
    dbbus_tx_data[3] = ( dbbus_rx_data[0] | 0x20 ); //Set Bit 5
    HalTscrCDevWriteI2CSeq ( FW_ADDR_MSG21XX, dbbus_tx_data, 4 );

    // WP overwrite
    dbbus_tx_data[0] = 0x10;
    dbbus_tx_data[1] = 0x1E;
    dbbus_tx_data[2] = 0x0E;
    dbbus_tx_data[3] = 0x02;
    HalTscrCDevWriteI2CSeq ( FW_ADDR_MSG21XX, dbbus_tx_data, 4 );

    // set pin high
    dbbus_tx_data[0] = 0x10;
    dbbus_tx_data[1] = 0x1E;
    dbbus_tx_data[2] = 0x10;
    dbbus_tx_data[3] = 0x08;
    HalTscrCDevWriteI2CSeq ( FW_ADDR_MSG21XX, dbbus_tx_data, 4 );

    dbbusDWIICIICNotUseBus();
    dbbusDWIICNotStopMCU();
    dbbusDWIICExitSerialDebugMode();

    ///////////////////////////////////////
    // Start to load firmware
    ///////////////////////////////////////
    drvISP_EntryIspMode();

    for ( i = 0; i < 94; i++ ) // total  94 KB : 1 byte per R/W
    {
        drvISP_Program ( i, temp[i] ); // program to slave's flash
        drvISP_Verify ( i, temp[i] ); //verify data
    }
    TP_DEBUG_ERR ( "update_C2 OK\n" );
    drvISP_ExitIspMode();
    _HalTscrHWReset();
    FwDataCnt = 0;
    HalEnableIrq();	
    return size;
}

static u32 Reflect ( u32 ref, char ch ) //unsigned int Reflect(unsigned int ref, char ch)
{
    u32 value = 0;
    u32 i = 0;

    for ( i = 1; i < ( ch + 1 ); i++ )
    {
        if ( ref & 1 )
        {
            value |= 1 << ( ch - i );
        }
        ref >>= 1;
    }
    return value;
}

u32 Get_CRC ( u32 text, u32 prevCRC, u32 *crc32_table )
{
    u32  ulCRC = prevCRC;
	ulCRC = ( ulCRC >> 8 ) ^ crc32_table[ ( ulCRC & 0xFF ) ^ text];
    return ulCRC ;
}
static void Init_CRC32_Table ( u32 *crc32_table )
{
    u32 magicnumber = 0x04c11db7;
    u32 i = 0, j;

    for ( i = 0; i <= 0xFF; i++ )
    {
        crc32_table[i] = Reflect ( i, 8 ) << 24;
        for ( j = 0; j < 8; j++ )
        {
            crc32_table[i] = ( crc32_table[i] << 1 ) ^ ( crc32_table[i] & ( 0x80000000L ) ? magicnumber : 0 );
        }
        crc32_table[i] = Reflect ( crc32_table[i], 32 );
    }
}

typedef enum
{
	EMEM_ALL = 0,
	EMEM_MAIN,
	EMEM_INFO,
} EMEM_TYPE_t;

static void drvDB_WriteReg8Bit ( u8 bank, u8 addr, u8 data )
{
    u8 tx_data[4] = {0x10, bank, addr, data};
    HalTscrCDevWriteI2CSeq ( FW_ADDR_MSG21XX, tx_data, 4 );
}

static void drvDB_WriteReg ( u8 bank, u8 addr, u16 data )
{
    u8 tx_data[5] = {0x10, bank, addr, data & 0xFF, data >> 8};
    HalTscrCDevWriteI2CSeq ( FW_ADDR_MSG21XX, tx_data, 5 );
}

static unsigned short drvDB_ReadReg ( u8 bank, u8 addr )
{
    u8 tx_data[3] = {0x10, bank, addr};
    u8 rx_data[2] = {0};

    HalTscrCDevWriteI2CSeq ( FW_ADDR_MSG21XX, tx_data, 3 );
    HalTscrCReadI2CSeq ( FW_ADDR_MSG21XX, &rx_data[0], 2 );
    return ( rx_data[1] << 8 | rx_data[0] );
}

static int drvTP_erase_emem_c32 ( void )
{
    /////////////////////////
    //Erase  all
    /////////////////////////
    
    //enter gpio mode
    drvDB_WriteReg ( 0x16, 0x1E, 0xBEAF );

    // before gpio mode, set the control pin as the orginal status
    drvDB_WriteReg ( 0x16, 0x08, 0x0000 );
    drvDB_WriteReg8Bit ( 0x16, 0x0E, 0x10 );
    mdelay ( 10 ); //MCR_CLBK_DEBUG_DELAY ( 10, MCU_LOOP_DELAY_COUNT_MS );

    // ptrim = 1, h'04[2]
    drvDB_WriteReg8Bit ( 0x16, 0x08, 0x04 );
    drvDB_WriteReg8Bit ( 0x16, 0x0E, 0x10 );
    mdelay ( 10 ); //MCR_CLBK_DEBUG_DELAY ( 10, MCU_LOOP_DELAY_COUNT_MS );

    // ptm = 6, h'04[12:14] = b'110
    drvDB_WriteReg8Bit ( 0x16, 0x09, 0x60 );
    drvDB_WriteReg8Bit ( 0x16, 0x0E, 0x10 );

    // pmasi = 1, h'04[6]
    drvDB_WriteReg8Bit ( 0x16, 0x08, 0x44 );
    // pce = 1, h'04[11]
    drvDB_WriteReg8Bit ( 0x16, 0x09, 0x68 );
    // perase = 1, h'04[7]
    drvDB_WriteReg8Bit ( 0x16, 0x08, 0xC4 );
    // pnvstr = 1, h'04[5]
    drvDB_WriteReg8Bit ( 0x16, 0x08, 0xE4 );
    // pwe = 1, h'04[9]
    drvDB_WriteReg8Bit ( 0x16, 0x09, 0x6A );
    // trigger gpio load
    drvDB_WriteReg8Bit ( 0x16, 0x0E, 0x10 );

    return ( 1 );
}

static ssize_t firmware_update_c32 ( struct device *dev, struct device_attribute *attr,
                                     const char *buf, size_t size,  EMEM_TYPE_t emem_type )
{
    u8  dbbus_tx_data[4];
    u8  dbbus_rx_data[2] = {0};
      // Buffer for slave's firmware

    u32 i, j;
    u32 crc_main, crc_main_tp;
    u32 crc_info, crc_info_tp;
    u16 reg_data = 0;

    crc_main = 0xffffffff;
    crc_info = 0xffffffff;

#if 1
    /////////////////////////
    // Erase  all
    /////////////////////////
    drvTP_erase_emem_c32();
    mdelay ( 1000 ); //MCR_CLBK_DEBUG_DELAY ( 1000, MCU_LOOP_DELAY_COUNT_MS );

    //ResetSlave();
    _HalTscrHWReset();
    //drvDB_EnterDBBUS();
    dbbusDWIICEnterSerialDebugMode();
    dbbusDWIICStopMCU();
    dbbusDWIICIICUseBus();
    dbbusDWIICIICReshape();
    mdelay ( 300 );

    // Reset Watchdog
    drvDB_WriteReg8Bit ( 0x3C, 0x60, 0x55 );
    drvDB_WriteReg8Bit ( 0x3C, 0x61, 0xAA );

    /////////////////////////
    // Program
    /////////////////////////

    //polling 0x3CE4 is 0x1C70
    do
    {
        reg_data = drvDB_ReadReg ( 0x3C, 0xE4 );
    }
    while ( reg_data != 0x1C70 );


    drvDB_WriteReg ( 0x3C, 0xE4, 0xE38F );  // for all-blocks

    //polling 0x3CE4 is 0x2F43
    do
    {
        reg_data = drvDB_ReadReg ( 0x3C, 0xE4 );
    }
    while ( reg_data != 0x2F43 );


    //calculate CRC 32
    Init_CRC32_Table ( &crc_tab[0] );

    for ( i = 0; i < 33; i++ ) // total  33 KB : 2 byte per R/W
    {
        if ( i < 32 )   //emem_main
        {
            if ( i == 31 )
            {
                temp[i][1014] = 0x5A; //Fmr_Loader[1014]=0x5A;
                temp[i][1015] = 0xA5; //Fmr_Loader[1015]=0xA5;

                for ( j = 0; j < 1016; j++ )
                {
                    //crc_main=Get_CRC(Fmr_Loader[j],crc_main,&crc_tab[0]);
                    crc_main = Get_CRC ( temp[i][j], crc_main, &crc_tab[0] );
                }
            }
            else
            {
                for ( j = 0; j < 1024; j++ )
                {
                    //crc_main=Get_CRC(Fmr_Loader[j],crc_main,&crc_tab[0]);
                    crc_main = Get_CRC ( temp[i][j], crc_main, &crc_tab[0] );
                }
            }
        }
        else  // emem_info
        {
            for ( j = 0; j < 1024; j++ )
            {
                //crc_info=Get_CRC(Fmr_Loader[j],crc_info,&crc_tab[0]);
                crc_info = Get_CRC ( temp[i][j], crc_info, &crc_tab[0] );
            }
        }

        //drvDWIIC_MasterTransmit( DWIIC_MODE_DWIIC_ID, 1024, Fmr_Loader );
        HalTscrCDevWriteI2CSeq ( FW_ADDR_MSG21XX_TP, temp[i], 1024 );

        // polling 0x3CE4 is 0xD0BC
        do
        {
            reg_data = drvDB_ReadReg ( 0x3C, 0xE4 );
        }
        while ( reg_data != 0xD0BC );

        drvDB_WriteReg ( 0x3C, 0xE4, 0x2F43 );
    }

    //write file done
    drvDB_WriteReg ( 0x3C, 0xE4, 0x1380 );

    mdelay ( 10 ); //MCR_CLBK_DEBUG_DELAY ( 10, MCU_LOOP_DELAY_COUNT_MS );
    // polling 0x3CE4 is 0x9432
    do
    {
        reg_data = drvDB_ReadReg ( 0x3C, 0xE4 );
    }
    while ( reg_data != 0x9432 );

    crc_main = crc_main ^ 0xffffffff;
    crc_info = crc_info ^ 0xffffffff;

    // CRC Main from TP
    crc_main_tp = drvDB_ReadReg ( 0x3C, 0x80 );
    crc_main_tp = ( crc_main_tp << 16 ) | drvDB_ReadReg ( 0x3C, 0x82 );
 
    //CRC Info from TP
    crc_info_tp = drvDB_ReadReg ( 0x3C, 0xA0 );
    crc_info_tp = ( crc_info_tp << 16 ) | drvDB_ReadReg ( 0x3C, 0xA2 );

    TP_DEBUG ( "crc_main=0x%x, crc_info=0x%x, crc_main_tp=0x%x, crc_info_tp=0x%x\n",
               crc_main, crc_info, crc_main_tp, crc_info_tp );

    //drvDB_ExitDBBUS();
    if ( ( crc_main_tp != crc_main ) || ( crc_info_tp != crc_info ) )
    {
        TP_DEBUG_ERR ( "update_C32 FAILED\n" );
		_HalTscrHWReset();
        FwDataCnt = 0;
    	HalEnableIrq();		
        return ( 0 );
    }

    TP_DEBUG_ERR ( "update_C32 OK\n" );
	_HalTscrHWReset();
    FwDataCnt = 0;
	HalEnableIrq();	

    return size;
#endif
}

static int drvTP_erase_emem_c33 ( EMEM_TYPE_t emem_type )
{
    // stop mcu
    drvDB_WriteReg ( 0x0F, 0xE6, 0x0001 );

    //disable watch dog
    drvDB_WriteReg8Bit ( 0x3C, 0x60, 0x55 );
    drvDB_WriteReg8Bit ( 0x3C, 0x61, 0xAA );

    // set PROGRAM password
    drvDB_WriteReg8Bit ( 0x16, 0x1A, 0xBA );
    drvDB_WriteReg8Bit ( 0x16, 0x1B, 0xAB );

    //proto.MstarWriteReg(F1.loopDevice, 0x1618, 0x80);
    drvDB_WriteReg8Bit ( 0x16, 0x18, 0x80 );

    if ( emem_type == EMEM_ALL )
    {
        drvDB_WriteReg8Bit ( 0x16, 0x08, 0x10 ); //mark
    }

    drvDB_WriteReg8Bit ( 0x16, 0x18, 0x40 );
    mdelay ( 10 );

    drvDB_WriteReg8Bit ( 0x16, 0x18, 0x80 );

    // erase trigger
    if ( emem_type == EMEM_MAIN )
    {
        drvDB_WriteReg8Bit ( 0x16, 0x0E, 0x04 ); //erase main
    }
    else
    {
        drvDB_WriteReg8Bit ( 0x16, 0x0E, 0x08 ); //erase all block
    }

    return ( 1 );
}

static int drvTP_read_emem_dbbus_c33 ( EMEM_TYPE_t emem_type, u16 addr, size_t size, u8 *p, size_t set_pce_high )
{
    u32 i;

    // Set the starting address ( must before enabling burst mode and enter riu mode )
    drvDB_WriteReg ( 0x16, 0x00, addr );

    // Enable the burst mode ( must before enter riu mode )
    drvDB_WriteReg ( 0x16, 0x0C, drvDB_ReadReg ( 0x16, 0x0C ) | 0x0001 );

    // Set the RIU password
    drvDB_WriteReg ( 0x16, 0x1A, 0xABBA );

    // Enable the information block if pifren is HIGH
    if ( emem_type == EMEM_INFO )
    {
        // Clear the PCE
        drvDB_WriteReg ( 0x16, 0x18, drvDB_ReadReg ( 0x16, 0x18 ) | 0x0080 );
        mdelay ( 10 );

        // Set the PIFREN to be HIGH
        drvDB_WriteReg ( 0x16, 0x08, 0x0010 );
    }

    // Set the PCE to be HIGH
    drvDB_WriteReg ( 0x16, 0x18, drvDB_ReadReg ( 0x16, 0x18 ) | 0x0040 );
    mdelay ( 10 );

    // Wait pce becomes 1 ( read data ready )
    while ( ( drvDB_ReadReg ( 0x16, 0x10 ) & 0x0004 ) != 0x0004 );

    for ( i = 0; i < size; i += 4 )
    {
        // Fire the FASTREAD command
        drvDB_WriteReg ( 0x16, 0x0E, drvDB_ReadReg ( 0x16, 0x0E ) | 0x0001 );

        // Wait the operation is done
        while ( ( drvDB_ReadReg ( 0x16, 0x10 ) & 0x0001 ) != 0x0001 );

        p[i + 0] = drvDB_ReadReg ( 0x16, 0x04 ) & 0xFF;
        p[i + 1] = ( drvDB_ReadReg ( 0x16, 0x04 ) >> 8 ) & 0xFF;
        p[i + 2] = drvDB_ReadReg ( 0x16, 0x06 ) & 0xFF;
        p[i + 3] = ( drvDB_ReadReg ( 0x16, 0x06 ) >> 8 ) & 0xFF;
    }

    // Disable the burst mode
    drvDB_WriteReg ( 0x16, 0x0C, drvDB_ReadReg ( 0x16, 0x0C ) & ( ~0x0001 ) );

    // Clear the starting address
    drvDB_WriteReg ( 0x16, 0x00, 0x0000 );

    //Always return to main block
    if ( emem_type == EMEM_INFO )
    {
        // Clear the PCE before change block
        drvDB_WriteReg ( 0x16, 0x18, drvDB_ReadReg ( 0x16, 0x18 ) | 0x0080 );
        mdelay ( 10 );
        // Set the PIFREN to be LOW
        drvDB_WriteReg ( 0x16, 0x08, drvDB_ReadReg ( 0x16, 0x08 ) & ( ~0x0010 ) );

        drvDB_WriteReg ( 0x16, 0x18, drvDB_ReadReg ( 0x16, 0x18 ) | 0x0040 );
        while ( ( drvDB_ReadReg ( 0x16, 0x10 ) & 0x0004 ) != 0x0004 );
    }

    // Clear the RIU password
    drvDB_WriteReg ( 0x16, 0x1A, 0x0000 );

    if ( set_pce_high )
    {
        // Set the PCE to be HIGH before jumping back to e-flash codes
        drvDB_WriteReg ( 0x16, 0x18, drvDB_ReadReg ( 0x16, 0x18 ) | 0x0040 );
        while ( ( drvDB_ReadReg ( 0x16, 0x10 ) & 0x0004 ) != 0x0004 );
    }

    return ( 1 );
}


static int drvTP_read_info_dwiic_c33 ( void )
{
    u8  dwiic_tx_data[5];
    u8  dwiic_rx_data[4];
    u16 reg_data=0;
    mdelay ( 300 );

    // Stop Watchdog
    drvDB_WriteReg8Bit ( 0x3C, 0x60, 0x55 );
    drvDB_WriteReg8Bit ( 0x3C, 0x61, 0xAA );

    drvDB_WriteReg ( 0x3C, 0xE4, 0xA4AB );

	drvDB_WriteReg ( 0x1E, 0x04, 0x7d60 );

    // TP SW reset
    drvDB_WriteReg ( 0x1E, 0x04, 0x829F );
	mdelay ( 1 );
    dwiic_tx_data[0] = 0x10;
    dwiic_tx_data[1] = 0x0F;
    dwiic_tx_data[2] = 0xE6;
    dwiic_tx_data[3] = 0x00;
    HalTscrCDevWriteI2CSeq ( FW_ADDR_MSG21XX, dwiic_tx_data, 4 );	
    mdelay ( 100 );
TP_DEBUG_ERR("2222222222");
    do{
        reg_data = drvDB_ReadReg ( 0x3C, 0xE4 );
    }
    while ( reg_data != 0x5B58 );
TP_DEBUG_ERR("33333333333333");
    dwiic_tx_data[0] = 0x72;
    dwiic_tx_data[1] = 0x80;
    dwiic_tx_data[2] = 0x00;
    dwiic_tx_data[3] = 0x04;
    dwiic_tx_data[4] = 0x00;
    HalTscrCDevWriteI2CSeq ( FW_ADDR_MSG21XX_TP , dwiic_tx_data, 5 );
TP_DEBUG_ERR("4444444444444");
    mdelay ( 50 );

    // recive info data
    //HalTscrCReadI2CSeq ( FW_ADDR_MSG21XX_TP, &g_dwiic_info_data[0], 1024 );
    HalTscrCReadI2CSeq ( FW_ADDR_MSG21XX_TP, &g_dwiic_info_data[0], 8 );
    TP_DEBUG_ERR("55555555555555");
    return ( 1 );
}

static int drvTP_info_updata_C33 ( u16 start_index, u8 *data, u16 size )
{
    // size != 0, start_index+size !> 1024
    u16 i;
    for ( i = 0; i < size; i++ )
    {
        g_dwiic_info_data[start_index] = * ( data + i );
        start_index++;
    }
    return ( 1 );
}

static ssize_t firmware_update_c33 ( struct device *dev, struct device_attribute *attr,
                                     const char *buf, size_t size, EMEM_TYPE_t emem_type )
{
    u8  dbbus_tx_data[4];
    u8  dbbus_rx_data[2] = {0};
    u8  life_counter[2];
    u32 i, j;
    u32 crc_main, crc_main_tp;
    u32 crc_info, crc_info_tp;
  
    int update_pass = 1;
    u16 reg_data = 0;

    crc_main = 0xffffffff;
    crc_info = 0xffffffff;
    TP_DEBUG_ERR("111111111111");
    drvTP_read_info_dwiic_c33();
	
    if ( 0/*g_dwiic_info_data[0] == 'M' && g_dwiic_info_data[1] == 'S' && g_dwiic_info_data[2] == 'T' && g_dwiic_info_data[3] == 'A' && g_dwiic_info_data[4] == 'R' && g_dwiic_info_data[5] == 'T' && g_dwiic_info_data[6] == 'P' && g_dwiic_info_data[7] == 'C' */)
    {
        // updata FW Version
        //drvTP_info_updata_C33 ( 8, &temp[32][8], 5 );

		g_dwiic_info_data[8]=temp[32][8];
		g_dwiic_info_data[9]=temp[32][9];
		g_dwiic_info_data[10]=temp[32][10];
		g_dwiic_info_data[11]=temp[32][11];
        // updata life counter
        life_counter[1] = (( ( (g_dwiic_info_data[13] << 8 ) | g_dwiic_info_data[12]) + 1 ) >> 8 ) & 0xFF;
        life_counter[0] = ( ( (g_dwiic_info_data[13] << 8 ) | g_dwiic_info_data[12]) + 1 ) & 0xFF;
		g_dwiic_info_data[12]=life_counter[0];
		g_dwiic_info_data[13]=life_counter[1];
        //drvTP_info_updata_C33 ( 10, &life_counter[0], 3 );
        drvDB_WriteReg ( 0x3C, 0xE4, 0x78C5 );
		drvDB_WriteReg ( 0x1E, 0x04, 0x7d60 );
        // TP SW reset
        drvDB_WriteReg ( 0x1E, 0x04, 0x829F );

        mdelay ( 50 );
        TP_DEBUG_ERR("666666666666");
        //polling 0x3CE4 is 0x2F43
        do
        {
            reg_data = drvDB_ReadReg ( 0x3C, 0xE4 );
        }
        while ( reg_data != 0x2F43 );
        TP_DEBUG_ERR("777777777777");
        // transmit lk info data
        HalTscrCDevWriteI2CSeq ( FW_ADDR_MSG21XX_TP , &g_dwiic_info_data[0], 1024 );
        TP_DEBUG_ERR("88888888888");
        //polling 0x3CE4 is 0xD0BC
        do
        {
            reg_data = drvDB_ReadReg ( 0x3C, 0xE4 );
        }
        while ( reg_data != 0xD0BC );
        TP_DEBUG_ERR("9999999999999");
    }

    //erase main
    TP_DEBUG_ERR("aaaaaaaaaaa");
    drvTP_erase_emem_c33 ( EMEM_MAIN );
    mdelay ( 1000 );

    //ResetSlave();
    _HalTscrHWReset();

    //drvDB_EnterDBBUS();
    dbbusDWIICEnterSerialDebugMode();
    dbbusDWIICStopMCU();
    dbbusDWIICIICUseBus();
    dbbusDWIICIICReshape();
    mdelay ( 300 );

    /////////////////////////
    // Program
    /////////////////////////

    //polling 0x3CE4 is 0x1C70
    if ( ( emem_type == EMEM_ALL ) || ( emem_type == EMEM_MAIN ) )
    {
        do
        {
            reg_data = drvDB_ReadReg ( 0x3C, 0xE4 );
        }
        while ( reg_data != 0x1C70 );
    }

    switch ( emem_type )
    {
        case EMEM_ALL:
            drvDB_WriteReg ( 0x3C, 0xE4, 0xE38F );  // for all-blocks
            break;
        case EMEM_MAIN:
            drvDB_WriteReg ( 0x3C, 0xE4, 0x7731 );  // for main block
            break;
        case EMEM_INFO:
            drvDB_WriteReg ( 0x3C, 0xE4, 0x7731 );  // for info block

            drvDB_WriteReg8Bit ( 0x0F, 0xE6, 0x01 );

            drvDB_WriteReg8Bit ( 0x3C, 0xE4, 0xC5 ); //
            drvDB_WriteReg8Bit ( 0x3C, 0xE5, 0x78 ); //

            drvDB_WriteReg8Bit ( 0x1E, 0x04, 0x9F );
            drvDB_WriteReg8Bit ( 0x1E, 0x05, 0x82 );

            drvDB_WriteReg8Bit ( 0x0F, 0xE6, 0x00 );
            mdelay ( 100 );
            break;
    }
TP_DEBUG_ERR("bbbbbbbbbbbbbb");
    // polling 0x3CE4 is 0x2F43
    do
    {
        reg_data = drvDB_ReadReg ( 0x3C, 0xE4 );
    }
    while ( reg_data != 0x2F43 );
TP_DEBUG_ERR("ccccccccccccc");
    // calculate CRC 32
    Init_CRC32_Table ( &crc_tab[0] );

    for ( i = 0; i < 33; i++ ) // total  33 KB : 2 byte per R/W
    {
        if ( emem_type == EMEM_INFO )
			i = 32;

        if ( i < 32 )   //emem_main
        {
            if ( i == 31 )
            {
                temp[i][1014] = 0x5A; //Fmr_Loader[1014]=0x5A;
                temp[i][1015] = 0xA5; //Fmr_Loader[1015]=0xA5;

                for ( j = 0; j < 1016; j++ )
                {
                    //crc_main=Get_CRC(Fmr_Loader[j],crc_main,&crc_tab[0]);
                    crc_main = Get_CRC ( temp[i][j], crc_main, &crc_tab[0] );
                }
            }
            else
            {
                for ( j = 0; j < 1024; j++ )
                {
                    //crc_main=Get_CRC(Fmr_Loader[j],crc_main,&crc_tab[0]);
                    crc_main = Get_CRC ( temp[i][j], crc_main, &crc_tab[0] );
                }
            }
        }
        else  //emem_info
        {
            for ( j = 0; j < 1024; j++ )
            {
                //crc_info=Get_CRC(Fmr_Loader[j],crc_info,&crc_tab[0]);
                crc_info = Get_CRC ( g_dwiic_info_data[j], crc_info, &crc_tab[0] );
            }
            if ( emem_type == EMEM_MAIN ) break;
        }
        //drvDWIIC_MasterTransmit( DWIIC_MODE_DWIIC_ID, 1024, Fmr_Loader );
        TP_DEBUG_ERR("dddddddddddddd");
        #if 1
        {
            u32 n = 0;
            for(n=0;n<UPDATE_TIMES;n++)
            {
               // TP_DEBUG_ERR("i=%d,n=%d",i,n);
                HalTscrCDevWriteI2CSeq ( FW_ADDR_MSG21XX_TP, temp[i]+n*N_BYTE_PER_TIME, N_BYTE_PER_TIME );
            }
        }
        #else
        HalTscrCDevWriteI2CSeq ( FW_ADDR_MSG21XX_TP, temp[i], 1024 );
        #endif
        TP_DEBUG_ERR("eeeeeeeeeeee");
        // polling 0x3CE4 is 0xD0BC
        do
        {
            reg_data = drvDB_ReadReg ( 0x3C, 0xE4 );
        }
        while ( reg_data != 0xD0BC );
        TP_DEBUG_ERR("ffffffffffffff");
        drvDB_WriteReg ( 0x3C, 0xE4, 0x2F43 );
    }
        TP_DEBUG_ERR("ggggggggg");
    if ( ( emem_type == EMEM_ALL ) || ( emem_type == EMEM_MAIN ) )
    {
        // write file done and check crc
        drvDB_WriteReg ( 0x3C, 0xE4, 0x1380 );
        TP_DEBUG_ERR("hhhhhhhhhhhhhh");
    }
    mdelay ( 10 ); //MCR_CLBK_DEBUG_DELAY ( 10, MCU_LOOP_DELAY_COUNT_MS );

    if ( ( emem_type == EMEM_ALL ) || ( emem_type == EMEM_MAIN ) )
    {
        // polling 0x3CE4 is 0x9432
        TP_DEBUG_ERR("iiiiiiiiii");
        do
        {
            reg_data = drvDB_ReadReg ( 0x3C, 0xE4 );
        }while ( reg_data != 0x9432 );
        TP_DEBUG_ERR("jjjjjjjjjjjjj");
    }

    crc_main = crc_main ^ 0xffffffff;
    crc_info = crc_info ^ 0xffffffff;

    if ( ( emem_type == EMEM_ALL ) || ( emem_type == EMEM_MAIN ) )
    {
        TP_DEBUG_ERR("kkkkkkkkkkk");
        // CRC Main from TP
        crc_main_tp = drvDB_ReadReg ( 0x3C, 0x80 );
        crc_main_tp = ( crc_main_tp << 16 ) | drvDB_ReadReg ( 0x3C, 0x82 );

        // CRC Info from TP
        crc_info_tp = drvDB_ReadReg ( 0x3C, 0xA0 );
        crc_info_tp = ( crc_info_tp << 16 ) | drvDB_ReadReg ( 0x3C, 0xA2 );
    }
    TP_DEBUG ( "crc_main=0x%x, crc_info=0x%x, crc_main_tp=0x%x, crc_info_tp=0x%x\n",
               crc_main, crc_info, crc_main_tp, crc_info_tp );

    //drvDB_ExitDBBUS();
    TP_DEBUG_ERR("lllllllllllll");
    update_pass = 1;
	if ( ( emem_type == EMEM_ALL ) || ( emem_type == EMEM_MAIN ) )
    {
        if ( crc_main_tp != crc_main )
            update_pass = 0;

        if ( crc_info_tp != crc_info )
            update_pass = 0;
    }

    if ( !update_pass )
    {
        TP_DEBUG_ERR ( "update_C33 ok111\n" );
		_HalTscrHWReset();
        FwDataCnt = 0;
    	HalEnableIrq();	
        return size;
    }

    TP_DEBUG_ERR ( "update_C33 OK\n" );
	_HalTscrHWReset();
    FwDataCnt = 0;
    HalEnableIrq();	
    return size;
}

static ssize_t firmware_update_store ( struct device *dev,
                                       struct device_attribute *attr, const char *buf, size_t size )
{	
	u8 i;
    u8 dbbus_tx_data[4];
    unsigned char dbbus_rx_data[2] = {0};

	if(curr_ic_type==CTP_ID_MSG21XXA)
	{
		HalDisableIrq();
	    _HalTscrHWReset();
		msleep(100);
		dbbusDWIICEnterSerialDebugMode();
		dbbusDWIICStopMCU();
		dbbusDWIICIICUseBus();
		dbbusDWIICIICReshape();
		mdelay ( 300 );
	    // Disable the Watchdog
	    dbbus_tx_data[0] = 0x10;
	    dbbus_tx_data[1] = 0x3C;
	    dbbus_tx_data[2] = 0x60;
	    dbbus_tx_data[3] = 0x55;
	    HalTscrCDevWriteI2CSeq ( FW_ADDR_MSG21XX, dbbus_tx_data, 4 );
	    dbbus_tx_data[0] = 0x10;
	    dbbus_tx_data[1] = 0x3C;
	    dbbus_tx_data[2] = 0x61;
	    dbbus_tx_data[3] = 0xAA;
	    HalTscrCDevWriteI2CSeq ( FW_ADDR_MSG21XX, dbbus_tx_data, 4 );
	    // Stop MCU
	    dbbus_tx_data[0] = 0x10;
	    dbbus_tx_data[1] = 0x0F;
	    dbbus_tx_data[2] = 0xE6;
	    dbbus_tx_data[3] = 0x01;
	    HalTscrCDevWriteI2CSeq ( FW_ADDR_MSG21XX, dbbus_tx_data, 4 );
	    /////////////////////////
	    // Difference between C2 and C3
	    /////////////////////////
		// c2:2133 c32:2133a(2) c33:2138
	    //check id
	    dbbus_tx_data[0] = 0x10;
	    dbbus_tx_data[1] = 0x1E;
	    dbbus_tx_data[2] = 0xCC;
	    HalTscrCDevWriteI2CSeq ( FW_ADDR_MSG21XX, dbbus_tx_data, 3 );
	    HalTscrCReadI2CSeq ( FW_ADDR_MSG21XX, &dbbus_rx_data[0], 2 );
	    TP_DEBUG_ERR ( "111dbbus_rx version[0]=0x%x", dbbus_rx_data[0] );
	    if ( dbbus_rx_data[0] == 2 )
	    {
	        // check version
	        dbbus_tx_data[0] = 0x10;
	        dbbus_tx_data[1] = 0x3C;
	        dbbus_tx_data[2] = 0xEA;
	        HalTscrCDevWriteI2CSeq ( FW_ADDR_MSG21XX, dbbus_tx_data, 3 );
	        HalTscrCReadI2CSeq ( FW_ADDR_MSG21XX, &dbbus_rx_data[0], 2 );
	        TP_DEBUG_ERR ( "dbbus_rx version[0]=0x%x", dbbus_rx_data[0] );

	        if ( dbbus_rx_data[0] == 3 ){
	            return firmware_update_c33 ( dev, attr, buf, size, EMEM_MAIN );
			}
	        else{

	            return firmware_update_c32 ( dev, attr, buf, size, EMEM_ALL );
	        }
	    }
	    else
	    {
	        return firmware_update_c2 ( dev, attr, buf, size );
	    } 
	}
    else if(curr_ic_type==CTP_ID_MSG21XX)
	{
		HalDisableIrq();
	    _HalTscrHWReset();
		msleep(100);
		dbbusDWIICEnterSerialDebugMode();
		dbbusDWIICStopMCU();
		dbbusDWIICIICUseBus();
		dbbusDWIICIICReshape();
		mdelay ( 300 );

	    // Disable the Watchdog
	    dbbus_tx_data[0] = 0x10;
	    dbbus_tx_data[1] = 0x3C;
	    dbbus_tx_data[2] = 0x60;
	    dbbus_tx_data[3] = 0x55;
	    HalTscrCDevWriteI2CSeq ( FW_ADDR_MSG21XX, dbbus_tx_data, 4 );
	    dbbus_tx_data[0] = 0x10;
	    dbbus_tx_data[1] = 0x3C;
	    dbbus_tx_data[2] = 0x61;
	    dbbus_tx_data[3] = 0xAA;
	    HalTscrCDevWriteI2CSeq ( FW_ADDR_MSG21XX, dbbus_tx_data, 4 );

	    // Stop MCU
	    dbbus_tx_data[0] = 0x10;
	    dbbus_tx_data[1] = 0x0F;
	    dbbus_tx_data[2] = 0xE6;
	    dbbus_tx_data[3] = 0x01;
	    HalTscrCDevWriteI2CSeq ( FW_ADDR_MSG21XX, dbbus_tx_data, 4 );

	    // set FRO to 50M
	    dbbus_tx_data[0] = 0x10;
	    dbbus_tx_data[1] = 0x11;
	    dbbus_tx_data[2] = 0xE2;
	    HalTscrCDevWriteI2CSeq ( FW_ADDR_MSG21XX, dbbus_tx_data, 3 );
	    dbbus_rx_data[0] = 0;
	    dbbus_rx_data[1] = 0;
	    HalTscrCReadI2CSeq ( FW_ADDR_MSG21XX, &dbbus_rx_data[0], 2 );
	    TP_DEBUG ( "dbbus_rx_data[0]=0x%x", dbbus_rx_data[0] );
	    dbbus_tx_data[3] = dbbus_rx_data[0] & 0xF7;  //Clear Bit 3
	    HalTscrCDevWriteI2CSeq ( FW_ADDR_MSG21XX, dbbus_tx_data, 4 );

	    // set MCU clock,SPI clock =FRO
	    dbbus_tx_data[0] = 0x10;
	    dbbus_tx_data[1] = 0x1E;
	    dbbus_tx_data[2] = 0x22;
	    dbbus_tx_data[3] = 0x00;
	    HalTscrCDevWriteI2CSeq ( FW_ADDR_MSG21XX, dbbus_tx_data, 4 );

	    dbbus_tx_data[0] = 0x10;
	    dbbus_tx_data[1] = 0x1E;
	    dbbus_tx_data[2] = 0x23;
	    dbbus_tx_data[3] = 0x00;
	    HalTscrCDevWriteI2CSeq ( FW_ADDR_MSG21XX, dbbus_tx_data, 4 );

	    // Enable slave's ISP ECO mode
	    dbbus_tx_data[0] = 0x10;
	    dbbus_tx_data[1] = 0x08;
	    dbbus_tx_data[2] = 0x0c;
	    dbbus_tx_data[3] = 0x08;
	    HalTscrCDevWriteI2CSeq ( FW_ADDR_MSG21XX, dbbus_tx_data, 4 );

	    // Enable SPI Pad
	    dbbus_tx_data[0] = 0x10;
	    dbbus_tx_data[1] = 0x1E;
	    dbbus_tx_data[2] = 0x02;
	    HalTscrCDevWriteI2CSeq ( FW_ADDR_MSG21XX, dbbus_tx_data, 3 );
	    HalTscrCReadI2CSeq ( FW_ADDR_MSG21XX, &dbbus_rx_data[0], 2 );
	    TP_DEBUG ( "dbbus_rx_data[0]=0x%x", dbbus_rx_data[0] );
	    dbbus_tx_data[3] = ( dbbus_rx_data[0] | 0x20 ); //Set Bit 5
	    HalTscrCDevWriteI2CSeq ( FW_ADDR_MSG21XX, dbbus_tx_data, 4 );

	    // WP overwrite
	    dbbus_tx_data[0] = 0x10;
	    dbbus_tx_data[1] = 0x1E;
	    dbbus_tx_data[2] = 0x0E;
	    dbbus_tx_data[3] = 0x02;
	    HalTscrCDevWriteI2CSeq ( FW_ADDR_MSG21XX, dbbus_tx_data, 4 );

	    // set pin high
	    dbbus_tx_data[0] = 0x10;
	    dbbus_tx_data[1] = 0x1E;
	    dbbus_tx_data[2] = 0x10;
	    dbbus_tx_data[3] = 0x08;
	    HalTscrCDevWriteI2CSeq ( FW_ADDR_MSG21XX, dbbus_tx_data, 4 );

	    dbbusDWIICIICNotUseBus();
	    dbbusDWIICNotStopMCU();
	    dbbusDWIICExitSerialDebugMode();

	    drvISP_EntryIspMode();
	    drvISP_ChipErase();
	    _HalTscrHWReset();
	    mdelay ( 300 );

	    // 2.Program and Verify
	    dbbusDWIICEnterSerialDebugMode();
	    dbbusDWIICStopMCU();
	    dbbusDWIICIICUseBus();
	    dbbusDWIICIICReshape();

	    // Disable the Watchdog
	    dbbus_tx_data[0] = 0x10;
	    dbbus_tx_data[1] = 0x3C;
	    dbbus_tx_data[2] = 0x60;
	    dbbus_tx_data[3] = 0x55;
	    HalTscrCDevWriteI2CSeq ( FW_ADDR_MSG21XX, dbbus_tx_data, 4 );
	    dbbus_tx_data[0] = 0x10;
	    dbbus_tx_data[1] = 0x3C;
	    dbbus_tx_data[2] = 0x61;
	    dbbus_tx_data[3] = 0xAA;
	    HalTscrCDevWriteI2CSeq ( FW_ADDR_MSG21XX, dbbus_tx_data, 4 );

	    // Stop MCU
	    dbbus_tx_data[0] = 0x10;
	    dbbus_tx_data[1] = 0x0F;
	    dbbus_tx_data[2] = 0xE6;
	    dbbus_tx_data[3] = 0x01;
	    HalTscrCDevWriteI2CSeq ( FW_ADDR_MSG21XX, dbbus_tx_data, 4 );

	    // set FRO to 50M
	    dbbus_tx_data[0] = 0x10;
	    dbbus_tx_data[1] = 0x11;
	    dbbus_tx_data[2] = 0xE2;
	    HalTscrCDevWriteI2CSeq ( FW_ADDR_MSG21XX, dbbus_tx_data, 3 );
	    dbbus_rx_data[0] = 0;
	    dbbus_rx_data[1] = 0;
	    HalTscrCReadI2CSeq ( FW_ADDR_MSG21XX, &dbbus_rx_data[0], 2 );
	    TP_DEBUG ( "dbbus_rx_data[0]=0x%x", dbbus_rx_data[0] );
	    dbbus_tx_data[3] = dbbus_rx_data[0] & 0xF7;  //Clear Bit 3
	    HalTscrCDevWriteI2CSeq ( FW_ADDR_MSG21XX, dbbus_tx_data, 4 );

	    // set MCU clock,SPI clock =FRO
	    dbbus_tx_data[0] = 0x10;
	    dbbus_tx_data[1] = 0x1E;
	    dbbus_tx_data[2] = 0x22;
	    dbbus_tx_data[3] = 0x00;
	    HalTscrCDevWriteI2CSeq ( FW_ADDR_MSG21XX, dbbus_tx_data, 4 );

	    dbbus_tx_data[0] = 0x10;
	    dbbus_tx_data[1] = 0x1E;
	    dbbus_tx_data[2] = 0x23;
	    dbbus_tx_data[3] = 0x00;
	    HalTscrCDevWriteI2CSeq ( FW_ADDR_MSG21XX, dbbus_tx_data, 4 );

	    // Enable slave's ISP ECO mode
	    dbbus_tx_data[0] = 0x10;
	    dbbus_tx_data[1] = 0x08;
	    dbbus_tx_data[2] = 0x0c;
	    dbbus_tx_data[3] = 0x08;
	    HalTscrCDevWriteI2CSeq ( FW_ADDR_MSG21XX, dbbus_tx_data, 4 );

	    // Enable SPI Pad
	    dbbus_tx_data[0] = 0x10;
	    dbbus_tx_data[1] = 0x1E;
	    dbbus_tx_data[2] = 0x02;
	    HalTscrCDevWriteI2CSeq ( FW_ADDR_MSG21XX, dbbus_tx_data, 3 );
	    HalTscrCReadI2CSeq ( FW_ADDR_MSG21XX, &dbbus_rx_data[0], 2 );
	    TP_DEBUG ( "dbbus_rx_data[0]=0x%x", dbbus_rx_data[0] );
	    dbbus_tx_data[3] = ( dbbus_rx_data[0] | 0x20 ); //Set Bit 5
	    HalTscrCDevWriteI2CSeq ( FW_ADDR_MSG21XX, dbbus_tx_data, 4 );

	    // WP overwrite
	    dbbus_tx_data[0] = 0x10;
	    dbbus_tx_data[1] = 0x1E;
	    dbbus_tx_data[2] = 0x0E;
	    dbbus_tx_data[3] = 0x02;
	    HalTscrCDevWriteI2CSeq ( FW_ADDR_MSG21XX, dbbus_tx_data, 4 );

	    // set pin high
	    dbbus_tx_data[0] = 0x10;
	    dbbus_tx_data[1] = 0x1E;
	    dbbus_tx_data[2] = 0x10;
	    dbbus_tx_data[3] = 0x08;
	    HalTscrCDevWriteI2CSeq ( FW_ADDR_MSG21XX, dbbus_tx_data, 4 );

	    dbbusDWIICIICNotUseBus();
	    dbbusDWIICNotStopMCU();
	    dbbusDWIICExitSerialDebugMode();

	    ///////////////////////////////////////
	    // Start to load firmware
	    ///////////////////////////////////////
	    drvISP_EntryIspMode();

	    for ( i = 0; i < 94; i++ ) // total  94 KB : 1 byte per R/W
	    {
	        drvISP_Program ( i, temp[i] ); // program to slave's flash
	        drvISP_Verify ( i, temp[i] ); //verify data
	    }
	    TP_DEBUG ( "update OK\n" );
	    drvISP_ExitIspMode();
		_HalTscrHWReset();
		FwDataCnt = 0;
		HalEnableIrq();
	    return size;
	}
	return 0;
}

static DEVICE_ATTR(update, CTP_AUTHORITY, firmware_update_show, firmware_update_store);
#ifdef ENABLE_AUTO_UPDATA
static unsigned char MSG21XX_update_bin[]=
{
#include "MSG21XX_update_bin.i"
};
static int fwAutoUpdate(void *unused)
{
    firmware_update_store(NULL, NULL, NULL, 0);	
}
#endif

static u8 getchipType(void)
{
    u8 curr_ic_type = 0;
    u8 dbbus_tx_data[4];
    unsigned char dbbus_rx_data[2] = {0};
	
	//_HalTscrHWReset();
    HalDisableIrq();
    mdelay ( 100 );
    
    dbbusDWIICEnterSerialDebugMode();
    dbbusDWIICStopMCU();
    dbbusDWIICIICUseBus();
    dbbusDWIICIICReshape();
    mdelay ( 100 );

    // Disable the Watchdog
    dbbus_tx_data[0] = 0x10;
    dbbus_tx_data[1] = 0x3C;
    dbbus_tx_data[2] = 0x60;
    dbbus_tx_data[3] = 0x55;
    HalTscrCDevWriteI2CSeq ( FW_ADDR_MSG21XX, dbbus_tx_data, 4 );
    dbbus_tx_data[0] = 0x10;
    dbbus_tx_data[1] = 0x3C;
    dbbus_tx_data[2] = 0x61;
    dbbus_tx_data[3] = 0xAA;
    HalTscrCDevWriteI2CSeq ( FW_ADDR_MSG21XX, dbbus_tx_data, 4 );
    // Stop MCU
    dbbus_tx_data[0] = 0x10;
    dbbus_tx_data[1] = 0x0F;
    dbbus_tx_data[2] = 0xE6;
    dbbus_tx_data[3] = 0x01;
    HalTscrCDevWriteI2CSeq ( FW_ADDR_MSG21XX, dbbus_tx_data, 4 );
    /////////////////////////
    // Difference between C2 and C3
    /////////////////////////
    // c2:2133 c32:2133a(2) c33:2138
    //check id
    dbbus_tx_data[0] = 0x10;
    dbbus_tx_data[1] = 0x1E;
    dbbus_tx_data[2] = 0xCC;
    HalTscrCDevWriteI2CSeq ( FW_ADDR_MSG21XX, dbbus_tx_data, 3 );
    HalTscrCReadI2CSeq ( FW_ADDR_MSG21XX, &dbbus_rx_data[0], 2 );
    
    if ( dbbus_rx_data[0] == 2 )
    {
    	curr_ic_type = CTP_ID_MSG21XXA;
    }
    else
    {
    	curr_ic_type = CTP_ID_MSG21XX;
    }
    TP_DEBUG_ERR("CURR_IC_TYPE = %d \n",curr_ic_type);
    dbbusDWIICIICNotUseBus();
    dbbusDWIICNotStopMCU();
    dbbusDWIICExitSerialDebugMode();
    HalEnableIrq();
    
    return curr_ic_type;
    
}
static void getMSG21XXFWVersion(u8 curr_ic_type)
{
    unsigned char dbbus_tx_data[3];
    unsigned char dbbus_rx_data[4] ;
    unsigned short major=0, minor=0;

    HalDisableIrq();
    mdelay ( 100 );
    
    dbbus_tx_data[0] = 0x53;
    dbbus_tx_data[1] = 0x00;
     if(curr_ic_type==CTP_ID_MSG21XXA)
    {
    dbbus_tx_data[2] = 0x2A;
    }
    else if(curr_ic_type==CTP_ID_MSG21XX)
    {
        dbbus_tx_data[2] = 0x74;
    }
    else
    {
        TP_DEBUG_ERR("***ic_type = %d ***\n", dbbus_tx_data[2]);
        dbbus_tx_data[2] = 0x2A;
    }
    HalTscrCDevWriteI2CSeq(FW_ADDR_MSG21XX_TP, &dbbus_tx_data[0], 3);
    HalTscrCReadI2CSeq(FW_ADDR_MSG21XX_TP, &dbbus_rx_data[0], 4);

    curr_ic_major = (dbbus_rx_data[1]<<8)+dbbus_rx_data[0];
    curr_ic_minor = (dbbus_rx_data[3]<<8)+dbbus_rx_data[2];

    TP_DEBUG("***FW Version major = %d ***\n", curr_ic_major);
    TP_DEBUG("***FW Version minor = %d ***\n", curr_ic_minor);
    _HalTscrHWReset();

    
    HalEnableIrq();
	
}

static ssize_t firmware_version_show(struct device *dev,
                                     struct device_attribute *attr, char *buf)
{
    TP_DEBUG("*** firmware_version_show fw_version = %s***\n", fw_version);
    return sprintf(buf, "%s\n", fw_version);
}

static ssize_t firmware_version_store(struct device *dev,
                                      struct device_attribute *attr, const char *buf, size_t size)
{
    unsigned char dbbus_tx_data[3];
    unsigned char dbbus_rx_data[4] ;
    unsigned short major=0, minor=0;
   
/*
    dbbusDWIICEnterSerialDebugMode();
    dbbusDWIICStopMCU();
    dbbusDWIICIICUseBus();
    dbbusDWIICIICReshape();

*/
    fw_version = kzalloc(sizeof(char), GFP_KERNEL);

    //Get_Chip_Version();
    dbbus_tx_data[0] = 0x53;
    dbbus_tx_data[1] = 0x00;
    if(curr_ic_type==CTP_ID_MSG21XXA)
    {
    dbbus_tx_data[2] = 0x2A;
    }
    else if(curr_ic_type==CTP_ID_MSG21XX)
    {
        dbbus_tx_data[2] = 0x74;
    }
    else
    {
        TP_DEBUG_ERR("***ic_type = %d ***\n", dbbus_tx_data[2]);
        dbbus_tx_data[2] = 0x2A;
    }
    HalTscrCDevWriteI2CSeq(FW_ADDR_MSG21XX_TP, &dbbus_tx_data[0], 3);
    HalTscrCReadI2CSeq(FW_ADDR_MSG21XX_TP, &dbbus_rx_data[0], 4);

    major = (dbbus_rx_data[1]<<8)+dbbus_rx_data[0];
    minor = (dbbus_rx_data[3]<<8)+dbbus_rx_data[2];
    curr_ic_major = major;
    curr_ic_minor = minor;

    TP_DEBUG_ERR("***major = %d ***\n", major);
    TP_DEBUG_ERR("***minor = %d ***\n", minor);
    sprintf(fw_version,"%03d%03d", major, minor);
    //TP_DEBUG(printk("***fw_version = %s ***\n", fw_version);)
   
    return size;
}
static DEVICE_ATTR(version, CTP_AUTHORITY, firmware_version_show, firmware_version_store);

static ssize_t firmware_data_show(struct device *dev,
                                  struct device_attribute *attr, char *buf)
{
    return FwDataCnt;
}

static ssize_t firmware_data_store(struct device *dev,
                                   struct device_attribute *attr, const char *buf, size_t size)
{
    int i;
	TP_DEBUG_ERR("***FwDataCnt = %d ***\n", FwDataCnt);
   // for (i = 0; i < 1024; i++)
    {
        memcpy(temp[FwDataCnt], buf, 1024);
    }
    FwDataCnt++;
    return size;
}
static DEVICE_ATTR(data, CTP_AUTHORITY, firmware_data_show, firmware_data_store);
#endif//adair:以上无需修改
#endif  

//end for update firmware

 static u8 Calculate_8BitsChecksum( u8 *msg, s32 s32Length )
 {
	 s32 s32Checksum = 0;
	 s32 i;
 
	 for( i = 0 ; i < s32Length; i++ )
	 {
		 s32Checksum += msg[i];
	 }
 
	 return ( u8 )( ( -s32Checksum ) & 0xFF );
 }

 static int tpd_touchinfo(TouchScreenInfo_t *touchData)
 {
	static int pre_event_is_key = 0;
    u8 val[8] = {0};
    u8 Checksum = 0;
    u8 i;
    u32 delta_x = 0, delta_y = 0;
    u32 u32X = 0;
    u32 u32Y = 0;
    

    TPD_DEBUG(KERN_ERR "[msg2133a]==tpd_touchinfo() \n");
#ifdef TPD_PROXIMITY
		int err;
		hwm_sensor_data sensor_data;
#endif
#if SWAP_X_Y
    int tempx;
    int tempy;
#endif

    /*Get Touch Raw Data*/
    i2c_master_recv( i2c_client, &val[0], REPORT_PACKET_LENGTH );
    TPD_DEBUG(KERN_ERR"[tpd_touchinfo]--val[0]:%x, REPORT_PACKET_LENGTH:%x \n",val[0], REPORT_PACKET_LENGTH);
    Checksum = Calculate_8BitsChecksum( &val[0], 7 ); //calculate checksum
    TPD_DEBUG(KERN_ERR"[tpd_touchinfo]--Checksum:%x, val[7]:%x, val[0]:%x \n",Checksum, val[7], val[0]);

    if( ( Checksum == val[7] ) && ( val[0] == 0x52 ) ) //check the checksum  of packet
    {
	    #ifdef TPD_PROXIMITY
			if((val[5] == 0x80)||(val[5] == 0x40))
			{
			   printk(TPD_DEVICE"TP_PROXIMITY g_bPsSensorOpen = %d,g_nPsSensorDate=%d\n",g_bPsSensorOpen,g_nPsSensorDate );		  
			   if(val[5] == 0x80) // close Panel
			   	{
					if((g_bPsSensorOpen == 1) /*&& (g_nPsSensorDate == 1)*/)
					{
						mutex_lock(&msg2133_sensor_mutex);
						g_nPsSensorDate = 0;
						mutex_unlock(&msg2133_sensor_mutex);
					}
			   	}
			    else// Open Panel
			    {
					if(/*(g_bPsSensorOpen == 1) && */(g_nPsSensorDate == 0))
					{
						mutex_lock(&msg2133_sensor_mutex);
						g_nPsSensorDate = 1;
						mutex_unlock(&msg2133_sensor_mutex);
					}
			    }
				sensor_data.values[0] = g_nPsSensorDate;
				sensor_data.value_divide = 1;
				sensor_data.status = SENSOR_STATUS_ACCURACY_MEDIUM;				
				if ((err = hwmsen_get_interrupt_data(ID_PROXIMITY, &sensor_data)))
				{
					printk(" Proximity call hwmsen_get_interrupt_data failed= %d\n", err);	
				}	
				return 0;
									
			}
			#ifdef CTP_PROXIMITY_ESD_MEASURE
			else if(val[5] == 0xC0) 
			{			
				if(g_bPsSensorOpen == 1)
				{
					msg2133_enable_ps(1);
				}
				return 0;
			}
			#endif
			else
		#endif
		    {
		        u32X = ( ( ( val[1] & 0xF0 ) << 4 ) | val[2] );   //parse the packet to coordinates
		        u32Y = ( ( ( val[1] & 0x0F ) << 8 ) | val[3] );

		        delta_x = ( ( ( val[4] & 0xF0 ) << 4 ) | val[5] );
		        delta_y = ( ( ( val[4] & 0x0F ) << 8 ) | val[6] );
				TPD_DEBUG(KERN_ERR"[tpd_touchinfo]--u32X:%d, u32Y:%d, delta_x:%d, delta_y:%d \n",u32X, u32Y,delta_x, delta_y);

				#if SWAP_X_Y
		        tempy = u32X;
		        tempx = u32Y;
		        u32X = tempx;
		        u32Y = tempy;

		        tempy = delta_x;
		        tempx = delta_y;
		        delta_x = tempx;
		        delta_y = tempy;
				#endif

				#if REVERSE_X
		        u32X = 2047 - u32X;
		        delta_x = 4095 - delta_x;
				#endif

				#if REVERSE_Y
		        u32Y = 2047 - u32Y;
		        delta_y = 4095 - delta_y;
				#endif

				TPD_DEBUG(KERN_ERR"[tpd_touchinfo]--u32X:%d, u32Y:%d, delta_x:%d, delta_y:%d \n",u32X, u32Y,delta_x, delta_y);

		        if( ( val[1] == 0xFF ) && ( val[2] == 0xFF ) && ( val[3] == 0xFF ) && ( val[4] == 0xFF ) && ( val[6] == 0xFF ) )
		        {  
		        	touchData->Point[0].X = 0; // final X coordinate
		            touchData->Point[0].Y = 0; // final Y coordinate

		            if( ( val[5] == 0x0 ) || ( val[5] == 0xFF ) )
		            {
		            	// key release or touch release                
		                touchData->nTouchKeyCode = 0; //TouchKeyMode
		                touchData->nFingerNum = 0; //touch end
						touchData->nTouchKeyMode = pre_event_is_key; //TouchKeyMode
						pre_event_is_key = 0;
		            }
		            else
		            {
		            	// Key event
		            	touchData->nFingerNum = 1;
		                touchData->nTouchKeyCode = val[5]; //TouchKeyCode
		            	touchData->nTouchKeyMode = 1; //TouchKeyMode
		            	pre_event_is_key = 1;
		            }
		        }
		        else
		        {
		        	// Touch event
		            touchData->nTouchKeyMode = 0; 
					pre_event_is_key = 0;

		            if(
						#if REVERSE_X
		                ( delta_x == 4095 )
						#else
		                ( delta_x == 0 )
						#endif
		                &&
						#if REVERSE_Y
		                ( delta_y == 4095 )
						#else
		                ( delta_y == 0 )
						#endif
		            )
		            {
		            	// one touch
		                touchData->nFingerNum = 1;
		                touchData->Point[0].X = ( u32X * TPD_RES_X ) / 2048;
		                touchData->Point[0].Y = ( u32Y * TPD_RES_Y ) / 2048;
			#if 0
				touchData->Point[0].X = MS_TS_MSG21XX_X_MAX - touchData->Point[0].X ;
				touchData->Point[0].Y = MS_TS_MSG21XX_Y_MAX - touchData->Point[0].Y;
			#endif

						TPD_DEBUG(KERN_ERR"[tpd_touchinfo]--FingerNum = 1 \n");
		            }
		            else
		            {
		            	// dual touch
		                u32 x2, y2;

		                touchData->nFingerNum = 2; //two touch

		                /* Finger 1 */
		                touchData->Point[0].X = ( u32X * TPD_RES_X ) / 2048;
		                touchData->Point[0].Y = ( u32Y * TPD_RES_Y ) / 2048;

			#if 0
				touchData->Point[0].X = MS_TS_MSG21XX_X_MAX - touchData->Point[0].X ;
				touchData->Point[0].Y = MS_TS_MSG21XX_Y_MAX - touchData->Point[0].Y;
			#endif


		                /* Finger 2 */
		                if( delta_x > 2048 )    //transform the unsigh value to sign value
		                {
		                    delta_x -= 4096;
		                }
		                if( delta_y > 2048 )
		                {
		                    delta_y -= 4096;
		                }

		                x2 = ( u32 )( u32X + delta_x );
		                y2 = ( u32 )( u32Y + delta_y );

		                touchData->Point[1].X = ( x2 * TPD_RES_X ) / 2048;
		                touchData->Point[1].Y = ( y2 * TPD_RES_Y ) / 2048;
			#if 0
				touchData->Point[1].X = MS_TS_MSG21XX_X_MAX - touchData->Point[1].X ;
				touchData->Point[1].Y = MS_TS_MSG21XX_Y_MAX - touchData->Point[1].Y;
			#endif

						
						TPD_DEBUG(KERN_ERR"[tpd_touchinfo]--FingerNum = 2 \n");
		            }
		        }
		    }
    }
    else
    {
		#if 0    
        DBG("Packet error 0x%x, 0x%x, 0x%x", val[0], val[1], val[2]);
        DBG("             0x%x, 0x%x, 0x%x", val[3], val[4], val[5]);
        DBG("             0x%x, 0x%x, 0x%x", val[6], val[7], Checksum);
		#endif		
        TPD_DEBUG( KERN_ERR "err status in tp\n" );
		return false;
    }

    //enable_irq( msg21xx_irq );
  
	 return true;

 };
 
 static void tpd_down(int x, int y, int p, int id) {

	  //if (id)
	  	  input_report_abs(tpd->dev, ABS_MT_TRACKING_ID, id);

	  input_report_abs(tpd->dev, ABS_MT_TOUCH_MAJOR, 100);
	  input_report_abs(tpd->dev, ABS_PRESSURE, 100);
	  input_report_key(tpd->dev, BTN_TOUCH, 1);
	  input_report_abs(tpd->dev, ABS_MT_POSITION_X, x);
	  input_report_abs(tpd->dev, ABS_MT_POSITION_Y, y);

	  /* track id Start 0 */
		//input_report_abs(tpd->dev, ABS_MT_TRACKING_ID, p); 
	  input_mt_sync(tpd->dev);
	  if (FACTORY_BOOT == get_boot_mode()|| RECOVERY_BOOT == get_boot_mode())
	  {   
		tpd_button(x, y, 1);  
	  }
	  if(y > TPD_RES_Y) //virtual key debounce to avoid android ANR issue
	  {
		  msleep(50);
		  TPD_DEBUG("D virtual key \n");
	  }
	  TPD_EM_PRINT(x, y, x, y, id, 1);
  }
  
 static void tpd_up(int x, int y,int *count) {

	  input_report_key(tpd->dev, BTN_TOUCH, 0);
	  input_mt_sync(tpd->dev);
	  TPD_EM_PRINT(x, y, x, y, 0, 0);
		  
	  if(FACTORY_BOOT == get_boot_mode()|| RECOVERY_BOOT == get_boot_mode())
	  {   
	  	 TPD_DEBUG(KERN_ERR "[msg2133a]--tpd_up-BOOT MODE--X:%d, Y:%d; \n", x, y);
		 tpd_button(x, y, 0); 
	  } 		  
 
  }

#ifdef TPD_HAVE_BUTTON
 static void tpd_key(int key_code, int key_val)
 {
 	u16 index = 0;
	
 	if(key_code == KEY_MENU)
	{
		index = 0;
	}
	else if(key_code == KEY_HOMEPAGE)
	{
		index = 1;
	}
	else if(key_code == KEY_BACK)
	{
		index = 2;
	}

	if(key_val == 0)
	{
		tpd_up(tpd_keys_dim_local[index][0], tpd_keys_dim_local[index][1], 0);
	}
	else
	{
		tpd_down(tpd_keys_dim_local[index][0], tpd_keys_dim_local[index][1], 1, index);
	}
	TPD_DEBUG(KERN_ERR "[msg2133a]button x = %d y = %d\n", tpd_keys_dim_local[index][0], tpd_keys_dim_local[index][1]);
 }
#else
static void tpd_key(int key_code, int key_val)
 {
 	input_report_key(tpd->kpd, key_code, key_val);
	TPD_DEBUG(KERN_ERR "[msg2133a] key_code = %d(%d)\n", key_code, key_val);
 }

#endif

 static void tpd_check_key(int *key, int *is_down)
 {
 	static int pre_key = 0;
	int cur_key;


	 if (*key == 1)	cur_key = KEY_MENU;
	 else if (*key == 2)	cur_key = KEY_HOMEPAGE;
	 else if (*key == 4)	cur_key = KEY_BACK;
	 else cur_key = 0;
	 
	 if ((pre_key != 0) && (cur_key != pre_key))
	 {
	 	// current key is different from previous key, then need to send key up
	 	cur_key = pre_key;
		pre_key = 0;
		*is_down = 0;
	 }
	 else 
	 {
	 	pre_key = cur_key;
	 	*is_down = 1;
	 }
	 
	 *key = cur_key;

 }
 static int touch_event_handler(void *unused)
 {
  
    TouchScreenInfo_t touchData;
	u8 touchkeycode = 0;
	static u32 preKeyStatus = 0;
	int i=0;
 
    TPD_DEBUG(KERN_ERR "[msg2133a]touch_event_handler() do while \n");

	touchData.nFingerNum = 0;
	TPD_DEBUG(KERN_ERR "[msg2133a]touch_event_handler() do while \n");
	 
	if (tpd_touchinfo(&touchData)) 
	{
	 
		TPD_DEBUG(KERN_ERR "[msg2133a]--KeyMode:%d, KeyCode:%d, FingerNum =%d \n", touchData.nTouchKeyMode, touchData.nTouchKeyCode, touchData.nFingerNum );
	 
		//key event
		if( touchData.nTouchKeyMode )
		{
			int key_code = touchData.nTouchKeyCode; 
			int key_val;
			
			tpd_check_key(&key_code, &key_val);

			tpd_key(key_code, key_val);
			#ifdef TPD_HAVE_BUTTON			
			input_sync( tpd->dev );
			#else
			input_sync(tpd->kpd);
			#endif
			
			goto TPD_EVENT_END;
		}
		//Touch event
		if( ( touchData.nFingerNum ) == 0 ) //touch end
		{
			TPD_DEBUG("------UP------ \n");
			TPD_DEBUG(KERN_ERR "[msg2133a]---X:%d, Y:%d; \n", touchData.Point[0].X, touchData.Point[0].Y);
			tpd_up(touchData.Point[0].X, touchData.Point[0].Y, 0);
			input_sync( tpd->dev );
		}
		else //touch on screen
		{
			TPD_DEBUG("------DOWN------ \n");
			for( i = 0; i < ( (int)touchData.nFingerNum ); i++ )
			{
			    tpd_down(touchData.Point[i].X, touchData.Point[i].Y, 1, i);
				TPD_DEBUG(KERN_ERR "[msg2133a]---X:%d, Y:%d; i=%d \n", touchData.Point[i].X, touchData.Point[i].Y, i);
			}
			input_sync( tpd->dev );
		}
	}

TPD_EVENT_END:
    mt_eint_unmask(CUST_EINT_TOUCH_PANEL_NUM); 
	return 0;
 }
 
 static int tpd_detect (struct i2c_client *client, struct i2c_board_info *info) 
 {
	 strcpy(info->type, TPD_DEVICE);	
	 return 0;
 }
 
 static void tpd_eint_interrupt_handler(void)
 {
 	 TPD_DEBUG_PRINT_INT;
	 mt_eint_mask(CUST_EINT_TOUCH_PANEL_NUM);
	 schedule_work( &msg21xx_wq );
 }

#if LCT_ADD_TP_VERSION
#define CTP_PROC_FILE "ctp_version"
static struct proc_dir_entry *g_ctp_proc = NULL;
static int g_major = 0;
static int g_minor = 0;

static int ctp_proc_read(char *page, char **start, off_t off, int count, int *eof, void *data)
{

	int cnt= 0;
	printk("Enter ctp_proc_read.\n");
	if(off != 0)
		return 0;
	cnt = sprintf(page, "vid:0x%04x,firmware:0x%04x\n",g_major, g_minor);
	
	*eof = 1;
	printk("vid:0x%04x,firmware:0x%04x\n",g_major, g_minor);
	printk("Leave ctp_proc_read. cnt = %d\n", cnt);
	return cnt;
}
#endif

#ifdef TPD_PROXIMITY
static int msg2133_enable_ps(int enable)
{
	u8 ps_store_data[4];

	mutex_lock(&msg2133_sensor_mutex);

	printk("msg2133 do enable: %d, current state: %d\n", enable, g_bPsSensorOpen);
	if(enable == 1)
	{
		if(g_bPsSensorOpen == 0)
		{
			ps_store_data[0] = 0x52;
			ps_store_data[1] = 0x00;
            if(curr_ic_type==CTP_ID_MSG21XXA)
            {
                ps_store_data[2] = 0x4a;
            }
            else if(curr_ic_type==CTP_ID_MSG21XX)
            {
                ps_store_data[2] = 0x62;
            }
            else
            {
                ps_store_data[2] = 0x4a;
            }
			ps_store_data[3] = 0xa0;		
			printk("msg2133 do enable: %d, current state: %d\n", enable, g_bPsSensorOpen);
			HalTscrCDevWriteI2CSeq(0x26, &ps_store_data[0], 4);
			g_bPsSensorOpen = 1;
		}
	}
	else
	{	
		if(g_bPsSensorOpen == 1)
		{
			ps_store_data[0] = 0x52;
			ps_store_data[1] = 0x00;
			if(curr_ic_type==CTP_ID_MSG21XXA)
            {
                ps_store_data[2] = 0x4a;
            }
            else if(curr_ic_type==CTP_ID_MSG21XX)
            {
                ps_store_data[2] = 0x62;
            }
            else
            {
                ps_store_data[2] = 0x4a;
            }
			ps_store_data[3] = 0xa1;
			HalTscrCDevWriteI2CSeq(0x26, &ps_store_data[0], 4);	
			g_bPsSensorOpen = 0;			
		}
		g_nPsSensorDate = 1;
	}
	mutex_unlock(&msg2133_sensor_mutex);
	return 0;
}

int msg2133_ps_operate(void* self, uint32_t command, void* buff_in, int size_in,
		void* buff_out, int size_out, int* actualout)
{
	int err = 0;
	int value;
	hwm_sensor_data* sensor_data;

	//APS_FUN(f);
	switch (command)
	{
		case SENSOR_DELAY:
			if((buff_in == NULL) || (size_in < sizeof(int)))
			{
				APS_ERR("Set delay parameter error!\n");
				err = -EINVAL;
			}
			// Do nothing
			break;

		case SENSOR_ENABLE:
			if((buff_in == NULL) || (size_in < sizeof(int)))
			{
				APS_ERR("Enable sensor parameter error!\n");
				err = -EINVAL;
			}
			else
			{				
				value = *(int *)buff_in;
				if(value)
				{
					printk("msg2133_ps_operate++++++++1\n");
					wake_lock(&ps_lock);		//wujinyou
					if(err = msg2133_enable_ps(1))
					{
						APS_ERR("enable ps fail: %d\n", err); 
						return -1;
					}
					g_bPsSensorOpen = 1;
				}
				else
				{
					printk("msg2133_ps_operate++++++++0\n");
					wake_unlock(&ps_lock);		//wujinyou
					if(err = msg2133_enable_ps(0))
					{
						APS_ERR("disable ps fail: %d\n", err); 
						return -1;
					}
					g_bPsSensorOpen = 0;
				}
			}
			break;

		case SENSOR_GET_DATA:
			printk("msg2133_ps get date: %d\n", g_nPsSensorDate);
			if((buff_out == NULL) || (size_out< sizeof(hwm_sensor_data)))
			{
				APS_ERR("get sensor data parameter error!\n");
				err = -EINVAL;
			}
			else
			{
				sensor_data = (hwm_sensor_data *)buff_out;	
				sensor_data->values[0] = g_nPsSensorDate;
				sensor_data->value_divide = 1;
				sensor_data->status = SENSOR_STATUS_ACCURACY_MEDIUM;			
			}
			break;
		default:
			APS_ERR("proxmy sensor operate function no this parameter %d!\n", command);
			err = -1;
			break;
	}
	
	return err;
}


static void tpd_initialize_ps_sensor_function()
{
	struct hwmsen_object obj_ps = {0};
	int err = 0;
	
	g_nPsSensorDate = 1;

	obj_ps.self = NULL;	// no use
	obj_ps.polling = 1;
	obj_ps.sensor_operate = msg2133_ps_operate;

	wake_lock_init(&ps_lock,WAKE_LOCK_SUSPEND,"ps wakelock"); //shaohui add
		
	if(err = hwmsen_attach(ID_PROXIMITY, &obj_ps))
	{
		TPD_DEBUG("attach fail = %d\n", err);
		return;
	}
}
#endif



 static int __devinit tpd_probe(struct i2c_client *client, const struct i2c_device_id *id)
 {	 
	int retval = TPD_OK;
	char data;
	u8 report_rate=0;
	int err=0;
	int reset_count = 0;

	i2c_client = client;
	//msg21xx_i2c_client = client;
	/*reset I2C clock*/
    //i2c_client->timing = 0;
    
   INIT_WORK( &msg21xx_wq, touch_event_handler );
//power on, need confirm with SA
/*
#ifdef TPD_POWER_SOURCE_CUSTOM
	hwPowerOn(TPD_POWER_SOURCE_CUSTOM, VOL_2800, "TP");
#else
	hwPowerOn(MT65XX_POWER_LDO_VGP2, VOL_2800, "TP");
#endif
#ifdef TPD_POWER_SOURCE_1800
	hwPowerOn(TPD_POWER_SOURCE_1800, VOL_1800, "TP");
#endif 
*/
//	hwPowerOn(TPD_POWER_SOURCE_CUSTOM, VOL_2800, "TP");

	//soso add for CTP FAE'request
//	hwPowerOn(MT6323_POWER_LDO_VCAM_AF, VOL_1800, "TP");


#ifdef TPD_CLOSE_POWER_IN_SLEEP	 
	hwPowerDown(TPD_POWER_SOURCE,"TP");
	hwPowerOn(TPD_POWER_SOURCE,VOL_2800,"TP");
	msleep(100);
#else
	// Reset panel
	msg2133_reset();
#endif
	
	// Configure interrupt pin
	mt_set_gpio_mode(GPIO_CTP_EINT_PIN, GPIO_CTP_EINT_PIN_M_EINT);
    mt_set_gpio_dir(GPIO_CTP_EINT_PIN, GPIO_DIR_IN);
   	mt_set_gpio_pull_enable(GPIO_CTP_EINT_PIN, GPIO_PULL_ENABLE);
    mt_set_gpio_pull_select(GPIO_CTP_EINT_PIN, GPIO_PULL_DOWN);
		
    msleep(10);

	// Register interrupt
	//mt_eint_set_hw_debounce(CUST_EINT_TOUCH_PANEL_NUM, CUST_EINT_DEBOUNCE_DISABLE);
	mt_eint_registration(CUST_EINT_TOUCH_PANEL_NUM, EINTF_TRIGGER_FALLING, tpd_eint_interrupt_handler, 1);
	msleep(50);
	mt_eint_unmask(CUST_EINT_TOUCH_PANEL_NUM);
	msleep(200);
	
    if((i2c_smbus_read_i2c_block_data(i2c_client, 0x00, 1, &data))< 0)
	{
		TPD_DMESG("I2C transfer error, line: %d\n", __LINE__);
		return -1; 
	}
	
    tpd_load_status = 1;
	TPD_DMESG("msg2133 Touch Panel Device Probe %s\n", (retval < TPD_OK) ? "FAIL" : "PASS");
	
    	
 #ifdef __FIRMWARE_UPDATE__
	firmware_class = class_create(THIS_MODULE, "ms-touchscreen-msg20xx");
    if (IS_ERR(firmware_class))
        pr_err("Failed to create class(firmware)!\n");
    firmware_cmd_dev = device_create(firmware_class,
                                     NULL, 0, NULL, "device");
    if (IS_ERR(firmware_cmd_dev))
        pr_err("Failed to create device(firmware_cmd_dev)!\n");

    // version
    if (device_create_file(firmware_cmd_dev, &dev_attr_version) < 0)
        pr_err("Failed to create device file(%s)!\n", dev_attr_version.attr.name);
    // update
    if (device_create_file(firmware_cmd_dev, &dev_attr_update) < 0)
        pr_err("Failed to create device file(%s)!\n", dev_attr_update.attr.name);
    // data
    if (device_create_file(firmware_cmd_dev, &dev_attr_data) < 0)
        pr_err("Failed to create device file(%s)!\n", dev_attr_data.attr.name);
	// clear
 //   if (device_create_file(firmware_cmd_dev, &dev_attr_clear) < 0)
 //       pr_err("Failed to create device file(%s)!\n", dev_attr_clear.attr.name);

	dev_set_drvdata(firmware_cmd_dev, NULL);
    
    curr_ic_type = getchipType();
    getMSG21XXFWVersion(curr_ic_type);
    #ifdef	ENABLE_AUTO_UPDATA
	TP_DEBUG_ERR("[TP] check auto updata\n");
	if(curr_ic_type == CTP_ID_MSG21XXA)
	{
	    update_bin_major = MSG21XX_update_bin[0x7f4f]<<8|MSG21XX_update_bin[0x7f4e];
        update_bin_minor = MSG21XX_update_bin[0x7f51]<<8|MSG21XX_update_bin[0x7f50];
        TP_DEBUG_ERR("bin_major = %d \n",update_bin_major);
        TP_DEBUG_ERR("bin_minor = %d \n",update_bin_minor);
        
    	if(update_bin_major==curr_ic_major
            &&update_bin_minor>curr_ic_minor)
    	{
    	    int i = 0;
    		for (i = 0; i < 33; i++)
    		{
    		    firmware_data_store(NULL, NULL, &(MSG21XX_update_bin[i*1024]), 0);
    		}
            kthread_run(fwAutoUpdate, 0, "MSG21XXA_fw_auto_update");
    	}
	}
    else if(curr_ic_type == CTP_ID_MSG21XX)
    {
	    update_bin_major = MSG21XX_update_bin[0x3076]<<8|MSG21XX_update_bin[0x3077];
        update_bin_minor = MSG21XX_update_bin[0x3074]<<8|MSG21XX_update_bin[0x3075];
        TP_DEBUG_ERR("bin_major = %d \n",update_bin_major);
        TP_DEBUG_ERR("bin_minor = %d \n",update_bin_minor);
        
    	if(update_bin_major==curr_ic_major
            &&update_bin_minor>curr_ic_minor)
    	{
    	    int i = 0;
    		for (i = 0; i < 94; i++)
    		{
    		    firmware_data_store(NULL, NULL, &(MSG21XX_update_bin[i*1024]), 0);
    		}
            kthread_run(fwAutoUpdate, 0, "MSG21XX_fw_auto_update");
    	}
	}  
	#endif
#endif 

#if LCT_ADD_TP_VERSION

	// get fw version
		unsigned char dbbus_tx_data[3];
		unsigned char dbbus_rx_data[4] ;
		unsigned short major=0, minor=0;
	
	//	Get_Chip_Version();
		dbbus_tx_data[0] = 0x53;
		dbbus_tx_data[1] = 0x00;
		dbbus_tx_data[2] = 0x74;
		HalTscrCDevWriteI2CSeq(0x4C, &dbbus_tx_data[0], 3);
		HalTscrCReadI2CSeq(0x4C, &dbbus_rx_data[0], 4);
		g_major = (dbbus_rx_data[1]<<8)+dbbus_rx_data[0];
		g_minor = (dbbus_rx_data[3]<<8)+dbbus_rx_data[2];
	// end get fw

		g_ctp_proc = create_proc_entry(CTP_PROC_FILE, 0444, NULL);
		if (g_ctp_proc == NULL) {
			printk("create_proc_entry failed\n");
		} else {
			g_ctp_proc->read_proc = ctp_proc_read;
			g_ctp_proc->write_proc = NULL;
			//g_ctp_proc->owner = THIS_MODULE;
			printk("create_proc_entry success\n");
		}
#endif

#ifdef TPD_PROXIMITY
	tpd_initialize_ps_sensor_function();
#endif
   return 0;
   
 }

 static int __devexit tpd_remove(struct i2c_client *client)
 
 {
   
	 TPD_DEBUG("TPD removed\n");
 
   return 0;
 }
 
 
 static int tpd_local_init(void)
 {

 
  	TPD_DMESG("Mstar msg2133 I2C Touchscreen Driver (Built %s @ %s)\n", __DATE__, __TIME__);
 
 
    if(i2c_add_driver(&tpd_i2c_driver)!=0)
   	{
  		TPD_DMESG("msg2133 unable to add i2c driver.\n");
      	return -1;
    }
    if(tpd_load_status == 0) 
    {
    	TPD_DMESG("msg2133 add error touch panel driver.\n");
    	i2c_del_driver(&tpd_i2c_driver);
    	return -1;
    }
	
#ifdef TPD_HAVE_BUTTON     
    tpd_button_setting(TPD_KEY_COUNT, tpd_keys_local, tpd_keys_dim_local);// initialize tpd button data
#endif   
  
//#if (defined(TPD_WARP_START) && defined(TPD_WARP_END))    
//WARP CHECK IS NEED --XB.PANG
//#endif 

	TPD_DMESG("end %s, %d\n", __FUNCTION__, __LINE__);  
		
    return 0; 
 }

 static void tpd_resume( struct early_suspend *h )
 {
 
   TPD_DMESG("TPD wake up\n");
#if 0//def TPD_PROXIMITY
	if(g_bPsSensorOpen == 0 && (!g_bSuspend))
	{
		TPD_DMESG("msg sensor resume in calling tp no need to resume\n");
		return 0;
	}
	g_bSuspend = 0;
#endif
   
#ifdef TPD_CLOSE_POWER_IN_SLEEP	
	hwPowerOn(TPD_POWER_SOURCE,VOL_2800,"TP");
#endif
	msleep(100);
	mt_set_gpio_mode(GPIO_CTP_RST_PIN, GPIO_CTP_RST_PIN_M_GPIO);
    mt_set_gpio_dir(GPIO_CTP_RST_PIN, GPIO_DIR_OUT);
	mt_set_gpio_out(GPIO_CTP_RST_PIN, GPIO_OUT_ONE);
	mt_set_gpio_mode(GPIO_CTP_RST_PIN, GPIO_CTP_RST_PIN_M_GPIO);
    mt_set_gpio_dir(GPIO_CTP_RST_PIN, GPIO_DIR_OUT);
    mt_set_gpio_out(GPIO_CTP_RST_PIN, GPIO_OUT_ZERO);  
	msleep(50);
	TPD_DMESG(" msg2133 reset\n");
	mt_set_gpio_mode(GPIO_CTP_RST_PIN, GPIO_CTP_RST_PIN_M_GPIO);
    mt_set_gpio_dir(GPIO_CTP_RST_PIN, GPIO_DIR_OUT);
    mt_set_gpio_out(GPIO_CTP_RST_PIN, GPIO_OUT_ONE);
	msleep(200);
	mt_eint_unmask(CUST_EINT_TOUCH_PANEL_NUM);
	TPD_DMESG("TPD wake up done\n");
	
#ifdef TPD_PROXIMITY
	if(g_bPsSensorOpen == 1)
	{
		TPD_DMESG("msg sensor resume in calling  \n");
		msg2133_enable_ps(1);// CTP Proximity function open command  again ;
	}
#endif	
 }

 static void tpd_suspend( struct early_suspend *h )
 {
 	
#ifdef TPD_PROXIMITY
	if(g_bPsSensorOpen == 1)//if(g_nPsSensorDate == 0)
	{
		TPD_DMESG("msg suspend in calling tp no need to suspend\n");
		return 0;
	}
	g_bSuspend = 1;
#endif
 	
	TPD_DMESG("TPD enter sleep\n");
	mt_eint_mask(CUST_EINT_TOUCH_PANEL_NUM);
	
	mt_set_gpio_mode(GPIO_CTP_RST_PIN, GPIO_CTP_RST_PIN_M_GPIO);
    mt_set_gpio_dir(GPIO_CTP_RST_PIN, GPIO_DIR_OUT);
    mt_set_gpio_out(GPIO_CTP_RST_PIN, GPIO_OUT_ZERO);  
	 
#ifdef TPD_CLOSE_POWER_IN_SLEEP	
	hwPowerDown(TPD_POWER_SOURCE,"TP");
#else
	//TP enter sleep mode----XB.PANG NEED CHECK
	//if have sleep mode
#endif
    TPD_DMESG("TPD enter sleep done\n");
 } 


 static struct tpd_driver_t tpd_device_driver = {
		 .tpd_device_name = "msg2133",
		 .tpd_local_init = tpd_local_init,
		 .suspend = tpd_suspend,
		 .resume = tpd_resume,
#ifdef TPD_HAVE_BUTTON
		 .tpd_have_button = 1,
#else
		 .tpd_have_button = 0,
#endif		
 };
 /* called when loaded into kernel */
 static int __init tpd_driver_init(void) {
	 TPD_DEBUG("MediaTek MSG2133 touch panel driver init\n");
	   i2c_register_board_info(TPD_I2C_NUMBER, &msg2133_i2c_tpd, 1);
		 if(tpd_driver_add(&tpd_device_driver) < 0)
			 TPD_DMESG("add MSG2133 driver failed\n");
	 return 0;
 }
 
 /* should never be called */
 static void __exit tpd_driver_exit(void) {
	 TPD_DMESG("MediaTek MSG2133 touch panel driver exit\n");
	 tpd_driver_remove(&tpd_device_driver);
 }
 
 module_init(tpd_driver_init);
 module_exit(tpd_driver_exit);


