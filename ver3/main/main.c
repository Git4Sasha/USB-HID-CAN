/*
 * main.c
 *
 *  Created on: 8 мар. 2020 г.
 *      Author: user
 */

#include <stdio.h>
#include "stm32f303xc.h"
#include "system_stm32f3xx.h"
#include "systimer.h"
#include "gpio.h"
#include "usbuser.h"
#include "can.h"

int RecNum=0;

void CANRecvFunc(uint32_t id, uint8_t len, uint8_t *data);

int main(void)
{
  SystemCoreClockUpdate(); // Определение частоты ядра
  SysTick_Config(SystemCoreClock / 1000); // SysTick 1 msec interrupts

//  ConfigGPIO(GPIOC, 13, 1, 0, 0, 0, 0);
//  GPIOHi(GPIOC, 13);

  CAN_SetFIFO0RecvFunc(CANRecvFunc);  // Назначение функции, которая будет принимать данные, приходящие по шине CAN

  // Важно инициализировать CAN шину до инициализации USB при их совмесной работе, иначе USB работать не будет
  CAN_Init(CanBaudRate1M, 1, 0, 1);   // Инициализация шины CAN
  USB_HID_Init(); // Инициализация USB HID устройства

  while(1)
  {
    if(FromHostBufFill) // Если есть данные от Хоста, то
    {
      uint16_t *id = (uint16_t*)&FromHostBuf[0];
      uint8_t *data = (uint8_t*)&FromHostBuf[2];
      uint8_t len = (id[0] & 0xf800)>>11; // Получаем размер из первых 2-х байт переданного сообщения

      CAN_Write(id[0] & 0x7ff, 0, len, data, 100);

      ClrFromHostBufFill();
    }
  }
}


void CANRecvFunc(uint32_t id, uint8_t len, uint8_t *data)
{
  // Если номер записи достиг номера 6, то данные не помещаются в USB пакет (64 байта), поэтому выходим из процедуры
  if(RecNum==6) return;

  // В зависимости от номера записи получаем указатели для заполнения (для каждой записи изпользуется 10 байт: 2 байта на идентификатор и длину, 8 байт на данные)

  uint16_t *idCAN = (uint16_t*)&ToHostBuf[RecNum*10];
  uint8_t *datausb = (uint8_t*)&idCAN[1];

  idCAN[0] = id | (len<<11); // В битах с 15-11 будет располагаться кол-во байт, которое пришло по CAN шине
  datausb[0] = data[0];
  datausb[1] = data[1];
  datausb[2] = data[2];
  datausb[3] = data[3];
  datausb[4] = data[4];
  datausb[5] = data[5];
  datausb[6] = data[6];
  datausb[7] = data[7];

  RecNum++;

  ToHostBufFill = 1;
}




