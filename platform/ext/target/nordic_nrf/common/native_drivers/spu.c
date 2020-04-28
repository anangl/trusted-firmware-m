/*
 * Copyright (c) 2020 Nordic Semiconductor ASA. All rights reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "spu.h"
#include <hal/nrf_spu.h>
#include "region_defs.h"

/* Platform-specific configuration */
#define FLASH_SECURE_ATTRIBUTION_REGION_SIZE SPU_FLASH_REGION_SIZE
#define SRAM_SECURE_ATTRIBUTION_REGION_SIZE  SPU_SRAM_REGION_SIZE

#define NUM_FLASH_SECURE_ATTRIBUTION_REGIONS \
	(FLASH_TOTAL_SIZE / FLASH_SECURE_ATTRIBUTION_REGION_SIZE)
#define NUM_SRAM_SECURE_ATTRIBUTION_REGIONS \
	(TOTAL_RAM_SIZE / SRAM_SECURE_ATTRIBUTION_REGION_SIZE)

#define DEVICE_FLASH_BASE_ADDRESS FLASH_BASE_ADDRESS
#define DEVICE_SRAM_BASE_ADDRESS SRAM_BASE_ADDRESS

/* Convenience macros for SPU Non-Secure Callable (NCS) attribution */

/*
 * Determine the SPU Region number the given address belongs to.
 *
 * addr shall be a valid flash memory address
 */
#define FLASH_NSC_REGION_FROM_ADDR(addr) \
	((uint32_t)addr / FLASH_SECURE_ATTRIBUTION_REGION_SIZE)

/*
 * Determine the NSC region size based on a given NCS region base address.
 */
#define FLASH_NSC_SIZE_FROM_ADDR(addr) (FLASH_SECURE_ATTRIBUTION_REGION_SIZE \
	- (((uint32_t)(addr)) % FLASH_SECURE_ATTRIBUTION_REGION_SIZE))

/*
 * Determine the encoded the SPU NCS Region Size value,
 * based on the absolute NCS region size in bytes.
 *
 * size shall be a valid SPU NCS Region size value
 */
#define FLASH_NSC_SIZE_REG(size) ((31 - __builtin_clz(size)) - 4)


void spu_enable_interrupts(void)
{
	nrf_spu_int_enable(NRF_SPU,
		NRF_SPU_INT_FLASHACCERR_MASK |
		NRF_SPU_INT_RAMACCERR_MASK |
		NRF_SPU_INT_PERIPHACCERR_MASK);
}

void spu_regions_reset_all_secure(void)
{
	for (size_t i = 0; i < NUM_FLASH_SECURE_ATTRIBUTION_REGIONS ; i++) {
		nrf_spu_flashregion_set(NRF_SPU, i,
			1 /* Secure */,
			NRF_SPU_MEM_PERM_READ
			| NRF_SPU_MEM_PERM_WRITE
			| NRF_SPU_MEM_PERM_EXECUTE,
			0 /* No lock */);
	}

	for (size_t i = 0; i < NUM_SRAM_SECURE_ATTRIBUTION_REGIONS ; i++) {
		nrf_spu_ramregion_set(NRF_SPU, i,
			1 /* Secure */,
			NRF_SPU_MEM_PERM_READ
			| NRF_SPU_MEM_PERM_WRITE
			| NRF_SPU_MEM_PERM_EXECUTE,
			0 /* No lock */);
	}
}

void spu_regions_flash_config_non_secure(uint32_t start_addr, uint32_t limit_addr)
{
	/* Determine start and last flash region number */
	size_t start_id =
		(start_addr - DEVICE_FLASH_BASE_ADDRESS) /
			FLASH_SECURE_ATTRIBUTION_REGION_SIZE;
	size_t last_id =
		(limit_addr - DEVICE_FLASH_BASE_ADDRESS) /
			FLASH_SECURE_ATTRIBUTION_REGION_SIZE;

	/* Configure all flash regions between start_id and last_id */
	for (size_t i = start_id; i <= last_id; i++) {
		nrf_spu_flashregion_set(NRF_SPU, i,
			0 /* Non-Secure */,
			NRF_SPU_MEM_PERM_READ
			| NRF_SPU_MEM_PERM_WRITE
			| NRF_SPU_MEM_PERM_EXECUTE,
			1 /* Lock */);
	}
}

void spu_regions_sram_config_non_secure(uint32_t start_addr, uint32_t limit_addr)
{
	/* Determine start and last ram region number */
	size_t start_id =
		(start_addr - DEVICE_SRAM_BASE_ADDRESS) /
			SRAM_SECURE_ATTRIBUTION_REGION_SIZE;
	size_t last_id =
		(limit_addr - DEVICE_SRAM_BASE_ADDRESS) /
			SRAM_SECURE_ATTRIBUTION_REGION_SIZE;

	/* Configure all ram regions between start_id and last_id */
	for (size_t i = start_id; i <= last_id; i++) {
		nrf_spu_ramregion_set(NRF_SPU, i,
			0 /* Non-Secure */,
			NRF_SPU_MEM_PERM_READ
			| NRF_SPU_MEM_PERM_WRITE
			| NRF_SPU_MEM_PERM_EXECUTE,
			1 /* Lock */);
	}
}

void spu_regions_flash_config_non_secure_callable(uint32_t start_addr,
	uint32_t limit_addr)
{
	size_t size = limit_addr - start_addr + 1;

	uint32_t nsc_size = FLASH_NSC_SIZE_FROM_ADDR(start_addr);

	/* Check Non-Secure Callable region possible overflow */
	NRFX_ASSERT(size <= nsc_size);

	/* Check Non-Secure Callable region ending on SPU boundary */
	NRFX_ASSERT(((start_addr + nsc_size) %
			FLASH_SECURE_ATTRIBUTION_REGION_SIZE) == 0);

	/* Check Non-Secure Callable region power-of-2 size compliance */
	NRFX_ASSERT((nsc_size & (nsc_size - 1)) == 0);

	/* Check Non-Secure Callable region size is within [32, 4096] range */
	NRFX_ASSERT((nsc_size >= 32) && (nsc_size <= 4096));

	nrf_spu_flashnsc_set(NRF_SPU, 0,
		FLASH_NSC_SIZE_REG(nsc_size),
		FLASH_NSC_REGION_FROM_ADDR(start_addr),
		1 /* Lock */);
}
