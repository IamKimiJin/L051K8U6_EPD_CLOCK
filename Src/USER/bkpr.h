#ifndef _BKPR_H_
#define _BKPR_H_

#include "main.h"

/* 可修改 */
#define RTC_BACKUPREG_BASEADDR 0x40002850 /* 0x40002800 + 0x50 = 0x40002850 */
/* 结束 */

uint8_t BKPR_ReadByte(uint8_t addr);
uint16_t BKPR_ReadWORD(uint8_t addr);
uint32_t BKPR_ReadDWORD(uint8_t addr);

uint8_t BKPR_WriteByte(uint8_t addr, uint8_t data);
uint8_t BKPR_WriteWORD(uint8_t addr, uint16_t data);
uint8_t BKPR_WriteDWORD(uint8_t addr, uint32_t data);

uint8_t BKPR_ResetAll(void);

#endif
