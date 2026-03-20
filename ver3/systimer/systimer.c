#include "stm32f303xc.h"
#include "systimer.h"
#include "system_stm32f3xx.h"

static volatile uint32_t sysTicks=0; // Переменная увеличивается на 1 в прерывании от системного таймера
void SysTick_Handler(void) // Обработчик прерывания от системного таймера
{
  sysTicks++;
}

uint32_t GetTickCount(void) // Возвращает текущее значение счётчика
{
  return sysTicks;
}

void Delay(uint32_t dlyTicks) // Задержка на заданное число миллисекунд
{                                              
  uint32_t countTicks;

  countTicks = sysTicks + dlyTicks;
  while(sysTicks < countTicks);
}

void SysTimerInit(unsigned int Freq) // Инициализация системного таймера ( в функцию передаётся частота срабатывания системного таймера в Герцах )
{
  SysTick_Config(SystemCoreClock / 1000000 * Freq); // В функцию передаётся количество тиков до формирования прерывания
}

