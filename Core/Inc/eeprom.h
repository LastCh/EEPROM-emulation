#ifndef EEPROM_H
#define EEPROM_H

#include "stm32f1xx_hal.h"
#include <stdint.h>
#include <stdbool.h>

/* ============ КОНФИГУРАЦИЯ ============ */

#define N_PAGES          4    // Количество страниц для эмуляции
#define PAGE_SIZE        1024 // Размер страницы в байтах (1 KB)
#define DATA_OFFSET      8    // Смещение данных от начала страницы (статус 4 байта + счетчик 4 байта)
#define VAR_RECORD_SIZE  8    // Размер записи (ключ 4 байта + значение 4 байта)

#define WEAR_THRESHOLD   100  // Порог (в единицах стираний) для статического wear leveling

/* Адреса страниц — последние 4 страницы Flash для STM32F103x8 (64 KB Flash) */
#define PAGE0_ADDR       0x0800F000UL  /* Page 60: 0x0800F000 */
#define PAGE1_ADDR       0x0800F400UL  /* Page 61: 0x0800F400 */
#define PAGE2_ADDR       0x0800F800UL  /* Page 62: 0x0800F800 */
#define PAGE3_ADDR       0x0800FC00UL  /* Page 63: 0x0800FC00 */

/* ============ ВИРТУАЛЬНЫЕ АДРЕСА ПЕРЕМЕННЫХ ============ */

#define VAR_TEMPERATURE  0x11111111UL
#define VAR_HUMIDITY     0x22222222UL
#define VAR_PRESSURE     0x33333333UL
#define VAR_COUNTER      0x44444444UL
#define VAR_VOLTAGE      0x55555555UL
#define VAR_CURRENT      0x66666666UL
#define VAR_POWER        0x77777777UL
#define VAR_ENERGY       0x88888888UL
#define VAR_FREQUENCY    0x99999999UL
#define VAR_PHASE        0xAAAAAAAAUL
#define VAR_STATUS       0xBBBBBBBBUL
#define VAR_ERROR_CODE   0xCCCCCCCCUL
#define VAR_TIMESTAMP    0xDDDDDDDDUL
#define VAR_MODE         0xEEEEEEEEUL
#define VAR_SETPOINT     0xFFFFFFFFUL
#define VAR_PID_KP       0x10101010UL
#define VAR_PID_KI       0x20202020UL
#define VAR_PID_KD       0x30303030UL
#define VAR_ALARM_LEVEL  0x40404040UL
#define VAR_CALIB_FACTOR 0x50505050UL

#define VAR_NUM          20  // Количество переменных

/* ============ ТИПЫ ДАННЫХ ============ */

/* Статусы страницы (значения записываются в первые 4 байта страницы) */
typedef enum {
    PAGE_STATE_ERASED  = 0xFFFFFFFFUL,
    PAGE_STATE_ACTIVE  = 0x00000000UL,
    PAGE_STATE_RECEIVE = 0x55555555UL
} PageState;

/* Результаты функций */
typedef enum {
    EEPROM_OK = 0,
    EEPROM_ERROR,
    EEPROM_ERROR_INVALID_KEY,
    EEPROM_ERROR_NOT_FOUND,
    EEPROM_ERROR_FULL
} EepromResult;

/* Информация о странице — для отладки/монитора */
typedef struct {
    uint32_t pageIndex;
    PageState status;
    uint32_t eraseCount;
    uint32_t usedBytes;
    uint32_t freeBytes;
} PageInfo_t;

/* ============ ВНЕШНИЕ ПЕРЕМЕННЫЕ ============ */

extern const uint32_t PageAddresses[N_PAGES];
extern const uint32_t ValidKeys[VAR_NUM];

/* ============ ОСНОВНЫЕ ФУНКЦИИ (API) ============ */

/* Инициализация подсистемы эмуляции EEPROM */
EepromResult EEPROM_Init(void);

/* Запись значения по виртуальному адресу (varID) */
EepromResult EEPROM_Write(uint32_t varID, uint32_t varValue);

/* Чтение значения по виртуальному адресу */
EepromResult EEPROM_Read(uint32_t varID, uint32_t *varValue);

/* Полное форматирование/очистка всех страниц (использовать с осторожностью) */
EepromResult EEPROM_Format(void);

/* ============ ВСПОМОГАТЕЛЬНЫЕ ФУНКЦИИ ============ */

/* Возвращает индекс активной страницы (или -1) */
int EEPROM_GetActivePageIdx(void);

/* Чтение статуса страницы (PAGE_STATE_...) */
PageState EEPROM_ReadPageState(int pageIdx);

/* Чтение счетчика стираний страницы */
uint32_t EEPROM_GetEraseCount(int pageIdx);

/* Поиск свободного слота (возвращает адрес или 0xFFFFFFFF) */
uint32_t EEPROM_FindFreeSlot(int pageIdx);

/* Поиск последнего значения переменной в указанной странице */
EepromResult EEPROM_FindLastValue(int pageIdx, uint32_t varID, uint32_t *value);

/* Выбор страницы для переноса (по минимальному erase count) */
int EEPROM_SelectPageForTransfer(int currentActive);

/* Перенос (page transfer) c копированием актуальных значений.
   В newVarID/newVarValue можно передать переменную, которую нужно записать при переносе. */
void EEPROM_PageTransfer(int oldPageIdx, int newPageIdx, uint32_t newVarID, uint32_t newVarValue);

/* Стирание страницы (эта функция обновляет счётчик стираний) */
void EEPROM_ErasePage(int pageIdx);

/* Проверка wear-leveling и принудительный перенос при необходимости */
void EEPROM_CheckStaticWearLeveling(void);

/* Получение информации по страницам для отладки */
void EEPROM_GetPagesInfo(PageInfo_t *pagesInfo, int numPages);

/* ============ МАКРОСЫ (чтение/запись слов) ============ */

/* Примечание:
   - READ_WORD читает напрямую из памяти.
   - WRITE_WORD вызывает HAL_FLASH_Program: вызывайте его только внутри
     HAL_FLASH_Unlock() ... HAL_FLASH_Lock() блока или замените на
     безопасную обёртку, возвращающую статус. */

#define READ_WORD(addr) (*(__IO uint32_t*)(addr))
#define WRITE_WORD(addr, data) HAL_FLASH_Program(FLASH_TYPEPROGRAM_WORD, (uint32_t)(addr), (uint32_t)(data))

/* Если ваша версия HAL для STM32F1 не поддерживает FLASH_TYPEPROGRAM_WORD,
   замените на FLASH_TYPEPROGRAM_HALFWORD и записывайте по 16 бит (или
   реализуйте функцию-обёртку, которая разбивает 32-битную запись на 2 полусловa). */

#endif // EEPROM_H
