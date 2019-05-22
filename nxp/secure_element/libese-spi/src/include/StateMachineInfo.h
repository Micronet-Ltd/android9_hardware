/******************************************************************************
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

#ifndef LIBESE_SPI_SRC_INCLUDE_STATEMACHINEINFO_H_
#define LIBESE_SPI_SRC_INCLUDE_STATEMACHINEINFO_H_

typedef enum {
  EVT_UNKNOWN,
  EVT_RF_ON,
  EVT_RF_ON_FELICA_APP,
  EVT_RF_OFF,
  EVT_RF_ACT_NTF_ESE,
  EVT_SPI_HW_SERVICE_START,
  EVT_SPI_OPEN,
  EVT_SPI_CLOSE,
  EVT_SPI_TX,
  EVT_SPI_TX_WTX_RSP,
  EVT_SPI_RX,
  EVT_SPI_RX_WTX_REQ,
  EVT_SPI_TIMER_START,
  EVT_SPI_TIMER_STOP,
  EVT_SPI_TIMER_EXPIRED
} eExtEvent_t;

typedef enum {
  ST_UNKNOWN,
  ST_SPI_CLOSED_RF_IDLE,
  ST_SPI_CLOSED_RF_BUSY,
  ST_SPI_OPEN_RF_IDLE,
  ST_SPI_BUSY_RF_IDLE,
  ST_SPI_RX_PENDING_RF_PENDING,
  ST_SPI_RX_PENDING_RF_PENDING_FELICA,
  ST_SPI_OPEN_SUSPENDED_RF_BUSY,
  ST_SPI_OPEN_RESUMED_RF_BUSY,
  ST_SPI_BUSY_RF_BUSY,
  ST_SPI_BUSY_RF_BUSY_TIMER_EXPIRED
} eStates_t;

typedef enum { SM_STATUS_SUCCESS, SM_STATUS_FAILED } eStatus_t;

#endif /* LIBESE_SPI_SRC_INCLUDE_STATEMACHINEINFO_H_ */
