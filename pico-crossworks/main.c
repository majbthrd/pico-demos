#include <stdbool.h>
#include <rp2040.h>

#define LED_PIN 25
#define LED_MASK (1ul << LED_PIN)
#define GPIO_FUNC_SIO 5

void main(void)
{
  resets_hw->reset &= ~(RESETS_RESET_IO_BANK0_BITS | RESETS_RESET_PADS_BANK0_BITS);

  sio_hw->gpio_oe_clr = LED_MASK;
  sio_hw->gpio_clr = LED_MASK;

  // Set input enable on, output disable off
  hw_write_masked(&padsbank0_hw->io[LED_PIN],
                 PADS_BANK0_GPIO0_IE_BITS,
                 PADS_BANK0_GPIO0_IE_BITS | PADS_BANK0_GPIO0_OD_BITS
  );

  // Zero all fields apart from fsel; we want this IO to do what the peripheral tells it.
  // This doesn't affect e.g. pullup/pulldown, as these are in pad controls.
  iobank0_hw->io[LED_PIN].ctrl = GPIO_FUNC_SIO << IO_BANK0_GPIO0_CTRL_FUNCSEL_LSB;

  sio_hw->gpio_oe_set = LED_MASK;

  SysTick->LOAD = 0x3FFFFF; /* set next reload value */
  SysTick->VAL = 0; /* cause counter to reset (actual value ignored) */
  SysTick->CTRL = SysTick_CTRL_ENABLE_Msk | SysTick_CTRL_CLKSOURCE_Msk | SysTick_CTRL_TICKINT_Msk; /* start counter */

  __enable_irq();

  for (;;)
  {
    __WFI();
  }
}

void SysTick_IRQHandler(void)
{
  static uint32_t counter = 0;
  bool illuminate = false;

  /* LED always on for first cycle */
  illuminate |= (0 == counter);
  /* also on for all but last cycle *if* cache is enabled */
  illuminate |= (xip_ctrl_hw->ctrl & XIP_CTRL_EN_BITS) && (15 != counter);
  /* last cycle is always off so we know everything is executing */

  if (illuminate)
    sio_hw->gpio_set = LED_MASK;
  else
    sio_hw->gpio_clr = LED_MASK;

  counter = (counter + 1) & 15;
}
