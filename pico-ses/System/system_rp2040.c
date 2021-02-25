#include <stdint.h>
#include <stdbool.h>
#include <rp2040.h>

#define MHZ 1000000UL
#define REF_MHZ (12 * MHZ)

static void pll_init(pll_hw_t *pll, uint32_t refdiv, uint32_t vco_freq, uint32_t post_div1, uint8_t post_div2)
{
  uint32_t ref_mhz = XOSC_MHZ / refdiv;

  // What are we multiplying the reference clock by to get the vco freq
  // (The regs are called div, because you divide the vco output and compare it to the refclk)
  uint32_t fbdiv = vco_freq / (ref_mhz * MHZ);

  // fbdiv
  assert(fbdiv >= 16 && fbdiv <= 320);

  // Check divider ranges
  assert((post_div1 >= 1 && post_div1 <= 7) && (post_div2 >= 1 && post_div2 <= 7));

  // post_div1 should be >= post_div2
  // from appnote page 11
  // postdiv1 is designed to operate with a higher input frequency
  // than postdiv2
  assert(post_div2 <= post_div1);

  // Check that reference frequency is no greater than vco / 16
  assert(ref_mhz <= (vco_freq / 16));

  // div1 feeds into div2 so if div1 is 5 and div2 is 2 then you get a divide by 10
  uint32_t pdiv = (post_div1 << PLL_PRIM_POSTDIV1_LSB) |
                  (post_div2 << PLL_PRIM_POSTDIV2_LSB);

  if ((pll->cs & PLL_CS_LOCK_BITS) &&
      (refdiv == (pll->cs & PLL_CS_REFDIV_BITS)) &&
      (fbdiv  == (pll->fbdiv_int & PLL_FBDIV_INT_BITS)) &&
      (pdiv   == (pll->prim & (PLL_PRIM_POSTDIV1_BITS & PLL_PRIM_POSTDIV2_BITS))))
  {
      // do not disrupt PLL that is already correctly configured and operating
      return;
  }

  uint32_t pll_reset = (pll_usb_hw == pll) ? RESETS_RESET_PLL_USB_BITS : RESETS_RESET_PLL_SYS_BITS;
  hw_set_bits(&resets_hw->reset, pll_reset);
  hw_clear_bits(&resets_hw->reset, pll_reset);
  while (~resets_hw->reset_done & pll_reset);

  // Turn off PLL in case it is already running
  pll->pwr = 0xffffffff;
  pll->fbdiv_int = 0;

  pll->cs = refdiv;

  // Put calculated value into feedback divider
  pll->fbdiv_int = fbdiv;

  // Turn on PLL
  uint32_t power = PLL_PWR_PD_BITS | // Main power
                   PLL_PWR_VCOPD_BITS; // VCO Power

  hw_clear_bits(&pll->pwr, power);

  // Wait for PLL to lock
  while (!(pll->cs & PLL_CS_LOCK_BITS));

  // Set up post dividers
  pll->prim = pdiv;

  // Turn on post divider
  hw_clear_bits(&pll->pwr, PLL_PWR_POSTDIVPD_BITS);
}

/* overhaul of clock_configure() from pico-sdk to use much less memory */
bool simple_clock_configure(enum clock_index clk_index, uint32_t src, uint32_t auxsrc, bool glitchless)
{
  const uint32_t div = 0x100; /* always 1:1 ratio */

  clock_hw_t *clock = &clocks_hw->clk[clk_index];

  // If increasing divisor, set divisor before source. Otherwise set source
  // before divisor. This avoids a momentary overspeed when e.g. switching
  // to a faster source and increasing divisor to compensate.
  if (div > clock->div)
    clock->div = div;

  // If switching a glitchless slice (ref or sys) to an aux source, switch
  // away from aux *first* to avoid passing glitches when changing aux mux.
  // Assume (!!!) glitchless source 0 is no faster than the aux source.
  if (glitchless)
  {
    hw_clear_bits(&clock->ctrl, CLOCKS_CLK_REF_CTRL_SRC_BITS);
    while (!(clock->selected & 1u));
  }
  // If no glitchless mux, cleanly stop the clock to avoid glitches
  // propagating when changing aux mux. Note it would be a really bad idea
  // to do this on one of the glitchless clocks (clk_sys, clk_ref).
  else
  {
    hw_clear_bits(&clock->ctrl, CLOCKS_CLK_GPOUT0_CTRL_ENABLE_BITS);
  }

  // Set aux mux first, and then glitchless mux if this clock has one
  hw_write_masked(&clock->ctrl,
    (auxsrc << CLOCKS_CLK_SYS_CTRL_AUXSRC_LSB),
    CLOCKS_CLK_SYS_CTRL_AUXSRC_BITS
  );

  if (glitchless)
  {
    hw_write_masked(&clock->ctrl,
      src << CLOCKS_CLK_REF_CTRL_SRC_LSB,
      CLOCKS_CLK_REF_CTRL_SRC_BITS
    );
    while (!(clock->selected & (1u << src)));
  }

  hw_set_bits(&clock->ctrl, CLOCKS_CLK_GPOUT0_CTRL_ENABLE_BITS);

  // Now that the source is configured, we can trust that the user-supplied
  // divisor is a safe value.
  clock->div = div;

  return true;
}

uint32_t SystemCoreClock = 0;

void SystemCoreClockUpdate(void)
{
  uint32_t refdiv   = (pll_sys_hw->cs & PLL_CS_REFDIV_BITS)        >> PLL_CS_REFDIV_LSB;
  uint32_t fbdiv    = (pll_sys_hw->fbdiv_int & PLL_FBDIV_INT_BITS) >> PLL_FBDIV_INT_LSB;
  uint32_t postdiv1 = (pll_sys_hw->prim & PLL_PRIM_POSTDIV1_BITS)  >> PLL_PRIM_POSTDIV1_LSB;
  uint32_t postdiv2 = (pll_sys_hw->prim & PLL_PRIM_POSTDIV2_BITS)  >> PLL_PRIM_POSTDIV2_LSB;

  uint32_t foutpostdiv = (REF_MHZ / refdiv) * fbdiv / (postdiv1 * postdiv2);

  SystemCoreClock = foutpostdiv;
}

void SystemInit(void)
{
  rosc_hw->ctrl = ROSC_CTRL_ENABLE_VALUE_ENABLE << ROSC_CTRL_ENABLE_LSB;
  hw_clear_bits(&clocks_hw->clk[clk_ref].ctrl, CLOCKS_CLK_REF_CTRL_SRC_BITS);

  xosc_hw->ctrl = XOSC_CTRL_ENABLE_VALUE_ENABLE << XOSC_CTRL_ENABLE_LSB;
  while (!(xosc_hw->status & XOSC_STATUS_STABLE_BITS));

  pll_init(pll_sys_hw, 1, 1500 * MHZ, 6, 2);

  // CLK SYS = PLL SYS (125Hz) / 1 = 125MHz
  simple_clock_configure(clk_sys,
                  CLOCKS_CLK_SYS_CTRL_SRC_VALUE_CLKSRC_CLK_SYS_AUX,
                  CLOCKS_CLK_SYS_CTRL_AUXSRC_VALUE_CLKSRC_PLL_SYS,
                  true);

  SystemCoreClock = 125 * MHZ;
  SystemCoreClockUpdate();
}
