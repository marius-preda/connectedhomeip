/*
 *
 *    Copyright (c) 2020 Project CHIP Authors
 *    Copyright (c) 2020 Google LLC.
 *    Copyright (c) 2018 Nest Labs, Inc.
 *
 *    Licensed under the Apache License, Version 2.0 (the "License");
 *    you may not use this file except in compliance with the License.
 *    You may obtain a copy of the License at
 *
 *        http://www.apache.org/licenses/LICENSE-2.0
 *
 *    Unless required by applicable law or agreed to in writing, software
 *    distributed under the License is distributed on an "AS IS" BASIS,
 *    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *    See the License for the specific language governing permissions and
 *    limitations under the License.
 */

/**
 *    @file
 *          Utilities for accessing persisted device configuration on
 *          platforms based on the NXP K32W SDK.
 */
/* this file behaves like a config.h, comes first */
#include <platform/internal/CHIPDeviceLayerInternal.h>

#include <platform/nxp/k32w/k32w0/K32W0Config.h>

#include <lib/core/CHIPEncoding.h>
#include <platform/internal/testing/ConfigUnitTest.h>

#include "FreeRTOS.h"

namespace chip {
namespace DeviceLayer {
namespace Internal {

#define BUFFER_LOG_SIZE 256
#define CHIP_CONFIG_RAM_BUFFER_SIZE 3072

#ifndef NVM_ID_CHIP_CONFIG_DATA
#define NVM_ID_CHIP_CONFIG_DATA 0x5000
#endif

typedef struct 
{
    uint16_t chipConfigRamBufferLen;
    uint8_t chipConfigRamBuffer[1];
} ChipConfigRamStruct_t;

static ChipConfigRamStruct_t *chipConfigRamStruct;
static ramBufferDescriptor ramDescr = { 0 };

static rsError AddToRamStorage(ramBufferDescriptor * pBuffer, uint16_t aKey, const uint8_t * aValue, uint16_t aValueLength)
{
    rsError err;
    uint32_t allocSize = ramDescr.ramBufferMaxLen;

    if ( allocSize <= *(pBuffer->ramBufferLen) + aValueLength )
    {
        while (allocSize < *(pBuffer->ramBufferLen) + aValueLength)
        {
            /* Need to realocate the memory buffer, increase size by 512B until nvm data fits */
            allocSize += 512;
        }

        allocSize += sizeof (uint16_t);
        chipConfigRamStruct = (ChipConfigRamStruct_t *) realloc((void *)chipConfigRamStruct, allocSize);
        VerifyOrExit((NULL !=  chipConfigRamStruct), err = RS_ERROR_NO_BUFS);

        ramDescr.ramBufferLen = &chipConfigRamStruct->chipConfigRamBufferLen;
        ramDescr.ramBufferMaxLen = allocSize - sizeof(uint16_t);
        ramDescr.pRamBuffer = &chipConfigRamStruct->chipConfigRamBuffer[0];
    }

   err = ramStorageSet(pBuffer, aKey, aValue, aValueLength);

exit:
    return err;
}
CHIP_ERROR K32WConfig::Init()
{
    CHIP_ERROR err;
    int pdmStatus;;
    uint16_t bytesRead, recordSize;
    bool bLoadDataFromNvm = false;
    uint32_t allocSize = CHIP_CONFIG_RAM_BUFFER_SIZE;

    /* Initialise the Persistent Data Manager */
    pdmStatus = PDM_Init();
    SuccessOrExit(err = MapPdmInitStatus(pdmStatus));

    /* Check if dataset is present and get its size */
    if (PDM_bDoesDataExist((uint16_t) NVM_ID_CHIP_CONFIG_DATA, &recordSize))
    {
        bLoadDataFromNvm = true;
        while (recordSize > allocSize)
        {
            //increase size by 1k until nvm data fits
            allocSize += 1024;
        }
    }

    chipConfigRamStruct = (ChipConfigRamStruct_t *) malloc(allocSize);
    VerifyOrExit((NULL !=  chipConfigRamStruct), err = CHIP_ERROR_NO_MEMORY);

    chipConfigRamStruct->chipConfigRamBufferLen = 0;
    ramDescr.ramBufferLen = &chipConfigRamStruct->chipConfigRamBufferLen;
    ramDescr.ramBufferMaxLen = allocSize - sizeof(uint16_t);
    ramDescr.pRamBuffer = &chipConfigRamStruct->chipConfigRamBuffer[0];

    if(bLoadDataFromNvm)
    {
        /* Try to load the dataset in RAM */
        PDM_eReadDataFromRecord((uint16_t) NVM_ID_CHIP_CONFIG_DATA, chipConfigRamStruct, recordSize, &bytesRead);
        
    }

exit:
    return err;
}

CHIP_ERROR K32WConfig::ReadConfigValue(Key key, bool & val)
{
    CHIP_ERROR err;
    bool tempVal;
    rsError status;
    uint16_t sizeToRead = sizeof(tempVal);

    VerifyOrExit(ValidConfigKey(key), err = CHIP_DEVICE_ERROR_CONFIG_NOT_FOUND); // Verify key id.
    status = ramStorageGet(&ramDescr, key, 0, (uint8_t*) &tempVal, &sizeToRead);
    SuccessOrExit(err = MapRamStorageStatus(status));
    val = tempVal;

exit:
    return err;
}

CHIP_ERROR K32WConfig::ReadConfigValue(Key key, uint32_t & val)
{
    CHIP_ERROR err;
    uint32_t tempVal;
    rsError status;
    uint16_t sizeToRead = sizeof(tempVal);
    
    VerifyOrExit(ValidConfigKey(key), err = CHIP_DEVICE_ERROR_CONFIG_NOT_FOUND); // Verify key id.
    status = ramStorageGet(&ramDescr, key, 0, (uint8_t*) &tempVal, &sizeToRead);
    SuccessOrExit(err = MapRamStorageStatus(status));
    val = tempVal;

exit:
    return err;
}

CHIP_ERROR K32WConfig::ReadConfigValue(Key key, uint64_t & val)
{
    CHIP_ERROR err;
    uint32_t tempVal;
    rsError status;
    uint16_t sizeToRead = sizeof(tempVal);
    
    VerifyOrExit(ValidConfigKey(key), err = CHIP_DEVICE_ERROR_CONFIG_NOT_FOUND); // Verify key id.
    status = ramStorageGet(&ramDescr, key, 0, (uint8_t*) &tempVal, &sizeToRead);
    SuccessOrExit(err = MapRamStorageStatus(status));
    val = tempVal;

exit:
    return err;
}

CHIP_ERROR K32WConfig::ReadConfigValueStr(Key key, char * buf, size_t bufSize, size_t & outLen)
{
    CHIP_ERROR err;
    rsError status;
    uint16_t sizeToRead = bufSize;

    VerifyOrExit(ValidConfigKey(key), err = CHIP_DEVICE_ERROR_CONFIG_NOT_FOUND); // Verify key id.
    
    //We can call ramStorageGet with null pointer to only retrieve the size
    status = ramStorageGet(&ramDescr, key, 0, (uint8_t*) buf, &sizeToRead);
    SuccessOrExit(err = MapRamStorageStatus(status));
    outLen = sizeToRead;

exit:
    return err;
}

CHIP_ERROR K32WConfig::ReadConfigValueBin(Key key, uint8_t * buf, size_t bufSize, size_t & outLen)
{
    return ReadConfigValueStr(key, (char*) buf, bufSize, outLen);
}

CHIP_ERROR K32WConfig::ReadConfigValueCounter(uint8_t counterIdx, uint32_t & val)
{
    Key key = kMinConfigKey_ChipCounter + counterIdx;
    return ReadConfigValue(key, val);
}

CHIP_ERROR K32WConfig::WriteConfigValue(Key key, bool val)
{
    CHIP_ERROR err;
    rsError status;
    PDM_teStatus pdmStatus;

    VerifyOrExit(ValidConfigKey(key), err = CHIP_DEVICE_ERROR_CONFIG_NOT_FOUND); // Verify key id.
    status = AddToRamStorage(&ramDescr, key, (uint8_t *) &val, sizeof(bool));
    SuccessOrExit(err = MapRamStorageStatus(status));

    pdmStatus = PDM_eSaveRecordDataInIdleTask((uint16_t) NVM_ID_CHIP_CONFIG_DATA, chipConfigRamStruct, 
                                              chipConfigRamStruct->chipConfigRamBufferLen + sizeof(uint16_t));
    SuccessOrExit(err = MapPdmStatus(pdmStatus));
exit:
    return err;
}

CHIP_ERROR K32WConfig::WriteConfigValue(Key key, uint32_t val)
{
    CHIP_ERROR err;
    rsError status;
    PDM_teStatus pdmStatus;

    VerifyOrExit(ValidConfigKey(key), err = CHIP_DEVICE_ERROR_CONFIG_NOT_FOUND); // Verify key id.
    status = AddToRamStorage(&ramDescr, key, (uint8_t *)&val, sizeof(uint32_t));
    SuccessOrExit(err = MapRamStorageStatus(status));

    pdmStatus = PDM_eSaveRecordDataInIdleTask((uint16_t) NVM_ID_CHIP_CONFIG_DATA, chipConfigRamStruct, 
                                               chipConfigRamStruct->chipConfigRamBufferLen + sizeof(uint16_t));
    SuccessOrExit(err = MapPdmStatus(pdmStatus));

exit:
    return err;
}

CHIP_ERROR K32WConfig::WriteConfigValue(Key key, uint64_t val)
{
    CHIP_ERROR err;
    rsError status;
    PDM_teStatus pdmStatus;

    VerifyOrExit(ValidConfigKey(key), err = CHIP_DEVICE_ERROR_CONFIG_NOT_FOUND); // Verify key id.
    status = AddToRamStorage(&ramDescr, key, (uint8_t *)&val, sizeof(uint64_t));
    SuccessOrExit(err = MapRamStorageStatus(status));

    pdmStatus = PDM_eSaveRecordDataInIdleTask((uint16_t) NVM_ID_CHIP_CONFIG_DATA, chipConfigRamStruct, 
                                               chipConfigRamStruct->chipConfigRamBufferLen + sizeof(uint16_t));
    SuccessOrExit(err = MapPdmStatus(pdmStatus));

exit:
    return err;
}

CHIP_ERROR K32WConfig::WriteConfigValueStr(Key key, const char * str)
{
    return WriteConfigValueStr(key, str, (str != NULL) ? strlen(str) : 0);
}

CHIP_ERROR K32WConfig::WriteConfigValueStr(Key key, const char * str, size_t strLen)
{
    CHIP_ERROR err;
    PDM_teStatus pdmStatus;
    rsError status;
    
    VerifyOrExit(ValidConfigKey(key), err = CHIP_DEVICE_ERROR_CONFIG_NOT_FOUND); // Verify key id.

    if (str != NULL)
    {
        status = AddToRamStorage(&ramDescr, key, (uint8_t *) str, strLen);
        SuccessOrExit(err = MapRamStorageStatus(status));

        pdmStatus = PDM_eSaveRecordDataInIdleTask((uint16_t) NVM_ID_CHIP_CONFIG_DATA, chipConfigRamStruct, 
                                                   chipConfigRamStruct->chipConfigRamBufferLen + sizeof(uint16_t));
        SuccessOrExit(err = MapPdmStatus(pdmStatus));
    }
    else
    {
        err = ClearConfigValue(key);
        SuccessOrExit(err);
    }
exit:
    return err;
}

CHIP_ERROR K32WConfig::WriteConfigValueBin(Key key, const uint8_t * data, size_t dataLen)
{
    return WriteConfigValueStr(key, (char *) data, dataLen);
}

CHIP_ERROR K32WConfig::WriteConfigValueCounter(uint8_t counterIdx, uint32_t val)
{
    Key key = kMinConfigKey_ChipCounter + counterIdx;
    return WriteConfigValue(key, val);
}

CHIP_ERROR K32WConfig::ClearConfigValue(Key key)
{
    CHIP_ERROR err = CHIP_NO_ERROR;
    rsError status;
    PDM_teStatus pdmStatus;
    
    VerifyOrExit(ValidConfigKey(key), err = CHIP_DEVICE_ERROR_CONFIG_NOT_FOUND); // Verify key id.
    status = ramStorageDelete(&ramDescr, key, 0);
    SuccessOrExit(err = MapRamStorageStatus(status));

    pdmStatus = PDM_eSaveRecordDataInIdleTask((uint16_t) NVM_ID_CHIP_CONFIG_DATA, chipConfigRamStruct, 
                                               chipConfigRamStruct->chipConfigRamBufferLen + sizeof(uint16_t));
    SuccessOrExit(err = MapPdmStatus(pdmStatus));

exit:
    return err;
}

bool K32WConfig::ConfigValueExists(Key key)
{
    rsError status;
    uint16_t sizeToRead;
    bool found = false;

    if (ValidConfigKey(key))
    {
        status = ramStorageGet(&ramDescr, key, 0, NULL, &sizeToRead);
        found = (status == RS_ERROR_NONE && sizeToRead != 0);
    }
    return found;
}

CHIP_ERROR K32WConfig::FactoryResetConfig(void)
{
    CHIP_ERROR err;
    PDM_teStatus pdmStatus;

    err = FactoryResetConfigInternal(kMinConfigKey_ChipConfig, kMaxConfigKey_ChipConfig);

    if (err == CHIP_NO_ERROR)
    {
        err = FactoryResetConfigInternal(kMinConfigKey_KVS, kMaxConfigKey_KVS);
    }

    pdmStatus = PDM_eSaveRecordData((uint16_t) NVM_ID_CHIP_CONFIG_DATA, chipConfigRamStruct,
                                    chipConfigRamStruct->chipConfigRamBufferLen + sizeof(uint16_t));
    SuccessOrExit(err = MapPdmStatus(pdmStatus));

exit:
    return err;
}

CHIP_ERROR K32WConfig::FactoryResetConfigInternal(Key firstKey, Key lastKey)
{
    CHIP_ERROR err = CHIP_NO_ERROR;

    for (Key key = firstKey; key <= lastKey; key++)
    {
        ramStorageDelete(&ramDescr, key, 0);
    }

    return err;
}

CHIP_ERROR K32WConfig::MapPdmStatus(PDM_teStatus pdmStatus)
{
    CHIP_ERROR err;

    switch (pdmStatus)
    {
    case PDM_E_STATUS_OK:
        err = CHIP_NO_ERROR;
        break;
    default:
        err = CHIP_ERROR(ChipError::Range::kPlatform, pdmStatus);
        break;
    }

    return err;
}

CHIP_ERROR K32WConfig::MapRamStorageStatus(rsError rsStatus)
{
    CHIP_ERROR err;

    switch (rsStatus)
    {
    case RS_ERROR_NONE:
        err = CHIP_NO_ERROR;
        break;
    case RS_ERROR_NOT_FOUND:
        err = CHIP_DEVICE_ERROR_CONFIG_NOT_FOUND;
        break;
    default:
        err = CHIP_ERROR_BUFFER_TOO_SMALL;
        break;
    }

    return err;  
}




CHIP_ERROR K32WConfig::MapPdmInitStatus(int pdmStatus)
{
    return (pdmStatus == 0) ? CHIP_NO_ERROR : CHIP_ERROR(ChipError::Range::kPlatform, pdmStatus);
}

bool K32WConfig::ValidConfigKey(Key key)
{
    // Returns true if the key is in the valid CHIP Config PDM key range.
    if ((key >= kMinConfigKey_ChipFactory) && (key <= kMaxConfigKey_KVS))
    {
        return true;
    }

    return false;
}

void K32WConfig::RunConfigUnitTest() {}

} // namespace Internal
} // namespace DeviceLayer
} // namespace chip
