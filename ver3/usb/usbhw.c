#include "stm32f303xc.h"
#include "usbcore.h"
#include "usbhw.h"
#include "usbdesc.h"

#define REG(x)  (*((volatile unsigned int *)(x)))

/* EndPoint Registers */
#define EPxREG(x)       REG(USB_BASE + 4*(x))
#define EP_COUNT_MASK   0x03FF      /* Count Mask */

typedef struct
{
  uint32_t ADDR_TX;
  uint32_t COUNT_TX;
  uint32_t ADDR_RX;
  uint32_t COUNT_RX;
}EP_BUF_DSCR;


#define EP_BUF_ADDR  (sizeof(EP_BUF_DSCR)*USB_EP_NUM) /* Endpoint Buffer Start Address */

/* Pointer to Endpoint Buffer Descriptors */
EP_BUF_DSCR *pBUF_DSCR = (EP_BUF_DSCR *)USB_PMAADDR;

uint16_t FreeBufAddr; /* Endpoint Free Buffer Address */

void (* const OnEndPointEvent[USB_EP_NUM]) (uint32_t event) = // Массив указателей на функций-обработчики для конечных точек
{
  EndPoint0Event,  // Функция обработчик для конечной точки 0
  EndPoint1Event   // Функция обработчик для конечной точки 1
  // По аналогии нужно добавлять сюда функции-обработчики для других конечных точек
};


void EP_Reset(uint32_t EPNum)
{
  uint32_t num, val;

  num = EPNum & 0x0F;
  val = EPxREG(num);
  if (EPNum & 0x80)                      /* IN Endpoint */
    EPxREG(num) = val & (USB_EPREG_MASK | USB_EP_DTOG_TX);
  else                                  /* OUT Endpoint */
    EPxREG(num) = val & (USB_EPREG_MASK | USB_EP_DTOG_RX);
}


void EP_Status(uint32_t EPNum, uint32_t stat) 
{
  uint32_t num, val;

  num = EPNum & 0x0F;
  val = EPxREG(num);
  if (EPNum & 0x80)                       /* IN Endpoint */
    EPxREG(num) = (val ^ (stat & USB_EPTX_STAT)) & (USB_EPREG_MASK | USB_EPTX_STAT);
  else                                  /* OUT Endpoint */
    EPxREG(num) = (val ^ (stat & USB_EPRX_STAT)) & (USB_EPREG_MASK | USB_EPRX_STAT);
}

void USB_Connect(uint32_t con) // Функция инициализации подключения
{
  uint32_t interruptmask = 0;

  if (con)
  {
    /* Set winterruptmask variable */
    interruptmask = USB_CNTR_CTRM  | USB_CNTR_WKUPM | USB_CNTR_SUSPM | USB_CNTR_ERRM  | USB_CNTR_ESOFM | USB_CNTR_RESETM;
    USB->CNTR &= ~interruptmask;
    USB->CNTR = USB_CNTR_FRES;
    USB->CNTR = 0;
    USB->ISTR = 0;
    USB->CNTR |= interruptmask;                     /* USB Reset Interrupt Mask */

    NVIC_EnableIRQ(USB_LP_CAN_RX0_IRQn);  // Разрешение прерывания от USB
  }
  else
  {
    USB->CNTR = USB_CNTR_FRES | USB_CNTR_PDWN;           /* Switch Off USB Device */
    NVIC_DisableIRQ(USB_LP_CAN_RX0_IRQn);  // Разрешение прерывания от USB
  }
}

void USB_Reset(void) // Сброс
{
  USB->ISTR = 0;                                 /* Clear Interrupt Status */

  // Полный набор флагов, показан тут, чтобы иметь представление о том какие бывают ( хотя можно и в *.h файле посмотреть)
  //CNTR = CNTR_CTRM | CNTR_RESETM | CNTR_SUSPM | CNTR_WKUPM | CNTR_ERRM | CNTR_PMAOVRM | CNTR_SOFM | CNTR_ESOFM;

  // Минимальный набор ( CNTR_CTRM - Прерывание возникае при корректной передаче, CNTR_RESETM - USB сброс)
  USB->CNTR = USB_CNTR_CTRM | USB_CNTR_RESETM;

  FreeBufAddr = EP_BUF_ADDR;

  USB->BTABLE = 0x00;                            /* set BTABLE Address */

  /* Setup Control Endpoint 0 */
  
  pBUF_DSCR->ADDR_TX = FreeBufAddr;
  FreeBufAddr += USB_MAX_PACKET0;
  pBUF_DSCR->ADDR_RX = FreeBufAddr;
  FreeBufAddr += USB_MAX_PACKET0;
  if (USB_MAX_PACKET0 > 62) 
    pBUF_DSCR->COUNT_RX = ((USB_MAX_PACKET0 << 5) - 1) | 0x8000;
  else 
    pBUF_DSCR->COUNT_RX =   USB_MAX_PACKET0 << 9;

  EPxREG(0) = USB_EP_CONTROL | USB_EP_RX_VALID;
  USB->DADDR = USB_DADDR_EF | 0;                     /* Enable USB Default Address */
}

void USB_Suspend(void) // Приостановка работы USB
{
  USB->CNTR |= USB_CNTR_FSUSP;                       /* Force Suspend */
  USB->CNTR |= USB_CNTR_LP_MODE;                      /* Low Power Mode */
}

void USB_WakeUp (void) 
{
  USB->CNTR &= ~USB_CNTR_FSUSP;                      /* Clear Suspend */
}

void USB_SetAddress(uint32_t adr) // Задание адреса для устройства
{
  USB->DADDR = USB_DADDR_EF | adr;
}

void USB_ConfigEP(void *p) // Конфигурация конечной точки
{
  uint32_t num, val;
  USB_ENDPOINT_DESCRIPTOR *pEPD = p;

  num = pEPD->bEndpointAddress & 0x0F; // Номер конфигурируемой точки
  val = pEPD->wMaxPacketSize;  // Максимальный размер пакета для конечной точки
  if (pEPD->bEndpointAddress & 0x80)  // Если точка передаёт данные хосту, то
  {
    (pBUF_DSCR + num)->ADDR_TX = FreeBufAddr; // Адрес буфера для 
    val = (val + 1) & ~1;
  }
  else
  {
    (pBUF_DSCR + num)->ADDR_RX = FreeBufAddr;
    if (val > 62)
    {
      val = (val + 31) & ~31;
      (pBUF_DSCR + num)->COUNT_RX = ((val << 5) - 1) | 0x8000;
    }
    else
    {
      val = (val + 1)  & ~1;
      (pBUF_DSCR + num)->COUNT_RX =   val << 9;
    }
  }
  FreeBufAddr += val;

  switch (pEPD->bmAttributes & USB_ENDPOINT_TYPE_MASK)
  {
    case USB_ENDPOINT_TYPE_CONTROL:
                                  val = USB_EP_CONTROL;
                                  break;
    case USB_ENDPOINT_TYPE_ISOCHRONOUS:
                                  val = USB_EP_ISOCHRONOUS;
                                  break;
    case USB_ENDPOINT_TYPE_BULK:
                                  val = USB_EP_BULK;
                                  if (USB_DBL_BUF_EP & (1 << num)) val |= USB_EP_KIND;
                                  break;
    case USB_ENDPOINT_TYPE_INTERRUPT:
                                  val = USB_EP_INTERRUPT;
                                  break;
  }
  val |= num;
  EPxREG(num) = val;
}


void USB_DisableEP(uint32_t EPNum) { EP_Status(EPNum, USB_EP_TX_DIS | USB_EP_RX_DIS); }
void USB_ResetEP(uint32_t EPNum) { EP_Reset(EPNum); }
void USB_SetStallEP(uint32_t EPNum) { EP_Status(EPNum, USB_EP_TX_STALL | USB_EP_RX_STALL); } // Перевод точки в состояние STALL
void USB_SetNAK(uint32_t EPNum) { EP_Status(EPNum, USB_EP_TX_NAK); }
void USB_SetValidEP(uint32_t EPNum) { EP_Status(EPNum, USB_EP_TX_VALID | USB_EP_RX_VALID); } // Перевод точки в нормальное состояние

uint32_t USB_ReadEP(uint32_t EPNum, uint8_t *pData) // Чтение данных из конечной токи с заданным номером
{
  uint32_t num, cnt, *pv, n;

  num = EPNum & 0x0F;

  pv  = (uint32_t *)(USB_PMAADDR + 2*((pBUF_DSCR + num)->ADDR_RX));
  cnt = (pBUF_DSCR + num)->COUNT_RX & EP_COUNT_MASK;
  for (n = 0; n < (cnt + 1) / 2; n++)
  {
    *((uint16_t *)pData) = *pv++;
    pData += 2;
  }
  EP_Status(EPNum, USB_EP_RX_VALID);

  return cnt; // Возвращается количество записанных данных
}


uint32_t USB_WriteEP(uint32_t EPNum, uint8_t *pData, uint32_t cnt) // Запись данных с памощью заданной конечной точки
{
  uint32_t num, *pv, n, len;

  num = EPNum & 0x0F;
  len = (cnt + 1) >> 1;

  pv  = (uint32_t *)(USB_PMAADDR + 2*((pBUF_DSCR + num)->ADDR_TX));
  for (n=0; n<len; n++)
  {
    *pv++ = *((uint16_t *)pData);
    pData += 2;
  }
  (pBUF_DSCR + num)->COUNT_TX = cnt;
  EP_Status(EPNum, USB_EP_TX_VALID);

  return cnt; // Возвращается количество записанных данных
}

uint32_t USB_GetFrame(void) // Крайний номер фрейма которым передавались данные
{
  return (USB->FNR & USB_FNR_FN);
}

void USB_IRQHandler(void)
{
  uint32_t istr, num, val;

  istr = USB->ISTR;  // Interrupt Status Register

  if(istr & USB_ISTR_RESET) // Сброс устройства
  {
    USB_Reset();
    USB->ISTR = ~USB_ISTR_RESET;
  }

  if(istr & USB_ISTR_SUSP) // Запрос на приостановку
  {
    USB_Suspend();
    USB->ISTR = ~USB_ISTR_SUSP;
  }

  if (istr & USB_ISTR_WKUP) // Устройство будет хост
  {
    USB_WakeUp();
    USB->ISTR = ~USB_ISTR_WKUP;
  }

  if (istr & USB_ISTR_SOF) // Начало пакета
    USB->ISTR = ~USB_ISTR_SOF;

  if((istr = USB->ISTR) & USB_ISTR_CTR) // Если "Correct Transfer", выполняем следующие действия
  {
    USB->ISTR &= ~USB_ISTR_CTR; // Снимается бит "Correct Transfer" в регистре ISTR ( Interrupt Status Register )

    num = istr & USB_ISTR_EP_ID; // Определяем номер конечной точки (идентификатор конечной точки)

    val = EPxREG(num);   // Получение регистра связанного с заданным номером конечной точки
    if(val & USB_EP_CTR_RX)  // Если возникло прерывание "Correct RX Transfer" (корректный приём данных)
    {
      EPxREG(num) = val & ~USB_EP_CTR_RX & USB_EPREG_MASK; // Сброс бита "Correct RX Transfer" и
      if(OnEndPointEvent[num]) // если существует обработчик для заданной точки, то идём дальше
      {
        if (val & USB_EP_SETUP) // Если был получен EP_SETUP, пакет, то запуск обработчика установочного пакета
          OnEndPointEvent[num](USB_EVT_SETUP);   // Запуск обработчика установочного пакета
        else
          OnEndPointEvent[num](USB_EVT_OUT); // Вызов обработчика для приёма данных от хоста
      }
    }

    if (val & USB_EP_CTR_TX) // Если была выполнена успешная передача данных, то
    {
      EPxREG(num) = val & ~USB_EP_CTR_TX & USB_EPREG_MASK;
      if (OnEndPointEvent[num])  // Если есть обработчик для конечной точки num, то запускаем его
        OnEndPointEvent[num](USB_EVT_IN); // Вызов обработчика для передачи данных хосту
    }
  }
}
