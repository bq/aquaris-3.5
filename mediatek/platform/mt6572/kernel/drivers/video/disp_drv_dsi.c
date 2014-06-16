#ifdef BUILD_UBOOT
#include <asm/arch/disp_drv_platform.h>
#else

#include <linux/delay.h>
#include <linux/kernel.h>
#include <linux/string.h>
#include <linux/semaphore.h>
#include <mach/m4u.h>
#include "disp_drv_log.h"

#include "disp_drv_platform.h"
#include "lcd_drv.h"
#include "dpi_drv.h"
#include "dsi_drv.h"

#include "lcm_drv.h"
#endif

#define ALIGN_TO(x, n)  \
	(((x) + ((n) - 1)) & ~((n) - 1))

// ---------------------------------------------------------------------------
//  Private Variables
// ---------------------------------------------------------------------------
static UINT32 dsiTmpBufBpp = 0;
extern LCM_DRIVER *lcm_drv;
extern LCM_PARAMS *lcm_params;
extern unsigned int is_video_mode_running;
extern BOOL DDMS_capturing;

typedef struct
{
    UINT32 pa;
    UINT32 pitchInBytes;
} TempBuffer;

#ifndef MT65XX_NEW_DISP
static TempBuffer s_tmpBuffers[3];
#endif
static bool needStartDSI = true;

// ---------------------------------------------------------------------------
//  Private Functions
// ---------------------------------------------------------------------------

static void init_lcd_te_control(void)
{
    const LCM_DBI_PARAMS *dbi = &(lcm_params->dbi);

    LCD_CHECK_RET(LCD_TE_Enable(FALSE));
	if(!DISP_IsLcmFound())
        return;
#ifdef BUILD_UBOOT
    {
        extern BOOTMODE g_boot_mode;
        printf("boot_mode = %d\n",g_boot_mode);
        if(g_boot_mode == META_BOOT)
            return;
    }
#endif
    if (LCM_DBI_TE_MODE_DISABLED == dbi->te_mode) {
        LCD_CHECK_RET(LCD_TE_Enable(FALSE));
        return;
    }

    if (LCM_DBI_TE_MODE_VSYNC_ONLY == dbi->te_mode) {
        LCD_CHECK_RET(LCD_TE_SetMode(LCD_TE_MODE_VSYNC_ONLY));
    } else if (LCM_DBI_TE_MODE_VSYNC_OR_HSYNC == dbi->te_mode) {
        LCD_CHECK_RET(LCD_TE_SetMode(LCD_TE_MODE_VSYNC_OR_HSYNC));
        LCD_CHECK_RET(LCD_TE_ConfigVHSyncMode(dbi->te_hs_delay_cnt,
                                              dbi->te_vs_width_cnt,
                     (LCD_TE_VS_WIDTH_CNT_DIV)dbi->te_vs_width_cnt_div));
    } else ASSERT(0);

    LCD_CHECK_RET(LCD_TE_SetEdgePolarity(dbi->te_edge_polarity));
    LCD_CHECK_RET(LCD_TE_Enable(TRUE));
}

__inline DPI_FB_FORMAT get_dsi_tmp_buffer_format(void)
{
    switch(lcm_params->dsi.data_format.format)
    {
    case LCM_DSI_FORMAT_RGB565 : return 0;
    case LCM_DSI_FORMAT_RGB666 :
    case LCM_DSI_FORMAT_RGB888 : return 1;
    default : ASSERT(0);
    }
    return 1;
}


__inline UINT32 get_dsi_tmp_buffer_bpp(void)
{
    static const UINT32 TO_BPP[] = {2, 3};
    return TO_BPP[get_dsi_tmp_buffer_format()];
}


__inline LCD_FB_FORMAT get_lcd_tmp_buffer_format(void)
{
    static const UINT32 TO_LCD_FORMAT[] = {
        LCD_FB_FORMAT_RGB565,
        LCD_FB_FORMAT_RGB888
    };
    
    return TO_LCD_FORMAT[get_dsi_tmp_buffer_format()];
}

static __inline LCD_IF_WIDTH to_lcd_if_width(LCM_DBI_DATA_WIDTH data_width)
{
    switch(data_width)
    {
    case LCM_DBI_DATA_WIDTH_8BITS  : return LCD_IF_WIDTH_8_BITS;
    case LCM_DBI_DATA_WIDTH_9BITS  : return LCD_IF_WIDTH_9_BITS;
    case LCM_DBI_DATA_WIDTH_16BITS : return LCD_IF_WIDTH_16_BITS;
    case LCM_DBI_DATA_WIDTH_18BITS : return LCD_IF_WIDTH_18_BITS;
    case LCM_DBI_DATA_WIDTH_24BITS : return LCD_IF_WIDTH_24_BITS;
    default : ASSERT(0);
    }
    return LCD_IF_WIDTH_18_BITS;
}

static BOOL disp_drv_dsi_init_context(void)
{
    if (lcm_drv != NULL && lcm_params != NULL){
        dsiTmpBufBpp=get_dsi_tmp_buffer_bpp();
        return TRUE;
    }

    if (NULL == lcm_drv) {
        return FALSE;
    }

    lcm_drv->get_params(lcm_params);

    dsiTmpBufBpp=get_dsi_tmp_buffer_bpp();
    
    return TRUE;
}

#ifndef MT65XX_NEW_DISP
static void init_intermediate_buffers(UINT32 fbPhysAddr)
{
    UINT32 tmpFbStartPA = fbPhysAddr;

    UINT32 tmpFbPitchInBytes = DISP_GetScreenWidth() * dsiTmpBufBpp;
    UINT32 tmpFbSizeInBytes  = tmpFbPitchInBytes * DISP_GetScreenHeight();

    UINT32 i;

    DISP_LOG_PRINT(ANDROID_LOG_INFO, "DSI", "init_intermediate_buffers \n");
    DISP_LOG_PRINT(ANDROID_LOG_INFO, "DSI", "tmpFbStartPA=%x, fbPhysAddr=%x \n", tmpFbStartPA, fbPhysAddr);
    
    for (i = 0; i < lcm_params->dsi.intermediat_buffer_num; ++ i)
    {
        TempBuffer *b = &s_tmpBuffers[i];
        #ifdef BUILD_UBOOT
        // clean the intermediate buffers as black to prevent from noise display
        memset(tmpFbStartPA, 0, tmpFbSizeInBytes);
        #endif
        b->pitchInBytes = tmpFbPitchInBytes;
        b->pa = tmpFbStartPA;
        ASSERT((tmpFbStartPA & 0x7) == 0);  // check if 8-byte-aligned
        tmpFbStartPA += tmpFbSizeInBytes;
    }

}

static void init_lcd(void)
{
    UINT32 i;		
    
    // Config LCD Controller
    LCD_CHECK_RET(LCD_LayerEnable(LCD_LAYER_ALL, FALSE));
    LCD_CHECK_RET(LCD_LayerSetTriggerMode(LCD_LAYER_ALL, LCD_SW_TRIGGER));
    LCD_CHECK_RET(LCD_EnableHwTrigger(FALSE));
    
    LCD_CHECK_RET(LCD_SetBackgroundColor(0));
    
    if(lcm_params->dsi.mode == CMD_MODE)
        LCD_CHECK_RET(LCD_SetRoiWindow(0, 0, lcm_params->width, lcm_params->height));
    else
        LCD_CHECK_RET(LCD_SetRoiWindow(0, 0, DISP_GetScreenWidth(), DISP_GetScreenHeight()));
    
    if(lcm_params->dsi.mode == CMD_MODE) {
        LCD_CHECK_RET(LCD_ConfigDSIIfFormat(lcm_params->dsi.data_format.color_order,
                                                                            lcm_params->dsi.data_format.trans_seq,
                                                                            lcm_params->dsi.data_format.padding,
                                                                            lcm_params->dsi.data_format.format,
                                                                            lcm_params->dsi.packet_size,
                                                                            TRUE));
        
        LCD_CHECK_RET(LCD_SetOutputMode(LCD_OUTPUT_TO_LCM));
        LCD_CHECK_RET(LCD_WaitDPIIndication(FALSE));
        LCD_CHECK_RET(LCD_FBEnable(LCD_FB_0, FALSE));
        LCD_CHECK_RET(LCD_FBEnable(LCD_FB_1, FALSE));
        LCD_CHECK_RET(LCD_FBEnable(LCD_FB_2, FALSE));
    } 
    else {
        LCD_CHECK_RET(LCD_FBSetFormat(get_lcd_tmp_buffer_format()));
        LCD_CHECK_RET(LCD_FBSetPitch(s_tmpBuffers[0].pitchInBytes));
        LCD_CHECK_RET(LCD_FBSetStartCoord(0, 0));
        
        for (i = 0; i < lcm_params->dsi.intermediat_buffer_num; ++ i)
        {
            LCD_CHECK_RET(LCD_FBSetAddress(LCD_FB_0 + i, s_tmpBuffers[i].pa));
            LCD_CHECK_RET(LCD_FBEnable(LCD_FB_0 + i, TRUE));
        }
        
        LCD_CHECK_RET(LCD_SetOutputMode(LCD_OUTPUT_TO_MEM));
        LCD_CHECK_RET(LCD_WaitDPIIndication(TRUE));
    }
}


static void init_dpi(BOOL isDpiPoweredOn)
{
    const LCM_DSI_PARAMS *dsi = &(lcm_params->dsi);
    UINT32 i;

    DPI_CHECK_RET(DPI_Init(isDpiPoweredOn));

    DPI_CHECK_RET(DPI_EnableSeqOutput(FALSE));

    DPI_CHECK_RET(DPI_FBSetSize(DISP_GetScreenWidth(), DISP_GetScreenHeight()));
    
    for (i = 0; i < lcm_params->dsi.intermediat_buffer_num; ++ i)
    {
        DPI_CHECK_RET(DPI_FBSetAddress(DPI_FB_0 + i, s_tmpBuffers[i].pa));
        DPI_CHECK_RET(DPI_FBSetPitch(DPI_FB_0 + i, s_tmpBuffers[i].pitchInBytes));
        DPI_CHECK_RET(DPI_FBEnable(DPI_FB_0 + i, TRUE));
    }
    DPI_CHECK_RET(DPI_FBSetFormat(get_dsi_tmp_buffer_format()));
    DPI_CHECK_RET(DPI_FBSyncFlipWithLCD(TRUE));

    if (LCM_COLOR_ORDER_BGR == dsi->data_format.color_order) {
        DPI_CHECK_RET(DPI_SetRGBOrder(DPI_RGB_ORDER_RGB, DPI_RGB_ORDER_BGR));
    } else {
        DPI_CHECK_RET(DPI_SetRGBOrder(DPI_RGB_ORDER_RGB, DPI_RGB_ORDER_RGB));
    }

    DPI_CHECK_RET(DPI_SetDSIMode(TRUE));

}
#endif


void init_dsi(BOOL isDsiPoweredOn)
{
    DSI_PHY_clk_setting(lcm_params);

    // DISP_LOG_PRINT(ANDROID_LOG_INFO, "DSI", "%s, line:%d\n", __func__, __LINE__);
    DSI_CHECK_RET(DSI_Init(isDsiPoweredOn));

    // DISP_LOG_PRINT(ANDROID_LOG_INFO, "DSI", "%s, line:%d\n", __func__, __LINE__);
    //if(1 == lcm_params->dsi.compatibility_for_nvk){
    if(0){
        DSI_CHECK_RET(DSI_TXRX_Control(TRUE,                    //cksm_en
                                   TRUE,                    //ecc_en
                                   lcm_params->dsi.LANE_NUM, //ecc_en
                                   0,                       //vc_num
                                   FALSE,                   //null_packet_en
                                   FALSE,                   //err_correction_en
                                   FALSE,                   //dis_eotp_en
                                   0));                     //max_return_size
        DSI_set_noncont_clk(false,0);
    }
    else
    {
        DSI_CHECK_RET(DSI_TXRX_Control(TRUE,                    //cksm_en
                                       TRUE,                    //ecc_en
                                       lcm_params->dsi.LANE_NUM, //ecc_en
                                       0,                       //vc_num
                                       FALSE,                   //null_packet_en
                                       FALSE,                   //err_correction_en
                                       FALSE,                   //dis_eotp_en
                                       0));                     //max_return_size
    }

    
    // DISP_LOG_PRINT(ANDROID_LOG_INFO, "DSI", "%s, line:%d\n", __func__, __LINE__);
    //initialize DSI_PHY
    DSI_PHY_clk_switch(TRUE);
    DSI_PHY_TIMCONFIG(lcm_params);

    // DISP_LOG_PRINT(ANDROID_LOG_INFO, "DSI", "%s, line:%d\n", __func__, __LINE__);
#ifndef MT65XX_NEW_DISP
    DSI_PHY_clk_setting(lcm_params->dsi.pll_div1, lcm_params->dsi.pll_div2, lcm_params->dsi.LANE_NUM);
#else

//    if (lcm_params->dsi.mode == CMD_MODE)
//    {
//        DSI_PHY_clk_setting(lcm_params);
//    }
    DSI_CHECK_RET(DSI_PS_Control(lcm_params->dsi.PS, lcm_params->height, lcm_params->width * dsiTmpBufBpp));
#endif

    if(lcm_params->dsi.mode != CMD_MODE)
    {
        DSI_Config_VDO_Timing(lcm_params);
		DSI_Set_VM_CMD(lcm_params);
        //if(1 == lcm_params->dsi.compatibility_for_nvk)
        if(0)
            DSI_Config_VDO_FRM_Mode();
#ifndef MT65XX_NEW_DISP
        DSI_CHECK_RET(DSI_PS_Control(lcm_params->dsi.PS, lcm_params->width * dsiTmpBufBpp));
#endif
    }
    
    DSI_CHECK_RET(DSI_enable_MIPI_txio(TRUE));
    
    // DISP_LOG_PRINT(ANDROID_LOG_INFO, "DSI", "%s, line:%d\n", __func__, __LINE__);
}

// ---------------------------------------------------------------------------
//  DBI Display Driver Public Functions
// ---------------------------------------------------------------------------

static DISP_STATUS dsi_init(UINT32 fbVA, UINT32 fbPA, BOOL isLcmInited)
{
    // DISP_LOG_PRINT(ANDROID_LOG_INFO, "DSI", "%s, line:%d\n", __func__, __LINE__);
    if (!disp_drv_dsi_init_context()) 
        return DISP_STATUS_NOT_IMPLEMENTED;

    // DISP_LOG_PRINT(ANDROID_LOG_INFO, "DSI", "%s, line:%d\n", __func__, __LINE__);
    if(lcm_params->dsi.mode == CMD_MODE) {
        #ifndef MT65XX_NEW_DISP
            init_lcd();
        #endif

        init_dsi(isLcmInited);
        mdelay(1);
        
        // DISP_LOG_PRINT(ANDROID_LOG_INFO, "DSI", "%s, line:%d\n", __func__, __LINE__);
        if (NULL != lcm_drv->init && !isLcmInited) 
        {
            lcm_drv->init();
            DSI_LP_Reset();
        }
        // DISP_LOG_PRINT(ANDROID_LOG_INFO, "DSI", "%s, line:%d\n", __func__, __LINE__);
        #ifndef MT65XX_NEW_DISP
            DSI_clk_HS_mode(0);
        #else
            DSI_clk_HS_mode(1);
        #endif

        // DISP_LOG_PRINT(ANDROID_LOG_INFO, "DSI", "%s, line:%d\n", __func__, __LINE__);
        DSI_SetMode(lcm_params->dsi.mode);
        #ifndef MT65XX_NEW_DISP
            init_lcd_te_control();
        #endif
    }
    else {
        #ifndef MT65XX_NEW_DISP
            init_intermediate_buffers(fbPA);
            
            init_lcd();
            init_dpi(isLcmInited);
        #endif

        if (!isLcmInited)
        {
            DSI_SetMode(0);
            mdelay(100);
            DSI_Stop();
        }
        else
        {
            is_video_mode_running = true;
        }
        init_dsi(isLcmInited);
        mdelay(1);
        
        if (NULL != lcm_drv->init && !isLcmInited) {
            lcm_drv->init();
            DSI_LP_Reset();
        }
        
        DSI_SetMode(lcm_params->dsi.mode);
        
        #ifndef BUILD_UBOOT	
            if(lcm_params->dsi.lcm_ext_te_monitor)
            {
                is_video_mode_running = false;
                LCD_TE_SetMode(LCD_TE_MODE_VSYNC_ONLY);
                LCD_TE_SetEdgePolarity(LCM_POLARITY_RISING);
                LCD_TE_Enable(FALSE);
            }
            
            if(lcm_params->dsi.noncont_clock)
                DSI_set_noncont_clk(false, lcm_params->dsi.noncont_clock_period);
            
            if(lcm_params->dsi.lcm_int_te_monitor)
                DSI_set_int_TE(false, lcm_params->dsi.lcm_int_te_period);			
        #endif			
    }

#ifdef MT65XX_NEW_DISP
    {
        struct disp_path_config_struct config = {0};
        config.srcModule = DISP_MODULE_OVL;
        config.bgROI.x = 0;
        config.bgROI.y = 0;
        config.bgROI.width = DISP_GetScreenWidth();
        config.bgROI.height = DISP_GetScreenHeight();
        config.bgColor = 0x0;	// background color
        
        config.pitch = DISP_GetScreenWidth()*2;
        config.srcROI.x = 0;config.srcROI.y = 0;
        config.srcROI.height= DISP_GetScreenHeight();config.srcROI.width= DISP_GetScreenWidth();
        config.ovl_config.source = OVL_LAYER_SOURCE_MEM; 

        config.ovl_config.layer = FB_LAYER;
        config.ovl_config.layer_en = 1; 
        config.ovl_config.fmt = OVL_INPUT_FORMAT_RGB565;
        config.ovl_config.addr = fbPA;	
        config.ovl_config.vaddr = fbVA;	
        config.ovl_config.source = OVL_LAYER_SOURCE_MEM; 
        config.ovl_config.src_x = 0;
        config.ovl_config.src_y = 0;
        config.ovl_config.src_w = DISP_GetScreenWidth();
        config.ovl_config.src_h = DISP_GetScreenHeight();
        config.ovl_config.dst_x = 0;	   // ROI
        config.ovl_config.dst_y = 0;
        config.ovl_config.dst_w = DISP_GetScreenWidth();
        config.ovl_config.dst_h = DISP_GetScreenHeight();
        config.ovl_config.src_pitch = ALIGN_TO(DISP_GetScreenWidth(),32)*2; //pixel number
        config.ovl_config.keyEn = 0;
        config.ovl_config.key = 0xFF;	   // color key
        config.ovl_config.aen = 0;			  // alpha enable
        config.ovl_config.alpha = 0;	

        if(lcm_params->dsi.mode == CMD_MODE)
            config.dstModule = DISP_MODULE_DSI_CMD;// DISP_MODULE_WDMA1
        else
            config.dstModule = DISP_MODULE_DSI_VDO;// DISP_MODULE_WDMA1

        config.outFormat = RDMA_OUTPUT_FORMAT_ARGB; 

        LCD_LayerSetAddress(FB_LAYER, fbPA);
        LCD_LayerSetFormat(FB_LAYER, LCD_LAYER_FORMAT_RGB565);
        LCD_LayerSetOffset(FB_LAYER, 0, 0);
        LCD_LayerSetSize(FB_LAYER,DISP_GetScreenWidth(),DISP_GetScreenHeight());
        LCD_LayerSetPitch(FB_LAYER, ALIGN_TO(DISP_GetScreenWidth(),32) * 2);
        LCD_LayerEnable(FB_LAYER, TRUE);                    

        if(lcm_params->dsi.mode != CMD_MODE){

            #define TIMECNT  1000000
            unsigned int reg1 = 0, reg2 = 0, reg3 = 0;
            unsigned int timeout_cnt = 0;
            unsigned int irq_mask;

            // dump before modification
            printk("[DISP] pa:0x%x, va:0x%x \n", fbPA, fbVA);		
            
            // enable frame done interrupt
            disp_path_get_mutex();
            OVLEnableIrq(0x2);
            disp_path_release_mutex();
            
            while (timeout_cnt < TIMECNT)
            {
                reg1 = DISP_REG_GET(DISP_REG_OVL_INTSTA);
                reg2 = DISP_REG_GET(DISP_REG_OVL_STA);
                // frame done interrupt
                if (((reg1 & 0x2) == 0x2) && ((reg2 & 0x1) == 0x0))
                {
                    DISP_REG_SET(DISP_REG_OVL_INTSTA, ~reg1);     

                    local_irq_save(irq_mask);
                    disp_path_get_mutex();
                    disp_path_config(&config);
                    disp_path_release_mutex();

                    #if 1  // defined(MTK_M4U_SUPPORT)
                    {
                        M4U_PORT_STRUCT portStruct;
                        
                        portStruct.ePortID = M4U_PORT_LCD_OVL;		   //hardware port ID, defined in M4U_PORT_ID_ENUM
                        portStruct.Virtuality = 1;
                        portStruct.Security = 0;
                        portStruct.domain = 3;			  //domain : 0 1 2 3
                        portStruct.Distance = 1;
                        portStruct.Direction = 0;
                        m4u_config_port(&portStruct);
                    }
                    // hook m4u debug callback function
                    m4u_set_tf_callback(M4U_CLNTMOD_DISP, &disp_m4u_dump_reg);
                    #endif
                    local_irq_restore(irq_mask);

                    break;
                }
                timeout_cnt++;
            }
            // sw timeout
            if (timeout_cnt >= TIMECNT)
            {
                printk("[DISP] timeout:%d \n", timeout_cnt);		
                ASSERT(0);
            }
            // dump after modification
            printk("[DISP] cnt:%d \n", timeout_cnt);		
        }
        else
        {
            disp_path_config(&config);

            #if 1  // defined(MTK_M4U_SUPPORT)
            {
                M4U_PORT_STRUCT portStruct;
                
                portStruct.ePortID = M4U_PORT_LCD_OVL;		   //hardware port ID, defined in M4U_PORT_ID_ENUM
                portStruct.Virtuality = 1;
                portStruct.Security = 0;
                portStruct.domain = 3;			  //domain : 0 1 2 3
                portStruct.Distance = 1;
                portStruct.Direction = 0;
                m4u_config_port(&portStruct);
            }
            // hook m4u debug callback function
            m4u_set_tf_callback(M4U_CLNTMOD_DISP, &disp_m4u_dump_reg);
            #endif
        }
    }
#endif
    return DISP_STATUS_OK;
}


// protected by sem_early_suspend, sem_update_screen
static DISP_STATUS dsi_enable_power(BOOL enable)
{
    disp_drv_dsi_init_context();
    
    if(lcm_params->dsi.mode == CMD_MODE) {
    
        if (enable) {
            #if 0
                // Switch bus to MIPI TX.
                DSI_CHECK_RET(DSI_enable_MIPI_txio(TRUE));
                DSI_PHY_clk_switch(1);
                DSI_PHY_clk_setting(lcm_params->dsi.pll_div1, lcm_params->dsi.pll_div2, lcm_params->dsi.LANE_NUM);
                DSI_CHECK_RET(DSI_PowerOn());
                DSI_WaitForNotBusy();		
                DSI_clk_HS_mode(0);
                DSI_clk_ULP_mode(0);			
                DSI_lane0_ULP_mode(0);	
                DSI_Reset();
                LCD_CHECK_RET(LCD_PowerOn());

            #else
                #ifndef MT65XX_NEW_DISP
                    DSI_PHY_clk_switch(1); 
                    DSI_CHECK_RET(DSI_PowerOn());
                    if(Need_Wait_ULPS())
                        Wait_ULPS_Mode();
                    
                    DSI_PHY_clk_setting(lcm_params->dsi.pll_div1, lcm_params->dsi.pll_div2, lcm_params->dsi.LANE_NUM);

                #else
                    // enable MMSYS CG
                    DSI_CHECK_RET(DSI_PowerOn());

                    // initialize clock setting
                    DSI_PHY_clk_setting(lcm_params);

                    // restore dsi register
                    DSI_CHECK_RET(DSI_RestoreRegisters());

                    // enable sleep-out mode
                    DSI_CHECK_RET(DSI_SleepOut());

                    // enter HS mode
                    DSI_PHY_clk_switch(1); 

                    // enter wakeup
                    DSI_CHECK_RET(DSI_Wakeup());

                    // enable clock
                    DSI_CHECK_RET(DSI_EnableClk());

                    // engine reset
                    DSI_Reset();
                #endif
                DSI_CHECK_RET(DSI_enable_MIPI_txio(TRUE));
                
                #ifndef MT65XX_NEW_DISP
                    Wait_WakeUp();
                #endif
            
            #endif
        }
        else {
            // backup dsi register
            DSI_CHECK_RET(DSI_WaitForEngineNotBusy());
            DSI_CHECK_RET(DSI_BackupRegisters());

            // enter ULPS mode
            DSI_clk_ULP_mode(1);
            DSI_lane0_ULP_mode(1);
            mdelay(1);
            
            // disable engine clock
            DSI_CHECK_RET(DSI_DisableClk());
            
            // disable CG
            DSI_CHECK_RET(DSI_PowerOff());

            // disable mipi pll
            DSI_PHY_clk_switch(0);

            // Switch bus to GPIO, then power level will be decided by GPIO setting.
            DSI_CHECK_RET(DSI_enable_MIPI_txio(FALSE));
        }
    } 
    else {
        if (enable) {
            #if 0
                // Switch bus to MIPI TX.
                DSI_CHECK_RET(DSI_enable_MIPI_txio(TRUE));
                DSI_PHY_clk_switch(1);
                DSI_PHY_clk_setting(lcm_params->dsi.pll_div1, lcm_params->dsi.pll_div2, lcm_params->dsi.LANE_NUM);
                DSI_CHECK_RET(DSI_PowerOn());			
                DSI_clk_ULP_mode(0);			
                DSI_lane0_ULP_mode(0);
                DSI_clk_HS_mode(0);	
                DSI_Reset();
                DPI_CHECK_RET(DPI_PowerOn());
                LCD_CHECK_RET(LCD_PowerOn());
            #else
                #ifndef MT65XX_NEW_DISP
                    DSI_PHY_clk_switch(1); 
                    DSI_CHECK_RET(DSI_PowerOn());
                    if(Need_Wait_ULPS())
                        Wait_ULPS_Mode();
                    
                    DSI_PHY_clk_setting(lcm_params->dsi.pll_div1, lcm_params->dsi.pll_div2, lcm_params->dsi.LANE_NUM);
                #else
                    needStartDSI = true;

                    // enable MMSYS CG
                    DSI_CHECK_RET(DSI_PowerOn());

                    // initialize clock setting
                    DSI_PHY_clk_setting(lcm_params);

                    // restore dsi register
                    DSI_CHECK_RET(DSI_RestoreRegisters());

                    // enable sleep-out mode
                    DSI_CHECK_RET(DSI_SleepOut());
                    
                    // enter HS mode
                    DSI_PHY_clk_switch(1); 

                    // enter wakeup
                    DSI_CHECK_RET(DSI_Wakeup());

                    // enable clock
                    DSI_CHECK_RET(DSI_EnableClk());

                    // engine reset
                    DSI_Reset();

                #endif
                DSI_CHECK_RET(DSI_enable_MIPI_txio(TRUE));
                
                #ifndef MT65XX_NEW_DISP
                    Wait_WakeUp();
                #endif
            #endif
        }
        else {
            #ifndef BUILD_UBOOT
                is_video_mode_running = false;
                
                if(lcm_params->dsi.noncont_clock)
                    DSI_set_noncont_clk(false, lcm_params->dsi.noncont_clock_period);
                
                if(lcm_params->dsi.lcm_int_te_monitor)
                    DSI_set_int_TE(false, lcm_params->dsi.lcm_int_te_period);
            #endif

            // backup dsi register
            DSI_CHECK_RET(DSI_WaitForEngineNotBusy());
            DSI_CHECK_RET(DSI_BackupRegisters());

            // enter ULPS mode
            DSI_clk_ULP_mode(1);
            DSI_lane0_ULP_mode(1);
            mdelay(1);

            // disable engine clock
            DSI_CHECK_RET(DSI_DisableClk());

            // disable CG
            DSI_CHECK_RET(DSI_PowerOff());

            // disable mipi pll
            DSI_PHY_clk_switch(0);

            // Switch bus to GPIO, then power level will be decided by GPIO setting.
            DSI_CHECK_RET(DSI_enable_MIPI_txio(FALSE));
        }
    }
    
    return DISP_STATUS_OK;
}


// protected by sem_flipping, sem_early_suspend, sem_overlay_buffer, sem_update_screen
static DISP_STATUS dsi_update_screen(BOOL isMuextLocked)
{
    disp_drv_dsi_init_context();

    DSI_CHECK_RET(DSI_enable_MIPI_txio(TRUE));

    //DSI_CHECK_RET(DSI_handle_TE());
    
    DSI_SetMode(lcm_params->dsi.mode);
#ifndef MT65XX_NEW_DISP
    LCD_CHECK_RET(LCD_StartTransfer(FALSE, isMuextLocked));
#endif

    if (lcm_params->type==LCM_TYPE_DSI && lcm_params->dsi.mode == CMD_MODE && !DDMS_capturing) {
        //if(1 != lcm_params->dsi.compatibility_for_nvk)
        if(1)
        {
            DSI_clk_HS_mode(1);
        }

        #ifndef MT65XX_NEW_DISP
            DSI_CHECK_RET(DSI_Start());
        #else
            DSI_CHECK_RET(DSI_StartTransfer(isMuextLocked));
        #endif
    }
    else if (lcm_params->type==LCM_TYPE_DSI && lcm_params->dsi.mode != CMD_MODE && !DDMS_capturing)
    {
        DSI_clk_HS_mode(1);
        #ifndef MT65XX_NEW_DISP
            DPI_CHECK_RET(DPI_EnableClk());
            DSI_CHECK_RET(DSI_Start());
        #else
            DSI_CHECK_RET(DSI_StartTransfer(isMuextLocked));
        #endif
        #ifndef BUILD_UBOOT
            is_video_mode_running = true;
            
            if(lcm_params->dsi.noncont_clock)
                DSI_set_noncont_clk(true, lcm_params->dsi.noncont_clock_period);
            
            if(lcm_params->dsi.lcm_int_te_monitor)
                DSI_set_int_TE(true, lcm_params->dsi.lcm_int_te_period);
        
        #endif		
    }
    
    if (DDMS_capturing)
        DISP_LOG_PRINT(ANDROID_LOG_INFO, "DSI", "[DISP] kernel - dsi_update_screen. DDMS is capturing. Skip one frame. \n");		
    
    return DISP_STATUS_OK;
}


static UINT32 dsi_get_working_buffer_size(void)
{
    disp_drv_dsi_init_context();

    if(lcm_params->dsi.mode != CMD_MODE) {
    
        return 
                    DISP_GetScreenWidth() *
                    DISP_GetScreenHeight() *
                    dsiTmpBufBpp *
                    lcm_params->dsi.intermediat_buffer_num;
    }
    
    return 0;
}

static UINT32 dsi_get_working_buffer_bpp(void)
{
    disp_drv_dsi_init_context();

    if(lcm_params->dsi.mode != CMD_MODE) {
        return dsiTmpBufBpp;
    }
    
    return 0;
}

static PANEL_COLOR_FORMAT dsi_get_panel_color_format(void)
{
    disp_drv_dsi_init_context();

    {
    
        switch(lcm_params->dsi.data_format.format)
        {
            case LCM_DSI_FORMAT_RGB565 : return PANEL_COLOR_FORMAT_RGB565;
            case LCM_DSI_FORMAT_RGB666 : return PANEL_COLOR_FORMAT_RGB666;
            case LCM_DSI_FORMAT_RGB888 : return PANEL_COLOR_FORMAT_RGB888;
            default : ASSERT(0);
        }
    
    }
}

static UINT32 dsi_get_dithering_bpp(void)
{
    return PANEL_COLOR_FORMAT_TO_BPP(dsi_get_panel_color_format());
}


// protected by sem_early_suspend
DISP_STATUS dsi_capture_framebuffer(UINT32 pvbuf, UINT32 bpp)
{
    DSI_CHECK_RET(DSI_WaitForNotBusy());
    
    DDMS_capturing=1;
    
    if(lcm_params->dsi.mode == CMD_MODE)
    {
        LCD_CHECK_RET(LCD_EnableDCtoDsi(FALSE));
        #ifndef MT65XX_NEW_DISP
            LCD_CHECK_RET(LCD_Capture_Framebuffer(pvbuf, bpp));
        #else
            DSI_CHECK_RET(DSI_Capture_Framebuffer(pvbuf, bpp, true));//cmd mode
        #endif
    }
    else
    {
        #ifndef MT65XX_NEW_DISP
            DPI_CHECK_RET(DPI_Capture_Framebuffer(pvbuf, bpp));
        #else
            DSI_CHECK_RET(DSI_Capture_Framebuffer(pvbuf, bpp, false));//video mode
        #endif
    }
    
    
    if(lcm_params->dsi.mode == CMD_MODE)
    {
        LCD_CHECK_RET(LCD_EnableDCtoDsi(TRUE));
    }
    
    DDMS_capturing=0;
    
    return DISP_STATUS_OK;	
}


// called by "esd_recovery_kthread"
// protected by sem_early_suspend, sem_update_screen
BOOL dsi_esd_check(void)
{
    BOOL result = false;
    
    if(lcm_params->dsi.mode == CMD_MODE)
    {
        result = lcm_drv->esd_check();
        return result;
    }
    else
    {
        #ifndef BUILD_UBOOT
            #ifndef MT65XX_NEW_DISP
                if(lcm_params->dsi.lcm_int_te_monitor)
                    result = DSI_esd_check();
                
                if(result)
                    return true;
                
                if(lcm_params->dsi.lcm_ext_te_monitor)
                    result = LCD_esd_check();
            #else
                result = DSI_esd_check();
                DSI_LP_Reset();
                needStartDSI = true;
                if(!result)
                    dsi_update_screen(false);
            #endif
            return result;
        #endif	
    }

}


// called by "esd_recovery_kthread"
// protected by sem_early_suspend, sem_update_screen
void dsi_esd_reset(void)
{
     /// we assume the power is on here
    ///  what we need is some setting for LCM init
    if(lcm_params->dsi.mode == CMD_MODE) {
        DSI_clk_HS_mode(0);
        DSI_clk_ULP_mode(0);            
        DSI_lane0_ULP_mode(0);  
    }
    else {
    
        DSI_SetMode(CMD_MODE);
        DSI_clk_HS_mode(0);
        // clock/data lane go to Ideal
        DSI_Reset();
        DPI_CHECK_RET(DPI_DisableClk());
    
    }
    
}

const DISP_DRIVER *DISP_GetDriverDSI()
{
    static const DISP_DRIVER DSI_DISP_DRV =
    {
        .init                   = dsi_init,
        .enable_power           = dsi_enable_power,
        .update_screen          = dsi_update_screen,       
        .get_working_buffer_size = dsi_get_working_buffer_size,

        .get_panel_color_format = dsi_get_panel_color_format,
        .get_working_buffer_bpp = dsi_get_working_buffer_bpp,
        .init_te_control        = init_lcd_te_control,
        .get_dithering_bpp	= dsi_get_dithering_bpp,
        .capture_framebuffer	= dsi_capture_framebuffer,
        .esd_reset              = dsi_esd_reset,
        .esd_check				= dsi_esd_check,
    };

    return &DSI_DISP_DRV;
}

