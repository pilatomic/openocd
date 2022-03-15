/***************************************************************************
 *   Copyright (C) 2005 by Dominic Rath                                    *
 *   Dominic.Rath@gmx.de                                                   *
 *                                                                         *
 *   Copyright (C) 2008 by Spencer Oliver                                  *
 *   spen@spen-soft.co.uk                                                  *
 *                                                                         *
 *   Copyright (C) 2011 Øyvind Harboe                                      *
 *   oyvind.harboe@zylin.com                                               *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program.  If not, see <http://www.gnu.org/licenses/>. *
 ***************************************************************************/

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "imp.h"
#include <helper/binarybuffer.h>
#include <target/algorithm.h>
#include <target/cortex_m.h>



/* Erase time can be as high as 1000ms, 10x this and it's toast... */
#define FLASH_ERASE_TIMEOUT 10000
#define FLASH_WRITE_TIMEOUT 5

/* Mass erase time can be as high as 32 s in x8 mode. */
#define FLASH_MASS_ERASE_TIMEOUT 33000

#define FLASH_BANK_BASE     0x80000000

#define STM32F2_OTP_SIZE 512
#define STM32F2_OTP_SECTOR_SIZE 32
#define STM32F2_OTP_BANK_BASE       0x1fff7800
#define STM32F2_OTP_LOCK_BASE       ((STM32F2_OTP_BANK_BASE) + (STM32F2_OTP_SIZE))

/* see RM0410 section 3.6 "One-time programmable bytes" */
#define STM32F7_OTP_SECTOR_SIZE 64
#define STM32F7_OTP_SIZE 1024
#define STM32F7_OTP_BANK_BASE       0x1ff0f000
#define STM32F7_OTP_LOCK_BASE       ((STM32F7_OTP_BANK_BASE) + (STM32F7_OTP_SIZE))

#define STM32_FLASH_BASE    0x40023c00
#define STM32_FLASH_ACR     0x40023c00
#define STM32_FLASH_KEYR    0x40023c04
#define STM32_FLASH_OPTKEYR 0x40023c08
#define STM32_FLASH_SR      0x40023c0C
#define STM32_FLASH_CR      0x40023c10
#define STM32_FLASH_OPTCR   0x40023c14
#define STM32_FLASH_OPTCR1  0x40023c18
#define STM32_FLASH_OPTCR2  0x40023c1c

/* FLASH_CR register bits */
#define FLASH_PG       (1 << 0)
#define FLASH_SER      (1 << 1)
#define FLASH_MER      (1 << 2)		/* MER/MER1 for f76x/77x */
#define FLASH_MER1     (1 << 15)	/* MER2 for f76x/77x, confusing ... */
#define FLASH_STRT     (1 << 16)
#define FLASH_PSIZE_8  (0 << 8)
#define FLASH_PSIZE_16 (1 << 8)
#define FLASH_PSIZE_32 (2 << 8)
#define FLASH_PSIZE_64 (3 << 8)
/* The sector number encoding is not straight binary for dual bank flash. */
#define FLASH_SNB(a)   ((a) << 3)
#define FLASH_LOCK     (1 << 31)

/* FLASH_SR register bits */
#define FLASH_BSY      (1 << 16)
#define FLASH_PGSERR   (1 << 7) /* Programming sequence error */
#define FLASH_PGPERR   (1 << 6) /* Programming parallelism error */
#define FLASH_PGAERR   (1 << 5) /* Programming alignment error */
#define FLASH_WRPERR   (1 << 4) /* Write protection error */
#define FLASH_OPERR    (1 << 1) /* Operation error */

#define FLASH_ERROR (FLASH_PGSERR | FLASH_PGPERR | FLASH_PGAERR | FLASH_WRPERR | FLASH_OPERR)

/* STM32_FLASH_OPTCR register bits */
#define OPTCR_LOCK     (1 << 0)
#define OPTCR_START    (1 << 1)
#define OPTCR_NDBANK   (1 << 29)	/* not dual bank mode */
#define OPTCR_DB1M     (1 << 30)	/* 1 MiB devices dual flash bank option */
#define OPTCR_SPRMOD   (1 << 31)	/* switches PCROPi/nWPRi interpretation */

/* STM32_FLASH_OPTCR2 register bits */
#define OPTCR2_PCROP_RDP	(1 << 31)	/* erase PCROP zone when decreasing RDP */

/* register unlock keys */
#define KEY1           0x45670123
#define KEY2           0xCDEF89AB

/* option register unlock key */
#define OPTKEY1        0x08192A3B
#define OPTKEY2        0x4C5D6E7F


struct at32x_flash_bank {
	bool probed;
};

static int at32x_probe(struct flash_bank *bank)
{
    struct at32x_flash_bank *at32x_info = bank->driver_priv;

    at32x_info->probed = false;

    LOG_ERROR("Cannot identify target as a Artery AT32 family.");

    return ERROR_FAIL;
}

static int at32x_auto_probe(struct flash_bank *bank)
{
    struct at32x_flash_bank *at32x_info = bank->driver_priv;
    if (at32x_info->probed)
		return ERROR_OK;
    return at32x_probe(bank);
}

const struct flash_driver artery_at32x_flash = {
    .name = "artery_at32x",
    .commands = NULL,
    .flash_bank_command = NULL,
    .erase = NULL,
    .protect = NULL,
    .write = NULL,
	.read = default_flash_read,
    .probe = at32x_probe,
    .auto_probe = at32x_auto_probe,
	.erase_check = default_flash_blank_check,
    .protect_check = NULL,
    .info = NULL,
	.free_driver_priv = default_flash_free_driver_priv,
};
