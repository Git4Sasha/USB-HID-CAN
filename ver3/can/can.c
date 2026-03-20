#include "can.h"
#include "stm32f303xc.h"
#include "system_stm32f3xx.h"
#include "systimer.h"
#include "gpio.h"


#define CAN_RDTR_DLC  ((uint32_t)0x0000000F) // для определения длины данных для любого фифо
#define CAN_RIR_IDE   ((uint32_t)0x00000004) // Бит для определения расширенного или стандартного идентификатора (для любого фифо)
#define CAN_TIR_TXRQ  ((uint32_t)0x00000001)       
#define CAN_TIR_IDE   ((uint32_t)0x00000004)
#define CAN_TIR_RTR   ((uint32_t)0x00000002)

static uint32_t numWriteTransmitMailbox; // номер fifo в которое последний раз писалось сообщение (переменная необходима для функции CAN1CheckLastWrite)
static void (*fifo0recvmsg)(uint32_t id, uint8_t len, uint8_t *data)=0; // Переменная, которая хранит адрес функции, которая будет запускаться в прерывании при приёме сообщения в FIFO_0

void CAN_IRQHandler(void) // Обработка прерывания при поступлении сообщения в FIFO_0
{
  uint32_t id;
  uint8_t data[8];
  uint8_t len;

  if(CAN->RF0R & CAN_RF0R_FMP0) // Если кол-во принятых сообщений не равно нулю, значит возникло прерывание по приходу сообщения
  { // тут необходимо организовать приём сообщений в очередь
    CAN_Read(0, &id, data, &len, 0); // Чтение данных из FIFO_0

    // Если задана функция для обработки данного события, то вызываем эту функцию
    if(fifo0recvmsg) fifo0recvmsg(id, len, data);
  }
}

void CAN_Init(CanBaudRate baudRate, uint32_t remappin, uint32_t mode, uint32_t useRecvInt) // инициализация CAN1 на заданную скорость обмена
{
  // инициализация ноже, которые будут CAN_TD и CAN_RD
  // ВНИМАНИЕ CAN не будет нормально работать (в нормальном режиме), если выводы CAN_TD и CAN_RD не подключены к схеме физического уровня CANа

  typedef struct
  {
    uint8_t brp;
    uint8_t ts1;
    uint8_t ts2;
    uint8_t sjw;
  }can_timing_t;

  static const can_timing_t brs[] = { // настройки для различных скоростей передачи по CAN шине
                                      {2, 6, 3, 0},    // Скорость 1Мбит / сек при условии, что частота шины APB1 = 36 МГц
                                      {7, 3, 3, 2},
                                      {15, 3, 3, 3},
                                      {31, 3, 3, 1},
                                      {63, 3, 3, 1}
                                    };

  
  if(remappin) // в режиме переназначения ног CAN_RD - PB8,   CAN_TD - PB9
  {
    ConfigGPIO(GPIOB, 8, MODE_ALTER_FUNC, OTYPE_PUSH_PULL, SPEED_HI, PULL_NO_UP_NO_DOWN, 9); // Альтернативная функция 9 - CAN_Rx
    ConfigGPIO(GPIOB, 9, MODE_ALTER_FUNC, OTYPE_PUSH_PULL, SPEED_HI, PULL_NO_UP_NO_DOWN, 9); // Альтернативная функция 9 - CAN_Tx
  }
  else // в режиме ног по умолчанию CAN_RD - PA11,   CAN_TD - PA12
  {
    ConfigGPIO(GPIOA, 11, MODE_ALTER_FUNC, OTYPE_PUSH_PULL, SPEED_HI, PULL_NO_UP_NO_DOWN, 9); // Альтернативная функция 9 - CAN_Rx
    ConfigGPIO(GPIOA, 12, MODE_ALTER_FUNC, OTYPE_PUSH_PULL, SPEED_HI, PULL_NO_UP_NO_DOWN, 9); // Альтернативная функция 9 - CAN_Tx
  }

  RCC->APB1ENR |= RCC_APB1ENR_CANEN;  // включение тактирования CAN1

  // Перед тем как настраивать CAN нужно войти в режим инициализации, по окончанию настройки нужно выйти из режима инициализации
  
  CAN->MCR = 0;                        // Сброс ( и автоматический выход из сна)
  CAN->MCR |= CAN_MCR_INRQ;            // запрос на вход в режим инициализации

  // ждем, пока хардваре не войдет в режим инициализации
  while(!(CAN->MSR & CAN_MSR_INAK));

  //настраиваем бит тайминг  
  CAN->BTR = 0;
  CAN->BTR |= brs[baudRate].brp;      // настраиваем частоту тактирования CAN(множитель). Частота рассчитывается как частота шины на которой работает CAN(APB1) домноженная на множитель.
  CAN->BTR |= brs[baudRate].ts1 << 16;
  CAN->BTR |= brs[baudRate].ts2 << 20;
  CAN->BTR |= brs[baudRate].sjw << 24;
  
  switch(mode) // CAN может работать в 4-х режимах
  {
    case 0: break; // ничего делать не нужно, т.к. до этого было CAN1->BTR = 0 // нормальный режим работы (не молчим и нет обратной петли)
    case 1: CAN->BTR |= CAN_BTR_LBKM; break; // не молчим и включена обратная петля (слушаем себя в шину пихаем данные)
    case 2: CAN->BTR |= CAN_BTR_SILM; break; // молчим и нет обратной петли (слушаем шину)
    case 3: CAN->BTR |= (CAN_BTR_SILM | CAN_BTR_LBKM); break; // молчание и петля (там можно отлаживаться без схемы физического уровня)
  }

  CAN->MCR |= CAN_MCR_AWUM;  // Автоматически выходить из спящего режима при приходе сообщения
  CAN->MCR |= CAN_MCR_ABOM;  // Автоматически выходить из buss-off режима
  CAN->MCR |= CAN_MCR_NART;  // Сообщение будет отправляться только один раз, если произошла ошибка, то больше попыток отправить сообщение не будет
  CAN->MCR |= CAN_MCR_RFLM;  // ФИФО не будет блокироваться при переполнении, при этом если ФИФО переполнится, то все приходящие пакеты будут игнорироваться
  CAN->MCR |= CAN_MCR_TXFP;  // Отправка пакетов не будет учитывать идентификатор как указатель приоритета передачи
  
  CAN->MCR &= ~CAN_MCR_INRQ;         // выходим из режима инициализации
  CAN->MCR &= ~CAN_MCR_SLEEP;        // это чтобы проснуться

  if(useRecvInt) // Если планируется изпользовать прерывание при приёме сообщения в FIFO_0, то
  {
    CAN->IER |= CAN_IER_FMPIE0; // Разрешение прерывания по приходу сообщения в FIFO_0
    NVIC_EnableIRQ(USB_LP_CAN_RX0_IRQn); // Глобальное разрешение прерывания по приёму пакета из CAN шины в FIFO_0
  }
  else
  {
    CAN->IER &= ~CAN_IER_FMPIE0; // Разрешение прерывания по приходу сообщения в FIFO_0
    NVIC_DisableIRQ(USB_LP_CAN_RX0_IRQn); // Глобальное разрешение прерывания по приёму пакета из CAN шины в FIFO_0
  }

  // По умолчанию устанавливается такой фильтр, который пропускает все сообщения приходящие из сети
  // хотябы один фильтр должен быть активен, иначе сообщения не будут доходить до приёмных ящиков
  
  // для настройки фильтра приёмных сообщений необходимо войти в режим настройки фильтров
  CAN->FMR |= CAN_FMR_FINIT;       // Входим в режим настройки фильтров

  CAN->FA1R = CAN->FM1R = CAN->FS1R = CAN->FFA1R = 0; // Сброс всех фильтров

  CAN->sFilterRegister[0].FR1 = 0 << 5;  // Это идентификатор для фильтрации
  CAN->sFilterRegister[0].FR1 |= 0 << 16 << 5; // это маска для фильтрации
  CAN->sFilterRegister[0].FR2 = 0;
 
  CAN->FA1R |= CAN_FA1R_FACT0; // Активизируем 0-й фильтр

  CAN->FMR &= ~CAN_FMR_FINIT;        // Выходим из режима настройки фильтров
}

void CAN_SetFIFO0RecvFunc(void (*intfunc)(uint32_t id, uint8_t len, uint8_t *data)) // эта процедура формирует адрес функции, которая будет вызываться в случае прихода сообщения в FIFO_0
{
  fifo0recvmsg = intfunc; // Формируем адрес функции, которая будет запускаться при возникновении прерывания
}

void CAN_ConfigFiltr(uint8_t useExtID, CanFilterIds idList[13], uint8_t idListSize) // конфигурация списка фильтруемых сообщений
{
  uint32_t frIndex;  
  
  // для настройки фильтра приёмных сообщений необходимо войти в режим настройки фильтров
  CAN->FMR |= CAN_FMR_FINIT;       // Входим в режим настройки фильтров

  CAN->FA1R = CAN->FM1R = CAN->FS1R = CAN->FFA1R = 0; // Сброс всех фильтров
  for(frIndex = 0; frIndex < idListSize; frIndex++) // цикл по списку переданных фильтров
  { 
    CAN->FM1R |= (CAN_FM1R_FBM0 << frIndex);    // Это значит, что 2 32-х разрядных регистра банка фильтра frIndex в режиме фильтрации по идентификатору
    if(useExtID) 
      CAN->FS1R |= CAN_FS1R_FSC0 << frIndex;    // А это значит, что 32-х битный регистр идентификатора фильтра frIndex используется как один 32-х битный
    else 
      CAN->FS1R &= ~(CAN_FS1R_FSC0 << frIndex); // А это значит, что 32-х битный регистр идентификатора фильтра frIndex разделяется на 2 16-ти битных
        
    if(idList[frIndex].assignedFifo == CanFifo0) 
      CAN->FFA1R &= ~(CAN_FFA1R_FFA0 << frIndex); // Ну а это означает что фильтр frIndex будет ассоциирован с FIFO 0
    else 
      CAN->FFA1R |= (CAN_FFA1R_FFA0 << frIndex);  // Ну а это означает что фильтр frIndex будет ассоциирован с FIFO 1
    
    if(useExtID)
    {
      CAN->sFilterRegister[frIndex].FR1 = (idList[frIndex].filters.id32[0] << 3);                          // Задаем в первый регистр фильтра frIndex идентификатор
      CAN->sFilterRegister[frIndex].FR1 = (idList[frIndex].filters.id32[1] << 3);                          // Задаем в первый регистр фильтра frIndex идентификатор
    }
    else
    {
      CAN->sFilterRegister[frIndex].FR1 = (idList[frIndex].filters.id16[0] << 5);                        // Задаем в первый регистр фильтра frIndex идентификатор
      CAN->sFilterRegister[frIndex].FR1 |= ((uint32_t)idList[frIndex].filters.id16[1] << 5 << 16);       // Задаем в первый регистр фильтра frIndex идентификатор
      CAN->sFilterRegister[frIndex].FR2 = (idList[frIndex].filters.id16[2] << 5);                      // Задаем во второуий регистр фильтра frIndex идентификатор
      CAN->sFilterRegister[frIndex].FR2 |= ((uint32_t)idList[frIndex].filters.id16[3] << 5 << 16);     // Задаем во второуий регистр фильтра frIndex идентификатор
    }
    
    CAN->FA1R |= (CAN_FA1R_FACT0 << frIndex);                // Фильтр frIndex теперь активирован и ипользуется
  }
  
  CAN->FMR &= ~CAN_FMR_FINIT;        // Выходим из режима настройки фильтров
}

CanResult CAN_Write(uint32_t id, uint8_t extID, uint8_t len, uint8_t *data, uint32_t timeoutMs) // Запись данных в CAN
{
  uint32_t numFreeTransmitMailbox; // номер fifo в которое последний раз писалось сообщение  
  timeoutMs = GetTickCount() + timeoutMs; // До этого времени будет выполняться ожидание освобождения ящика для отправки данных

  // поиск свободного ящика для отправки
  while(1)
  {
    numFreeTransmitMailbox = ((CAN->TSR & CAN_TSR_CODE) >> 24); //номер свободного ящика для передачи сообщения

    if(CAN->TSR & (CAN_TSR_TME0 << numFreeTransmitMailbox)) break; // сободен, передаем
    if(GetTickCount() > timeoutMs) return CanTimeout;
  }    
  //есть свободное местечко, для записи, поэтому можно продолжать

  // запоминаем номер ящика в который выполняется запись, чтобы в функции can1_check_last_write, знать какой ящик проверять на предмет ошибок или не ошибок отправки
  numWriteTransmitMailbox = numFreeTransmitMailbox; // номер fifo в которое последний раз писалось сообщение  
    
  CAN->sTxMailBox[numFreeTransmitMailbox].TIR = 0;  // всё сбрасываем, чтобы заполнить заново
    
  if(extID) // При изпользовании разширенного идентификатора 29 бит
  {
    CAN->sTxMailBox[numFreeTransmitMailbox].TIR |= (id << 3);    // 29 битный идентификатор хранится начиная с 3-го бит регистра
    CAN->sTxMailBox[numFreeTransmitMailbox].TIR |= CAN_TIR_IDE;  // выставляется бит разширенного идентификатора
  }    
  else
  { // при изпользовании стандартного идентификатора 11 бит
    CAN->sTxMailBox[numFreeTransmitMailbox].TIR |= (id << 21);    // 11 битный идентификатор хранится начиная с 21 бит регистра
    CAN->sTxMailBox[numFreeTransmitMailbox].TIR &= ~CAN_TIR_IDE;  // сброс бита разширенного идентификатора
  }

  CAN->sTxMailBox[numFreeTransmitMailbox].TIR &= ~CAN_TIR_RTR;        // будут передаваться данные, а не управляющая передача
  CAN->sTxMailBox[numFreeTransmitMailbox].TDTR = (len & 0xf);    // записываем размер передаваемых данных

  CAN->sTxMailBox[numFreeTransmitMailbox].TDLR = data[0] | (uint32_t)data[1]<<8 | (uint32_t)data[2]<<16 | (uint32_t)data[3]<<24;
  CAN->sTxMailBox[numFreeTransmitMailbox].TDHR = data[4] | (uint32_t)data[5]<<8 | (uint32_t)data[6]<<16 | (uint32_t)data[7]<<24;
  
  // После установки бита CAN_TIR_TXRQ, CAN контролер поймёт, что для данного ящика готовы данные для отправки
  // когда данные будут отправлены это бит сбросится аппаратно для данного ящика
  CAN->sTxMailBox[numFreeTransmitMailbox].TIR |= CAN_TIR_TXRQ;   // устанавливаем запрос на запись

  return CanSuccess;  
}

CanResult CAN_Write8b(uint32_t id, uint8_t extID, uint64_t data, uint32_t timeoutMs) // Запись 8ми байт данных в CAN
{
  return CAN_Write(id, extID, 8, (uint8_t*)&data, timeoutMs); // Запись данных в CAN
}

CanResult CAN_Write4b(uint32_t id, uint8_t extID, uint32_t data, uint32_t timeoutMs) // Запись 4х байт данных в CAN
{
  return CAN_Write(id, extID, 4, (uint8_t*)&data, timeoutMs); // Запись данных в CAN
}

CanResult CAN_Write2b(uint32_t id, uint8_t extID, uint16_t data, uint32_t timeoutMs) // Запись 2х байт данных в CAN
{
  return CAN_Write(id, extID, 2, (uint8_t*)&data, timeoutMs); // Запись данных в CAN
}

CanResult CAN_Write1b(uint32_t id, uint8_t extID, uint8_t data, uint32_t timeoutMs) // Запись 1го байт данных в CAN
{
  return CAN_Write(id, extID, 1, (uint8_t*)&data, timeoutMs); // Запись данных в CAN
}

CanResult CAN_CheckLastWrite(uint32_t timeoutMs)
{
  uint32_t absTime = GetTickCount() + timeoutMs;  

  switch(numWriteTransmitMailbox) // для какого ящика выполняется проверка
  {
    case 0: // ящик номер 0
    {
      while(!(CAN->TSR & CAN_TSR_RQCP0)) // произошла запись или ошибка
        if(GetTickCount() > absTime) return CanOperationInProgress;
      
      if(CAN->TSR & CAN_TSR_TXOK0) return CanSuccess; // успешная запис
      if(CAN->TSR & CAN_TSR_ALST0) return CanArbitrationLost; // потерян арбитраж
      if(CAN->TSR & CAN_TSR_TERR0) return CanTransmissionError; // ошибка в передаче
      break;
    }
    case 1: // ящик номер 1
    {
      while(!(CAN->TSR & CAN_TSR_RQCP1)) // произошла запись или ошибка
        if(GetTickCount() > absTime) return CanOperationInProgress;
    
      if(CAN->TSR & CAN_TSR_TXOK1) return CanSuccess; // успешная запис
      if(CAN->TSR & CAN_TSR_ALST1) return CanArbitrationLost; // потерян арбитраж
      if(CAN->TSR & CAN_TSR_TERR1) return CanTransmissionError; // ошибка в передаче
      break;
    }
    case 2: // ящик номер 2
    {
      while(!(CAN->TSR & CAN_TSR_RQCP2)) // произошла запись или ошибка
        if(GetTickCount() > absTime) return CanOperationInProgress;
    
      if(CAN->TSR & CAN_TSR_TXOK2) return CanSuccess; // успешная запис
      if(CAN->TSR & CAN_TSR_ALST2) return CanArbitrationLost; // потерян арбитраж
      if(CAN->TSR & CAN_TSR_TERR2) return CanTransmissionError; // ошибка в передаче
      break;
    }
  }
  
  return CanTransmissionError;
}

static void CAN1ReadData(CanFifo fifo, uint8_t data[8], uint8_t dataSize)
{  
  data[0] = CAN->sFIFOMailBox[fifo].RDLR    ;
  data[1] = CAN->sFIFOMailBox[fifo].RDLR>>8 ;
  data[2] = CAN->sFIFOMailBox[fifo].RDLR>>16;
  data[3] = CAN->sFIFOMailBox[fifo].RDLR>>24;
  
  data[4] = CAN->sFIFOMailBox[fifo].RDHR    ;
  data[5] = CAN->sFIFOMailBox[fifo].RDHR>>8 ;
  data[6] = CAN->sFIFOMailBox[fifo].RDHR>>16;
  data[7] = CAN->sFIFOMailBox[fifo].RDHR>>24;
}

CanResult CAN_Read(CanFifo fifo, uint32_t* id, uint8_t data[8], uint8_t* dataSize, uint32_t timeoutMs)
{
  uint32_t endtime = GetTickCount() + timeoutMs; // время до которого будет ожидание наличия данных в буфере fifo

  // Перед чтением данных необходимо проверить есть ли сообщения в заданном FIFO
  // в циклах ниже проверяется значение FPMx (регистра RFxR), которое хранит кол-во сообщений, которые уже находятся в приёмном FIFO_0 или FIFO_1
  // сообщение в FIFO_0 или в FIFO_1 попадает по правилам фильтрации, 
  // т.е. фильтр входящих сообщений можно настроить так, что какие-то сообщения будут попадать только в FIFO_0,
  // а какие-то только в FIFO_1 (вобщем см. настройку фильтров входящих сообщений)
  switch(fifo)
  {
    case 0: {
              while(!(CAN->RF0R & CAN_RF0R_FMP0)) if(GetTickCount() > endtime) return CanTimeout;
              break;
            }
    case 1: {
              while(!(CAN->RF1R & CAN_RF1R_FMP1)) if(GetTickCount() > endtime) return CanTimeout;
              break;
            }
    default: return CanFIFINumReadError;
  }
  // сюда попадём в том случае, если установлен факт наличия сообщения в заданном FIFO
  // если сообщение есть, то можно определить его идентификатор и длину

  if(CAN->sFIFOMailBox[fifo].RIR & CAN_RIR_IDE)
    *id = CAN->sFIFOMailBox[fifo].RIR >> 3;    // имеем расширенный идентификатор у принятого пакета
  else
    *id = CAN->sFIFOMailBox[fifo].RIR >> 21;  // имеем стандартный идентификатор у принятого пакета
  *dataSize = CAN->sFIFOMailBox[fifo].RDTR & CAN_RDTR_DLC;  // получаем длину пришедшего сообщения

  CAN1ReadData(fifo, data, *dataSize); // кода определена длина, то можно считывать данные
  switch(fifo)
  {
    case 0: { 
              // такая запись напрямую не испортит другие биты в регистре, т.к. они либо только для чтения или в них нужно записывать обязательно 1 (см. документацию на регистр RF0R)
              CAN->RF0R = CAN_RF0R_RFOM0; // установка бита, чтобы сообщить модулю CAN о том, что было прочитанно сообщение из FIFO_0
              break;
            }
    case 1: { // такая запись напрямую не испортит другие биты в регистре, т.к. они либо только для чтения или в них нужно записывать обязательно 1 (см. документацию на регистр RF1R)
              CAN->RF1R = CAN_RF1R_RFOM1; // установка бита, чтобы сообщить модулю CAN о том, что было прочитанно сообщение из FIFO_1
              break;
            }
  }
  return CanSuccess;
}

CanState_t CAN_GetLastError(void)
{
  CanState_t ret;
  uint32_t CAN_ESR = CAN->ESR;
  
  ret.receiveErrorCounter = (CAN_ESR>>24)&0xff;
  ret.transmitErrorCounter = (CAN_ESR>>16)&0xff;
  ret.lecandcnterr.u = CAN_ESR&0x7f;   // получение значения LEC, BOFF, EPVF, EWGF
  
  return ret;
}

