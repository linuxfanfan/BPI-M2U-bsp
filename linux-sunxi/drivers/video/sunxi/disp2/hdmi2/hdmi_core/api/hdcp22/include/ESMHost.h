/*
 * Allwinner SoCs hdmi2.0 driver.
 *
 * Copyright (C) 2016 Allwinner.
 *
 * This file is licensed under the terms of the GNU General Public
 * License version 2.  This program is licensed "as is" without any
 * warranty of any kind, whether express or implied.
 */
#ifndef _ESMHOST_H_
#define _ESMHOST_H_

#include <linux/kernel.h>
#include <linux/types.h>
#include "esm_host_lib_internal.h"
#include "../../log.h"

ESM_STATUS ESM_Reset(esm_instance_t *esm);

ESM_STATUS ESM_Kill(esm_instance_t *esm);

ESM_STATUS ESM_EnableLowValueContent(esm_instance_t *esm);

ESM_STATUS ESM_Initialize(esm_instance_t *esm, unsigned int ESMImage, uint32_t ESMImageSize,
	uint32_t Flags, esm_host_driver_t *HostDriver, esm_config_t *Config);

/**
 * \details
 * This function begins authentication negotiation with the remote if the remote
 * is ready to begin the process. The ESM will only commence authentication if
 * it has determined the remote is capable of supporting HDCP 2.x otherwise it
 * will indicate the remote unit is not HDCP 2.2 compatible. This function does
 * not wait for authentication to complete and returns immediately.
 *
 * <b>IMPORTANT</b>
 * The application cannot issue any other ESM commands until the ESM has completed this
 * process. Use the status command or callback to wait on the completion status.
 *
 * \param [in] esm The ESM instance
 * \param [in] Cmd 1: Start authentication; 0: Exit the authenticated state
 * \param [in] StreamID   Stream ID must be 0 for this version, Tx only, ignored for Rx
 * \param [in] ContentType  The content type, Tx only, ignored for Rx. See the \ref ContentTypes
 *
 * See the #ESM_GetState function to get the status of the authentication.
 *
 * \return
 *    - #ESM_HL_FAILED The ESM failed to control the authentication
 *    - #ESM_HL_INVALID_PARAMETERS Incorrect \ref ContentTypes has been set
 *    - #ESM_HL_MB_FAILED Mailbox failure
 *    - #ESM_HL_NO_INSTANCE No instance provided
 *    - #ESM_HL_SUCCESS The ESM has transitioned to the authenticating state
 *
 */
ESM_STATUS ESM_Authenticate(esm_instance_t *esm, uint32_t Cmd, uint32_t StreamID,
	uint32_t ContentType);

/**
 * \details
 * This user defined function is called by the ESM when the status has changed
 * for the specific handle. This function can be called at any time and the user
 * must ensure the callback executes in a timely fashion.
 *
 * Note this function only returns the ESM status, in order to
 * see an ESM state the #ESM_GetState function must be called.
 *
 * \param [in]  esm The ESM instance
 * \param [out] status Current ESM status
 * \param [in] clear Pass a value of 1 to clear the status after reading it (0 leaves status unchanged)
 *
 */
ESM_STATUS ESM_GetStatusRegister(esm_instance_t *esm, esm_status_t *status, uint8_t clear);

/**
 * \details
 * This function is used only on the Transmitter and is used to initiate a HDCP
 * capability check with the Receiver. Before calling this API the ESM does not
 * know the state of the downstream device and its TMDS output will be the fixed
 * (BSOD) value. Once a downstream device indicates it is HDCP 2.2 capable, the
 * ESM will transition to allow TMDS data to pass through permitting display of
 * Low Value Content (the EESS signalling is set to not encrypted).
 *
 * There are several reasons why a capability check can fail.  The
 * #ESM_GetLastError function can be used to determine what went wrong
 * with a particular request.
 *
 * Once this command is issued an internal timer starts and the
 * #ESM_Authenticate command must be called before this timer expires otherwise
 * the ESM will transition back to output the BSOD value, requiring this
 * function to be called again. The timer's timeout value is set by the
 * <b>[CAPABLE_BYPASS_TIME]</b> in the ESM configuration file.
 *
 * \param [in] esm The ESM instance
 *
 * \return
 *    - #ESM_HL_INVALID_COMMAND Incorrect firmware type (must be TX or RPTX)
 *    - #ESM_HL_FAILED HDCP 2.2 capability check failed
 *    - #ESM_HL_MB_FAILED Mailbox failure
 *    - #ESM_HL_NO_INSTANCE No instance provided
 *    - #ESM_HL_SUCCESS Rx is HDCP 2.2 capable
 *
 */
ESM_STATUS ESM_SetCapability(esm_instance_t *esm);

/**
 * \details
 *
 * This function enables and disables the ESM logging.
 *
 * \param [in] esm The ESM instance
 * \param [in] Cmd 1: Enable logging, 0: Disable logging
 * \param [in] Size Dynamic logging minimum size to indicate from ESM; 0 means no
 * dynamic logging from ESM just static log dump
 *
 * \return
 *    - #ESM_HL_FAILED Logging control command was not set
 *    - #ESM_HL_MB_FAILED Mailbox failure
 *    - #ESM_HL_NO_INSTANCE No instance provided
 *    - #ESM_HL_SUCCESS Logging control command was set
 *
 */
ESM_STATUS ESM_LogControl(esm_instance_t *esm, uint32_t Cmd, uint32_t Size);

void ESM_FlushExceptions(esm_instance_t *esm);
ESM_STATUS ESM_LoadPairing(esm_instance_t *esm, uint8_t *PairData, uint32_t BufferSize);
ESM_STATUS ESM_SavePairing(esm_instance_t *esm, uint8_t *PairData, uint32_t *BufferSize);
ESM_STATUS ESM_EnablePairing(esm_instance_t *esm, uint32_t OnOff);


#endif
