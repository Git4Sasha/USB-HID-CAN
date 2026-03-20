#include "usbcore.h"
#include "usbdesc.h"


/* HID Report Descriptor */
const uint8_t HID_ReportDescriptor[HID_REPORT_DESC_SIZE] = 
{
  0x05, 0x40,                    // USAGE_PAGE (Instruments)
  0x09, 0x00,                    // USAGE (Undefined)
  0xA1, 0x00,                    // COLLECTION (Vendor Defined) // объединяет следующие за ним элементы в группу, которую замыкает элемент END COLLECTION

  0x85, 0x01,                    // REPORT_ID (1)            // идентификатор (префикс) сообщения (его необходимо отправлять обязательно
  0x75, 0x08,                    // REPORT_SIZE (8 bits)  // каждый элемент репорта это 8 бит (т.е. один байт)
  0x95, HID_OUT_PACKET_SIZE-1,   // REPORT_COUNT (HID_OUT_PACKET bytes) количество элементов репорта (кол-во указанно на 1 меньше, т.к. первый байт репорта уже занят под поле REPORT_ID)
  0x09, 0x00,                    // USAGE (0)
  0x91, 0x00,                    // OUTPUT (Data,Var,Abs)  // это выходной от хоста репорт

  0x85, 0x02,                    // REPORT_ID (2)        // В отправляемом буфере этот идентификатор необходимо отправлять обязательно, иначе будет зависиние функции чтения хоста
  0x75, 0x08,                    // REPORT_SIZE (8 bits)
  0x95, HID_IN_PACKET_SIZE-1,    // REPORT_COUNT (HID_IN_PACKET bytes) количество элементов репорта (кол-во указанно на 1 меньше, т.к. первый байт репорта уже занят под поле REPORT_ID)
  0x09, 0x00,                    // USAGE (0)
  0x81, 0x00,                    // INPUT (Data,Var,Abs) // это входной репорт, который идёт в хост

  0xC0                           // END_COLLECTION
};

/* Описатель устройства USB */
const uint8_t USB_DeviceDescriptor[USB_DEVICE_DESC_SIZE] =
{
  USB_DEVICE_DESC_SIZE,       /*bLength */ // // общая длина дескриптора устройства в байтах
  USB_DEVICE_DESCRIPTOR_TYPE, /*bDescriptorType*/ // bDescriptorType - показывает, что это за дескриптор. В данном случае - Device descriptor (описатель устройства)
  WBVAL(0x0100),              /*bcdUSB */  // // bcdUSB - какую версию стандарта USB поддерживает устройство. 1.0

  // класс, подкласс устройства и протокол, по стандарту USB. У нас нули, означает каждый интерфейс сам за себя
  0x00,                       /*bDeviceClass*/
  0x00,                       //bDeviceSubClass - это значение должно быть таким в дескрипторе устройства
  0x00,                       //bDeviceProtocol - это значение должно быть таким в дескрипторе устройства

  USB_MAX_PACKET0,           /*bMaxPacketSize*/ //bMaxPacketSize - максимальный размер пакетов для Endpoint 0 (64 байта для HS устройств и 8 для других)
  // VID и PID,  по которым и определяется, что же это за устройство.
  WBVAL(USBD_VID),   /*idVendor*/
  WBVAL(USBD_PID),   /*idProduct*/

  WBVAL(0x0100),     // bcdDevice версия устройства

  // дальше идут индексы строк, описывающих производителя, устройства и серийный номер.
  // Отображаются в свойствах устройства в диспетчере устройств
  // А по серийному номеру подключенные устройства с одинаковым VID/PID различаются системой.   
  
  0x04,       /*Index of manufacturer  string*/
  0x20,       /*Index of product string*/
  0x3E,       /*Index of serial number string*/
  1           /*bNumConfigurations*/ // bNumConfigurations - количество возможных конфигураций
}; /* CustomHID_DeviceDescriptor */



/* USB Configuration Descriptor All Descriptors (Configuration, Interface, Endpoint, Class, Vendor */
const uint8_t USB_ConfigDescriptor[USB_CONFIG_DESC_SIZE] =
{
  0x09,                               // bLength: Configuration Descriptor size  // bLength: длина дескриптора конфигурации
  USB_CONFIGURATION_DESCRIPTOR_TYPE,  // bDescriptorType: Configuration  // bDescriptorType: тип дескриптора - конфигурация
  WBVAL(USB_CONFIG_DESC_SIZE),         // wTotalLength: общий размер всего дерева под данной конфигурацией в байтах

  0x01,         /*bNumInterfaces: 1 interface*/ // bNumInterfaces: в конфигурации всего один интерфейс
  0x01,         /*bConfigurationValue: Configuration value*/ // bConfigurationValue: индекс данной конфигурации
  0x00,         /*iConfiguration: Index of string descriptor describing the configuration*/ // iConfiguration: индекс строки, которая описывает эту конфигурацию
  0x60,         /*bmAttributes: bus powered and Support Remote Wake-up */ // bmAttributes: признак того, что устройство имеет свой источник питания и имеет возможность пробуждаться
  0x32,         /*MaxPower 100 mA: this current is used for detecting Vbus*/ // MaxPower 100 mA: устройству хватит 100 мА
  
    /************** Дескриптор интерфейса ****************/
    0x09,                               // bLength: размер дескриптора интерфейса
    USB_INTERFACE_DESCRIPTOR_TYPE,      // bDescriptorType: тип дескриптора - интерфейс
    0x00,                               // bInterfaceNumber: порядковый номер интерфейса - 0
    0x00,                               // bAlternateSetting: признак альтернативного интерфейса, у нас не используется
    0x02,                               // bNumEndpoints - количество конечных точек
    0x03,                               // bInterfaceClass: класс интерфеса - HID

    // если бы мы косили под стандартное устройство, например клавиатуру или мышь, то надо было бы указать правильно класс и подкласс, а так у нас общее HID-устройство
    0x00,         // bInterfaceSubClass : подкласс интерфейса.
    0x00,         // nInterfaceProtocol : протокол интерфейса
    0x00,         // iInterface: индекс строки, описывающей интерфейс

      // ****************** Описание устройства Custom HID
      0x09,                                     // bLength: длина HID-дескриптора
      0x21,                                     // bDescriptorType: тип дескриптора - HID
      WBVAL(0x0100),                            // bcdHID: номер версии HID 1.0
      0x00,                                     // bCountryCode: код страны (если нужен)
      0x01,                                     // bNumDescriptors: Сколько дальше будет report дескрипторов
      HID_REPORT_DESCRIPTOR_TYPE,               // bDescriptorType: Тип дескриптора - report
      WBVAL(HID_REPORT_DESC_SIZE), // wItemLength: длина report-дескриптора

        /******************** дескриптор конечных точек (endpoints) ********************/
        // конечная точка может иметь номер от 0 до 15
        // 0 - конечная точка должна быть всегда она зарезервированна для конфигурации USB устройства
        // через неё можно принимать и передавать данные не относящиеся к конфигурации USB устройства, но это не по стандарту (подробнее см. libusb.dll)
        // конечные точки кроме 0-й работают только в одном направлении
        // конечные точки с 1 по 15 могут работать только на выход (по отношению к хосту, как принято при "разговорах о USB")
        // конечные точки с 1 по 15 с установленым 7-м битом в номере точки (т.е. реально номер конечной точки получается 129-143) работают только вход.
        // т.е. на самом деле конечных точек 30 штук + 1 (15 - вход, 15 - выход, 1 - двунаправленная для управления)

        // 1-я конечная точка
        0x07,	                        // bLength: Endpoint Descriptor size
        USB_ENDPOINT_DESCRIPTOR_TYPE,	// bDescriptorType:  Endpoint descriptor type
        0x01,                         // bEndpointAddress: Endpoint Address (OUT)
        0x03,                         // bmAttributes: Interrupt endpoint
        WBVAL(HID_OUT_PACKET_SIZE),   // wMaxPacketSize:  Bytes max
        1,  	                        // bInterval: Polling Interval [ms] частота опроса конечной точки

        // 2-я конечная точка
        0x07,                           // bLength: длина дескриптора
        USB_ENDPOINT_DESCRIPTOR_TYPE,   // тип дескриптора - endpoints
        0x81,                           // bEndpointAddress: адрес конечной точки и направление (IN)
        0x03,                           // bmAttributes: тип конечной точки - Interrupt endpoint
        WBVAL(HID_IN_PACKET_SIZE),      // wMaxPacketSize:  Bytes max // Максимальный размер пакета, для обмена с управляющей машиной
        1                               // bInterval: Polling Interval [ms] // частота опроса конечной точки
};


/* USB String Descriptor (optional) */
const uint8_t USB_StringDescriptor[] = 
{
  /* Index 0x00: LANGID Codes */
  0x04,                              /* bLength */
  USB_STRING_DESCRIPTOR_TYPE,        /* bDescriptorType */
  WBVAL(0x0409), /* US English */    /* wLANGID */

  /* Index 0x04: Manufacturer */
  28,                              // bLength - Указывается в байтах и включает поля bLength и bDescriptorType, т.е. сам текст должен быть на 2 байта меньше, чем указанно в этом поле
  USB_STRING_DESCRIPTOR_TYPE,      // bDescriptorType - тип дескриптора (строковый дескриптор)
  'K',0,                           // Строка передаётся в формате UNICODE, поэтому для каждого символа 2 байта
  'e',0,
  'i',0,
  'l',0,
  ' ',0,
  'S',0,
  'o',0,
  'f',0,
  't',0,
  'w',0,
  'a',0,
  'r',0,
  'e',0,

  /* Index 0x20: Product */
  30,                              /* bLength */
  USB_STRING_DESCRIPTOR_TYPE,        /* bDescriptorType */
  'U',0,
  'S',0,
  'B',0,
  '-',0,
  'T',0,
  'o',0,
  '-',0,
  'C',0,
  'A',0,
  'N',0,
  ' ',0,
  'H',0,
  'I',0,
  'D',0,

  /* Index 0x40: Serial Number */
  22,                              /* bLength */
  USB_STRING_DESCRIPTOR_TYPE,        /* bDescriptorType */
  'v',0,
  'e',0,
  'r',0,
  '-',0,
  '0',0,
  '0',0,
  '0',0,
  '0',0,
  '0',0,
  '1',0,

  /* Index 0x56: Interface 0, Alternate Setting 0 */
  0x08,                              /* bLength */
  USB_STRING_DESCRIPTOR_TYPE,        /* bDescriptorType */
  'H',0,
  'I',0,
  'D',0,
};
