#ifndef __USBCORE_H__
#define __USBCORE_H__

#include <stdint.h>

#define USB_POWER           0
#define USB_IF_NUM          1  // Количество изпользуемых интерфейсов
#define USB_EP_NUM          2  // Количество конечных точек (0-я + остальные (точки 0x01 и 0x81, считаются за одну точку)
#define USB_MAX_PACKET0     8  // Максимальный размер пакета для 0-й точки для FullSpeed устройств (это по стандарту, а на самом деле можно и 16, 32, 64 задать)
#define USB_DBL_BUF_EP      0  // Будет ли изпользоваться двойная буферизация для конечных точек типа bulk


// Коды событий, которые передаются в функции обработчики событий конечных точек
#define USB_EVT_SETUP       1   /* Setup Packet */
#define USB_EVT_OUT         2   /* OUT Packet */
#define USB_EVT_IN          3   /*  IN Packet */
#define USB_EVT_OUT_STALL   4   /* OUT Packet - Stalled */
#define USB_EVT_IN_STALL    5   /*  IN Packet - Stalled */

typedef union __attribute__((packed))
{
  uint16_t W;
  struct __attribute__((packed))
  {
    uint8_t L;
    uint8_t H;
  }WB;
}TValue2b;

/* bmRequestType Definition */
typedef union __attribute__((packed))
{
  struct _BM
  {
    uint8_t Recipient : 5;  // Код получателя (0 - USB-устройство, 1 - интерфейс, 2 - другой получатель)
    uint8_t Type      : 2;  // Код типа запроса (0 - стандартный запрос, 1 - специфический запрос для данного класса, 2 - специфический запрос изготовителя, 3 - зарезервированно)
    uint8_t Dir       : 1;  // Направление (0 - от Хоста к USB-устройству, 1 - от USB-устройства к Хосту)
  }BM;
  uint8_t B;
}REQUEST_TYPE;

typedef struct __attribute__((packed))  // Конфигурационный пакет (книга "Прак. программ. USB, стр 42"
{
  REQUEST_TYPE bmRequestType;  // Тип запроса
  uint8_t    bRequest;         // Код запроса (определяет операцию выполняемую запросом)
  TValue2b   wValue;           // Параметр запроса - Зависит от типа запроса
  TValue2b   wIndex;           // Индект или смещение - Зависит от типа запроса
  uint16_t   wLength;          // Число байт для передачи 
}USB_SETUP_PACKET;


/* USB Endpoint Data Structure */
typedef struct _USB_EP_DATA
{
  uint8_t  *pData;
  uint16_t Count;
}USB_EP_DATA;

typedef struct
{
  uint8_t  Address;  // Адрес устройства выданный хостом
  uint8_t  ConfigOK;  // Поле хранит номер конфигурации, который задал хост (номер конфигурации нумеруется с 1, если это поле равно 0, значит USB устройство не сконфигурированно)
  uint32_t EPEnableMask;   // Маска включённых конечных точек

  uint8_t  EP0Buf[USB_MAX_PACKET0]; // Массив для приёма входных данных для конечной точки с номером 0
  USB_EP_DATA EP0Data; // указатель на текущий буфер для передачи данных Хосту
  USB_SETUP_PACKET sp; // это поле хранит конфигурационный пакет, который приходит от Хоста (конфигурационный пакет приходит всегда для 0-й точки)
}USB_HID_Device; // Структура, которая хранит параметры HID устройства


#endif  /* __USBCORE_H__ */
