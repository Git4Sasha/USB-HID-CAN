#include "stm32f303xc.h"
#include "usbhw.h"
#include "usbcore.h"
#include "usbdesc.h"
#include "gpio.h"

// Чтобы инициализировать USB устройство необходимо вызвать функцию USB_HID_Init;

// Буферы для обмена данными с хостом
static uint8_t InReport[HID_IN_PACKET_SIZE];  // In report  (для устройства выходной буфер)
static uint8_t OutReport[HID_OUT_PACKET_SIZE]; // Out report

uint8_t *ToHostBuf = &InReport[1];   // Указатель на буфер, который будет передан хосту
uint8_t ToHostBufSize = HID_IN_PACKET_SIZE - 1; // размер буфера для передачи данных хосту
uint8_t ToHostBufFill = 0; // признак того, что буфер для хоста заполнен

uint8_t *FromHostBuf = &OutReport[1]; // Указатель на буфер в который приходят данные от хоста
uint8_t FromHostBufSize = HID_OUT_PACKET_SIZE - 1; // размер буфера для приёма данных от хоста
uint8_t FromHostBufFill = 0; // Признак того, что буфер от хоста заполнен данными

void USB_HID_Init(void) // Инициализация и включение USB HID устройства
{
  RCC->APB1ENR |= RCC_APB1ENR_USBEN;

  // В качестве ног данных D+ и D- изпользуются PA11(D-) и  PA12(D+)  (14-я альтернативная функция для каждой ноги)
  ConfigGPIO(GPIOA, 11, MODE_ALTER_FUNC, OTYPE_PUSH_PULL, SPEED_HI, PULL_NO_UP_NO_DOWN, 14);
  ConfigGPIO(GPIOA, 12, MODE_ALTER_FUNC, OTYPE_PUSH_PULL, SPEED_HI, PULL_NO_UP_NO_DOWN, 14);

  USB_Connect(1); // Функция инициализации подключения
}

void ClrFromHostBufFill(void) // Сброс признака "буфер полон" и разрешение работы точки
{
  FromHostBufFill = 0;  // Сброс признака "буфер заполнен"
  USB_SetValidEP(0x01);
}

void EndPoint1Event(uint32_t event) // Событие от нулевой точки
{
  switch(event)
  {
    case USB_EVT_OUT: // Хост выдаёт данные
                      USB_ReadEP(0x01, OutReport); // Чтение данных из буфера
                      USB_DisableEP(0x01); // Выключаем точку (точка будет включена, в процедуре ClrFromHostBufFill)

                      // OutReport[0] = 1 - PID репорта, 0-й байт занят под значение PID. Если со стороны Хоста приложение не заполнит этот бит,
                      // то пакет просто не дойдёт до USB устройства
                      FromHostBufFill = 1; // установка признака "Буфер заполнен"
                      break;

    case USB_EVT_IN:  // Хост запрашивает данные
                      if(ToHostBufFill) // Только когда данные заполнены их можно передавать хосту
                      {
                        // Передача пакета хосту
                        InReport[0] = 2; // PID - Если со стороны USB устройства не заполнить это значение (см. дескриптор репорта), то 
                                         // сообщение не дойдёт до адресата
                        USB_WriteEP(0x81, &InReport[0], HID_IN_PACKET_SIZE); // Передача пакета Хосту
                        ToHostBufFill = 0; // Обнуляем признак того, что буфер заполнен
                        break;
                      }
                      USB_WriteEP(0x81, ((void *)0), 0); // Передача нулевого пакета Хосту (если передавать нечего)
                      break;
  }
}


