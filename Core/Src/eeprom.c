#include "eeprom.h"
#include <string.h>
#include <limits.h>

// ===================================================
// =============== ГЛОБАЛЬНЫЕ ПЕРЕМЕННЫЕ ==============
// ===================================================

const uint32_t PageAddresses[N_PAGES] = {
    PAGE0_ADDR,
    PAGE1_ADDR,
    PAGE2_ADDR,
    PAGE3_ADDR
};

const uint32_t ValidKeys[VAR_NUM] = {
    VAR_TEMPERATURE, VAR_HUMIDITY, VAR_PRESSURE, VAR_COUNTER, VAR_VOLTAGE,
    VAR_CURRENT, VAR_POWER, VAR_ENERGY, VAR_FREQUENCY, VAR_PHASE,
    VAR_STATUS, VAR_ERROR_CODE, VAR_TIMESTAMP, VAR_MODE, VAR_SETPOINT,
    VAR_PID_KP, VAR_PID_KI, VAR_PID_KD, VAR_ALARM_LEVEL, VAR_CALIB_FACTOR
};

static int CurrentActivePage = -1;

// ===================================================
// =============== ВСПОМОГАТЕЛЬНЫЕ ФУНКЦИИ ============
// ===================================================

PageState EEPROM_ReadPageState(int pageIdx) {
    if (pageIdx < 0 || pageIdx >= N_PAGES) return PAGE_STATE_ERASED;
    return (PageState)READ_WORD(PageAddresses[pageIdx]);
}

uint32_t EEPROM_GetEraseCount(int pageIdx) {
    if (pageIdx < 0 || pageIdx >= N_PAGES) return 0;
    uint32_t ec = READ_WORD(PageAddresses[pageIdx] + 4);
    return (ec == 0xFFFFFFFF) ? 0 : ec;
}

static void EEPROM_SetPageState(int pageIdx, PageState state) {
    if (pageIdx < 0 || pageIdx >= N_PAGES) return;
    HAL_FLASH_Unlock();
    WRITE_WORD(PageAddresses[pageIdx], (uint32_t)state);
    HAL_FLASH_Lock();
}

void EEPROM_ErasePage(int pageIdx) {
    if (pageIdx < 0 || pageIdx >= N_PAGES) return;

    uint32_t oldEC = EEPROM_GetEraseCount(pageIdx);
    if (oldEC == 0xFFFFFFFF) oldEC = 0;

    FLASH_EraseInitTypeDef eraseInit;
    uint32_t pageError = 0;

    eraseInit.TypeErase = FLASH_TYPEERASE_PAGES;
    eraseInit.PageAddress = PageAddresses[pageIdx];
    eraseInit.NbPages = 1;

    HAL_FLASH_Unlock();
    HAL_FLASHEx_Erase(&eraseInit, &pageError);

    uint32_t newEC = oldEC + 1;
    HAL_FLASH_Program(FLASH_TYPEPROGRAM_WORD, PageAddresses[pageIdx] + 4, newEC);

    HAL_FLASH_Program(FLASH_TYPEPROGRAM_WORD, PageAddresses[pageIdx], PAGE_STATE_ERASED);

    HAL_FLASH_Lock();
}


uint32_t EEPROM_FindFreeSlot(int pageIdx) {
    if (pageIdx < 0 || pageIdx >= N_PAGES) return 0xFFFFFFFF;

    uint32_t startAddr = PageAddresses[pageIdx] + DATA_OFFSET;
    uint32_t endAddr = PageAddresses[pageIdx] + PAGE_SIZE;

    for (uint32_t addr = startAddr; addr < endAddr; addr += VAR_RECORD_SIZE) {
        if (READ_WORD(addr) == 0xFFFFFFFF) {
            return addr;
        }
    }
    return 0xFFFFFFFF;
}

EepromResult EEPROM_FindLastValue(int pageIdx, uint32_t varID, uint32_t *value) {
    if (pageIdx < 0 || pageIdx >= N_PAGES) return EEPROM_ERROR;

    uint32_t startAddr = PageAddresses[pageIdx] + DATA_OFFSET;
    uint32_t endAddr = PageAddresses[pageIdx] + PAGE_SIZE - VAR_RECORD_SIZE;

    for (uint32_t addr = endAddr; addr >= startAddr; addr -= VAR_RECORD_SIZE) {
        uint32_t readKey = READ_WORD(addr);
        if (readKey == varID) {
            *value = READ_WORD(addr + 4);
            return EEPROM_OK;
        }
        if (addr <= startAddr) break;
    }

    return EEPROM_ERROR_NOT_FOUND;
}

int EEPROM_SelectPageForTransfer(int currentActive) {
    uint32_t minEraseCount = UINT_MAX;
    int selectedPage = -1;

    for (int i = 0; i < N_PAGES; i++) {
        if (i == currentActive) continue;
        PageState status = EEPROM_ReadPageState(i);
        if (status != PAGE_STATE_ERASED) continue;

        uint32_t eraseCount = EEPROM_GetEraseCount(i);
        if (eraseCount < minEraseCount) {
            minEraseCount = eraseCount;
            selectedPage = i;
        }
    }
    return selectedPage;
}

// ===================================================
// ================== ОСНОВНЫЕ ФУНКЦИИ ================
// ===================================================

EepromResult EEPROM_Init(void) {
    PageState pageStates[N_PAGES];
    uint32_t eraseCounts[N_PAGES];
    int countErased = 0, countActive = 0, countReceive = 0;
    int activeIdx = -1, receiveIdx = -1;

    for (int i = 0; i < N_PAGES; i++) {
        pageStates[i] = EEPROM_ReadPageState(i);
        eraseCounts[i] = EEPROM_GetEraseCount(i);

        if (pageStates[i] == PAGE_STATE_ERASED) countErased++;
        else if (pageStates[i] == PAGE_STATE_ACTIVE) {
            countActive++;
            activeIdx = i;
        }
        else if (pageStates[i] == PAGE_STATE_RECEIVE) {
            countReceive++;
            receiveIdx = i;
        }
    }

    if (countActive == 1 && countReceive == 0) {
        CurrentActivePage = activeIdx;
        return EEPROM_OK;
    }
    else if (countActive == 0 && countReceive == 0 && countErased == N_PAGES) {
        int minIdx = 0;
        uint32_t minEC = UINT_MAX;
        for (int i = 0; i < N_PAGES; i++) {
            if (eraseCounts[i] < minEC) {
                minEC = eraseCounts[i];
                minIdx = i;
            }
        }
        EEPROM_SetPageState(minIdx, PAGE_STATE_ACTIVE);
        CurrentActivePage = minIdx;
        return EEPROM_OK;
    }
    else if (countActive == 1 && countReceive == 1) {
        EEPROM_SetPageState(receiveIdx, PAGE_STATE_ACTIVE);
        EEPROM_ErasePage(activeIdx);
        CurrentActivePage = receiveIdx;
        return EEPROM_OK;
    }
    else {
        int newest = 0;
        uint32_t maxEC = 0;
        for (int i = 0; i < N_PAGES; i++) {
            if (eraseCounts[i] > maxEC) {
                maxEC = eraseCounts[i];
                newest = i;
            }
        }
        EEPROM_SetPageState(newest, PAGE_STATE_ACTIVE);
        CurrentActivePage = newest;
        return EEPROM_OK;
    }
}

void EEPROM_PageTransfer(int oldPageIdx, int newPageIdx, uint32_t newVarID, uint32_t newVarValue) {
    HAL_FLASH_Unlock();

    WRITE_WORD(PageAddresses[newPageIdx], PAGE_STATE_RECEIVE);

    uint32_t writeAddr = PageAddresses[newPageIdx] + DATA_OFFSET;
    WRITE_WORD(writeAddr, newVarID);
    WRITE_WORD(writeAddr + 4, newVarValue);
    writeAddr += VAR_RECORD_SIZE;

    for (int i = 0; i < VAR_NUM; i++) {
        uint32_t key = ValidKeys[i];
        if (key == newVarID) continue;

        uint32_t value;
        if (EEPROM_FindLastValue(oldPageIdx, key, &value) == EEPROM_OK) {
            WRITE_WORD(writeAddr, key);
            WRITE_WORD(writeAddr + 4, value);
            writeAddr += VAR_RECORD_SIZE;
        }
    }

    WRITE_WORD(PageAddresses[newPageIdx], PAGE_STATE_ACTIVE);
    HAL_FLASH_Lock();

    EEPROM_ErasePage(oldPageIdx);
}

EepromResult EEPROM_Write(uint32_t varID, uint32_t varValue) {
    bool validKey = false;
    for (int i = 0; i < VAR_NUM; i++) {
        if (ValidKeys[i] == varID) {
            validKey = true;
            break;
        }
    }
    if (!validKey) return EEPROM_ERROR_INVALID_KEY;

    uint32_t freeAddr = EEPROM_FindFreeSlot(CurrentActivePage);

    if (freeAddr != 0xFFFFFFFF) {
        HAL_FLASH_Unlock();
        HAL_FLASH_Program(FLASH_TYPEPROGRAM_WORD, freeAddr, varID);
        HAL_FLASH_Program(FLASH_TYPEPROGRAM_WORD, freeAddr + 4, varValue);
        HAL_FLASH_Lock();
        return EEPROM_OK;
    } else {
        int newPage = EEPROM_SelectPageForTransfer(CurrentActivePage);
        if (newPage == -1) return EEPROM_ERROR_FULL;

        EEPROM_PageTransfer(CurrentActivePage, newPage, varID, varValue);
        CurrentActivePage = newPage;
        return EEPROM_OK;
    }
}

EepromResult EEPROM_Read(uint32_t varID, uint32_t *varValue) {
    return EEPROM_FindLastValue(CurrentActivePage, varID, varValue);
}

EepromResult EEPROM_Format(void) {
    for (int i = 0; i < N_PAGES; i++) {
        EEPROM_ErasePage(i);
    }
    EEPROM_SetPageState(0, PAGE_STATE_ACTIVE);
    CurrentActivePage = 0;
    return EEPROM_OK;
}

void EEPROM_CheckStaticWearLeveling(void) {
    uint32_t maxEC = 0, minEC = UINT_MAX;
    int maxIdx = -1, minIdx = -1;

    for (int i = 0; i < N_PAGES; i++) {
        uint32_t ec = EEPROM_GetEraseCount(i);
        if (ec > maxEC) {
            maxEC = ec;
            maxIdx = i;
        }
        if (ec < minEC) {
            minEC = ec;
            minIdx = i;
        }
    }

    if ((maxEC - minEC) > WEAR_THRESHOLD && maxIdx == CurrentActivePage) {
        int newPage = EEPROM_SelectPageForTransfer(CurrentActivePage);
        if (newPage >= 0) {
            uint32_t dummyVarID = ValidKeys[0]; 
            uint32_t dummyVarValue = 0xAAAAAAAA;
            EEPROM_PageTransfer(CurrentActivePage, newPage, dummyVarID, dummyVarValue);
            CurrentActivePage = newPage;
        }
    }
}

void EEPROM_GetPagesInfo(PageInfo_t* pagesInfo, int numPages) {
    for (int i = 0; i < numPages && i < N_PAGES; i++) {
        pagesInfo[i].pageIndex = i;
        pagesInfo[i].status = EEPROM_ReadPageState(i);
        pagesInfo[i].eraseCount = EEPROM_GetEraseCount(i);

        uint32_t usedBytes = 0;
        uint32_t startAddr = PageAddresses[i] + DATA_OFFSET;
        uint32_t endAddr = PageAddresses[i] + PAGE_SIZE;

        for (uint32_t addr = startAddr; addr < endAddr; addr += VAR_RECORD_SIZE) {
            if (READ_WORD(addr) != 0xFFFFFFFF) {
                usedBytes += VAR_RECORD_SIZE;
            }
            else {
                break; 
            }
        }

        pagesInfo[i].usedBytes = usedBytes;
        pagesInfo[i].freeBytes = (PAGE_SIZE - DATA_OFFSET) - usedBytes;
    }
}

