/* ----------------------------------------------------------------------------
 *         SAM Software Package License
 * ----------------------------------------------------------------------------
 * Copyright (c) 2015, Atmel Corporation
 *
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * - Redistributions of source code must retain the above copyright notice,
 * this list of conditions and the disclaimer below.
 *
 * Atmel's name may not be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * DISCLAIMER: THIS SOFTWARE IS PROVIDED BY ATMEL "AS IS" AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT ARE
 * DISCLAIMED. IN NO EVENT SHALL ATMEL BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA,
 * OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
 * LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
 * EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 * ----------------------------------------------------------------------------
 */

/** \addtogroup pmc_module Working with PMC
 * \section Purpose
 * The PMC driver provides the Interface for configuration the Power Management
 *  Controller (PMC).
 *
 * \section Usage
 * <ul>
 * <li>  Enable & disable peripherals using pmc_enable_peripheral() and
 * pmc_enable_all_peripherals() or pmc_disable_peripheral() and
 * pmc_disable_all_peripherals().
 * <li>  Get & set maximum frequency clock for giving peripheral using
 * pmc_get_peri_max_freq() and pmc_set_peri_max_clock().
 * <li>  Get Peripheral Status for the given peripheral using pmc_is_periph_enabled()
 * <li>  Select clocks's source using pmc_select_external_crystal(),
 * pmc_select_internal_crystal(), pmc_select_external_osc() and pmc_select_internal_osc().
 * <li>  Switch MCK using pmc_switch_mck_to_pll(), pmc_switch_mck_to_main() and
 * pmc_switch_mck_to_slck().
 * <li>  Config PLL using pmc_set_pll_a() and pmc_disable_pll_a().
 * </li>
 * </ul>
 * For more accurate information, please look at the PMC section of the
 * Datasheet.
 *
 * Related files :\n
 * \ref pmc.c\n
 * \ref pmc.h\n
*/
/*@{*/
/*@}*/

/**
 * \file
 *
 * Implementation of PIO (Parallel Input/Output) controller.
 *
 */
/*----------------------------------------------------------------------------
 *        Headers
 *----------------------------------------------------------------------------*/

#include "chip.h"
#include "board.h"
#include "peripherals/pmc.h"
#include "trace.h"
#include <assert.h>

/*----------------------------------------------------------------------------
 *        Variables
 *----------------------------------------------------------------------------*/

static uint32_t _pmc_mck = 0;

/*----------------------------------------------------------------------------
 *        Private functions
 *----------------------------------------------------------------------------*/

static void _pmc_compute_mck(void)
{
	uint32_t clk = 0;
	uint32_t mckr = PMC->PMC_MCKR;

	uint32_t css = mckr & PMC_MCKR_CSS_Msk;
	switch (css) {
	case PMC_MCKR_CSS_SLOW_CLK:
		clk = pmc_get_slow_clock();
		break;
	case PMC_MCKR_CSS_MAIN_CLK:
		clk = pmc_get_main_clock();
		break;
	case PMC_MCKR_CSS_PLLA_CLK:
		clk = pmc_get_plla_clock();
		break;
	case PMC_MCKR_CSS_UPLL_CLK:
		clk = BOARD_MAIN_CLOCK_EXT_OSC; /* external crystal */
		break;
	default:
		/* should never get here... */
		break;
	}

	uint32_t pres = mckr & PMC_MCKR_PRES_Msk;
	switch (pres) {
	case PMC_MCKR_PRES_CLOCK:
		break;
	case PMC_MCKR_PRES_CLOCK_DIV2:
		clk >>= 1;
		break;
	case PMC_MCKR_PRES_CLOCK_DIV4:
		clk >>= 2;
		break;
	case PMC_MCKR_PRES_CLOCK_DIV8:
		clk >>= 3;
		break;
	case PMC_MCKR_PRES_CLOCK_DIV16:
		clk >>= 4;
		break;
	case PMC_MCKR_PRES_CLOCK_DIV32:
		clk >>= 5;
		break;
	case PMC_MCKR_PRES_CLOCK_DIV64:
		clk >>= 6;
		break;
	default:
		/* should never get here... */
		break;
	}

	uint32_t mdiv = mckr & PMC_MCKR_MDIV_Msk;
	switch (mdiv) {
	case PMC_MCKR_MDIV_EQ_PCK:
		break;
	case PMC_MCKR_MDIV_PCK_DIV2:
		clk >>= 1; // divide by 2
		break;
	case PMC_MCKR_MDIV_PCK_DIV4:
		clk >>= 2; // divide by 4
		break;
	case PMC_MCKR_MDIV_PCK_DIV3:
		clk /= 3;  // divide by 3
		break;
	default:
		/* should never get here... */
		break;
	}

	_pmc_mck = clk;
}

static uint32_t _pmc_get_pck_clock(uint32_t index)
{
	uint32_t clk = 0;
	uint32_t pck = PMC->PMC_PCK[index];

	switch (pck & PMC_PCK_CSS_Msk) {
	case PMC_PCK_CSS_SLOW_CLK:
		clk = pmc_get_slow_clock();
		break;
	case PMC_PCK_CSS_MAIN_CLK:
		clk = pmc_get_main_clock();
		break;
	case PMC_PCK_CSS_PLLA_CLK:
		clk = pmc_get_plla_clock();
		break;
	case PMC_PCK_CSS_UPLL_CLK:
		//TODO: clk = pmc_get_upll_clock();
		clk = 0;
		break;
	case PMC_PCK_CSS_MCK_CLK:
		clk = pmc_get_master_clock();
		break;
#ifdef CONFIG_HAVE_PMC_AUDIO_CLOCK
	case PMC_PCK_CSS_AUDIO_CLK:
		//TODO: clk = pmc_get_audio_clock();
		clk = 0;
		break;
#endif
	}

	uint32_t prescaler = (pck & PMC_PCK_PRES_Msk) >> PMC_PCK_PRES_Pos;
	return clk / (prescaler + 1);
}

static bool _pmc_get_system_clock_bits(enum _pmc_system_clock clock,
	uint32_t *scer, uint32_t* scdr, uint32_t *scsr)
{
	uint32_t e, d, s;

	switch (clock)
	{
#ifdef PMC_SCER_PCK
	case PMC_SYSTEM_CLOCK_PCK:
		e = PMC_SCER_PCK;
		d = PMC_SCDR_PCK;
		s = PMC_SCSR_PCK;
		break;
#endif
	case PMC_SYSTEM_CLOCK_DDR:
		e = PMC_SCER_DDRCK;
		d = PMC_SCDR_DDRCK;
		s = PMC_SCSR_DDRCK;
		break;
	case PMC_SYSTEM_CLOCK_LCD:
		e = PMC_SCER_LCDCK;
		d = PMC_SCDR_LCDCK;
		s = PMC_SCSR_LCDCK;
		break;
#ifdef PMC_SCER_SMDCK
	case PMC_SYSTEM_CLOCK_SMD:
		e = PMC_SCER_SMDCK;
		d = PMC_SCDR_SMDCK;
		s = PMC_SCSR_SMDCK;
		break;
#endif
	case PMC_SYSTEM_CLOCK_UHP:
		e = PMC_SCER_UHP;
		d = PMC_SCDR_UHP;
		s = PMC_SCSR_UHP;
		break;
	case PMC_SYSTEM_CLOCK_UDP:
		e = PMC_SCER_UDP;
		d = PMC_SCDR_UDP;
		s = PMC_SCSR_UDP;
		break;
	case PMC_SYSTEM_CLOCK_PCK0:
		e = PMC_SCER_PCK0;
		d = PMC_SCDR_PCK0;
		s = PMC_SCSR_PCK0;
		break;
	case PMC_SYSTEM_CLOCK_PCK1:
		e = PMC_SCER_PCK1;
		d = PMC_SCDR_PCK1;
		s = PMC_SCSR_PCK1;
		break;
	case PMC_SYSTEM_CLOCK_PCK2:
		e = PMC_SCER_PCK2;
		d = PMC_SCDR_PCK2;
		s = PMC_SCSR_PCK2;
		break;
#ifdef PMC_SCER_ISCCK
	case PMC_SYSTEM_CLOCK_ISC:
		e = PMC_SCER_ISCCK;
		d = PMC_SCDR_ISCCK;
		s = PMC_SCSR_ISCCK;
		break;
#endif
	default:
		return false;
	}

	if (scer)
		*scer = e;
	if (scdr)
		*scdr = d;
	if (scsr)
		*scsr = s;
	return true;
}

/*----------------------------------------------------------------------------
 *        Exported functions (General)
 *----------------------------------------------------------------------------*/

uint32_t pmc_get_master_clock(void)
{
	if (!_pmc_mck) {
		_pmc_compute_mck();
	}
	return _pmc_mck;
}

uint32_t pmc_get_slow_clock(void)
{
	if (SCKC->SCKC_CR & SCKC_CR_OSCSEL)
		return SLOW_CLOCK_INT_OSC; /* on-chip slow clock RC */
	else
		return BOARD_SLOW_CLOCK_EXT_OSC; /* external crystal */
}

uint32_t pmc_get_main_clock(void)
{
	if (PMC->CKGR_MOR & CKGR_MOR_MOSCSEL)
		return MAIN_CLOCK_INT_OSC; /* on-chip main clock RC */
	else
		return BOARD_MAIN_CLOCK_EXT_OSC; /* external crystal */
}

uint32_t pmc_get_plla_clock(void)
{
	uint32_t pllaclk, pllar, pllmula, plldiva;

	if (PMC->CKGR_MOR & CKGR_MOR_MOSCSEL)
		pllaclk = MAIN_CLOCK_INT_OSC; /* on-chip main clock RC */
	else
		pllaclk = BOARD_MAIN_CLOCK_EXT_OSC; /* external crystal */

	pllar = PMC->CKGR_PLLAR;
	pllmula = (pllar & CKGR_PLLAR_MULA_Msk) >> CKGR_PLLAR_MULA_Pos;
	plldiva = (pllar & CKGR_PLLAR_DIVA_Msk) >> CKGR_PLLAR_DIVA_Pos;
	if (plldiva == 0) {
		pllaclk = 0;
	} else {
		pllaclk = pllaclk * (pllmula + 1) / plldiva;
		if (PMC->PMC_MCKR & PMC_MCKR_PLLADIV2)
			pllaclk >>= 1;
	}

	return pllaclk;
}

uint32_t pmc_get_processor_clock(void)
{
	uint32_t procclk, mdiv;

	procclk = pmc_get_master_clock();

	mdiv = PMC->PMC_MCKR & PMC_MCKR_MDIV_Msk;
	switch (mdiv) {
	case PMC_MCKR_MDIV_EQ_PCK:
		break;
	case PMC_MCKR_MDIV_PCK_DIV2:
		procclk <<= 1; // multiply by 2
		break;
	case PMC_MCKR_MDIV_PCK_DIV3:
		procclk *= 3;  // multiply by 3
		break;
	case PMC_MCKR_MDIV_PCK_DIV4:
		procclk <<= 2; // multiply by 4
		break;
	default:
		/* should never get here... */
		break;
	}

	return procclk;
}

void pmc_select_external_crystal(void)
{
	int return_to_sclock = 0;

	if (PMC->PMC_MCKR == PMC_MCKR_CSS(PMC_MCKR_CSS_SLOW_CLK)) {
		pmc_switch_mck_to_main();
		return_to_sclock = 1;
	}

	/* switch from internal RC 32kHz to external OSC 32 kHz */
	SCKC->SCKC_CR = (SCKC->SCKC_CR & ~SCKC_CR_OSCSEL) | SCKC_CR_OSCSEL_XTAL;

	/* Wait 5 slow clock cycles for internal resynchronization */
	volatile int count;
	for (count = 0; count < 0x1000; count++);

	/* Switch to slow clock again if needed */
	if (return_to_sclock)
		pmc_switch_mck_to_slck();
}

void pmc_select_internal_crystal(void)
{
	int return_to_sclock = 0;

	if (PMC->PMC_MCKR == PMC_MCKR_CSS(PMC_MCKR_CSS_SLOW_CLK)) {
		pmc_switch_mck_to_main();
		return_to_sclock = 1;
	}

	/* switch from extenal OSC 32kHz to internal RC 32 kHz */
	/* switch slow clock source to internal OSC 32 kHz */
	SCKC->SCKC_CR = (SCKC->SCKC_CR & ~SCKC_CR_OSCSEL) | SCKC_CR_OSCSEL_RC;

	/* Wait 5 slow clock cycles for internal resynchronization */
	volatile int count;
	for (count = 0; count < 0x1000; count++);

	/* Switch to slow clock again if needed */
	if (return_to_sclock)
		pmc_switch_mck_to_slck();
}

void pmc_select_external_osc(void)
{
	/* switch from internal RC 12 MHz to external OSC 12 MHz */
	/* wait Main XTAL Oscillator stabilization */
	if ((PMC->CKGR_MOR & CKGR_MOR_MOSCSEL) == CKGR_MOR_MOSCSEL)
		return;

	/* enable external OSC 12 MHz */
	PMC->CKGR_MOR |= (1 << 5) | CKGR_MOR_MOSCXTST(18) | CKGR_MOR_MOSCXTEN | CKGR_MOR_KEY_PASSWD;

	/* Wait Main Oscillator ready */
	while (!(PMC->PMC_SR & PMC_SR_MOSCXTS));

	/* switch MAIN clock to external OSC 12 MHz */
	PMC->CKGR_MOR |= CKGR_MOR_MOSCSEL | CKGR_MOR_KEY_PASSWD;

	/* wait for the command to be taken into account */
	while ((PMC->CKGR_MOR & CKGR_MOR_MOSCSEL) != CKGR_MOR_MOSCSEL);

	/* wait MAIN clock status change for external OSC 12 MHz selection */
	while (!(PMC->PMC_SR & PMC_SR_MOSCSELS));

}

void pmc_select_internal_osc(void)
{
	/* switch from external OSC 12 MHz to internal RC 12 MHz */
	/* Wait internal 12 MHz RC Startup Time for clock stabilization (software loop) */
	while (!(PMC->PMC_SR & PMC_SR_MOSCRCS));

	/* switch MAIN clock to internal RC 12 MHz */
	PMC->CKGR_MOR = (PMC->CKGR_MOR & ~CKGR_MOR_MOSCSEL) | CKGR_MOR_KEY_PASSWD;

	/* in case where MCK is running on MAIN CLK */
	while (!(PMC->PMC_SR & PMC_SR_MCKRDY));

	/* disable external OSC 12 MHz   */
	PMC->CKGR_MOR = (PMC->CKGR_MOR & ~CKGR_MOR_MOSCXTEN) | CKGR_MOR_KEY_PASSWD;
	while (!(PMC->PMC_SR & PMC_SR_MCKRDY));
}

void pmc_switch_mck_to_pll(void)
{
	/* Select PLL as input clock for PCK and MCK */
	PMC->PMC_MCKR = (PMC->PMC_MCKR & ~PMC_MCKR_CSS_Msk) | PMC_MCKR_CSS_PLLA_CLK;
	while (!(PMC->PMC_SR & PMC_SR_MCKRDY));

	_pmc_mck = 0;
}

void pmc_switch_mck_to_main(void)
{
	/* Select Main Oscillator as input clock for PCK and MCK */
	PMC->PMC_MCKR = (PMC->PMC_MCKR & ~PMC_MCKR_CSS_Msk) | PMC_PCK_CSS_MAIN_CLK;
	while (!(PMC->PMC_SR & PMC_SR_MCKRDY));

	_pmc_mck = 0;
}

void pmc_switch_mck_to_slck(void)
{
	/* Select Slow Clock as input clock for PCK and MCK */
	PMC->PMC_MCKR = (PMC->PMC_MCKR & ~PMC_MCKR_CSS_Msk) | PMC_PCK_CSS_SLOW_CLK;
	while (!(PMC->PMC_SR & PMC_SR_MCKRDY));

	_pmc_mck = 0;
}

void pmc_set_mck_prescaler(uint32_t prescaler)
{
	/* Change MCK Prescaler divider in PMC_MCKR register */
	PMC->PMC_MCKR = (PMC->PMC_MCKR & ~PMC_MCKR_PRES_Msk) | prescaler;
	while (!(PMC->PMC_SR & PMC_SR_MCKRDY));
}

void pmc_set_mck_plla_div(uint32_t divider)
{
	if ((PMC->PMC_MCKR & PMC_MCKR_PLLADIV2) == PMC_MCKR_PLLADIV2) {
		if (divider == 0) {
			PMC->PMC_MCKR = (PMC->PMC_MCKR & ~PMC_MCKR_PLLADIV2);
		}
	} else {
		if (divider == PMC_MCKR_PLLADIV2) {
			PMC->PMC_MCKR = (PMC->PMC_MCKR | PMC_MCKR_PLLADIV2);
		}
	}
	while (!(PMC->PMC_SR & PMC_SR_MCKRDY));
}

void pmc_set_mck_h32mxdiv(uint32_t divider)
{
	if ((PMC->PMC_MCKR & PMC_MCKR_H32MXDIV) == PMC_MCKR_H32MXDIV_H32MXDIV2) {
		if (divider == PMC_MCKR_H32MXDIV_H32MXDIV1) {
			PMC->PMC_MCKR = (PMC->PMC_MCKR & ~PMC_MCKR_H32MXDIV);
		}
	} else {
		if (divider == PMC_MCKR_H32MXDIV_H32MXDIV2) {
			PMC->PMC_MCKR = (PMC->PMC_MCKR | PMC_MCKR_H32MXDIV_H32MXDIV2);
		}
	}
	while (!(PMC->PMC_SR & PMC_SR_MCKRDY));
}

void pmc_set_mck_divider(uint32_t divider)
{
	/* change MCK Prescaler divider in PMC_MCKR register */
	PMC->PMC_MCKR = (PMC->PMC_MCKR & ~PMC_MCKR_MDIV_Msk) | divider;
	while (!(PMC->PMC_SR & PMC_SR_MCKRDY));
}

void pmc_set_plla(uint32_t pll, uint32_t cpcr)
{
	PMC->CKGR_PLLAR = pll;
	PMC->PMC_PLLICPR = cpcr;

	if ((pll & CKGR_PLLAR_DIVA_Msk) != CKGR_PLLAR_DIVA_0) {
		while (!(PMC->PMC_SR & PMC_SR_LOCKA));
	}
}

void pmc_disable_plla(void)
{
	PMC->CKGR_PLLAR = (PMC->CKGR_PLLAR & ~CKGR_PLLAR_MULA_Msk) | CKGR_PLLAR_MULA(0);
}

void pmc_enable_system_clock(enum _pmc_system_clock clock)
{
	uint32_t scer, scsr;
	if (!_pmc_get_system_clock_bits(clock, &scer, NULL, &scsr))
		return;

	PMC->PMC_SCER |= scer;
	while (!(PMC->PMC_SCSR & scsr));
}

void pmc_disable_system_clock(enum _pmc_system_clock clock)
{
	uint32_t scdr, scsr;
	if (!_pmc_get_system_clock_bits(clock, NULL, &scdr, &scsr))
		return;

	PMC->PMC_SCDR |= scdr;
	while (PMC->PMC_SCSR & scsr);
}

/*----------------------------------------------------------------------------
 *        Exported functions (Peripherals)
 *----------------------------------------------------------------------------*/

void pmc_enable_peripheral(uint32_t id)
{
	assert(id > 1 && id < ID_PERIPH_COUNT);

	PMC->PMC_PCR = PMC_PCR_PID(id);
	volatile uint32_t pcr = PMC->PMC_PCR;

	PMC->PMC_PCR = pcr | PMC_PCR_CMD | PMC_PCR_EN;
}

void pmc_disable_peripheral(uint32_t id)
{
	assert(id > 1 && id < ID_PERIPH_COUNT);

	PMC->PMC_PCR = PMC_PCR_PID(id);
	volatile uint32_t pcr = PMC->PMC_PCR;

	PMC->PMC_PCR = PMC_PCR_CMD | (pcr & ~PMC_PCR_EN);
}

uint32_t pmc_is_peripheral_enabled(uint32_t id)
{
	assert(id > 1 && id < ID_PERIPH_COUNT);

	PMC->PMC_PCR = PMC_PCR_PID(id);
	volatile uint32_t pcr = PMC->PMC_PCR;

	return !!(pcr & PMC_PCR_EN);
}

uint32_t pmc_get_peripheral_clock(uint32_t id)
{
	assert(id > 1 && id < ID_PERIPH_COUNT);

	uint32_t div = get_peripheral_clock_divider(id);
	if (div)
		return pmc_get_master_clock() / div;

	return 0;
}

void pmc_disable_all_peripherals(void)
{
	int i;
	for (i = 2; i < ID_PERIPH_COUNT; i++)
		pmc_disable_peripheral(i);
}

/*----------------------------------------------------------------------------
 *        Exported functions (PCK0-2)
 *----------------------------------------------------------------------------*/

void pmc_configure_pck0(uint32_t clock_source, uint32_t prescaler)
{
	pmc_disable_pck0();
	PMC->PMC_PCK[0] = (clock_source & PMC_PCK_CSS_Msk) | PMC_PCK_PRES(prescaler);
}

void pmc_enable_pck0(void)
{
	PMC->PMC_SCER = PMC_SCER_PCK0;
	while (!(PMC->PMC_SR & PMC_SR_PCKRDY0));
}

void pmc_disable_pck0(void)
{
	PMC->PMC_SCDR = PMC_SCDR_PCK0;
	while (PMC->PMC_SCSR & PMC_SCSR_PCK0);
}

uint32_t pmc_get_pck0_clock(void)
{
	return _pmc_get_pck_clock(0);
}

void pmc_configure_pck1(uint32_t clock_source, uint32_t prescaler)
{
	pmc_disable_pck1();
	PMC->PMC_PCK[1] = (clock_source & PMC_PCK_CSS_Msk) | PMC_PCK_PRES(prescaler);
}

void pmc_enable_pck1(void)
{
	PMC->PMC_SCER = PMC_SCER_PCK1;
	while (!(PMC->PMC_SR & PMC_SR_PCKRDY1));
}

void pmc_disable_pck1(void)
{
	PMC->PMC_SCDR = PMC_SCDR_PCK1;
	while (PMC->PMC_SCSR & PMC_SCSR_PCK1);
}

uint32_t pmc_get_pck1_clock(void)
{
	return _pmc_get_pck_clock(1);
}

void pmc_configure_pck2(uint32_t clock_source, uint32_t prescaler)
{
	pmc_disable_pck2();
	PMC->PMC_PCK[2] = (clock_source & PMC_PCK_CSS_Msk) | PMC_PCK_PRES(prescaler);
}

void pmc_enable_pck2(void)
{
	PMC->PMC_SCER = PMC_SCER_PCK2;
	while (!(PMC->PMC_SR & PMC_SR_PCKRDY2));
}

void pmc_disable_pck2(void)
{
	PMC->PMC_SCDR = PMC_SCDR_PCK2;
	while (PMC->PMC_SCSR & PMC_SCSR_PCK2);
}

uint32_t pmc_get_pck2_clock(void)
{
	return _pmc_get_pck_clock(2);
}

/*----------------------------------------------------------------------------
 *        Exported functions (DDR)
 *----------------------------------------------------------------------------*/

void pmc_enable_ddr_clock(void)
{
	PMC->PMC_SCER |= PMC_SCER_DDRCK;
	while (!(PMC->PMC_SCSR & PMC_SCSR_DDRCK));
}

void pmc_disable_ddr_clock(void)
{
	PMC->PMC_SCDR |= PMC_SCER_DDRCK;
	while (PMC->PMC_SCSR & PMC_SCSR_DDRCK);
}

/*----------------------------------------------------------------------------
 *        Exported functions (Generated clocks)
 *----------------------------------------------------------------------------*/

#ifdef CONFIG_HAVE_PMC_GENERATED_CLOCKS
void pmc_configure_gck(uint32_t id, uint32_t clock_source, uint32_t div)
{
	assert(id > 1 && id < ID_PERIPH_COUNT);
	assert(!(clock_source & ~PMC_PCR_GCKCSS_Msk));
	assert(!(div << PMC_PCR_GCKDIV_Pos & ~PMC_PCR_GCKDIV_Msk));

	pmc_disable_gck(id);
	PMC->PMC_PCR = PMC_PCR_PID(id);
	volatile uint32_t pcr = PMC->PMC_PCR;
	PMC->PMC_PCR = pcr | (clock_source & PMC_PCR_GCKCSS_Msk) | PMC_PCR_CMD
	    | PMC_PCR_GCKDIV(div);
}

void pmc_enable_gck(uint32_t id)
{
	assert(id > 1 && id < ID_PERIPH_COUNT);

	PMC->PMC_PCR = PMC_PCR_PID(id);
	volatile uint32_t pcr = PMC->PMC_PCR;
	PMC->PMC_PCR = pcr | PMC_PCR_CMD | PMC_PCR_GCKEN;
	while (!(PMC->PMC_SR & PMC_SR_GCKRDY));
}

void pmc_disable_gck(uint32_t id)
{
	assert(id > 1 && id < ID_PERIPH_COUNT);

	PMC->PMC_PCR = PMC_PCR_PID(id);
	volatile uint32_t pcr = PMC->PMC_PCR;
	PMC->PMC_PCR = PMC_PCR_CMD | (pcr & ~PMC_PCR_GCKEN);
	while (!(PMC->PMC_SR & PMC_SR_GCKRDY));
}

uint32_t pmc_get_gck_clock(uint32_t id)
{
	uint32_t clk = 0;
	assert(id > 1 && id < ID_PERIPH_COUNT);

	PMC->PMC_PCR = PMC_PCR_PID(id);
	volatile uint32_t pcr = PMC->PMC_PCR;

	switch (pcr & PMC_PCR_GCKCSS_Msk) {
	case PMC_PCR_GCKCSS_SLOW_CLK:
		clk = pmc_get_slow_clock();
		break;
	case PMC_PCR_GCKCSS_MAIN_CLK:
		clk = pmc_get_main_clock();
		break;
	case PMC_PCR_GCKCSS_PLLA_CLK:
		clk = pmc_get_plla_clock();
		break;
	case PMC_PCR_GCKCSS_UPLL_CLK:
		//TODO: clk = pmc_get_upll_clock();
		clk = 0;
		break;
	case PMC_PCR_GCKCSS_MCK_CLK:
		clk = pmc_get_master_clock();
		break;
#ifdef CONFIG_HAVE_PMC_AUDIO_CLOCK
	case PMC_PCR_GCKCSS_AUDIO_CLK:
		//TODO: clk = pmc_get_audio_clock();
		clk = 0;
		break;
#endif
	}

	uint32_t div = (pcr & PMC_PCR_GCKDIV_Msk) >> PMC_PCR_GCKDIV_Pos;
	return ROUND_INT_DIV(clk, div + 1);
}
#endif /* CONFIG_HAVE_PMC_GENERATED_CLOCKS */

