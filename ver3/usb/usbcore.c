#include "usbhw.h"
#include "usbcore.h"
#include "usbdesc.h"


USB_HID_Device Test_USB_HID_Dev; // Переменная, которая хранит параметры USB устройства

void USB_DataInStage(USB_HID_Device *uhd) // Передача данных Хосту
{ // Передача данных хосту 
  // т.к. количество байт, которое конечная точка может передать за одну передачу ограниченно
  // весь буфер для выдачи (данных хосту) выполняется за несколько раз
  uint32_t cnt;

  if (uhd->EP0Data.Count > USB_MAX_PACKET0) 
    cnt = USB_MAX_PACKET0;
  else 
    cnt = uhd->EP0Data.Count;

  cnt = USB_WriteEP(0x80, uhd->EP0Data.pData, cnt);
  uhd->EP0Data.pData += cnt;
  uhd->EP0Data.Count -= cnt;
}

void USB_DataOutStage(USB_HID_Device *uhd) // приём данных через нулевую точку
{ // Приём организован так, что недополученные данные (конечная точка имеет ограниченный буфер приёма) с прошлого приёма добавляются к уже полученным в текущем приёме
  uint32_t cnt;

  cnt = USB_ReadEP(0x00, uhd->EP0Data.pData);
  uhd->EP0Data.pData += cnt;
  uhd->EP0Data.Count -= cnt;
}

uint32_t USB_GetDescriptor(USB_HID_Device *uhd) // Обработчик запроса GET_DESCRIPTOR 
{
  uint8_t  *pD;
  uint32_t len, n;

  switch (uhd->sp.bmRequestType.BM.Recipient)
  {
    case REQUEST_TO_DEVICE:  // получатель запроса - устройство USB
                            switch (uhd->sp.wValue.WB.H) 
                            {
                              case USB_DEVICE_DESCRIPTOR_TYPE:
                                                              uhd->EP0Data.pData = (uint8_t *)USB_DeviceDescriptor; // Адрес дескриптора устройства
                                                              len = USB_DEVICE_DESC_SIZE; // Указываем размер дескриптора устройства

                                                              break;
                              case USB_CONFIGURATION_DESCRIPTOR_TYPE:
                                                                    // При запросе дескриптора конфигурации в младшем байте SetupPacket.wValue (SetupPacket.wValue.WB.L) 
                                                                    // хранится индекс дескриптора (т.к. дескрипторов конфигурации может быть несколько)
                                                                    pD = (uint8_t *)USB_ConfigDescriptor;

                                                                    // В этом цикле выполняется переход на указанный номер дескриптора конфигурации
                                                                    // этот цикл не нужен, если у стройства есть только одна конфигурация
                                                                    for (n=0; n != uhd->sp.wValue.WB.L; n++) 
                                                                    {
                                                                      if (((USB_CONFIGURATION_DESCRIPTOR *)pD)->bLength != 0)
                                                                        pD += ((USB_CONFIGURATION_DESCRIPTOR *)pD)->wTotalLength;

                                                                    }
                                                                    // Если после перехода к указанному дескриптору конфигурации его длина оказалась 0-й, то выходим из функции
                                                                    if (((USB_CONFIGURATION_DESCRIPTOR *)pD)->bLength == 0)
                                                                      return 0; // Если возвращается 0, то в ответ будет передан пакет STALL

                                                                    uhd->EP0Data.pData = pD;  // Адрес с которого начинается передача
                                                                    len = ((USB_CONFIGURATION_DESCRIPTOR *)pD)->wTotalLength;  // Количество данных которое необходимо отправить

                                                                    //stdprintf("EP0Data.Count=%d  len=%d\n", EP0Data.Count, len);
                                                                    
                                                                    break;
                              case USB_STRING_DESCRIPTOR_TYPE:
                                                              uhd->EP0Data.pData = (uint8_t *)USB_StringDescriptor + uhd->sp.wValue.WB.L;
                                                              len = ((USB_STRING_DESCRIPTOR *)uhd->EP0Data.pData)->bLength;
                                                              break;
                              default: return 0;
                            }
                            break;
    case REQUEST_TO_INTERFACE:  // Получатель запроса - интерфейс
                              switch (uhd->sp.wValue.WB.H) // какой дескриптор должно устройство передать хосту
                              {
                                case HID_HID_DESCRIPTOR_TYPE:   // Хост "просит" HID дескриптор (SetupPacket.wIndex.WB.L - для какого интерфейса возвращать дескриптор)
                                                            if (uhd->sp.wIndex.WB.L != 0) return 0;  // Если номер интерфейса не 0-й, то выходим т.к. в устройстве поддерживается только один интерфейс под номером 0
                                                            uhd->EP0Data.pData = (uint8_t *)USB_ConfigDescriptor + HID_DESC_OFFSET;
                                                            len = HID_DESC_SIZE;
                                                            break;
                                case HID_REPORT_DESCRIPTOR_TYPE:  // Хост "просит" репорт дескриптор
                                                                if (uhd->sp.wIndex.WB.L != 0) return 0; // Если номер интерфейса не 0-й, то выходим т.к. в устройстве поддерживается только один интерфейс под номером 0
                                                                uhd->EP0Data.pData = (uint8_t *)HID_ReportDescriptor;
                                                                len = HID_REPORT_DESC_SIZE;
                                                                break;
                                default:
                                        return 0;
                              }
                              break;
    default: return 0;
  }

  if(uhd->EP0Data.Count>len) uhd->EP0Data.Count=len; // Формируем количество байт для передачи
  USB_DataInStage(uhd);
 
  return 1;
}

uint32_t USB_SetConfiguration(USB_HID_Device *uhd) 
{
  uint8_t *pD;
  uint32_t alt, n, m;

  if(uhd->sp.wValue.WB.L) // Если есть номер конфигурации (номера конфигурации нумеруются с 1) , то
  {
    pD = (uint8_t *)USB_ConfigDescriptor;
    while (((USB_COMMON_DESCRIPTOR *)pD)->bLength) 
    {
      switch (((USB_COMMON_DESCRIPTOR*)pD)->bDescriptorType) 
      {
        case USB_CONFIGURATION_DESCRIPTOR_TYPE: // У дескриптора конфигурации выполняется задаётся номер конфигурации, который передаётся в параметрах конфигурационного пакета
                                              if(((USB_CONFIGURATION_DESCRIPTOR *)pD)->bConfigurationValue == uhd->sp.wValue.WB.L) 
                                              {
                                                uhd->ConfigOK = uhd->sp.wValue.WB.L; // Формирование номера текущей, выбранной хостом конфигурации

                                                for (n = 1; n < 16; n++) 
                                                {
                                                  if(uhd->EPEnableMask & (1 << n)) USB_DisableEP(n);
                                                  if(uhd->EPEnableMask & ((1 << 16) << n)) USB_DisableEP(n | 0x80);
                                                }
                                                uhd->EPEnableMask = 0x00010001;
                                              } 
                                              else 
                                              {
                                                pD += ((USB_CONFIGURATION_DESCRIPTOR *)pD)->wTotalLength;
                                                continue;
                                              }
                                              break;
        case USB_INTERFACE_DESCRIPTOR_TYPE:
                                          alt = ((USB_INTERFACE_DESCRIPTOR *)pD)->bAlternateSetting; // Определяем признак альтернативного интерфейса
                                          break;
        case USB_ENDPOINT_DESCRIPTOR_TYPE:
                                          if (alt == 0) 
                                          {
                                            n = ((USB_ENDPOINT_DESCRIPTOR *)pD)->bEndpointAddress & 0x8F;

                                            // Формирование маски включённых точек
                                            m = (n & 0x80) ? ((1 << 16) << (n & 0x0F)) : (1 << n);
                                            uhd->EPEnableMask |= m;
                                            USB_ConfigEP((USB_ENDPOINT_DESCRIPTOR *)pD);
                                            USB_SetValidEP(n);
                                            USB_ResetEP(n);
                                          }
                                          break;
      }
      pD += ((USB_COMMON_DESCRIPTOR *)pD)->bLength;
    }
  }
  else 
  { // Если в конфигурационном пакете не задан номер конфигурации (нумеруется с 1), то
    uhd->ConfigOK = 0;    // текущая конфигурация 0-я (т.е. конфигурации нет)
    for (n = 1; n < 16; n++)   // Отключение всех точек
    {
      if(uhd->EPEnableMask & (1 << n)) USB_DisableEP(n);
      if(uhd->EPEnableMask & ((1 << 16) << n)) USB_DisableEP(n | 0x80);
    }
    uhd->EPEnableMask  = 0x00010001;
    return 0;
  }

  if(uhd->ConfigOK == uhd->sp.wValue.WB.L) // Если номер текущей конфигурации совпадает с номером в конфигурационным пакетом, то
    return 1; // возвращаем положительный результат
  return 0;
}

void USB_EP0_Setup_package(USB_HID_Device *uhd) // Процедура обрабатывает приход к USB устройству конфигурационного пакета
{
  int stall=0;

  USB_ReadEP(0x00, (uint8_t *)&uhd->sp); // Выполняем чтение конфигурационного покета
  uhd->EP0Data.Count = uhd->sp.wLength;

  switch(uhd->sp.bmRequestType.BM.Type) 
  {
    case REQUEST_STANDARD: // Если это стандартный запрос, то
                          switch (uhd->sp.bRequest) // Проверка стандартных запросов к устройству
                          {
                            case USB_REQUEST_GET_DESCRIPTOR: // запрос к устройству для получения дескриптора (дескриптора устройства, кофигурации, конечной точки ...)
                                                            if(!USB_GetDescriptor(uhd)) stall = 1;  // USB_GetDescriptor() - передача хосту запрошенного дескриптора
                                                            break;

                            case USB_REQUEST_SET_ADDRESS:  // Запрос для задания адрес USB устройства на шине
                                                           // адрес переданый устройству пока только запоминается
                                                           // адрес будет установлен в регистр адреса DADDR, в функции
                                                           // USB_SetAddress; (почему так сделано нужно разбираться)

                                                          switch (uhd->sp.bmRequestType.BM.Recipient) 
                                                          {
                                                            case REQUEST_TO_DEVICE:
                                                                                    uhd->Address = 0x80 | uhd->sp.wValue.WB.L;  // Запоминаем адрес устройства на шине
                                                                                    USB_WriteEP(0x80, ((void *)0), 0); // Эта запись означает, что передача данных не производится
                                                                                    break;
                                                            default: stall = 1;
                                                          }
                                                          break;

                            case USB_REQUEST_SET_CONFIGURATION:  // Хост указывает устройству какую конфигурацию необходимо установить
                                                              switch (uhd->sp.bmRequestType.BM.Recipient) 
                                                              {
                                                                case REQUEST_TO_DEVICE:
                                                                                        if(!USB_SetConfiguration(uhd)) {stall = 1; break;} 
                                                                                        USB_WriteEP(0x80, ((void *)0), 0); // Эта запись означает, что передача данных не производится
                                                                                        USB_WriteEP(0x81, ((void *)0), 0);
                                                                                        break;
                                                                default:  stall = 1;
                                                              }
                                                              break;
                            default: stall = 1;
                          }
                          break;

    case REQUEST_CLASS:  // Запрос специфический для данного класса (для работы HID устройства все эти запросы можно проигнорировать)
    default: stall = 1;
  }

  if(stall)
  {
    USB_SetValidEP(0x80); // передача пакета STALL, такой пакет передаётся в том случае, когда произошол сбой в устройстве или запрос устройством не поддерживается
    uhd->EP0Data.Count = 0;
  }
}

void EndPoint0Event(uint32_t event) // Процедура обрабатывает пакет приходящий от Хоста (конфигурационный пакет, передача данных, приём данных, перевод точки в состояние STALL)
{
  USB_HID_Device *uhd;
  uhd = &Test_USB_HID_Dev; // Формируем указатель на USB устройства

  switch (event)
  {
    case USB_EVT_SETUP:  // пакет Setup от хоста (конфигурационный пакет)
                      USB_EP0_Setup_package(uhd);
                      break;

    case USB_EVT_OUT: // Если хост передаёт данные устройству
                    if(uhd->sp.bmRequestType.BM.Dir == 0) // Данные приходят от хоста к USB устройству
                    {
                      if(uhd->EP0Data.Count) // Если EP0Data.Count<>0, значит предыдущие данные были получены не полностью
                      {
                        USB_DataOutStage(uhd);  // получаем очередную порцию данных
                        if(uhd->EP0Data.Count == 0) // Если хост "говорит", что передаёт данные, но количество данных равно нулю, то
                        {
                          USB_SetStallEP(0x80); // передача пакета STALL, такой пакет передаётся в том случае, когда произошёл сбой в устройстве или запрос устройством не поддерживается
                          break;
                        }
                      }
                    } 
                    else 
                      USB_ReadEP(0x00, uhd->EP0Buf); // Получение статуса (USB_StatusOutStage();)

                    break;

    case USB_EVT_IN:  // Хост запрашивает данные у USB устройства
                    if (uhd->sp.bmRequestType.BM.Dir == 1) 
                      USB_DataInStage(uhd);
                    else
                      if(uhd->Address & 0x80) 
                      {
                        uhd->Address &= 0x7F;
                        USB_SetAddress(uhd->Address); // Формирование адреса в регистре адреса USB устройства
                      }
                    break;

    case USB_EVT_IN_STALL:
                          USB_SetValidEP(0x80);
                          break;

    case USB_EVT_OUT_STALL:
                          USB_SetValidEP(0x00);
                          break;

  }
}
