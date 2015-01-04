
#include "encoder.h"

static int StartCapture(ENCODER *pAppData);
static int StopCapture(ENCODER *pAppData);
static OMX_ERRORTYPE InitialOutputResources(COMPONENT_PRIVATE *pComp);
static void InputTask(void *pArg);
static void WriteTask(void *pArg);
static void UartReadTask(void *pArg);
static OMX_ERRORTYPE SetDeiParams(ENCODER *pAppData);
static OMX_ERRORTYPE SetEncodeParams(ENCODER *pAppData);
static OMX_ERRORTYPE SetDisplayParams(ENCODER *pAppData);
static void InitEncoder(ENCODER ** pAppData);
static void DestroyEncoder(ENCODER * pAppData);

static int StartCapture(ENCODER *pAppData)
{
    int nFlags = V4L2_BUF_TYPE_VIDEO_CAPTURE, nRet;

    nRet = ioctl(pAppData->m_stCaptComp.m_handle, VIDIOC_STREAMON, &nFlags);
    if (nRet < 0) {
        return -1;
    }

    return nRet;
}

static int StopCapture(ENCODER *pAppData)
{
    int nFlags = V4L2_BUF_TYPE_VIDEO_CAPTURE, nRet;

    nRet = ioctl(pAppData->m_stCaptComp.m_handle, VIDIOC_STREAMOFF, &nFlags);
    if (nRet < 0) {
        return -1;
    }

    return nRet;
}

static OMX_ERRORTYPE InitialOutputResources(COMPONENT_PRIVATE *pComp)
{
    unsigned int nCnt = 0;

    for (nCnt = 0; nCnt < pComp->m_pOutPortParams->m_nBufferCountActual; nCnt++) {
        OMX_FillThisBuffer(pComp->m_handle, pComp->m_pOutPortParams->m_pOutPortBuff[nCnt]);
    }
}

static void InputTask(void *pArg)
{
    COMPONENT_PRIVATE *pDeiILComp = NULL;
    COMPONENT_V4L2 *pCaptComp = NULL;
    OMX_BUFFERHEADERTYPE *pBufferIn = NULL;
    OMX_ERRORTYPE eErr = OMX_ErrorNone;
    int nCnt;
    unsigned int nGpioValue = 0;
    UART_REQUEST_MESSAGE stReq;
    int nReadSize = 0, nDataSize = 0;
    char aCmd[10], *pData;
    
    pDeiILComp = ((ENCODER *)pArg)->m_pDeiILComp;
    pCaptComp = &((ENCODER *)pArg)->m_stCaptComp;

    for (nCnt = 0; nCnt < pDeiILComp->m_pInPortParams->m_nBufferCountActual; nCnt++) {
        pCaptComp->m_stBuffer.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        pCaptComp->m_stBuffer.memory = V4L2_MEMORY_USERPTR;
        pCaptComp->m_stBuffer.index = nCnt;
        pCaptComp->m_stBuffer.m.userptr = (unsigned long)pDeiILComp->m_pInPortParams->m_pInPortBuff[nCnt]->pBuffer;
        pCaptComp->m_stBuffer.length = pCaptComp->m_stFmt.fmt.pix.sizeimage;
        if (ioctl(pCaptComp->m_handle, VIDIOC_QBUF, &pCaptComp->m_stBuffer) < 0) {
            printf("VIDIOC_QBUF error\n");
            return;
        }
    }

    //while (1) {
    //    GpioGetValue(ENCODER_VIDEO_INDICATOR, &nGpioValue);
    //    printf("GPIO VALUE : %d\n", nGpioValue);
    //    if (nGpioValue) {
    //        printf("GPIO HIGH START ---------------------------------------------\n");
    //        break;
    //    }
    //}

    if (StartCapture((ENCODER *)pArg) < 0) {

        printf("Start capture error\n");
        return;
    }
    memset(&stReq, 0x00, sizeof(UART_REQUEST_MESSAGE));
#if 1 /* Car Information Test */
    stReq.m_nStx = UART_PROTOCOL_START_FLAG;
    stReq.m_nLength = sizeof(UART_REQUEST_MESSAGE);
    stReq.m_nType = UART_PROTOCOL_REQ_TYPE;
    stReq.m_nMsgId = UART_PROTOCOL_ID_START;
    stReq.m_nData = 0x01;

    stReq.m_nNumberOfCarId = 5;

    stReq.m_aCarId[0] = 0x01;
    stReq.m_aCarId[1] = 0x11;
    stReq.m_nCheckSum += stReq.m_aCarId[0];
    stReq.m_nCheckSum += stReq.m_aCarId[1];
    stReq.m_aCarId[2] = 0x02;
    stReq.m_aCarId[3] = 0x22;
    stReq.m_nCheckSum += stReq.m_aCarId[2];
    stReq.m_nCheckSum += stReq.m_aCarId[3];
    stReq.m_aCarId[4] = 0x03;
    stReq.m_aCarId[5] = 0x33;
    stReq.m_nCheckSum += stReq.m_aCarId[4];
    stReq.m_nCheckSum += stReq.m_aCarId[5];
    stReq.m_aCarId[6] = 0x04;
    stReq.m_aCarId[7] = 0x44;
    stReq.m_nCheckSum += stReq.m_aCarId[6];
    stReq.m_nCheckSum += stReq.m_aCarId[7];
    stReq.m_aCarId[8] = 0x05;
    stReq.m_aCarId[9] = 0x55;
    stReq.m_nCheckSum += stReq.m_aCarId[8];
    stReq.m_nCheckSum += stReq.m_aCarId[9];
    stReq.m_nCheckSum += (
                    stReq.m_nStx+
                    sizeof(UART_REQUEST_MESSAGE)+
                    stReq.m_nType+
                    stReq.m_nMsgId+
                    stReq.m_nNumberOfCarId+
                    stReq.m_nData
                    ) & 0xff;
    stReq.m_nEtx = 0xe2;

    WriteUart(g_stManager.m_nUart, &stReq, stReq.m_nLength);
    memset(g_stManager.m_aUartBuffer, 0x00, 255);
    while (1) {
        ReadUart(g_stManager.m_nUart, g_stManager.m_aUartBuffer);
        if (g_stManager.m_aUartBuffer[3] == UART_PROTOCOL_ID_START)
            break;
    }
    printf("Start Capture\n");
#endif

    while (1) {
        ioctl(pCaptComp->m_handle, VIDIOC_DQBUF, &pCaptComp->m_stBuffer);
        for (nCnt = 0; nCnt < pDeiILComp->m_pInPortParams->m_nBufferCountActual; nCnt++) {
            if (pCaptComp->m_stBuffer.m.userptr == (unsigned long)pDeiILComp->m_pInPortParams->m_pInPortBuff[nCnt]->pBuffer) {
                pBufferIn = pDeiILComp->m_pInPortParams->m_pInPortBuff[nCnt];
                break;
            }
        }

        pBufferIn->nFilledLen = pCaptComp->m_stFmt.fmt.pix.sizeimage;
        pBufferIn->nOffset = 0;
        pBufferIn->nAllocLen = pCaptComp->m_stFmt.fmt.pix.sizeimage;
        pBufferIn->nInputPortIndex = 0;
        eErr = OMX_EmptyThisBuffer(pDeiILComp->m_handle, pBufferIn);
        if (OMX_ErrorNone != eErr) {
            write(pDeiILComp->m_pInPortParams->m_aInPortBufPipe[1], &pBufferIn, sizeof(pBufferIn));
            pthread_exit(pDeiILComp);
        }

        if (g_stManager.m_bEnd) {
            printf("InputTask exiting\n");
            StopCapture((ENCODER *)pArg);
            pBufferIn->nFlags |= OMX_BUFFERFLAG_EOS;
            OMX_EmptyThisBuffer(pDeiILComp->m_handle, pBufferIn);
            pthread_exit(pDeiILComp);
            break;
        }

        if (g_stManager.m_nUartOffset == 0)
            memset(g_stManager.m_aUartBuffer, 0x00, 255);

        while ((55+7) - g_stManager.m_nUartOffset) {
            
            nReadSize = read(g_stManager.m_nUart, &g_stManager.m_aUartBuffer[g_stManager.m_nUartOffset], (55+7)-g_stManager.m_nUartOffset);
            if (nReadSize > 0) {

                g_stManager.m_nUartOffset += nReadSize;
                printf("ReadSize : %d\n", nReadSize);
            } else
                break;
        }

        if (g_stManager.m_nUartOffset == (55+7))
            g_stManager.m_nUartOffset = 0;
#if 0
        for (nCnt=0; nCnt<(60+6); nCnt++) {
            printf("%02x ", g_stManager.m_aUartBuffer[nCnt]);
        }
        printf("\n");
#endif

        if (g_stManager.m_aUartBuffer[0] == UART_PROTOCOL_START_FLAG &&
            g_stManager.m_aUartBuffer[(55+7)-1] == UART_PROTOCOL_END_FLAG &&
            g_stManager.m_aUartBuffer[3] == UART_PROTOCOL_ID_CAR) {
        
            pData = &g_stManager.m_aUartBuffer[4];
            nDataSize = g_stManager.m_aUartBuffer[1]-6;
        
            memset(aCmd, 0x00, 10);
            strcpy(aCmd, "CAN");
            send(g_stManager.m_nCarInfoSocket, aCmd, 10, 0);
            memset(aCmd, 0x00, 10);
            sprintf(aCmd, "%d", nDataSize);
            send(g_stManager.m_nCarInfoSocket, aCmd, 10, 0);
            send(g_stManager.m_nCarInfoSocket, pData, nDataSize, 0);
            printf("Car Info Send : %d\t %d\n", nDataSize, g_stManager.m_nFrameCount);
#if 1
            for (nCnt=0; nCnt<g_stManager.m_aUartBuffer[1]; nCnt++) {
                printf("%02x ", g_stManager.m_aUartBuffer[nCnt]);
            }
            printf("\n");
#endif
        }

        read(pDeiILComp->m_pInPortParams->m_aInPortBufPipe[0], &pBufferIn, sizeof(pBufferIn));
        pCaptComp->m_stBuffer.m.userptr = (unsigned long)pBufferIn->pBuffer;
        pCaptComp->m_stBuffer.length = pCaptComp->m_stFmt.fmt.pix.sizeimage;
        ioctl(pCaptComp->m_handle, VIDIOC_QBUF, &pCaptComp->m_stBuffer);
    }
}

static void WriteTask(void *pArg)
{
    OMX_ERRORTYPE eErr = OMX_ErrorNone;
    COMPONENT_PRIVATE *pEncILComp = NULL;
    OMX_BUFFERHEADERTYPE *pBufferOut = NULL;

    pEncILComp = ((ENCODER *)pArg)->m_pEncILComp;
    eErr = InitialOutputResources(pEncILComp);

    while (1)
    {
        read(pEncILComp->m_pOutPortParams->m_aOutPortBufPipe[0], &pBufferOut, sizeof(pBufferOut));

        if (!g_stManager.m_bPause) {
            WriteEthernet(g_stManager.m_nVideoSocket, pBufferOut->pBuffer, pBufferOut->nFilledLen);
        }

        if (g_stManager.m_bEnd) {
            printf("WriteTask Exit\n");
            semp_post(pEncILComp->m_pEos);
            pthread_exit(pEncILComp);
        }

        eErr = OMX_FillThisBuffer(pEncILComp->m_handle, pBufferOut);
        if (OMX_ErrorNone != eErr) {
            write(pEncILComp->m_pOutPortParams->m_aOutPortBufPipe[1], &pBufferOut, sizeof(pBufferOut));
            pthread_exit(pEncILComp);
        }
    }
}

static OMX_ERRORTYPE SetDeiParams(ENCODER *pAppData)
{
    OMX_ERRORTYPE eErr = OMX_ErrorNone;
    OMX_PARAM_BUFFER_MEMORYTYPE stMemTypeCfg;
    OMX_PARAM_PORTDEFINITIONTYPE stParamPort;
    OMX_PARAM_VFPC_NUMCHANNELPERHANDLE stNumChPerHandle;
    OMX_CONFIG_ALG_ENABLE stAlgEnable;
    OMX_CONFIG_VIDCHANNEL_RESOLUTION stVideoChannelResolution;
    OMX_CONFIG_SUBSAMPLING_FACTOR stSubSamplingInfo = {0};
  
    OMX_INIT_PARAM(&stSubSamplingInfo);
    stSubSamplingInfo.nSubSamplingFactor = 1;
    eErr = OMX_SetConfig(pAppData->m_pDeiHandle, (OMX_INDEXTYPE)(OMX_TI_IndexConfigSubSamplingFactor), &stSubSamplingInfo);

    OMX_INIT_PARAM(&stMemTypeCfg);
    stMemTypeCfg.nPortIndex = OMX_VFPC_INPUT_PORT_START_INDEX;
    stMemTypeCfg.eBufMemoryType = OMX_BUFFER_MEMORY_DEFAULT;
    eErr = OMX_SetParameter(pAppData->m_pDeiHandle, OMX_TI_IndexParamBuffMemType, &stMemTypeCfg);
    if (eErr != OMX_ErrorNone)
    {
        printf("SYNERGYS DEBUG : failed to set memory Type at input port\n");
    }

    OMX_INIT_PARAM(&stMemTypeCfg);
    stMemTypeCfg.nPortIndex = OMX_VFPC_OUTPUT_PORT_START_INDEX;
    stMemTypeCfg.eBufMemoryType = OMX_BUFFER_MEMORY_DEFAULT;
    eErr = OMX_SetParameter(pAppData->m_pDeiHandle, OMX_TI_IndexParamBuffMemType, &stMemTypeCfg);
    if (eErr != OMX_ErrorNone)
    {
        printf("SYNERGYS DEBUG : failed to set memory Type at output port\n");
    }

    OMX_INIT_PARAM(&stMemTypeCfg);
    stMemTypeCfg.nPortIndex = OMX_VFPC_OUTPUT_PORT_START_INDEX + 1;
    stMemTypeCfg.eBufMemoryType = OMX_BUFFER_MEMORY_DEFAULT;
    eErr = OMX_SetParameter(pAppData->m_pDeiHandle, OMX_TI_IndexParamBuffMemType, &stMemTypeCfg);
    if (eErr != OMX_ErrorNone)
    {
        printf("SYNERGYS DEBUG : failed to set memory Type at output port\n");
    }

    OMX_INIT_PARAM(&stParamPort);
    stParamPort.nPortIndex = OMX_VFPC_INPUT_PORT_START_INDEX;
    OMX_GetParameter(pAppData->m_pDeiHandle, OMX_IndexParamPortDefinition, &stParamPort);
    stParamPort.nPortIndex = OMX_VFPC_INPUT_PORT_START_INDEX;
    stParamPort.format.video.nFrameWidth = pAppData->m_nWidth;
    stParamPort.format.video.nFrameHeight = pAppData->m_nHeight;
    stParamPort.format.video.nStride = pAppData->m_nWidth * 2;
    stParamPort.format.video.eCompressionFormat = OMX_VIDEO_CodingUnused;
    stParamPort.format.video.eColorFormat = OMX_COLOR_FormatYCbYCr;
    stParamPort.nBufferSize = stParamPort.format.video.nStride * pAppData->m_nHeight;
    stParamPort.nBufferAlignment = 0;
    stParamPort.bBuffersContiguous = 0;
    stParamPort.nBufferCountActual = DEI_INPUT_BUFFER_COUNT;
    printf("set input port params (width = %d, height = %d)\n", (int)pAppData->m_nWidth, (int)pAppData->m_nHeight);
    OMX_SetParameter(pAppData->m_pDeiHandle, OMX_IndexParamPortDefinition, &stParamPort);

    OMX_INIT_PARAM(&stParamPort);
    stParamPort.nPortIndex = OMX_VFPC_OUTPUT_PORT_START_INDEX;
    OMX_GetParameter(pAppData->m_pDeiHandle, OMX_IndexParamPortDefinition, &stParamPort);
    stParamPort.nPortIndex = OMX_VFPC_OUTPUT_PORT_START_INDEX;
    stParamPort.format.video.nFrameWidth = pAppData->m_nWidth;
    stParamPort.format.video.nFrameHeight = pAppData->m_nHeight;
    stParamPort.format.video.eCompressionFormat = OMX_VIDEO_CodingUnused;
    stParamPort.format.video.eColorFormat = OMX_COLOR_FormatYCbYCr;
    stParamPort.nBufferAlignment = 0;
    stParamPort.nBufferCountActual = DEI_OUTPUT_BUFFER_COUNT;
    stParamPort.format.video.nStride = pAppData->m_nWidth * 2;
    stParamPort.nBufferSize = stParamPort.format.video.nStride * stParamPort.format.video.nFrameHeight;
    printf("set output port params (width = %d, height = %d)\n", (int)pAppData->m_nWidth, (int)pAppData->m_nHeight);
    OMX_SetParameter(pAppData->m_pDeiHandle, OMX_IndexParamPortDefinition, &stParamPort);

    OMX_INIT_PARAM(&stParamPort);
    stParamPort.nPortIndex = OMX_VFPC_OUTPUT_PORT_START_INDEX + 1;
    OMX_GetParameter(pAppData->m_pDeiHandle, OMX_IndexParamPortDefinition, &stParamPort);
    stParamPort.nPortIndex = OMX_VFPC_OUTPUT_PORT_START_INDEX + 1;
    stParamPort.format.video.nFrameWidth = pAppData->m_nWidth;
    stParamPort.format.video.nFrameHeight = pAppData->m_nHeight;
    stParamPort.format.video.eCompressionFormat = OMX_VIDEO_CodingUnused;
    stParamPort.format.video.eColorFormat = OMX_COLOR_FormatYUV420SemiPlanar;
    stParamPort.nBufferAlignment = 0;
    stParamPort.nBufferCountActual = ENC_INPUT_BUFFER_COUNT;
    stParamPort.format.video.nStride = pAppData->m_nWidth;
    stParamPort.nBufferSize = (stParamPort.format.video.nStride * stParamPort.format.video.nFrameHeight * 3) >> 1;
    printf("set output port params (width = %d, height = %d)\n", (int)pAppData->m_nWidth, (int)pAppData->m_nHeight);
    OMX_SetParameter(pAppData->m_pDeiHandle, OMX_IndexParamPortDefinition, &stParamPort);

    OMX_INIT_PARAM(&stNumChPerHandle);
    stNumChPerHandle.nNumChannelsPerHandle = 1;
    eErr = OMX_SetParameter(pAppData->m_pDeiHandle, (OMX_INDEXTYPE)OMX_TI_IndexParamVFPCNumChPerHandle, &stNumChPerHandle);
    if (eErr != OMX_ErrorNone) {
        printf("failed to set num of channels\n");
    }

    OMX_INIT_PARAM(&stVideoChannelResolution);
    stVideoChannelResolution.Frm0Width = pAppData->m_nWidth;
    stVideoChannelResolution.Frm0Height = pAppData->m_nHeight;
    stVideoChannelResolution.Frm0Pitch = pAppData->m_nWidth*2;
    stVideoChannelResolution.Frm1Width = 0;
    stVideoChannelResolution.Frm1Height = 0;
    stVideoChannelResolution.Frm1Pitch = 0;
    stVideoChannelResolution.FrmStartX = 0;
    stVideoChannelResolution.FrmStartY = 0;
    stVideoChannelResolution.FrmCropWidth = pAppData->m_nWidth;
    stVideoChannelResolution.FrmCropHeight = pAppData->m_nHeight;  
    stVideoChannelResolution.eDir = OMX_DirInput;
    stVideoChannelResolution.nChId = 0;
    eErr = OMX_SetConfig(pAppData->m_pDeiHandle, (OMX_INDEXTYPE)OMX_TI_IndexConfigVidChResolution, &stVideoChannelResolution);
    if (eErr != OMX_ErrorNone) {
        printf("failed to set input channel resolution\n");
    }

    OMX_INIT_PARAM(&stVideoChannelResolution);
    stVideoChannelResolution.Frm0Width = pAppData->m_nWidth;
    stVideoChannelResolution.Frm0Height = pAppData->m_nHeight;
    stVideoChannelResolution.Frm0Pitch = pAppData->m_nWidth*2;
    stVideoChannelResolution.Frm1Width  = pAppData->m_nWidth;
    stVideoChannelResolution.Frm1Height = pAppData->m_nHeight;
    stVideoChannelResolution.Frm1Pitch  = pAppData->m_nWidth;
    stVideoChannelResolution.FrmStartX  = 0;
    stVideoChannelResolution.FrmStartY  = 0;
    stVideoChannelResolution.FrmCropWidth = 0;
    stVideoChannelResolution.FrmCropHeight = 0;
    stVideoChannelResolution.eDir = OMX_DirOutput;
    stVideoChannelResolution.nChId = 0;
    eErr = OMX_SetConfig(pAppData->m_pDeiHandle, (OMX_INDEXTYPE)OMX_TI_IndexConfigVidChResolution, &stVideoChannelResolution);
    if (eErr != OMX_ErrorNone) {
        printf("failed to set output channel resolution\n");
    }

    OMX_INIT_PARAM(&stAlgEnable);
    stAlgEnable.nPortIndex = 0;
    stAlgEnable.nChId = 0;
    stAlgEnable.bAlgBypass = 1;
    eErr = OMX_SetConfig(pAppData->m_pDeiHandle, (OMX_INDEXTYPE)OMX_TI_IndexConfigAlgEnable, &stAlgEnable);
    if (eErr != OMX_ErrorNone) {
        printf("failed to disable algo by pass mode\n");
    }

    return (eErr);
}

static OMX_ERRORTYPE SetEncodeParams(ENCODER *pAppData)
{
    OMX_ERRORTYPE eErr = OMX_ErrorUndefined;
    OMX_HANDLETYPE pHandle = NULL;
    OMX_VIDEO_PARAM_PROFILELEVELTYPE stProfileLevel;
    OMX_VIDEO_PARAM_ENCODER_PRESETTYPE stEncoderPreset;
    OMX_VIDEO_PARAM_BITRATETYPE stVidEncBitRate;
    OMX_VIDEO_PARAM_PORTFORMATTYPE stVideoParams;
    OMX_PARAM_PORTDEFINITIONTYPE stPortDef;
    OMX_VIDEO_CONFIG_DYNAMICPARAMS stDynParams;

    pHandle = pAppData->m_pEncHandle;

    OMX_INIT_PARAM (&stPortDef);
    stPortDef.nPortIndex = OMX_VIDENC_INPUT_PORT;
    eErr = OMX_GetParameter(pHandle, OMX_IndexParamPortDefinition, &stPortDef);
    stPortDef.nBufferCountActual = ENC_INPUT_BUFFER_COUNT;
    stPortDef.format.video.nFrameWidth = pAppData->m_nWidth;
    stPortDef.format.video.nStride = pAppData->m_nWidth;
    stPortDef.format.video.nFrameHeight = pAppData->m_nHeight;
    stPortDef.format.video.eColorFormat = OMX_COLOR_FormatYUV420SemiPlanar;
    stPortDef.nBufferSize = (pAppData->m_nWidth * pAppData->m_nHeight * 3) >> 1;
    eErr = OMX_SetParameter(pHandle, OMX_IndexParamPortDefinition, &stPortDef);
    if (eErr != OMX_ErrorNone) {
        printf("failed to set Encode OMX_IndexParamPortDefinition for input\n");
    }

    OMX_INIT_PARAM(&stPortDef);
    stPortDef.nPortIndex = OMX_VIDENC_OUTPUT_PORT;
    eErr = OMX_GetParameter(pHandle, OMX_IndexParamPortDefinition, &stPortDef);
    stPortDef.nBufferCountActual = ENC_OUTPUT_BUFFER_COUNT;
    stPortDef.format.video.nFrameWidth = pAppData->m_nWidth;
    stPortDef.format.video.nFrameHeight = pAppData->m_nHeight;
    stPortDef.format.video.eCompressionFormat = OMX_VIDEO_CodingAVC;
    stPortDef.format.video.xFramerate = (pAppData->m_nFrameRate << 16);
    stVideoParams.xFramerate = (pAppData->m_nFrameRate << 16);
    stPortDef.format.video.nBitrate = pAppData->m_nBitRate;
    eErr = OMX_SetParameter(pHandle, OMX_IndexParamPortDefinition, &stPortDef);
    if (eErr != OMX_ErrorNone) {
        printf("failed to set Encode OMX_IndexParamPortDefinition for output\n");
    }

    OMX_INIT_PARAM(&stVidEncBitRate);
    stVidEncBitRate.nPortIndex = OMX_VIDENC_OUTPUT_PORT;
    eErr = OMX_GetParameter(pHandle, OMX_IndexParamVideoBitrate, &stVidEncBitRate);
    stVidEncBitRate.eControlRate = OMX_Video_ControlRateVariable;
    stVidEncBitRate.nTargetBitrate = pAppData->m_nBitRate;
    eErr = OMX_SetParameter(pHandle, OMX_IndexParamVideoBitrate, &stVidEncBitRate);
    if (eErr != OMX_ErrorNone) {
        printf("failed to set Encode bitrate \n");
    }

    OMX_INIT_PARAM(&stProfileLevel);
    stProfileLevel.nPortIndex = OMX_VIDENC_OUTPUT_PORT;
    eErr = OMX_GetParameter(pHandle, OMX_IndexParamVideoProfileLevelCurrent, &stProfileLevel);
    stProfileLevel.eProfile = OMX_VIDEO_AVCProfileBaseline;
    stProfileLevel.eLevel = OMX_VIDEO_AVCLevel42;
    eErr = OMX_SetParameter(pHandle, OMX_IndexParamVideoProfileLevelCurrent, &stProfileLevel);
    if (eErr != OMX_ErrorNone) {
        printf("failed to set encoder pfofile \n");
    }

    OMX_INIT_PARAM(&stEncoderPreset);
    stEncoderPreset.nPortIndex = OMX_VIDENC_OUTPUT_PORT;
    eErr = OMX_GetParameter(pHandle, OMX_TI_IndexParamVideoEncoderPreset, &stEncoderPreset);
    stEncoderPreset.eEncodingModePreset = OMX_Video_Enc_Default;
    stEncoderPreset.eRateControlPreset = OMX_Video_RC_None;
    eErr = OMX_SetParameter(pHandle, OMX_TI_IndexParamVideoEncoderPreset, &stEncoderPreset);
    if (eErr != OMX_ErrorNone) {
        printf("failed to Encoder Preset \n");
    }

    OMX_INIT_PARAM(&stDynParams);
    stDynParams.nPortIndex = OMX_VIDENC_OUTPUT_PORT;
    eErr = OMX_GetParameter(pHandle, OMX_TI_IndexParamVideoDynamicParams, &stDynParams);
    stDynParams.videoDynamicParams.h264EncDynamicParams.videnc2DynamicParams.intraFrameInterval = 1;
    eErr = OMX_SetParameter(pHandle, OMX_TI_IndexParamVideoDynamicParams, &stDynParams);

    return eErr;
}

static OMX_ERRORTYPE SetDisplayParams(ENCODER *pAppData)
{
    OMX_ERRORTYPE eErr = OMX_ErrorNone;
    OMX_PARAM_BUFFER_MEMORYTYPE stMemTypeCfg;
    OMX_PARAM_PORTDEFINITIONTYPE stParamPort;
    OMX_PARAM_VFDC_DRIVERINSTID stDriverId;
    OMX_PARAM_VFDC_CREATEMOSAICLAYOUT stMosaicLayout;
    OMX_CONFIG_VFDC_MOSAICLAYOUT_PORT2WINMAP stPort2Winmap;

    OMX_INIT_PARAM(&stParamPort);
    stParamPort.nPortIndex = OMX_VFDC_INPUT_PORT_START_INDEX;
    OMX_GetParameter(pAppData->m_pDisHandle, OMX_IndexParamPortDefinition, &stParamPort);
    stParamPort.nPortIndex = OMX_VFDC_INPUT_PORT_START_INDEX;
    stParamPort.format.video.nFrameWidth = pAppData->m_nWidth;
    stParamPort.format.video.nFrameHeight = pAppData->m_nHeight;
    stParamPort.format.video.nStride = pAppData->m_nWidth * 2;
    stParamPort.nBufferCountActual = ENCODE_DISPLAY_INPUT_BUFFER_COUNT;
    stParamPort.format.video.eCompressionFormat = OMX_VIDEO_CodingUnused;
    stParamPort.format.video.eColorFormat = OMX_COLOR_FormatYCbYCr;
    stParamPort.nBufferSize = stParamPort.format.video.nStride * pAppData->m_nHeight;
    printf("Buffer Size computed: %d\n", (int)stParamPort.nBufferSize);
    printf("set input port params (width = %d, height = %d)\n", (int)pAppData->m_nWidth, (int)pAppData->m_nHeight);
    OMX_SetParameter(pAppData->m_pDisHandle, OMX_IndexParamPortDefinition, &stParamPort);
    if (eErr != OMX_ErrorNone) {
        printf("failed to set input port params\n");
    }

    OMX_INIT_PARAM(&stDriverId);
    stDriverId.nDrvInstID = OMX_VIDEO_DISPLAY_ID_HD0;
    stDriverId.eDispVencMode = OMX_DC_MODE_720P_60;  
    eErr = OMX_SetParameter(pAppData->m_pDisHandle, (OMX_INDEXTYPE)OMX_TI_IndexParamVFDCDriverInstId, &stDriverId);
    if (eErr != OMX_ErrorNone) {
        printf("failed to set driver mode to 720P@60\n");
    }

    OMX_INIT_PARAM(&stDriverId);
    stDriverId.nDrvInstID = OMX_VIDEO_DISPLAY_ID_HD0;
    stDriverId.eDispVencMode = OMX_DC_MODE_720P_60;
    eErr = OMX_SetParameter(pAppData->m_pCtrlHandle, (OMX_INDEXTYPE)OMX_TI_IndexParamVFDCDriverInstId, &stDriverId);
    if (eErr != OMX_ErrorNone) {
        printf("failed to set driver mode to 720P@60\n");
    }

    OMX_INIT_PARAM(&stMosaicLayout);
    stMosaicLayout.sMosaicWinFmt[0].winStartX = 0;
    stMosaicLayout.sMosaicWinFmt[0].winStartY = 0;
    stMosaicLayout.sMosaicWinFmt[0].winWidth = pAppData->m_nWidth;
    stMosaicLayout.sMosaicWinFmt[0].winHeight = pAppData->m_nHeight;
    stMosaicLayout.sMosaicWinFmt[0].pitch[VFDC_YUV_INT_ADDR_IDX] =
    pAppData->m_nWidth * 2;
    stMosaicLayout.sMosaicWinFmt[0].dataFormat = VFDC_DF_YUV422I_YVYU;
    stMosaicLayout.sMosaicWinFmt[0].bpp = VFDC_BPP_BITS16;
    stMosaicLayout.sMosaicWinFmt[0].priority = 0;
    stMosaicLayout.nDisChannelNum = 0;
    stMosaicLayout.nNumWindows = 1;
    eErr = OMX_SetParameter(pAppData->m_pDisHandle, (OMX_INDEXTYPE)OMX_TI_IndexParamVFDCCreateMosaicLayout, &stMosaicLayout);
    if (eErr != OMX_ErrorNone) {
        printf("failed to set mosaic window parameter\n");
    }

    OMX_INIT_PARAM(&stPort2Winmap);
    stPort2Winmap.nLayoutId = 0;
    stPort2Winmap.numWindows = 1;
    stPort2Winmap.omxPortList[0] = OMX_VFDC_INPUT_PORT_START_INDEX + 0;
    eErr = OMX_SetConfig(pAppData->m_pDisHandle, (OMX_INDEXTYPE)OMX_TI_IndexConfigVFDCMosaicPort2WinMap, &stPort2Winmap);
    if (eErr != OMX_ErrorNone) {
        printf("failed to map port to windows\n");
    }

    OMX_INIT_PARAM(&stMemTypeCfg);
    stMemTypeCfg.nPortIndex = OMX_VFPC_INPUT_PORT_START_INDEX;
    stMemTypeCfg.eBufMemoryType = OMX_BUFFER_MEMORY_DEFAULT;
    eErr = OMX_SetParameter(pAppData->m_pDisHandle, OMX_TI_IndexParamBuffMemType, &stMemTypeCfg);
    if (eErr != OMX_ErrorNone) {
        printf("failed to set memory Type at input port\n");
    }

    return (eErr);
}

static void InitEncoder(ENCODER ** pAppData)
{
    int nCnt = 0;
    ENCODER *pAppDataTemp = NULL;
    INPORT_PARAMS *pInPortParams = NULL;
    OUTPORT_PARAMS *pOutPortParams = NULL;

    pAppDataTemp = (ENCODER *)malloc(sizeof(ENCODER));
    memset(pAppDataTemp, 0x00, sizeof(ENCODER));

    pAppDataTemp->m_nFrameRate = 60;
    pAppDataTemp->m_nBitRate = 1000000;
    pAppDataTemp->m_nHeight = 672;
    pAppDataTemp->m_nWidth = 1280;

    pAppDataTemp->m_stCaptComp.m_handle = open((const char *)"/dev/video5", O_RDWR);
    if (pAppDataTemp->m_stCaptComp.m_handle == -1) {
        printf("Failed to open capture device\n");
        exit(2);
    }

    if (ioctl(pAppDataTemp->m_stCaptComp.m_handle, VIDIOC_QUERYCAP, &pAppDataTemp->m_stCaptComp.m_stCap)) {
        printf("Query capability failed\n");
        exit(2);
    }

    pAppDataTemp->m_stCaptComp.m_stDvPreset.preset = 0x0;
    if (ioctl(pAppDataTemp->m_stCaptComp.m_handle, VIDIOC_QUERY_DV_PRESET, &pAppDataTemp->m_stCaptComp.m_stDvPreset)) {
        printf("Querying DV Preset failed\n");
        exit(2);
    }

    if (ioctl(pAppDataTemp->m_stCaptComp.m_handle, VIDIOC_S_DV_PRESET, &pAppDataTemp->m_stCaptComp.m_stDvPreset)) {
        printf("Setting DV Preset failed\n");
        exit(2);
    }
    
    pAppDataTemp->m_stCaptComp.m_stFmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    if (ioctl(pAppDataTemp->m_stCaptComp.m_handle, VIDIOC_G_FMT, &pAppDataTemp->m_stCaptComp.m_stFmt) < 0) {
        printf("Get Format failed\n");
        exit(2);
    }

    pAppDataTemp->m_stCaptComp.m_stFmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    pAppDataTemp->m_stCaptComp.m_stFmt.fmt.pix.width = 1280;
    pAppDataTemp->m_stCaptComp.m_stFmt.fmt.pix.height = 672;
    pAppDataTemp->m_stCaptComp.m_stFmt.fmt.pix.pixelformat = V4L2_PIX_FMT_YUYV;
    pAppDataTemp->m_stCaptComp.m_stFmt.fmt.pix.bytesperline = pAppDataTemp->m_stCaptComp.m_stFmt.fmt.pix.width*2;
    pAppDataTemp->m_stCaptComp.m_stFmt.fmt.pix.sizeimage = pAppDataTemp->m_stCaptComp.m_stFmt.fmt.pix.bytesperline * pAppDataTemp->m_stCaptComp.m_stFmt.fmt.pix.height;
    pAppDataTemp->m_stCaptComp.m_stFmt.fmt.pix.field = V4L2_FIELD_ANY;
    pAppDataTemp->m_stCaptComp.m_stFmt.fmt.pix.colorspace = V4L2_COLORSPACE_REC709;
    if (ioctl(pAppDataTemp->m_stCaptComp.m_handle, VIDIOC_S_FMT, &pAppDataTemp->m_stCaptComp.m_stFmt) < 0) {
        printf("Set Format failed\n");
        exit(2);
    }

    pAppDataTemp->m_stCaptComp.m_stFmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    if (ioctl(pAppDataTemp->m_stCaptComp.m_handle, VIDIOC_G_FMT, &pAppDataTemp->m_stCaptComp.m_stFmt) < 0) {
        printf("Get Format failed\n");
        exit(2);
    }

    pAppDataTemp->m_stCaptComp.m_stReqBuffers.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    pAppDataTemp->m_stCaptComp.m_stReqBuffers.count = CAPTURE_OUTPUT_BUFFER_COUNT;
    pAppDataTemp->m_stCaptComp.m_stReqBuffers.memory = V4L2_MEMORY_USERPTR;
    if (ioctl(pAppDataTemp->m_stCaptComp.m_handle, VIDIOC_REQBUFS, &pAppDataTemp->m_stCaptComp.m_stReqBuffers) < 0) {
        printf("Could not allocate the buffers\n");
        exit(2);
    }

    pAppDataTemp->m_pDeiILComp = (COMPONENT_PRIVATE *)malloc(sizeof(COMPONENT_PRIVATE));
    memset(pAppDataTemp->m_pDeiILComp, 0x00, sizeof(COMPONENT_PRIVATE));

    pAppDataTemp->m_pDeiILComp->m_pEos = malloc(sizeof(semp_t));
    semp_init(pAppDataTemp->m_pDeiILComp->m_pEos, 0);

    pAppDataTemp->m_pDeiILComp->m_pDoneSem = malloc(sizeof(semp_t));
    semp_init(pAppDataTemp->m_pDeiILComp->m_pDoneSem, 0);

    pAppDataTemp->m_pDeiILComp->m_pPortSem = malloc(sizeof(semp_t));
    semp_init(pAppDataTemp->m_pDeiILComp->m_pPortSem, 0);

    pAppDataTemp->m_pEncILComp = (COMPONENT_PRIVATE *)malloc(sizeof(COMPONENT_PRIVATE));
    memset(pAppDataTemp->m_pEncILComp, 0x00, sizeof(COMPONENT_PRIVATE));

    pAppDataTemp->m_pEncILComp->m_pEos = malloc(sizeof(semp_t));
    semp_init(pAppDataTemp->m_pEncILComp->m_pEos, 0);

    pAppDataTemp->m_pEncILComp->m_pDoneSem = malloc(sizeof(semp_t));
    semp_init(pAppDataTemp->m_pEncILComp->m_pDoneSem, 0);

    pAppDataTemp->m_pEncILComp->m_pPortSem = malloc(sizeof(semp_t));
    semp_init(pAppDataTemp->m_pEncILComp->m_pPortSem, 0);

    pAppDataTemp->m_pDisILComp = (COMPONENT_PRIVATE *)malloc(sizeof(COMPONENT_PRIVATE));
    memset(pAppDataTemp->m_pDisILComp, 0x00, sizeof(COMPONENT_PRIVATE));

    pAppDataTemp->m_pDisILComp->m_pEos = malloc(sizeof(semp_t));
    semp_init(pAppDataTemp->m_pDisILComp->m_pEos, 0);

    pAppDataTemp->m_pDisILComp->m_pDoneSem = malloc(sizeof(semp_t));
    semp_init(pAppDataTemp->m_pDisILComp->m_pDoneSem, 0);

    pAppDataTemp->m_pDisILComp->m_pPortSem = malloc(sizeof(semp_t));
    semp_init(pAppDataTemp->m_pDisILComp->m_pPortSem, 0);

    pAppDataTemp->m_pDeiILComp->m_nInports = 1;
    pAppDataTemp->m_pDeiILComp->m_nOutports = 2;
    pAppDataTemp->m_pDeiILComp->m_nStartOutportIndex = OMX_VFPC_NUM_INPUT_PORTS;

    pAppDataTemp->m_pEncILComp->m_nInports = 1;
    pAppDataTemp->m_pEncILComp->m_nOutports = 1;
    pAppDataTemp->m_pEncILComp->m_nStartOutportIndex = 1;

    pAppDataTemp->m_pDisILComp->m_nInports = 1;
    pAppDataTemp->m_pDisILComp->m_nOutports = 0;
    pAppDataTemp->m_pDisILComp->m_nStartOutportIndex = 0;

    pAppDataTemp->m_pDeiILComp->m_pInPortParams = malloc(sizeof(INPORT_PARAMS) * pAppDataTemp->m_pDeiILComp->m_nInports);
    memset(pAppDataTemp->m_pDeiILComp->m_pInPortParams, 0x00, sizeof (INPORT_PARAMS));

    pAppDataTemp->m_pDeiILComp->m_pOutPortParams = malloc(sizeof(INPORT_PARAMS) * pAppDataTemp->m_pDeiILComp->m_nOutports);
    memset(pAppDataTemp->m_pDeiILComp->m_pOutPortParams, 0x00, pAppDataTemp->m_pDeiILComp->m_nOutports * sizeof(OUTPORT_PARAMS));

    pAppDataTemp->m_pEncILComp->m_pInPortParams =  malloc(sizeof(INPORT_PARAMS) * pAppDataTemp->m_pEncILComp->m_nInports);
    memset(pAppDataTemp->m_pEncILComp->m_pInPortParams, 0x00, sizeof(INPORT_PARAMS));

    pAppDataTemp->m_pEncILComp->m_pOutPortParams = malloc(sizeof(INPORT_PARAMS) * pAppDataTemp->m_pEncILComp->m_nOutports);
    memset(pAppDataTemp->m_pEncILComp->m_pOutPortParams, 0x00, sizeof(OUTPORT_PARAMS));

    pAppDataTemp->m_pDisILComp->m_pInPortParams = malloc(sizeof(INPORT_PARAMS) * pAppDataTemp->m_pDisILComp->m_nInports);
    memset(pAppDataTemp->m_pDisILComp->m_pInPortParams, 0x00, sizeof(INPORT_PARAMS));

    for (nCnt = 0; nCnt < pAppDataTemp->m_pDeiILComp->m_nInports; nCnt++) {
        pInPortParams = pAppDataTemp->m_pDeiILComp->m_pInPortParams + nCnt;
        pInPortParams->m_nBufferCountActual = DEI_INPUT_BUFFER_COUNT;
        pInPortParams->m_nBufferSize = pAppDataTemp->m_nHeight * pAppDataTemp->m_nWidth * 2;
        pipe((int *)pInPortParams->m_aInPortBufPipe);
    }

    for (nCnt = 0; nCnt < pAppDataTemp->m_pDeiILComp->m_nOutports; nCnt++) {
        pOutPortParams = pAppDataTemp->m_pDeiILComp->m_pOutPortParams + nCnt;
        pOutPortParams->m_nBufferCountActual = DEI_OUTPUT_BUFFER_COUNT;
        if (!(nCnt % 2)) {
            pOutPortParams->m_nBufferSize = pAppDataTemp->m_nHeight * pAppDataTemp->m_nWidth * 2;
            pOutPortParams->m_nBufferCountActual = DEI_OUTPUT_BUFFER_COUNT;
        } else {
            pOutPortParams->m_nBufferSize = (pAppDataTemp->m_nHeight * pAppDataTemp->m_nWidth * 3) >> 1;
            pOutPortParams->m_nBufferCountActual = ENC_INPUT_BUFFER_COUNT;
        }
        pipe((int *)pOutPortParams->m_aOutPortBufPipe);
    }
    pipe((int *)pAppDataTemp->m_pDeiILComp->m_aLocalPipe);

    for (nCnt = 0; nCnt < pAppDataTemp->m_pEncILComp->m_nInports; nCnt++) {
        pInPortParams = pAppDataTemp->m_pEncILComp->m_pInPortParams + nCnt;
        pInPortParams->m_nBufferCountActual = ENC_INPUT_BUFFER_COUNT;
        pInPortParams->m_nBufferSize = (pAppDataTemp->m_nHeight * pAppDataTemp->m_nWidth * 3) >> 1;
        pipe((int *)pInPortParams->m_aInPortBufPipe);
    }

    for (nCnt = 0; nCnt < pAppDataTemp->m_pEncILComp->m_nOutports; nCnt++) {
        pOutPortParams = pAppDataTemp->m_pEncILComp->m_pOutPortParams + nCnt;
        pOutPortParams->m_nBufferCountActual = ENC_OUTPUT_BUFFER_COUNT;
        pOutPortParams->m_nBufferSize = (pAppDataTemp->m_nHeight * pAppDataTemp->m_nWidth * 3) >> 1;
        pipe((int *)pOutPortParams->m_aOutPortBufPipe);
    }
    pipe((int *)pAppDataTemp->m_pEncILComp->m_aLocalPipe);

    for (nCnt = 0; nCnt < pAppDataTemp->m_pDisILComp->m_nInports; nCnt++) {
        pInPortParams = pAppDataTemp->m_pDisILComp->m_pInPortParams + nCnt;
        pInPortParams->m_nBufferCountActual = ENCODE_DISPLAY_INPUT_BUFFER_COUNT;
        pInPortParams->m_nBufferSize = pAppDataTemp->m_nHeight * pAppDataTemp->m_nWidth * 2;
        pipe((int *)pInPortParams->m_aInPortBufPipe);
    }
    pipe((int *)pAppDataTemp->m_pDisILComp->m_aLocalPipe);

    *pAppData = pAppDataTemp;
}

static void DestroyEncoder(ENCODER * pAppData)
{
    int nCnt = 0;
    INPORT_PARAMS *pInPortParams;
    OUTPORT_PARAMS *pOutPortParams;

    close((int)pAppData->m_pDisILComp->m_aLocalPipe);    
    for (nCnt = 0; nCnt < pAppData->m_pDisILComp->m_nInports; nCnt++) {
        pInPortParams = pAppData->m_pDisILComp->m_pInPortParams + nCnt;
        close((int)pInPortParams->m_aInPortBufPipe);
    }
    
    close((int)pAppData->m_pDeiILComp->m_aLocalPipe);
    for (nCnt = 0; nCnt < pAppData->m_pDeiILComp->m_nInports; nCnt++) {
        pInPortParams = pAppData->m_pDeiILComp->m_pInPortParams + nCnt;
        close((int)pInPortParams->m_aInPortBufPipe);
    }

    for (nCnt = 0; nCnt < pAppData->m_pDeiILComp->m_nOutports; nCnt++) {
        pOutPortParams = pAppData->m_pDeiILComp->m_pOutPortParams + nCnt;
        close((int)pOutPortParams->m_aOutPortBufPipe);
    }

    close ((int) pAppData->m_pEncILComp->m_aLocalPipe);
    for (nCnt = 0; nCnt < pAppData->m_pEncILComp->m_nInports; nCnt++) {
        pInPortParams = pAppData->m_pEncILComp->m_pInPortParams + nCnt;
        close((int)pInPortParams->m_aInPortBufPipe);
    }

    for (nCnt = 0; nCnt < pAppData->m_pEncILComp->m_nOutports; nCnt++) {
        pOutPortParams = pAppData->m_pEncILComp->m_pOutPortParams + nCnt;
        close((int)pOutPortParams->m_aOutPortBufPipe);
    }

    close(pAppData->m_stCaptComp.m_handle);

    free(pAppData->m_pDisILComp->m_pInPortParams);
    free(pAppData->m_pDeiILComp->m_pInPortParams);
    free(pAppData->m_pDeiILComp->m_pOutPortParams);
    free(pAppData->m_pEncILComp->m_pInPortParams);
    free(pAppData->m_pEncILComp->m_pOutPortParams);
    
    semp_deinit(pAppData->m_pDeiILComp->m_pEos);
    free(pAppData->m_pDeiILComp->m_pEos);
    
    semp_deinit(pAppData->m_pDeiILComp->m_pDoneSem);
    free(pAppData->m_pDeiILComp->m_pDoneSem);
    
    semp_deinit(pAppData->m_pDeiILComp->m_pPortSem);
    free(pAppData->m_pDeiILComp->m_pPortSem);

    semp_deinit(pAppData->m_pDisILComp->m_pEos);
    free(pAppData->m_pDisILComp->m_pEos);

    semp_deinit(pAppData->m_pDisILComp->m_pDoneSem);
    free(pAppData->m_pDisILComp->m_pDoneSem);

    semp_deinit(pAppData->m_pDisILComp->m_pPortSem);
    free(pAppData->m_pDisILComp->m_pPortSem);

    semp_deinit(pAppData->m_pEncILComp->m_pEos);
    free(pAppData->m_pEncILComp->m_pEos);

    semp_deinit(pAppData->m_pEncILComp->m_pDoneSem);
    free(pAppData->m_pEncILComp->m_pDoneSem);

    semp_deinit(pAppData->m_pEncILComp->m_pPortSem);
    free(pAppData->m_pEncILComp->m_pPortSem);

    free(pAppData->m_pEncILComp);

    free(pAppData->m_pDeiILComp);

    free(pAppData->m_pDisILComp);

    free(pAppData);
}

#if 0
void UartEncoderReadTask(void *pArg)
{
    int nReadSize = 0, nDataSize = 0;
    char aCmd[10], *pData;
    UART_REQUEST_MESSAGE stReq;

    memset(&stReq, 0x00, sizeof(UART_REQUEST_MESSAGE));
#if 1 /* Car Information Test */
    stReq.m_nStx = UART_PROTOCOL_START_FLAG;
    stReq.m_nLength = sizeof(UART_REQUEST_MESSAGE);
    stReq.m_nType = UART_PROTOCOL_REQ_TYPE;
    stReq.m_nMsgId = UART_PROTOCOL_ID_START;
    stReq.m_nData = 0x01;

    stReq.m_nNumberOfCarId = 5;

    stReq.m_aCarId[0] = 0x01;
    stReq.m_aCarId[1] = 0x11;
    stReq.m_nCheckSum += stReq.m_aCarId[0];
    stReq.m_nCheckSum += stReq.m_aCarId[1];
    stReq.m_aCarId[2] = 0x02;
    stReq.m_aCarId[3] = 0x22;
    stReq.m_nCheckSum += stReq.m_aCarId[2];
    stReq.m_nCheckSum += stReq.m_aCarId[3];
    stReq.m_aCarId[4] = 0x03;
    stReq.m_aCarId[5] = 0x33;
    stReq.m_nCheckSum += stReq.m_aCarId[4];
    stReq.m_nCheckSum += stReq.m_aCarId[5];
    stReq.m_aCarId[6] = 0x04;
    stReq.m_aCarId[7] = 0x44;
    stReq.m_nCheckSum += stReq.m_aCarId[6];
    stReq.m_nCheckSum += stReq.m_aCarId[7];
    stReq.m_aCarId[8] = 0x05;
    stReq.m_aCarId[9] = 0x55;
    stReq.m_nCheckSum += stReq.m_aCarId[8];
    stReq.m_nCheckSum += stReq.m_aCarId[9];
    stReq.m_nCheckSum += (
                    stReq.m_nStx+
                    sizeof(UART_REQUEST_MESSAGE)+
                    stReq.m_nType+
                    stReq.m_nMsgId+
                    stReq.m_nNumberOfCarId+
                    stReq.m_nData
                    ) & 0xff;
    stReq.m_nEtx = 0xe2;

    WriteUart(g_stManager.m_nUart, &stReq, stReq.m_nLength);
#endif

    while (!g_stManager.m_bEnd) {

        if (g_stManager.m_bUartInt) {

            nReadSize = read(g_stManager.m_nUart, g_stManager.m_aUartBuffer, 255);
            g_stManager.m_bUartInt = 0;

            if (g_stManager.m_aUartBuffer[0] == UART_PROTOCOL_START_FLAG &&
                g_stManager.m_aUartBuffer[nReadSize-1] == UART_PROTOCOL_END_FLAG &&
                g_stManager.m_aUartBuffer[3] == UART_PROTOCOL_ID_CAR) {

                pData = &g_stManager.m_aUartBuffer[4];
                nDataSize = g_stManager.m_aUartBuffer[1]-6;

                memset(aCmd, 0x00, 10);
                strcpy(aCmd, "CAN");
                send(g_stManager.m_nCarInfoSocket, aCmd, 10, 0);
                memset(aCmd, 0x00, 10);
                sprintf(aCmd, "%d", nDataSize);
                send(g_stManager.m_nCarInfoSocket, aCmd, 10, 0);
                send(g_stManager.m_nCarInfoSocket, pData, nDataSize, 0);
                printf("Car Info Send : %d\t %d\n", nDataSize, g_stManager.m_nFrameCount);
            }
        }

        usleep(15000);
    }

    stReq.m_nStx = UART_PROTOCOL_START_FLAG;
    stReq.m_nLength = sizeof(UART_REQUEST_MESSAGE);
    stReq.m_nType = UART_PROTOCOL_REQ_TYPE;
    stReq.m_nMsgId = UART_PROTOCOL_ID_END;
    stReq.m_nData = 0x01;
    stReq.m_nCheckSum = (UART_PROTOCOL_START_FLAG+sizeof(UART_REQUEST_MESSAGE)+UART_PROTOCOL_REQ_TYPE+UART_PROTOCOL_ID_END+0x01) & 0xff;
    stReq.m_nEtx = UART_PROTOCOL_END_FLAG;
    WriteUart(g_stManager.m_nUart, &stReq, stReq.m_nLength);
}
#endif

void StreamEncoder()
{
    ENCODER *pAppData = NULL;
    OMX_ERRORTYPE eErr = OMX_ErrorNone;
    OMX_U32 nCntI, nCntJ;
    OMX_S32 nRetValue;
    PIPE_MSG stPipeMsg;;
    INPORT_PARAMS *pInPortParams = NULL;
    OUTPORT_PARAMS *pOutPortParams = NULL;

    g_stManager.m_bEnd = 0;
    
    /* Video Indicator */
    GpioExport(ENCODER_VIDEO_INDICATOR);
    GpioSetDirection(ENCODER_VIDEO_INDICATOR, 0);

    /* Default setting */
    GpioExport(51);
    GpioExport(52);
    GpioSetDirection(51, 1);
    GpioSetDirection(52, 1);

    /* Encoder setting */
    GpioSetValue(51, 0);
    GpioSetValue(52, 1);

    /* LDWS Enable */
    GpioExport(48);
    GpioSetDirection(48, 1);
    GpioSetValue(48, 0);

    /* ECU Power on */
    ECUInit();
    ECUPowerOn();

    /* ECU Power Enable Delay */
    usleep(1000000);

    /* SerDes */
    system("/home/root/serdes");

    InitEncoder(&pAppData);
    printf("InitEncoder Success\n");
    
    pAppData->m_pCb.EventHandler = CbEventHandler;
    pAppData->m_pCb.EmptyBufferDone = CbEmptyBufferDone;
    pAppData->m_pCb.FillBufferDone = CbFillBufferDone;

    /* VFPC-DEI */
    eErr = OMX_GetHandle(&pAppData->m_pDeiHandle, (OMX_STRING)"OMX.TI.VPSSM3.VFPC.DEIMDUALOUT", pAppData->m_pDeiILComp, &pAppData->m_pCb);
    if ((eErr != OMX_ErrorNone) || (pAppData->m_pDeiHandle == NULL)) {
        printf("Error in Get Handle function : VFPC-DEI\n");
        goto EXIT;
    }
    pAppData->m_pDeiILComp->m_handle = pAppData->m_pDeiHandle;
    printf("VFPC-DEI compoenent is created\n");

    SetDeiParams(pAppData);

    OMX_SendCommand(pAppData->m_pDeiHandle, OMX_CommandPortEnable, OMX_VFPC_INPUT_PORT_START_INDEX, NULL);    
    semp_pend (pAppData->m_pDeiILComp->m_pPortSem);
    
    OMX_SendCommand(pAppData->m_pDeiHandle, OMX_CommandPortEnable, OMX_VFPC_OUTPUT_PORT_START_INDEX, NULL);
    semp_pend (pAppData->m_pDeiILComp->m_pPortSem);
    
    OMX_SendCommand (pAppData->m_pDeiHandle, OMX_CommandPortEnable, OMX_VFPC_OUTPUT_PORT_START_INDEX + 1, NULL);
    semp_pend (pAppData->m_pDeiILComp->m_pPortSem);

    /* VENC */
    eErr = OMX_GetHandle(&pAppData->m_pEncHandle, (OMX_STRING)"OMX.TI.DUCATI.VIDENC", pAppData->m_pEncILComp, &pAppData->m_pCb);
    if ((eErr != OMX_ErrorNone) || (pAppData->m_pEncHandle == NULL)) {
        printf("Error in Get Handle function : VENC\n");
        goto EXIT;
    }
    pAppData->m_pEncILComp->m_handle = pAppData->m_pEncHandle;
    printf("VENC compoenent is created\n");

    SetEncodeParams(pAppData);

    /* VFDC */
    eErr = OMX_GetHandle(&pAppData->m_pDisHandle, "OMX.TI.VPSSM3.VFDC", pAppData->m_pDisILComp, &pAppData->m_pCb);
    if (eErr != OMX_ErrorNone) {
        printf("Error in Get Handle function : VFDC\n");
        goto EXIT;
    }
    pAppData->m_pDisILComp->m_handle = pAppData->m_pDisHandle;    
    printf("VFDC compoenent is created\n");

    /* CTRL-DC */
    eErr = OMX_GetHandle (&pAppData->m_pCtrlHandle, "OMX.TI.VPSSM3.CTRL.DC", pAppData->m_pDisILComp, &pAppData->m_pCb);
    if (eErr != OMX_ErrorNone) {
        printf("Error in Get Handle function : CTRL-DC\n");
    }
    printf("CTRL-DC compoenent is created\n");

    SetDisplayParams(pAppData);

    OMX_SendCommand (pAppData->m_pDisHandle, OMX_CommandPortEnable, OMX_VFDC_INPUT_PORT_START_INDEX, NULL);
    semp_pend(pAppData->m_pDisILComp->m_pPortSem);

    /* Connect Buffer */
    ConnectComponents(pAppData->m_pDeiILComp,
                      OMX_VFPC_OUTPUT_PORT_START_INDEX + 1,
                      pAppData->m_pEncILComp,
                      OMX_VIDENC_INPUT_PORT);
    ConnectComponents(pAppData->m_pDeiILComp,
                      OMX_VFPC_OUTPUT_PORT_START_INDEX,
                      pAppData->m_pDisILComp,
                      OMX_VFDC_INPUT_PORT_START_INDEX);

    eErr = OMX_SendCommand(pAppData->m_pDeiHandle, OMX_CommandStateSet, OMX_StateIdle, NULL);
    if (eErr != OMX_ErrorNone) {
        printf("SYNERGYS DEBUG : Error in SendCommand()-OMX_StateIdle State set : VFPC-DEI\n");
        goto EXIT;
    }

    /* DEI In-buffer */
    printf("VFPC-DEI In-buffer start\n");
    for (nCntI = 0; nCntI < pAppData->m_pDeiILComp->m_pInPortParams->m_nBufferCountActual; nCntI++) {
        pInPortParams = pAppData->m_pDeiILComp->m_pInPortParams;
        eErr = OMX_AllocateBuffer(pAppData->m_pDeiHandle,
                                    &pInPortParams->m_pInPortBuff[nCntI],
                                    OMX_VFPC_INPUT_PORT_START_INDEX,
                                    pAppData,
                                    pInPortParams->m_nBufferSize);
        if (eErr != OMX_ErrorNone) {
            printf("Error in OMX_AllocateBuffer()-Input Port State set\n");
            goto EXIT;
        }
    }
    printf("VFPC-DEI In-buffer end\n");

    /* DEI Out-buffer */
    printf("VFPC-DEI Out-buffer start\n");
    for (nCntJ = 0; nCntJ < pAppData->m_pDeiILComp->m_nOutports; nCntJ++) {
        pOutPortParams = pAppData->m_pDeiILComp->m_pOutPortParams + nCntJ;
        for (nCntI = 0; nCntI < pOutPortParams->m_nBufferCountActual; nCntI++) {
            eErr = OMX_AllocateBuffer(pAppData->m_pDeiHandle,
                                        &pOutPortParams->m_pOutPortBuff[nCntI],
                                        OMX_VFPC_OUTPUT_PORT_START_INDEX + nCntJ,
                                        pAppData,
                                        pOutPortParams->m_nBufferSize);
            if (eErr != OMX_ErrorNone) {
                printf("Error in OMX_AllocateBuffer()-Output Port State set\n");
                goto EXIT;
            }
        }
    }
    printf("VFPC-DEI Out-buffer end\n");
    semp_pend(pAppData->m_pDeiILComp->m_pDoneSem);

    eErr = OMX_SendCommand(pAppData->m_pEncHandle, OMX_CommandStateSet, OMX_StateIdle, NULL);
    if (eErr != OMX_ErrorNone) {
        printf("Error in SendCommand()-OMX_StateIdle State set\n");
        goto EXIT;
    }

    printf("VENC In-buffer start\n");
    for (nCntI = 0; nCntI < pAppData->m_pEncILComp->m_pInPortParams->m_nBufferCountActual; nCntI++) {
        pOutPortParams = pAppData->m_pDeiILComp->m_pOutPortParams + 1;
        eErr = OMX_UseBuffer(pAppData->m_pEncHandle,
                               &pAppData->m_pEncILComp->m_pInPortParams->m_pInPortBuff[nCntI],
                               OMX_VIDENC_INPUT_PORT,
                               pAppData->m_pEncILComp,
                               pOutPortParams->m_nBufferSize,
                               pOutPortParams->m_pOutPortBuff[nCntI]->pBuffer);
    
        if (eErr != OMX_ErrorNone) {
            printf("Error in encode OMX_UseBuffer()\n");
            goto EXIT;
        }
    }
    printf("VENC In-buffer end\n");

    printf("VENC Out-buffer start\n");
    for (nCntI = 0; nCntI < pAppData->m_pEncILComp->m_pOutPortParams->m_nBufferCountActual; nCntI++) {
        eErr = OMX_AllocateBuffer(pAppData->m_pEncHandle,
                                    &pAppData->m_pEncILComp->m_pOutPortParams->m_pOutPortBuff[nCntI],
                                    OMX_VIDENC_OUTPUT_PORT,
                                    pAppData,
                                    pAppData->m_pEncILComp->m_pOutPortParams->m_nBufferSize);
        if (eErr != OMX_ErrorNone) {
            printf("Error in OMX_AllocateBuffer()-Output Port State set\n");
            goto EXIT;
        }
    }
    printf("VENC Out-buffer end\n");
    semp_pend(pAppData->m_pEncILComp->m_pDoneSem);

    eErr = OMX_SendCommand(pAppData->m_pCtrlHandle, OMX_CommandStateSet, OMX_StateIdle, NULL);
    if (eErr != OMX_ErrorNone) {
        printf("Error in SendCommand()-OMX_StateIdle State set\n");
        goto EXIT;
    }
    semp_pend(pAppData->m_pDisILComp->m_pDoneSem);

    eErr = OMX_SendCommand(pAppData->m_pDisHandle, OMX_CommandStateSet, OMX_StateIdle, NULL);
    if (eErr != OMX_ErrorNone) {
        printf("Error in SendCommand()-OMX_StateIdle State set\n");
        goto EXIT;
    }

    printf("VFDC In-buffer start\n");
    for (nCntI = 0; nCntI < pAppData->m_pDisILComp->m_pInPortParams->m_nBufferCountActual; nCntI++)
    {
        pOutPortParams = pAppData->m_pDeiILComp->m_pOutPortParams;
        eErr = OMX_UseBuffer(pAppData->m_pDisHandle,
                               &pAppData->m_pDisILComp->m_pInPortParams->m_pInPortBuff[nCntI],
                               OMX_VFDC_INPUT_PORT_START_INDEX,
                               pAppData->m_pDisILComp,
                               pOutPortParams->m_nBufferSize,
                               pOutPortParams->m_pOutPortBuff[nCntI]->pBuffer);
        if (eErr != OMX_ErrorNone) {
            printf("Error in Display OMX_UseBuffer()\n");
            goto EXIT;
        }
    }
    printf("VFDC In-buffer end\n");
    semp_pend(pAppData->m_pDisILComp->m_pDoneSem);

    printf("VFDC-CTRL State Idle\n");
    eErr = OMX_SendCommand(pAppData->m_pCtrlHandle, OMX_CommandStateSet, OMX_StateExecuting, NULL);
    if (eErr != OMX_ErrorNone) {
        printf("Error in SendCommand()-OMX_StateIdle State set\n");
        goto EXIT;
    }    
    semp_pend(pAppData->m_pDisILComp->m_pDoneSem);

    printf("VFDC State Execute\n");
    eErr = OMX_SendCommand(pAppData->m_pDisHandle, OMX_CommandStateSet, OMX_StateExecuting, NULL);
    if (eErr != OMX_ErrorNone) {
        printf("Error from SendCommand-Executing State set\n");
        goto EXIT;
    }
    semp_pend(pAppData->m_pDisILComp->m_pDoneSem);

    printf("VENC State Execute\n");
    eErr = OMX_SendCommand(pAppData->m_pEncHandle, OMX_CommandStateSet, OMX_StateExecuting, NULL);
    if (eErr != OMX_ErrorNone) {
        printf("Error from SendCommand-Executing State set\n");
        goto EXIT;
    }
    semp_pend(pAppData->m_pEncILComp->m_pDoneSem);

    printf("VFPC-DEI State Execute\n");
    eErr = OMX_SendCommand(pAppData->m_pDeiHandle, OMX_CommandStateSet, OMX_StateExecuting, NULL);
    if (eErr != OMX_ErrorNone) {
        printf("Error from SendCommand-Executing State set\n");
        goto EXIT;
    }
    semp_pend(pAppData->m_pDeiILComp->m_pDoneSem);

    printf("VENC Out Task Create\n");
    pthread_attr_init(&pAppData->m_pEncILComp->m_stThreadAttr);
    if (0 != pthread_create(&pAppData->m_pEncILComp->m_nOutDataStrmThrdId,
                            &pAppData->m_pEncILComp->m_stThreadAttr,
                            (FcnPtr)WriteTask,
                            pAppData)) {
        printf("Create_Task failed !\n");
        goto EXIT;
    }

    printf("VENC Conn Task Create\n");
    pthread_attr_init(&pAppData->m_pEncILComp->m_stThreadAttr);
    if (0 != pthread_create(&pAppData->m_pEncILComp->m_nConnDataStrmThrdId,
                            &pAppData->m_pEncILComp->m_stThreadAttr,
                            (FcnPtr)ConnInConnOutTask,
                            pAppData->m_pEncILComp)) {
        printf("Create_Task failed !\n");
        goto EXIT;
    }

    printf("VFPC-DEI Conn Task Create\n");
    pthread_attr_init(&pAppData->m_pDeiILComp->m_stThreadAttr);
    if (0 != pthread_create(&pAppData->m_pDeiILComp->m_nConnDataStrmThrdId,
                            &pAppData->m_pDeiILComp->m_stThreadAttr,
                            (FcnPtr)ConnInConnOutTask,
                            pAppData->m_pDeiILComp)) {
        printf("Create_Task failed !\n");
        goto EXIT;
    }

    printf("CAPTURE Input Task Create\n");
    pthread_attr_init(&pAppData->m_pDeiILComp->m_stThreadAttr);
    if (0 != pthread_create(&pAppData->m_pDeiILComp->m_nInDataStrmThrdId,
                            &pAppData->m_pDeiILComp->m_stThreadAttr,
                            (FcnPtr)InputTask,
                            pAppData)) {
        printf("Create_Task failed !");
        goto EXIT;
    }

    printf("VFDC Conn Task Create\n");
    pthread_attr_init(&pAppData->m_pDisILComp->m_stThreadAttr);
    if (0 != pthread_create(&pAppData->m_pDisILComp->m_nConnDataStrmThrdId,
                            &pAppData->m_pDisILComp->m_stThreadAttr,
                            (FcnPtr)ConnInConnOutTask,
                            pAppData->m_pDisILComp)) {
        printf("Create_Task failed !");
        goto EXIT;
    }

    semp_pend(pAppData->m_pEncILComp->m_pEos);
    
    stPipeMsg.m_eCmd = PIPE_CMD_EXIT;
    write(pAppData->m_pDeiILComp->m_aLocalPipe[1], &stPipeMsg, sizeof(PIPE_MSG));
    write(pAppData->m_pDisILComp->m_aLocalPipe[1], &stPipeMsg, sizeof(PIPE_MSG));
    write(pAppData->m_pEncILComp->m_aLocalPipe[1], &stPipeMsg, sizeof(PIPE_MSG));
        
    eErr = OMX_SendCommand(pAppData->m_pDeiHandle, OMX_CommandStateSet, OMX_StateIdle, NULL);
    if (eErr != OMX_ErrorNone) {
        printf("Error from SendCommand-Idle State set\n");
        goto EXIT;
    }    
    semp_pend(pAppData->m_pDeiILComp->m_pDoneSem);

    eErr = OMX_SendCommand(pAppData->m_pDisHandle, OMX_CommandStateSet, OMX_StateIdle, NULL);
    if (eErr != OMX_ErrorNone) {
        printf("Error from SendCommand-Idle State set\n");
        goto EXIT;
    }
    semp_pend(pAppData->m_pDisILComp->m_pDoneSem);
    
    eErr = OMX_SendCommand(pAppData->m_pCtrlHandle, OMX_CommandStateSet, OMX_StateIdle, NULL);
    if (eErr != OMX_ErrorNone) {
        printf("Error from SendCommand-Idle State set\n");
        goto EXIT;
    }    
    semp_pend(pAppData->m_pDisILComp->m_pDoneSem);
        
    eErr = OMX_SendCommand(pAppData->m_pEncHandle, OMX_CommandStateSet, OMX_StateIdle, NULL);
    if (eErr != OMX_ErrorNone) {
        printf("Error from SendCommand-Idle State set\n");
        goto EXIT;

    }
    semp_pend(pAppData->m_pEncILComp->m_pDoneSem);

    eErr =  OMX_SendCommand(pAppData->m_pDisHandle, OMX_CommandStateSet,  OMX_StateLoaded, NULL);
    if (eErr != OMX_ErrorNone) {
        printf("Error from SendCommand-Idle State set\n");
        goto EXIT;
    }

    for (nCntI = 0; nCntI < pAppData->m_pDisILComp->m_pInPortParams->m_nBufferCountActual; nCntI++) {
        eErr =  OMX_FreeBuffer(pAppData->m_pDisHandle, OMX_VFDC_INPUT_PORT_START_INDEX, pAppData->m_pDisILComp->m_pInPortParams->m_pInPortBuff[nCntI]);
        if (eErr != OMX_ErrorNone) {
            printf("Error in OMX_FreeBuffer\n");
            goto EXIT;
        }
    }
    semp_pend(pAppData->m_pDisILComp->m_pDoneSem);
    
    eErr = OMX_SendCommand(pAppData->m_pCtrlHandle, OMX_CommandStateSet, OMX_StateLoaded, NULL);
    if (eErr != OMX_ErrorNone) {
        printf("Error in SendCommand()-OMX_StateLoaded State set\n");
        goto EXIT;
    }
    semp_pend(pAppData->m_pDisILComp->m_pDoneSem);

    eErr = OMX_SendCommand(pAppData->m_pEncHandle, OMX_CommandStateSet, OMX_StateLoaded, NULL);
    if (eErr != OMX_ErrorNone) {
        printf("Error in SendCommand()-OMX_StateLoaded State set\n");
        goto EXIT;
    }

    for (nCntI = 0; nCntI < pAppData->m_pEncILComp->m_pInPortParams->m_nBufferCountActual; nCntI++) {
        eErr = OMX_FreeBuffer(pAppData->m_pEncHandle, OMX_VIDENC_INPUT_PORT, pAppData->m_pEncILComp->m_pInPortParams->m_pInPortBuff[nCntI]);
        if (eErr != OMX_ErrorNone) {
            printf("Error in OMX_FreeBuffer\n");
            goto EXIT;
        }
    }
    
    for (nCntI = 0; nCntI < pAppData->m_pEncILComp->m_pOutPortParams->m_nBufferCountActual; nCntI++) {
        eErr =  OMX_FreeBuffer(pAppData->m_pEncHandle, OMX_VIDENC_OUTPUT_PORT, pAppData->m_pEncILComp->m_pOutPortParams->m_pOutPortBuff[nCntI]);
        if (eErr != OMX_ErrorNone) {
            printf("Error in OMX_FreeBuffer\n");
            goto EXIT;
        }
    }
    semp_pend(pAppData->m_pEncILComp->m_pDoneSem);
    
    eErr = OMX_SendCommand(pAppData->m_pDeiHandle, OMX_CommandStateSet, OMX_StateLoaded, NULL);
    if (eErr != OMX_ErrorNone) {
        printf ("Error from SendCommand-Idle State set\n");
        goto EXIT;
    }

    for (nCntI = 0; nCntI < pAppData->m_pDeiILComp->m_pInPortParams->m_nBufferCountActual; nCntI++) {
        eErr = OMX_FreeBuffer(pAppData->m_pDeiHandle, OMX_VFPC_INPUT_PORT_START_INDEX, pAppData->m_pDeiILComp->m_pInPortParams->m_pInPortBuff[nCntI]);
        if (eErr != OMX_ErrorNone) {
            printf("Error in OMX_FreeBuffer\n");
            goto EXIT;
        }
    }
    
    for (nCntJ = 0; nCntJ < pAppData->m_pDeiILComp->m_nOutports; nCntJ++) {
        pOutPortParams = pAppData->m_pDeiILComp->m_pOutPortParams + nCntJ;
        for (nCntI = 0; nCntI < pOutPortParams->m_nBufferCountActual; nCntI++) {
            eErr = OMX_FreeBuffer(pAppData->m_pDeiHandle, OMX_VFPC_OUTPUT_PORT_START_INDEX + nCntJ, pOutPortParams->m_pOutPortBuff[nCntI]);
            if (eErr != OMX_ErrorNone) {
                printf("Error in OMX_FreeBuffer\n");
                goto EXIT;
            }
        }
    }
    semp_pend(pAppData->m_pDeiILComp->m_pDoneSem);
    
    eErr = OMX_FreeHandle(pAppData->m_pEncHandle);
    if ((eErr != OMX_ErrorNone)) {
        printf("Error in Free Handle function\n");
        goto EXIT;
    }

    eErr = OMX_FreeHandle(pAppData->m_pDeiHandle);
    if ((eErr != OMX_ErrorNone)) {
        printf("Error in Free Handle function\n");
        goto EXIT;
    }
    
    eErr = OMX_FreeHandle(pAppData->m_pDisHandle);
    if ((eErr != OMX_ErrorNone)) {
        printf("Error in Free Handle function\n");
        goto EXIT;
    }
    
    eErr = OMX_FreeHandle(pAppData->m_pCtrlHandle);
    if ((eErr != OMX_ErrorNone)) {
        printf("Error in Free Handle function\n");
        goto EXIT;
    }

    pthread_join(pAppData->m_pDeiILComp->m_nInDataStrmThrdId, (void **) &nRetValue);
    pthread_join(pAppData->m_pEncILComp->m_nConnDataStrmThrdId, (void **)&nRetValue);
    pthread_join(pAppData->m_pDeiILComp->m_nConnDataStrmThrdId, (void **)&nRetValue);
    pthread_join(pAppData->m_pDisILComp->m_nConnDataStrmThrdId, (void **)&nRetValue);
    pthread_join(pAppData->m_pEncILComp->m_nOutDataStrmThrdId, (void **)&nRetValue);

    DestroyEncoder(pAppData);
    
EXIT:
    return;
}

