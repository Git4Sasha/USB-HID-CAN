#ifndef CAN_H
#define CAN_H

#include <stdint.h>

typedef uint8_t CanFifo;
enum
{
  CanFifo0 = 0,
  CanFifo1 = 1
};

typedef struct _CanFilterIds  
{
  union
  {
    uint32_t id32[2];
    uint16_t id16[4];
  }filters;
  
  CanFifo assignedFifo;
}CanFilterIds;

//struct // структура с помощью которой настраивается скорость передачи данны
//{ // Времена указываются в квантах. Один квант времени равен div / частота шины (от которой тактируется CAN)
//  uint8_t div;  // делитель для шины на которой "висит" CAN    
//  uint8_t ts1;  // длина 1-й фазы одного бита      (0-15)
//  uint8_t ts2;  // время 2-й фазы одного бита      (0-7)
//  uint8_t sjw;  // время синхронизации одного бита (0-3)
//}CanBaudRate_t;

//#define CAN_BAUND_RATE_1M {.div=1, .ts1=2}
//#define CAN_BAUND_RATE_500kBit 1 

typedef uint32_t CanBaudRate;
enum
{
  CanBaudRate1M = 0,
  CanBaudRate500K = 1,
  CanBaudRate250K = 2,
  CanBaudRate125K = 3,
  CanBaudRate62500 = 4
};

typedef int32_t CanResult;
enum
{
  CanSuccess = 0,
    
  CanNoDataAvailable = -2,
  CanTimeout = -3,
  CanOperationInProgress = -4,  // операция находится в стадии выполнения
  CanArbitrationLost = -5,      // потерян арбитраж
  CanTransmissionError = -6,    // ошибка передачи данных
  CanFIFINumReadError = -7,     // не верный номер FIFO для чтения
};

typedef uint8_t CanError;
enum
{
   CanNoError = 0,
   CanStuffError = 1,
   CanFormError = 2,
   CanAcknowledgmentError = 3,
   CanBitRecessiveError = 4,
   CanBitDominantError = 5,
   CanCRCError = 6,   
};

typedef uint8_t CanErrorFlag;
enum
{
  CanErrorWarningFlag = 0x1,  // количество ошибок превысило или равно 96
  CanErrorPassiveFlag = 0x2,  // перешли в состояние пассивной ошибки(receiveErrorCounter или transmitErrorCounter > 127)
  CanBusOffFlag = 0x4,        // перешли в состояние отключенной шины (пиздосс...)
};

typedef struct
{
  CanError error;
  CanErrorFlag errorFlag;   
  uint8_t receiveErrorCounter;
  uint8_t transmitErrorCounter;
  union
  {
    uint8_t u;   // Ошибки возникающие при работе CAN шины
    struct  // при таком описании структуры верхний бит будет указывать на младший бит
    {
      uint8_t ewgf:1;   // Кол-во ошибок приёма или передачи больше или равно 96  (бит предупреждения)
      uint8_t epvf:1;   // Кол-во ошибок приёма или передачи превысило 127
      uint8_t bof:1;    // CAN контроллер вошёл в состояния bus_off (отключился от шины)
      uint8_t reserv:2;    // 
      uint8_t lec:3;    // код последней ошибки   0 - нет ошибки, 
                                              //  1 - Staff Error, 
                                              //  2 - Form error (Ошибка формы сигнала), 
                                              //  3 - Acknowlegdment error (нет подтверждения),
                                              //  4 - Bit recessive error (ошибка рецессивного бита),
                                              //  5 - Bit dominant error (ошибка доминантного бита)
                                              //  6 - CRC Error (ошибка контрольной суммы)
                                              //  7 - Set by software (устанавливается программно)
    }bits;
  }lecandcnterr;
}CanState_t;


void CAN_IRQHandler(void); // Обработка прерывания при поступлении сообщения в FIFO_0

/**
* @brief Инициализация работы CAN1
 * @param baudRate - Скорость передачи
 * @param remappin - Нужно ли изпользовать переназначение ног
 * @param mode - Режим в котором будет работать CAN
 */

void CAN_Init(CanBaudRate baudRate, uint32_t remappin, uint32_t mode, uint32_t useRecvInt); // инициализация CAN на заданную скорость обмена

/**
 * @brief  Эта процедура формирует адрес функции, которая будет вызываться в случае прихода сообщения в FIFO_0
 * @param intfunc - Адрес на функцию, которая обрабатывает прерывание прихода сообщения в FIFO_0
                    функция должна иметь обязательные параметры:
                    id - идентификатор входящего сообщения
                    len - количество пришедших данных
                    data - адрес на массив с данными
 */
void CAN_SetFIFO0RecvFunc(void (*intfunc)(uint32_t id, uint8_t len, uint8_t *data));

/**
* @brief Инициализация филтра приёма сообщений CAN1
 * @param useExtID - Признак изпользования расширенного идентификатора
 * @param remappin - Нужно ли изпользовать переназначение ног
 * @param idListSize - Размер списка фильтров
*/
void CAN_ConfigFiltr(uint8_t useExtID, CanFilterIds idList[13], uint8_t idListSize); // конфигурация списка фильтруемых сообщений


/**
 * @brief Чтение принятого сообщения
 * @param Очередь из которой читаем сообщение 
 * @param id возвращаемый параметр - идентификатор принятого сообщения
 * @param data принятые данные
 * @param dataSize возвращаемый параметр - количество принятых байт
 * @param timeoutMs таймаут в миллисекундах чтения сообщения 
 */
CanResult CAN_Read(CanFifo fifo, uint32_t* id, uint8_t data[8], uint8_t* dataSize, uint32_t timeoutMs);

/**
 * @brief Процедуры для запроса на передачу данных по шине CAN
 * @param id идентификатор сообщения
 * @param extID признак того, что изпользуется расширенный идентификатор
 * @param len кол-во передаваемых байт данных от 0 для 8
 * @param data указатель на передаваемые данные
 * @param timeoutMs предельное время ожидания свободной ячейки для сохранения данных (задаётся в миллисекундах)
 * @warning Последовательность байт сохраняется соответствующей архитектуре(LITTLE ENDIAN)
 */
CanResult CAN_Write(uint32_t id, uint8_t extID, uint8_t len, uint8_t *data, uint32_t timeoutMs); // Запись данных в CAN
CanResult CAN_Write8b(uint32_t id, uint8_t extID, uint64_t data, uint32_t timeoutMs); // Запись 8ми байт данных в CAN
CanResult CAN_Write4b(uint32_t id, uint8_t extID, uint32_t data, uint32_t timeoutMs); // Запись 4х байт данных в CAN
CanResult CAN_Write2b(uint32_t id, uint8_t extID, uint16_t data, uint32_t timeoutMs); // Запись 2х байт данных в CAN
CanResult CAN_Write1b(uint32_t id, uint8_t extID, uint8_t data, uint32_t timeoutMs); // Запись 1го байт данных в CAN


/**
 * @brief Проверить состояние записи, которая выполнялась последний раз.
 *        
 *        После последнего вызова функции CAN1Write или CAN1WriteХХ, сообщение не сразу отправляется в шину CAN, а происходит
 *        только помещение данных в буфер и выставляется запрос не передачу данных. Чтобы проверить факт передачи сообщения 
 *        в шину служит эта функция.
 * @param timeoutMs таймаут в миллисекундах проверки последней записи
 */
CanResult CAN_CheckLastWrite(uint32_t timeoutMs);

/**
 * @brief Получить состояние линии CAN
 *
 *        функция читает значение регистра CAN_ESR и упаковывает его значение в структуру CanState_t
 */
CanState_t CAN_GetLastError(void);

#endif
