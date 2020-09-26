#include "func.h"

#include <stdio.h>
#include <string.h>
#include <math.h>

const struct RTC_Time DefaultTime = {0, 0, 0, 4, 1, 10, 20, 0, 0}; /* 2020年10月1日，星期4，00:00:00，Is_12hr = 0，PM = 0  */

char String[256];
uint8_t ResetInfo;
struct RTC_Time Time;
struct RTC_Alarm Alarm;
struct TH_Value Sensor;

static void Power_EnableGDEH029A1(void);
static void Power_DisableGDEH029A1(void);
static void Power_EnableSHT30_I2C(void);
static void Power_DisableSHT30_I2C(void);
static void Power_EnableADC(void);
static void Power_DisableADC(void);
static void Power_EnableBUZZER(void);
static void Power_DisableBUZZER(void);
static void DumpRTCReg(void);
static void DumpEEPROM(void);

/**
 * @brief  延时100ns的倍数（不准确，只是大概）。
 * @param  nsX100 延时时间。
 */
static void Delay_100ns(volatile uint16_t nsX100)
{
    while (nsX100)
    {
        nsX100--;
    }
    ((void)nsX100);
}

/* ==================== 菜单 ==================== */

static void Menu_MainMenu(void) /* 主菜单 */
{
}

static void Menu_SetTime(void) /* 时间设置页面 */
{
}

static void FullInit(void) /* 重新初始化全部数据 */
{
    char str_tmp[10];
    Power_EnableBUZZER();
    BUZZER_SetVolume(5);

    BUZZER_Beep(1000, 49);
    LL_mDelay(49);
    BUZZER_Beep(1000, 49);
    LL_mDelay(49);
    BUZZER_Beep(1000, 49);

    SERIAL_DebugPrint("BEGIN RESET ALL DATA");
    SERIAL_DebugPrint("RTC reset...");
    if (RTC_ResetAllRegToDefault() != 0)
    {
        SERIAL_DebugPrint("RTC reset fail");
    }
    else
    {
        SERIAL_DebugPrint("RTC reset done");
    }
    SERIAL_DebugPrint("SENSOR reset...");
    if (TH_SoftReset() != 0)
    {
        SERIAL_DebugPrint("SENSOR reset fail");
    }
    else
    {
        SERIAL_DebugPrint("SENSOR reset done");
    }
    SERIAL_DebugPrint("BKPR reset...");
    if (BKPR_ResetAll() != 0)
    {
        SERIAL_DebugPrint("BKPR reset fail");
    }
    else
    {
        SERIAL_DebugPrint("BKPR reset done");
    }
    SERIAL_DebugPrint("EEPROM reset...");
    snprintf(str_tmp, sizeof(str_tmp), "EEPROM: %d", EEPROM_WriteByte(0, 0xFF));
    SERIAL_SendStringRN(str_tmp);
    //if (EEPROM_EraseRange(0, 510) != 0)
    //{
    //    SERIAL_DebugPrint("EEPROM reset fail");
    //}
    //else
    //{
    //    SERIAL_DebugPrint("EEPROM reset done");
    //}
    SERIAL_DebugPrint("Process done");

    DumpEEPROM();

    BUZZER_Beep(1000, 199);
    Power_DisableBUZZER();
}

/* ==================== 主要功能 ==================== */

static void UpdateDisplay(void) /* 更新显示时间和温度等数据 */
{
    RTC_GetTime(&Time);
    snprintf(String, sizeof(String), "RTC: 2%03d %d %d %d %02d:%02d:%02d TEMP:%5.2f", Time.Year, Time.Month, Time.Date, Time.Day, Time.Hours, Time.Minutes, Time.Seconds, RTC_GetTemp());
    SERIAL_SendStringRN(String);

    TH_GetValue_SingleShotWithCS(TH_ACC_HIGH, &Sensor);
    snprintf(String, sizeof(String), "TH : TEMP:%5.2f RH:%5.2f STATUS:0x%02X", Sensor.CEL, Sensor.RH, TH_GetStatus());
    SERIAL_SendStringRN(String);

    snprintf(String, sizeof(String), "ADC: VDDA:%5.2f TEMP:%5.2f CH1:%5.2f", ADC_GetVDDA(), ADC_GetTemp(), ADC_GetChannel(ADC_CHANNEL_BATTERY));
    SERIAL_SendStringRN(String);

    RTC_ModifyAM2Mask(0x07);
    RTC_ModifyA2IE(1);
    RTC_ModifyINTCN(1);
}

/* ==================== 主循环 ==================== */

void Init(void) /* 系统复位后首先进入此函数并执行一次 */
{
    SERIAL_SendStringRN("\r\n\r\n***** SYSTEM RESET *****\r\n");

    LL_mDelay(19);                                                   /* 防止下载预复位时打印出来东西，非调试时可以去掉 */
    LL_EXTI_DisableIT_0_31(EPD_BUSY_EXTI0_EXTI_IRQn);                /* 禁用唤醒中断 */
    LL_SYSCFG_VREFINT_SetConnection(LL_SYSCFG_VREFINT_CONNECT_NONE); /* 禁用VREFINT输出 */
    ResetInfo = LP_GetResetInfo();
    switch (ResetInfo)
    {
    case LP_RESET_POWERON:
        SERIAL_DebugPrint("Power on reset");
        break;
    case LP_RESET_NORMALRESET:
        SERIAL_DebugPrint("Normal reset");
        break;
    case LP_RESET_WKUPSTANDBY:
        SERIAL_DebugPrint("Wakeup from standby");
        break;
    }

    Power_DisableGDEH029A1(); /* 默认关闭EPD电源 */
    Power_DisableBUZZER();    /* 默认关闭蜂鸣器电源 */
    Power_EnableSHT30_I2C();  /* 默认打开SHT30和I2C电源 */
    Power_EnableADC();        /* 默认打开ADC电源 */
}

void Loop(void) /* 在Init()执行完成后循环执行 */
{
    switch (ResetInfo)
    {
    case LP_RESET_POWERON:
        if (RTC_GetOSF() != 0)
        {
            RTC_SetTime(&DefaultTime);
            Menu_SetTime();
        }
        break;
    case LP_RESET_NORMALRESET:
        if (LL_GPIO_IsInputPinSet(BTN_UP_GPIO_Port, BTN_UP_Pin) == 0 && LL_GPIO_IsInputPinSet(BTN_DOWN_GPIO_Port, BTN_DOWN_Pin) == 0)
        {
            LL_mDelay(9);
            if (LL_GPIO_IsInputPinSet(BTN_UP_GPIO_Port, BTN_UP_Pin) == 0 && LL_GPIO_IsInputPinSet(BTN_DOWN_GPIO_Port, BTN_DOWN_Pin) == 0)
            {
                FullInit();
                Menu_SetTime();
            }
        }
        break;
    case LP_RESET_WKUPSTANDBY:
        if (RTC_GetA2F() != 0)
        {
            RTC_ClearA2F();
        }
        else
        {
            LL_mDelay(9);
            while (LL_GPIO_IsInputPinSet(BTN_SET_GPIO_Port, BTN_SET_Pin) == 0)
            {
                LL_mDelay(0);
            }
            Menu_MainMenu();
        }
        break;
    }

    UpdateDisplay();

    SERIAL_DebugPrint("Ready to enter standby mode");
    LP_EnterStandby(); /* 程序停止，等待下一次唤醒复位 */

    /* 正常情况下进入Standby模式后，程序会停止在此处，直到下次复位或唤醒再重头开始执行 */

    SERIAL_DebugPrint("Enter standby mode fail");
    LL_mDelay(999); /* 等待1000ms */
    SERIAL_DebugPrint("Try to enter standby mode again");
    LP_EnterStandby(); /* 再次尝试进去Standby模式 */
    SERIAL_DebugPrint("Enter standby mode fail");
    SERIAL_DebugPrint("Try to reset the system");
    NVIC_SystemReset(); /* 两次未成功进入Standby模式，执行软复位 */
}

/* ==================== 电源控制 ==================== */

static void Power_EnableGDEH029A1(void)
{
    LL_GPIO_ResetOutputPin(EPD_POWER_GPIO_Port, EPD_POWER_Pin);
    LL_GPIO_SetOutputPin(EPD_CS_PORT, EPD_CS_PIN);
    LL_GPIO_SetOutputPin(EPD_DC_PORT, EPD_DC_PIN);
    LL_GPIO_SetOutputPin(EPD_RST_PORT, EPD_RST_PIN);
    Delay_100ns(100); /* 10us，未要求，短暂延时 */
    if (LL_SPI_IsEnabled(SPI1) == 0)
    {
        LL_SPI_Enable(SPI1);
    }
}

static void Power_DisableGDEH029A1(void)
{
    if (LL_SPI_IsEnabled(SPI1) != 0)
    {
        LL_SPI_Disable(SPI1);
    }
    LL_GPIO_ResetOutputPin(EPD_RST_PORT, EPD_RST_PIN);
    LL_GPIO_ResetOutputPin(EPD_DC_PORT, EPD_DC_PIN);
    LL_GPIO_ResetOutputPin(EPD_CS_PORT, EPD_CS_PIN);
    LL_GPIO_SetOutputPin(EPD_POWER_GPIO_Port, EPD_POWER_Pin);
}

static void Power_EnableSHT30_I2C(void)
{
    LL_GPIO_ResetOutputPin(SHT30_POWER_GPIO_Port, SHT30_POWER_Pin); /* 打开SHT30电源 */
    LL_GPIO_SetOutputPin(I2C1_PULLUP_GPIO_Port, I2C1_PULLUP_Pin);   /* 打开I2C上拉电阻 */
    LL_GPIO_SetOutputPin(SHT30_RST_GPIO_Port, SHT30_RST_Pin);       /* 释放SHT30复位引脚 */
    Delay_100ns(20);                                                /* 最少1us宽度，设置为2us */
    LL_GPIO_ResetOutputPin(SHT30_RST_GPIO_Port, SHT30_RST_Pin);     /* SHT30硬复位 */
    Delay_100ns(20);                                                /* 最少1us宽度，设置为2us */
    LL_GPIO_SetOutputPin(SHT30_RST_GPIO_Port, SHT30_RST_Pin);       /* SHT30硬复位 */
    LL_mDelay(1);                                                   /* SHT30复位后需要最少1ms启动时间，设置为2ms */
    if (LL_I2C_IsEnabled(I2C1) == 0)                                /* 打开I2C */
    {
        LL_I2C_Enable(I2C1);
    }
}

static void Power_DisableSHT30_I2C(void)
{
    if (LL_I2C_IsEnabled(I2C1) != 0) /* 关闭I2C */
    {
        LL_I2C_Disable(I2C1);
    }
    LL_GPIO_ResetOutputPin(I2C1_PULLUP_GPIO_Port, I2C1_PULLUP_Pin); /* 关闭I2C上拉电阻 */
    LL_GPIO_ResetOutputPin(SHT30_RST_GPIO_Port, SHT30_RST_Pin);     /* 拉低SHT30复位引脚 */
    LL_GPIO_SetOutputPin(SHT30_POWER_GPIO_Port, SHT30_POWER_Pin);   /* 关闭SHT30电源 */
}

static void Power_EnableADC(void)
{
    ADC_Disable();
    ADC_StartCal();
    ADC_Enable();
}

static void Power_DisableADC(void)
{
    ADC_Disable();
}

static void Power_EnableBUZZER(void)
{
    BUZZER_Enable();
    BUZZER_SetVolume(10);
}

static void Power_DisableBUZZER(void)
{
    BUZZER_Disable();
}

/* ==================== 辅助功能 ==================== */

static void DumpRTCReg(void)
{
    uint8_t i, j, reg_tmp;
    char byte_str[9];
    SERIAL_SendStringRN("= DS3231 REG =");
    for (i = 0; i < 19; i++)
    {
        reg_tmp = RTC_ReadREG(RTC_REG_SEC + i);
        for (j = 0; j < 8; j++)
        {
            if ((reg_tmp & (0x80 >> j)) != 0)
            {
                byte_str[j] = '1';
            }
            else
            {
                byte_str[j] = '0';
            }
        }
        byte_str[8] = '\0';
        SERIAL_SendString(byte_str);
        snprintf(byte_str, sizeof(byte_str), " 0x%02X", reg_tmp);
        SERIAL_SendStringRN(byte_str);
    }
    SERIAL_SendStringRN("==============");
    SERIAL_SendStringRN("");
}

static void DumpEEPROM(void)
{
    uint16_t i;
    char byte_str[9];
    SERIAL_SendStringRN("= EEPROM =");
    for (i = 0; i < 2048; i++)
    {
        if (i % 16 == 0)
        {
            SERIAL_SendStringRN("");
        }
        snprintf(byte_str, sizeof(byte_str), "0x%02X ", EEPROM_ReadByte(i));
        SERIAL_SendString(byte_str);
    }
    SERIAL_SendStringRN("");
    SERIAL_SendStringRN("==========");
    SERIAL_SendStringRN("");
}
