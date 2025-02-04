/******************************************************************************
 *
 *  Copyright 2018 NXP
 *
 *  Licensed under the Apache License, Version 2.0 (the "License");
 *  you may not use this file except in compliance with the License.
 *  You may obtain a copy of the License at
 *
 *  http://www.apache.org/licenses/LICENSE-2.0
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS,
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
 *
 ******************************************************************************/
#define LOG_TAG "LSClient"
#include "LsClient.h"
#include <cutils/properties.h>
#include <dirent.h>
#include <log/log.h>
#include <openssl/evp.h>
#include <pthread.h>
#include <stdlib.h>
#include <string>
#include "LsLib.h"

uint8_t datahex(char c);
unsigned char* getHASH(uint8_t* buffer, size_t buffSize);
extern bool ese_debug_enabled;

#define ls_script_source_prefix "/vendor/etc/loaderservice_updater_"
#define ls_script_source_suffix ".lss"
#define ls_script_output_prefix \
  "/data/vendor/secure_element/loaderservice_updater_out_"
#define ls_script_output_suffix ".txt"
const size_t HASH_DATA_LENGTH = 21;
const uint16_t HASH_STATUS_INDEX = 20;
const uint8_t LS_MAX_COUNT = 10;
const uint8_t LS_DOWNLOAD_SUCCESS = 0x00;
const uint8_t LS_DOWNLOAD_FAILED = 0x01;

static android::sp<ISecureElementHalCallback> cCallback;
void* performLSDownload_thread(void* data);
static void getLSScriptSourcePrefix(std::string& prefix);

void getLSScriptSourcePrefix(std::string& prefix) {
  char source_path[PROPERTY_VALUE_MAX] = {0};
  int len = property_get("vendor.ese.loader_script_path", source_path, "");
  if (len > 0) {
    FILE* fd = fopen(source_path, "rb");
    if (fd != NULL) {
      char c;
      while (!feof(fd) && fread(&c, 1, 1, fd) == 1) {
        if (c == ' ' || c == '\n' || c == '\r' || c == 0x00) break;
        prefix.push_back(c);
      }
    } else {
      ALOGD("%s Cannot open file %s\n", __func__, source_path);
    }
  }
  if (prefix.empty()) {
    prefix.assign(ls_script_source_prefix);
  }
}

/*******************************************************************************
**
** Function:        LSC_Start
**
** Description:     Starts the LSC update with encrypted data privided in the
                    updater file
**
** Returns:         SUCCESS if ok.
**
*******************************************************************************/
LSCSTATUS LSC_Start(const char* name, const char* dest, uint8_t* pdata,
                    uint16_t len, uint8_t* respSW) {
  static const char fn[] = "LSC_Start";
  LSCSTATUS status = LSCSTATUS_FAILED;
  if (name != NULL) {
    status = Perform_LSC(name, dest, pdata, len, respSW);
  } else {
    ALOGE("%s: LS script file is missing", fn);
  }
  ALOGD_IF(ese_debug_enabled, "%s: Exit; status=0x0%X", fn, status);
  return status;
}

/*******************************************************************************
**
** Function:        LSC_doDownload
**
** Description:     Start LS download process by creating thread
**
** Returns:         SUCCESS of ok
**
*******************************************************************************/
LSCSTATUS LSC_doDownload(
    const android::sp<ISecureElementHalCallback>& clientCallback) {
  static const char fn[] = "LSC_doDownload";
  LSCSTATUS status;
  pthread_t thread;
  pthread_attr_t attr;
  pthread_attr_init(&attr);
  pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
  cCallback = clientCallback;
  if (pthread_create(&thread, &attr, &performLSDownload_thread, NULL) < 0) {
    ALOGE("%s: Thread creation failed", fn);
    status = LSCSTATUS_FAILED;
  } else {
    status = LSCSTATUS_SUCCESS;
  }
  pthread_attr_destroy(&attr);
  return status;
}

/*******************************************************************************
**
** Function:        performLSDownload_thread
**
** Description:     Perform LS during hal init
**
** Returns:         None
**
*******************************************************************************/
void* performLSDownload_thread(__attribute__((unused)) void* data) {
  ALOGD_IF(ese_debug_enabled, "%s enter  ", __func__);
  /*generated SHA-1 string for secureElementLS
  This will remain constant as handled in secureElement HAL*/
  char sha1[] = "6d583e84f2710e6b0f06beebc1a12a1083591373";
  uint8_t hash[20] = {0};

  for (int i = 0; i < 40; i = i + 2) {
    hash[i / 2] =
        (((datahex(sha1[i]) & 0x0F) << 4) | (datahex(sha1[i + 1]) & 0x0F));
  }

  uint8_t resSW[4] = {0x4e, 0x02, 0x69, 0x87};

  std::string sourcePath;
  std::string sourcePrefix;
  std::string outPath;
  int index = 1;
  LSCSTATUS status = LSCSTATUS_SUCCESS;
  Lsc_HashInfo_t lsHashInfo;

  getLSScriptSourcePrefix(sourcePrefix);
  do {
    /*Open the script file from specified location and name*/
    sourcePath.assign(sourcePrefix);
    sourcePath += ('0' + index);
    sourcePath += ls_script_source_suffix;
    FILE* fIn = fopen(sourcePath.c_str(), "rb");
    if (fIn == NULL) {
      ALOGE("%s Cannot open LS script file %s\n", __func__, sourcePath.c_str());
      ALOGE("%s Error : %s", __func__, strerror(errno));
      break;
    }
    ALOGD_IF(ese_debug_enabled, "%s File opened %s\n", __func__,
             sourcePath.c_str());

    outPath.assign(ls_script_output_prefix);
    outPath += ('0' + index);
    outPath += ls_script_output_suffix;

    FILE* fOut = fopen(outPath.c_str(), "wb+");
    if (fOut == NULL) {
      ALOGE("%s Failed to open file %s\n", __func__, outPath.c_str());
      break;
    }
    /*Read the script content to a local buffer*/
    fseek(fIn, 0, SEEK_END);
    long lsBufSize = ftell(fIn);
    rewind(fIn);
    if (lsHashInfo.lsRawScriptBuf == nullptr) {
      lsHashInfo.lsRawScriptBuf = (uint8_t*)phNxpEse_memalloc(lsBufSize + 1);
    }
    memset(lsHashInfo.lsRawScriptBuf, 0x00, (lsBufSize + 1));
    fread(lsHashInfo.lsRawScriptBuf, lsBufSize, 1, fIn);

    LSCSTATUS lsHashStatus = LSCSTATUS_FAILED;

    /*Get 20bye SHA1 of the script*/
    lsHashInfo.lsScriptHash =
        getHASH(lsHashInfo.lsRawScriptBuf, (size_t)lsBufSize);
    phNxpEse_free(lsHashInfo.lsRawScriptBuf);
    lsHashInfo.lsRawScriptBuf = nullptr;
    if (lsHashInfo.lsScriptHash == nullptr) break;

    if (lsHashInfo.readBuffHash == nullptr) {
      lsHashInfo.readBuffHash = (uint8_t*)phNxpEse_memalloc(HASH_DATA_LENGTH);
    }
    memset(lsHashInfo.readBuffHash, 0x00, HASH_DATA_LENGTH);

    /*Read the hash from applet for specified slot*/
    lsHashStatus =
        LSC_ReadLsHash(lsHashInfo.readBuffHash, &lsHashInfo.readHashLen, index);

    /*Check if previously script is successfully installed.
    if yes, continue reading next script else try update wit current script*/
    if ((lsHashStatus == LSCSTATUS_SUCCESS) &&
        (lsHashInfo.readHashLen == HASH_DATA_LENGTH) &&
        (0 == memcmp(lsHashInfo.lsScriptHash, lsHashInfo.readBuffHash,
                     HASH_DATA_LENGTH - 1)) &&
        (lsHashInfo.readBuffHash[HASH_STATUS_INDEX] == LS_DOWNLOAD_SUCCESS)) {
      ALOGD_IF(ese_debug_enabled, "%s LS Loader sript is already installed \n",
               __func__);
      continue;
    }

    /*Uptdates current script*/
    status = LSC_Start(sourcePath.c_str(), outPath.c_str(), (uint8_t*)hash,
                       (uint16_t)sizeof(hash), resSW);
    ALOGD_IF(ese_debug_enabled, "%s script %s perform done, result = %d\n",
             __func__, sourcePath.c_str(), status);
    if (status != LSCSTATUS_SUCCESS) {
      lsHashInfo.lsScriptHash[HASH_STATUS_INDEX] = LS_DOWNLOAD_FAILED;
      /*If current script updation fails, update the status with hash to the
       * applet then clean and exit*/
      lsHashStatus =
          LSC_UpdateLsHash(lsHashInfo.lsScriptHash, HASH_DATA_LENGTH, index);
      if (lsHashStatus != LSCSTATUS_SUCCESS) {
        ALOGD_IF(ese_debug_enabled, "%s LSC_UpdateLsHash Failed\n", __func__);
      }
      ESESTATUS estatus = phNxpEse_deInit();
      if (estatus == ESESTATUS_SUCCESS) {
        estatus = phNxpEse_close();
        if (estatus == ESESTATUS_SUCCESS) {
          ALOGD_IF(ese_debug_enabled, "%s: Ese_close success\n", __func__);
        }
      } else {
        ALOGE("%s: Ese_deInit failed", __func__);
      }
      cCallback->onStateChange(false);
      break;
    } else {
      /*If current script execution is succes, update the status along with the
       * hash to the applet*/
      lsHashInfo.lsScriptHash[HASH_STATUS_INDEX] = LS_DOWNLOAD_SUCCESS;
      lsHashStatus =
          LSC_UpdateLsHash(lsHashInfo.lsScriptHash, HASH_DATA_LENGTH, index);
      if (lsHashStatus != LSCSTATUS_SUCCESS) {
        ALOGD_IF(ese_debug_enabled, "%s LSC_UpdateLsHash Failed\n", __func__);
      }
    }
  } while (++index <= LS_MAX_COUNT);

  phNxpEse_free(lsHashInfo.readBuffHash);

  if (status == LSCSTATUS_SUCCESS) {
    cCallback->onStateChange(true);
  }
  pthread_exit(NULL);
  ALOGD_IF(ese_debug_enabled, "%s pthread_exit\n", __func__);
  return NULL;
}

/*******************************************************************************
**
** Function:        getHASH
**
** Description:     generates SHA1 of given buffer
**
** Returns:         20 bytes of SHA1
**
*******************************************************************************/
unsigned char* getHASH(uint8_t* buffer, size_t buffSize) {
  static uint8_t outHash[HASH_DATA_LENGTH] = {0};
  unsigned int md_len = -1;
  const EVP_MD* md = EVP_get_digestbyname("SHA1");
  if (NULL != md) {
    EVP_MD_CTX mdctx;
    EVP_MD_CTX_init(&mdctx);
    EVP_DigestInit_ex(&mdctx, md, NULL);
    EVP_DigestUpdate(&mdctx, buffer, buffSize);
    EVP_DigestFinal_ex(&mdctx, outHash, &md_len);
    EVP_MD_CTX_cleanup(&mdctx);
  }
  return outHash;
}

/*******************************************************************************
**
** Function:        datahex
**
** Description:     Converts char to uint8_t
**
** Returns:         uint8_t variable
**
*******************************************************************************/
uint8_t datahex(char c) {
  uint8_t value = 0;
  if (c >= '0' && c <= '9')
    value = (c - '0');
  else if (c >= 'A' && c <= 'F')
    value = (10 + (c - 'A'));
  else if (c >= 'a' && c <= 'f')
    value = (10 + (c - 'a'));
  return value;
}
