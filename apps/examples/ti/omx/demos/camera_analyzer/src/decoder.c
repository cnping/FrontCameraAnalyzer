
#include "decoder.h"

#if 1
int PADX = 32;
int PADY = 24;

unsigned int CROPPED_WIDTH = 1280;
unsigned int CROPPED_HEIGHT = 720;

static void ChunkingCtxInit (H264_ChunkingCtx *pChunkingCtx)
{
    pChunkingCtx->workingWord = H264_WORKING_WORD_INIT;
    pChunkingCtx->fifthByte = 0xFF;
    pChunkingCtx->state = H264_ST_LOOKING_FOR_SPS;
}

static void ParserInit(H264_ParsingCtx *pParsingCtx)
{
    /* Initialize H.264 Parsing Context */
    pParsingCtx->readBuf = (unsigned char *)malloc(READSIZE);
    if (!pParsingCtx->readBuf) {
        printf("h264 Parser read buf memory allocation failed\n");
    }

    ChunkingCtxInit(&pParsingCtx->ctx);
    pParsingCtx->frameNo = 0;
    pParsingCtx->frameSize = 0;
    pParsingCtx->bytes = 0;
    pParsingCtx->tmp = 0;
    pParsingCtx->firstParse = 1;
    pParsingCtx->chunkCnt = 1;
    pParsingCtx->outBuf.ptr = NULL;
    pParsingCtx->outBuf.bufsize = 0;
    pParsingCtx->outBuf.bufused = 0;
}

static unsigned int DoChunking(
    H264_ChunkingCtx *pChunkingCtx,
    AVChunk_Buf *pOutBuffers,
    unsigned int nNumberOfOutBuffers,
    AVChunk_Buf *pInBuffer,
    void *pAttrs)
{
    int nCntI = 0, nCntJ, nFrameEnd, nSCFound, tmp, bytes;
    unsigned int w, z;
    unsigned char *inp;

    inp = &pInBuffer->ptr[pInBuffer->bufused];
    bytes = pInBuffer->bufsize - pInBuffer->bufused;

BACK:
    if (H264_ST_INSIDE_PICTURE == pChunkingCtx->state)
    {
        tmp = nCntI;
        nSCFound = nFrameEnd = 0;
        while (nCntI < bytes)
        {
            z = pChunkingCtx->workingWord << 8;
            w = pChunkingCtx->fifthByte;
            pChunkingCtx->workingWord = z | w;
            pChunkingCtx->fifthByte = inp[nCntI++];

            if (z == 0x100)
            {
                w &= 0x1f;
                if (w == H264_NAL_ACCESS_UNIT_CODEDSLICE_CODE_FOR_NONIDR ||
                    w == H264_NAL_ACCESS_UNIT_CODEDSLICE_CODE_FOR_IDR)
                {
                    if (pChunkingCtx->fifthByte & 0x80)
                    {
                        nSCFound = nFrameEnd = 1;
                        break;
                    }
                }

                if (w == H264_PPS_START_CODE || w == H264_SPS_START_CODE)
                {
                    nSCFound = nFrameEnd = 1;
                    break;
                }
            }
        }
        nCntJ = nCntI - tmp;

        if (nFrameEnd)
        {
            nCntJ -= 5;
        }

        if (nCntJ > (int32_t) (pOutBuffers->bufsize - pOutBuffers->bufused))
        {
            memcpy(&pOutBuffers->ptr[pOutBuffers->bufused], &inp[tmp], pOutBuffers->bufsize - pOutBuffers->bufused);
            pOutBuffers->bufused = pOutBuffers->bufsize;
            pChunkingCtx->state = H264_ST_LOOKING_FOR_ZERO_SLICE;
            nFrameEnd = 1;
        }
        else if (nCntJ > 0)
        {
            memcpy(&pOutBuffers->ptr[pOutBuffers->bufused], &inp[tmp], nCntJ);
            pOutBuffers->bufused += nCntJ;
        }
        else
        {
            pOutBuffers->bufused += nCntJ;
        }

        if (nFrameEnd)
        {
            if (nSCFound)
            {
                pChunkingCtx->state = H264_ST_HOLDING_SC;
            }
            pInBuffer->bufused += nCntI;
            return PARSER_SUCCESS;
        }
    }

    if (H264_ST_LOOKING_FOR_ZERO_SLICE == pChunkingCtx->state)
    {
        tmp = nCntI;
        nSCFound = 0;
        while (nCntI < bytes)
        {
            z = pChunkingCtx->workingWord << 8;
            w = pChunkingCtx->fifthByte;
            pChunkingCtx->workingWord = z | w;
            pChunkingCtx->fifthByte = inp[nCntI++];

            if (z == 0x100)
            {
                w &= 0x1f;
                if (w == H264_NAL_ACCESS_UNIT_CODEDSLICE_CODE_FOR_NONIDR ||
                    w == H264_NAL_ACCESS_UNIT_CODEDSLICE_CODE_FOR_IDR)
                {
                    if (pChunkingCtx->fifthByte & 0x80)
                    {
                        nSCFound = 1;
                        break;
                    }
                }
            }
        } /* while (nCntI) */
        nCntJ = nCntI - tmp;

        if (nCntJ > (pOutBuffers->bufsize - pOutBuffers->bufused))
        {
            pOutBuffers->bufused = 0;
            pChunkingCtx->state = H264_ST_LOOKING_FOR_SPS;
        }
        else if (nCntJ > 0)
        {
            memcpy(&pOutBuffers->ptr[pOutBuffers->bufused], &inp[tmp], nCntJ);
            pOutBuffers->bufused += nCntJ;
        }

        if (nSCFound)
        {
            pChunkingCtx->state = H264_ST_INSIDE_PICTURE;
        }
    }

    if (H264_ST_STREAM_ERR == pChunkingCtx->state)
    {
        while (nCntI < bytes)
        {
            z = pChunkingCtx->workingWord << 8;
            w = pChunkingCtx->fifthByte;
            pChunkingCtx->workingWord = z | w;
            pChunkingCtx->fifthByte = inp[nCntI++];

            if (z == 0x100)
            {
                w &= 0x1f;
                if (w == H264_NAL_ACCESS_UNIT_CODEDSLICE_CODE_FOR_NONIDR ||
                    w == H264_NAL_ACCESS_UNIT_CODEDSLICE_CODE_FOR_IDR)
                {
                    if (pChunkingCtx->fifthByte & 0x80)
                    {
                        pChunkingCtx->state = H264_ST_HOLDING_SC;
                        break;
                    }
                }

                if (w == H264_PPS_START_CODE || w == H264_SPS_START_CODE)
                {
                    pChunkingCtx->state = H264_ST_HOLDING_SC;
                    break;
                }
            }
        }
    }

    if (H264_ST_LOOKING_FOR_SPS == pChunkingCtx->state)
    {
        while (nCntI < bytes)
        {
            z = pChunkingCtx->workingWord << 8;
            w = pChunkingCtx->fifthByte;
            pChunkingCtx->workingWord = z | w;
            pChunkingCtx->fifthByte = inp[nCntI++];

            if (z == 0x100)
            {
                w &= 0x1f;
                if (w == H264_SPS_START_CODE)
                {
                    pChunkingCtx->state = H264_ST_HOLDING_SC;
                    break;
                }
            }
        }
    }

    if (H264_ST_HOLDING_SC == pChunkingCtx->state)
    {
        w = pChunkingCtx->workingWord;
        pOutBuffers->bufused = 0;
        if (pOutBuffers->bufsize < 5)
        {
        }

        for (nCntJ = 0; nCntJ < 4; nCntJ++, w <<= 8)
        {
            pOutBuffers->ptr[pOutBuffers->bufused + nCntJ] = ((w >> 24) & 0xFF);
        }
        pOutBuffers->ptr[pOutBuffers->bufused + nCntJ] = pChunkingCtx->fifthByte;
        pOutBuffers->bufused += 5;
        w = pChunkingCtx->workingWord & 0x1f;
        if (w == H264_PPS_START_CODE || w == H264_SPS_START_CODE)
        {
            pChunkingCtx->state = H264_ST_LOOKING_FOR_ZERO_SLICE;
        }
        else
        {
            pChunkingCtx->state = H264_ST_INSIDE_PICTURE;
        }

        pChunkingCtx->workingWord = H264_WORKING_WORD_INIT;
        pChunkingCtx->fifthByte = 0xFF;
    }

    if (nCntI < bytes)
    {
        goto BACK;
    }

    pInBuffer->bufused += nCntI;
    return PARSER_ERR_INSUFFICIENT_INPUT;
}

static unsigned int GetNextFrameSize(H264_ParsingCtx *pParsingCtx)
{
    unsigned char *readBuf = pParsingCtx->readBuf;
    H264_ChunkingCtx *pChunkingCtx = &pParsingCtx->ctx;
    AVChunk_Buf *pInBuffer = &pParsingCtx->inBuf;
    AVChunk_Buf *pOutBuf = &pParsingCtx->outBuf;
    unsigned char nTermCond = 1;

    if (pParsingCtx->firstParse == 1)
        nTermCond = 0;

    while ((g_stManager.m_nVideoSocket) ||
         ((((pParsingCtx->firstParse == 0) && (pParsingCtx->bytes != 0))
           && (pParsingCtx->bytes <= READSIZE) && (pParsingCtx->tmp <= pParsingCtx->bytes))))
    {
        if (pParsingCtx->firstParse == 1)
        {
            pParsingCtx->bytes = ReadEthernet(g_stManager.m_nVideoSocket, pParsingCtx->readBuf);
            if (!pParsingCtx->bytes)
            {
                return 0;
            }
            pInBuffer->ptr = readBuf;
            pParsingCtx->tmp = 0;
            pParsingCtx->firstParse = 0;
        }
        else
        {
            if (pParsingCtx->bytes <= pParsingCtx->tmp)
            {
                pParsingCtx->bytes = ReadEthernet(g_stManager.m_nVideoSocket, pParsingCtx->readBuf);
                if (!pParsingCtx->bytes)
                {
                    return 0;
                }
                pInBuffer->ptr = readBuf;
                pParsingCtx->tmp = 0;
            }
        }

        while (pParsingCtx->bytes > pParsingCtx->tmp)
        {
            pInBuffer->bufsize = ((pParsingCtx->bytes - pParsingCtx->tmp) > 184) ? 184 : (pParsingCtx->bytes - pParsingCtx->tmp);
            pInBuffer->bufused = 0;

            while (pInBuffer->bufsize != pInBuffer->bufused)
            {
                if (PARSER_SUCCESS == DoChunking(pChunkingCtx, pOutBuf, 1, pInBuffer, NULL))
                {
                    pParsingCtx->frameNo = pParsingCtx->chunkCnt++;
                    pParsingCtx->frameSize = pOutBuf->bufused;
                    pParsingCtx->tmp += pInBuffer->bufused;
                    pInBuffer->ptr += pInBuffer->bufused;
                    return pParsingCtx->frameSize;
                }
            }
            pParsingCtx->tmp += pInBuffer->bufused;
            pInBuffer->ptr += pInBuffer->bufused;
        }
    }

    return 0;
}

OMX_ERRORTYPE SetDecodeParams(DECODER *pAppData)
{
    OMX_ERRORTYPE eErr = OMX_ErrorNone;
    OMX_HANDLETYPE pHandle = pAppData->m_pDecHandle;
    OMX_PORT_PARAM_TYPE stPortParam;
    OMX_PARAM_PORTDEFINITIONTYPE pInPortDef, pOutPortDef;
    OMX_VIDEO_PARAM_STATICPARAMS stStaticParam;
    OMX_PARAM_COMPPORT_NOTIFYTYPE stNotifyType;
  
    if (!pHandle)
    {
        eErr = OMX_ErrorBadParameter;
        goto EXIT;
    }

    OMX_INIT_PARAM (&stPortParam);
    stPortParam.nPorts = 2;
    stPortParam.nStartPortNumber = 0;
    eErr = OMX_SetParameter(pHandle, OMX_IndexParamVideoInit, &stPortParam);
    if (eErr != OMX_ErrorNone)
    {
        goto EXIT;
    }

    OMX_INIT_PARAM (&pInPortDef);
    pInPortDef.nPortIndex = OMX_VIDDEC_INPUT_PORT;
    pInPortDef.eDir = OMX_DirInput;
    pInPortDef.nBufferCountActual = DECODER_INPUT_BUFFER_COUNT;
    pInPortDef.nBufferSize = pAppData->m_nWidth * pAppData->m_nHeight;
    pInPortDef.bEnabled = OMX_TRUE;
    pInPortDef.bPopulated = OMX_FALSE;
    pInPortDef.format.video.nFrameWidth = pAppData->m_nWidth;
    pInPortDef.format.video.nFrameHeight = pAppData->m_nHeight;
    pInPortDef.format.video.nStride = -1;
    pInPortDef.format.video.nBitrate = 104857600;
    pInPortDef.format.video.xFramerate = (pAppData->m_nFrameRate) << 16;
    pInPortDef.format.video.eCompressionFormat = OMX_VIDEO_CodingAVC;
    pInPortDef.format.video.eColorFormat = OMX_COLOR_FormatYUV420Planar;
    eErr = OMX_SetParameter(pHandle, OMX_IndexParamPortDefinition, &pInPortDef);
    if (eErr != OMX_ErrorNone)
    {
        goto EXIT;
    }

    OMX_INIT_PARAM(&pOutPortDef);    
    pOutPortDef.nPortIndex = OMX_VIDDEC_OUTPUT_PORT;
    pOutPortDef.eDir = OMX_DirOutput;
    pOutPortDef.nBufferCountActual = DECODER_OUTPUT_BUFFER_COUNT;    
    pOutPortDef.nBufferSize =
        ((((pAppData->m_nWidth + (2 * PADX) +
            127) & 0xFFFFFF80) * ((((pAppData->m_nHeight + 15) & 0xfffffff0) + (4 * PADY))) * 3) >> 1);
    pOutPortDef.bEnabled = OMX_TRUE;
    pOutPortDef.bPopulated = OMX_FALSE;
    pOutPortDef.format.video.nFrameWidth = pAppData->m_nWidth;
    pOutPortDef.format.video.nFrameHeight = ((pAppData->m_nHeight + 15) & 0xfffffff0);
    pOutPortDef.format.video.nStride =
        UTIL_ALIGN((pAppData->m_nWidth + (2 * PADX)), 128);
    pOutPortDef.format.video.nBitrate = 25000000;
    pOutPortDef.format.video.xFramerate = (pAppData->m_nFrameRate) << 16;
    pOutPortDef.format.video.eCompressionFormat = OMX_VIDEO_CodingUnused;
    pOutPortDef.format.video.eColorFormat = OMX_COLOR_FormatYUV420SemiPlanar;    
    eErr = OMX_SetParameter(pHandle, OMX_IndexParamPortDefinition, &pOutPortDef);
    if (eErr != OMX_ErrorNone)
    {
        eErr = OMX_ErrorBadParameter;
        goto EXIT;
    }
    
    eErr = OMX_GetParameter(pHandle, OMX_IndexParamPortDefinition, &pOutPortDef);
    if (eErr != OMX_ErrorNone)
    {
        eErr = OMX_ErrorBadParameter;
        goto EXIT;
    }
    pAppData->m_nDecStride = pOutPortDef.format.video.nStride;
    
    OMX_INIT_PARAM(&stNotifyType);
    stNotifyType.eNotifyType = OMX_NOTIFY_TYPE_NONE;
    stNotifyType.nPortIndex = OMX_VIDDEC_INPUT_PORT;
    eErr = OMX_SetParameter(pHandle, OMX_TI_IndexParamCompPortNotifyType, &stNotifyType);
    if (eErr != OMX_ErrorNone)
    {
        printf("input port OMX_SetParameter failed\n");
        eErr = OMX_ErrorBadParameter;
        goto EXIT;
    }
    stNotifyType.eNotifyType = OMX_NOTIFY_TYPE_NONE;
    stNotifyType.nPortIndex = OMX_VIDDEC_OUTPUT_PORT;
    eErr = OMX_SetParameter(pHandle, OMX_TI_IndexParamCompPortNotifyType, &stNotifyType);
    if (eErr != OMX_ErrorNone)
    {
        printf("output port OMX_SetParameter failed\n");
        eErr = OMX_ErrorBadParameter;
        goto EXIT;
    }
    
    OMX_INIT_PARAM(&stStaticParam);
    stStaticParam.nPortIndex = OMX_VIDDEC_OUTPUT_PORT;
    eErr = OMX_GetParameter(pHandle, OMX_TI_IndexParamVideoStaticParams, &stStaticParam);
    printf("level set is %d \n", (int) stStaticParam.videoStaticParams.h264DecStaticParams.presetLevelIdc);
    stStaticParam.videoStaticParams.h264DecStaticParams.presetLevelIdc = IH264VDEC_LEVEL42;
    eErr = OMX_SetParameter(pHandle, OMX_TI_IndexParamVideoStaticParams, &stStaticParam);
EXIT:
    return eErr;
}

OMX_ERRORTYPE SetScalarParams(DECODER *pAppData)
{
    OMX_ERRORTYPE eErr = OMX_ErrorNone;
    OMX_PARAM_BUFFER_MEMORYTYPE stMemTypeCfg;
    OMX_PARAM_PORTDEFINITIONTYPE stParamPort;
    OMX_PARAM_VFPC_NUMCHANNELPERHANDLE stNumChPerHandle;
    OMX_CONFIG_ALG_ENABLE stAlgEnable;
    OMX_CONFIG_VIDCHANNEL_RESOLUTION chResolution;
    
    OMX_INIT_PARAM(&stMemTypeCfg);
    stMemTypeCfg.nPortIndex = OMX_VFPC_INPUT_PORT_START_INDEX;
    stMemTypeCfg.eBufMemoryType = OMX_BUFFER_MEMORY_DEFAULT;
    eErr = OMX_SetParameter (pAppData->m_pScHandle, OMX_TI_IndexParamBuffMemType, &stMemTypeCfg);
    if (eErr != OMX_ErrorNone)
    {
        printf("failed to set memory Type at input port\n");
    }
    
    OMX_INIT_PARAM(&stMemTypeCfg);
    stMemTypeCfg.nPortIndex = OMX_VFPC_OUTPUT_PORT_START_INDEX;
    stMemTypeCfg.eBufMemoryType = OMX_BUFFER_MEMORY_DEFAULT;
    eErr = OMX_SetParameter(pAppData->m_pScHandle, OMX_TI_IndexParamBuffMemType, &stMemTypeCfg);
    if (eErr != OMX_ErrorNone)
    {
        printf("failed to set memory Type at output port\n");
    }
    
    OMX_INIT_PARAM(&stParamPort);
    stParamPort.nPortIndex = OMX_VFPC_INPUT_PORT_START_INDEX;
    
    OMX_GetParameter(pAppData->m_pScHandle, OMX_IndexParamPortDefinition, &stParamPort);
    stParamPort.nPortIndex = OMX_VFPC_INPUT_PORT_START_INDEX;
    stParamPort.format.video.nFrameWidth = pAppData->m_nWidth;
    stParamPort.format.video.nFrameHeight = pAppData->m_nHeight;    
    stParamPort.format.video.nStride = pAppData->m_nDecStride;
    stParamPort.format.video.eCompressionFormat = OMX_VIDEO_CodingUnused;
    stParamPort.format.video.eColorFormat = OMX_COLOR_FormatYUV420SemiPlanar;
    stParamPort.nBufferSize = (stParamPort.format.video.nStride * pAppData->m_nHeight * 3) >> 1;
    stParamPort.nBufferAlignment = 0;
    stParamPort.bBuffersContiguous = 0;
    stParamPort.nBufferCountActual = SCALAR_INPUT_BUFFER_COUNT;
    printf("set input port params (width = %u, height = %u) \n", (unsigned int)pAppData->m_nWidth, (unsigned int)pAppData->m_nHeight);
    eErr = OMX_SetParameter(pAppData->m_pScHandle, OMX_IndexParamPortDefinition, &stParamPort);
    if(eErr != OMX_ErrorNone)
    {
        printf(" Invalid INPUT color formats for Scalar \n");
        OMX_FreeHandle(pAppData->m_pDisHandle);    
        return eErr;
    }
    
    OMX_INIT_PARAM(&stParamPort);
    stParamPort.nPortIndex = OMX_VFPC_OUTPUT_PORT_START_INDEX;
    OMX_GetParameter(pAppData->m_pScHandle, OMX_IndexParamPortDefinition, &stParamPort);
    stParamPort.nPortIndex = OMX_VFPC_OUTPUT_PORT_START_INDEX;
    stParamPort.format.video.nFrameWidth = pAppData->m_nWidth;
    stParamPort.format.video.nFrameHeight = pAppData->m_nHeight;
    stParamPort.format.video.eCompressionFormat = OMX_VIDEO_CodingUnused;
    stParamPort.format.video.eColorFormat = OMX_COLOR_FormatYCbYCr;
    stParamPort.nBufferAlignment = 0;
    stParamPort.bBuffersContiguous = 0;
    stParamPort.nBufferCountActual = SCALAR_OUTPUT_BUFFER_COUNT;
    stParamPort.format.video.nStride = ((pAppData->m_nWidth + 15) & 0xfffffff0) * 2;
    stParamPort.nBufferSize = stParamPort.format.video.nStride * stParamPort.format.video.nFrameHeight;
    printf("set output port params (width = %d, height = %d)\n", (int)stParamPort.format.video.nFrameWidth, (int)stParamPort.format.video.nFrameHeight);
    eErr = OMX_SetParameter(pAppData->m_pScHandle, OMX_IndexParamPortDefinition, &stParamPort);
    if(eErr != OMX_ErrorNone)
    {
        printf("Invalid OUTPUT color formats for Scalar \n");
        OMX_FreeHandle(pAppData->m_pDisHandle);    
        return eErr;
    }
    
    printf("set number of channels \n");
    OMX_INIT_PARAM(&stNumChPerHandle);
    stNumChPerHandle.nNumChannelsPerHandle = 1;
    eErr = OMX_SetParameter(pAppData->m_pScHandle, (OMX_INDEXTYPE)OMX_TI_IndexParamVFPCNumChPerHandle, &stNumChPerHandle);
    if (eErr != OMX_ErrorNone)
    {
        printf("failed to set num of channels\n");
    }
    
    printf ("set input resolution \n");    
    OMX_INIT_PARAM(&chResolution);
    chResolution.Frm0Width = pAppData->m_nWidth;
    chResolution.Frm0Height = pAppData->m_nHeight;
    chResolution.Frm0Pitch = pAppData->m_nDecStride;
    chResolution.Frm1Width = 0;
    chResolution.Frm1Height = 0;
    chResolution.Frm1Pitch = 0;
    chResolution.FrmStartX = CROP_START_X;
    chResolution.FrmStartY = CROP_START_Y;
    chResolution.FrmCropWidth = CROPPED_WIDTH;
    chResolution.FrmCropHeight = CROPPED_HEIGHT;
    chResolution.eDir = OMX_DirInput;
    chResolution.nChId = 0;
    eErr = OMX_SetConfig(pAppData->m_pScHandle, (OMX_INDEXTYPE)OMX_TI_IndexConfigVidChResolution, &chResolution);
    if (eErr != OMX_ErrorNone)
    {
        printf("failed to set input channel resolution\n");
    }
    
    printf("set output resolution \n");
    OMX_INIT_PARAM(&chResolution);
    chResolution.Frm0Width = CROPPED_WIDTH; 
    chResolution.Frm0Height = CROPPED_HEIGHT;
    chResolution.Frm0Pitch = ((CROPPED_WIDTH + 15) & 0xfffffff0) * 2;
    chResolution.Frm1Width = 0;
    chResolution.Frm1Height = 0;
    chResolution.Frm1Pitch = 0;
    chResolution.FrmStartX = 0;
    chResolution.FrmStartY = 0;
    chResolution.FrmCropWidth = 0;
    chResolution.FrmCropHeight = 0;
    chResolution.eDir = OMX_DirOutput;
    chResolution.nChId = 0;
    eErr = OMX_SetConfig(pAppData->m_pScHandle, (OMX_INDEXTYPE)OMX_TI_IndexConfigVidChResolution, &chResolution);
    if (eErr != OMX_ErrorNone) {
        printf("failed to set output channel resolution\n");
    }
    
    OMX_INIT_PARAM(&stAlgEnable);
    stAlgEnable.nPortIndex = 0;
    stAlgEnable.nChId = 0;
    stAlgEnable.bAlgBypass = 0;
    
    eErr = OMX_SetConfig(pAppData->m_pScHandle, (OMX_INDEXTYPE)OMX_TI_IndexConfigAlgEnable, &stAlgEnable);
    if (eErr != OMX_ErrorNone)
    {
        printf("failed to disable algo by pass mode\n");
    }
    
    return (eErr);
}

OMX_ERRORTYPE SetDecDisplayParams(DECODER *pAppData)
{
    OMX_ERRORTYPE eErr = OMX_ErrorNone;
    OMX_PARAM_BUFFER_MEMORYTYPE stMemTypeCfg;
    OMX_PARAM_PORTDEFINITIONTYPE stParamPort;
    //OMX_PARAM_VFPC_NUMCHANNELPERHANDLE stNumChPerHandle;
    OMX_PARAM_VFDC_DRIVERINSTID stDriverId;
    OMX_PARAM_VFDC_CREATEMOSAICLAYOUT stMosaicLayout;
    OMX_CONFIG_VFDC_MOSAICLAYOUT_PORT2WINMAP stPort2Winmap;
    
    OMX_INIT_PARAM(&stParamPort);
    stParamPort.nPortIndex = OMX_VFDC_INPUT_PORT_START_INDEX;
    OMX_GetParameter(pAppData->m_pDisHandle, OMX_IndexParamPortDefinition, &stParamPort);
    stParamPort.nPortIndex = OMX_VFDC_INPUT_PORT_START_INDEX;
    stParamPort.format.video.nFrameWidth = CROPPED_WIDTH;
    stParamPort.format.video.nFrameHeight = CROPPED_HEIGHT;
    stParamPort.format.video.nStride = ((CROPPED_WIDTH + 15) & 0xfffffff0) * 2;
    stParamPort.nBufferCountActual = DECODE_DISPLAY_INPUT_BUFFER_COUNT;
    stParamPort.format.video.eCompressionFormat = OMX_VIDEO_CodingUnused;
    stParamPort.format.video.eColorFormat = OMX_COLOR_FormatYCbYCr;
    stParamPort.nBufferSize = stParamPort.format.video.nStride * stParamPort.format.video.nFrameHeight;
      
    printf("Buffer Size computed: %d\n", (int)stParamPort.nBufferSize);
    printf("set input port params (width = %d, height = %d) \n", (int)pAppData->m_nWidth, (int)pAppData->m_nHeight);
      
    eErr = OMX_SetParameter(pAppData->m_pDisHandle, OMX_IndexParamPortDefinition, &stParamPort);
    if (eErr != OMX_ErrorNone)
    {
        printf("failed to set display params\n");
        OMX_FreeHandle(pAppData->m_pDisHandle);    
        return eErr;
    }

    OMX_INIT_PARAM(&stDriverId);
    stDriverId.nDrvInstID = OMX_VIDEO_DISPLAY_ID_HD0;
    stDriverId.eDispVencMode = OMX_DC_MODE_720P_60;

    eErr = OMX_SetParameter(pAppData->m_pDisHandle, (OMX_INDEXTYPE)OMX_TI_IndexParamVFDCDriverInstId, &stDriverId);
    if (eErr != OMX_ErrorNone)
    {
        printf("failed to set driver mode to 1080P@60\n");
    }
    
    OMX_INIT_PARAM(&stDriverId);
    stDriverId.nDrvInstID = OMX_VIDEO_DISPLAY_ID_HD0;
    stDriverId.eDispVencMode = OMX_DC_MODE_720P_60;
    eErr = OMX_SetParameter(pAppData->m_pCtrlHandle, (OMX_INDEXTYPE)OMX_TI_IndexParamVFDCDriverInstId, &stDriverId);
    if (eErr != OMX_ErrorNone)
    {
        printf("failed to set driver mode to 1080P@60\n");
    }

    OMX_INIT_PARAM (&stMosaicLayout);
    stMosaicLayout.sMosaicWinFmt[0].winStartX = (HD_WIDTH - CROPPED_WIDTH) / 2;
    stMosaicLayout.sMosaicWinFmt[0].winStartY = (HD_HEIGHT - CROPPED_HEIGHT) / 2;
    stMosaicLayout.sMosaicWinFmt[0].winWidth = CROPPED_WIDTH;
    stMosaicLayout.sMosaicWinFmt[0].winHeight = CROPPED_HEIGHT;
    stMosaicLayout.sMosaicWinFmt[0].pitch[VFDC_YUV_INT_ADDR_IDX] = ((CROPPED_WIDTH + 15) & 0xfffffff0) * 2;
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

    printf("setting input and output memory type to default\n");
    OMX_INIT_PARAM (&stMemTypeCfg);
    stMemTypeCfg.nPortIndex = OMX_VFPC_INPUT_PORT_START_INDEX;
    stMemTypeCfg.eBufMemoryType = OMX_BUFFER_MEMORY_DEFAULT;
    eErr = OMX_SetParameter(pAppData->m_pDisHandle, OMX_TI_IndexParamBuffMemType, &stMemTypeCfg);
    if (eErr != OMX_ErrorNone) {
        printf("failed to set memory Type at input port\n");
    }
    
    return (eErr);
}

static void InitDecoder(DECODER **pAppData)
{
    int nCnt;
    DECODER *pAppDataTemp;
    INPORT_PARAMS *pInPortParams;
    OUTPORT_PARAMS *pOutPortParams;

    pAppDataTemp = (DECODER *)malloc(sizeof(DECODER));
    memset(pAppDataTemp, 0x00, sizeof(DECODER));

    pAppDataTemp->m_nHeight = 720;
    pAppDataTemp->m_nWidth = 1280;
    pAppDataTemp->m_nFrameRate = 60;
   
    pAppDataTemp->m_pDecILComp = (COMPONENT_PRIVATE *)malloc(sizeof(COMPONENT_PRIVATE));
    memset(pAppDataTemp->m_pDecILComp, 0x00, sizeof (COMPONENT_PRIVATE));

    pAppDataTemp->m_pDecILComp->m_pEos = malloc(sizeof(semp_t));
    semp_init(pAppDataTemp->m_pDecILComp->m_pEos, 0);

    pAppDataTemp->m_pDecILComp->m_pDoneSem = malloc(sizeof(semp_t));
    semp_init(pAppDataTemp->m_pDecILComp->m_pDoneSem, 0);

    pAppDataTemp->m_pDecILComp->m_pPortSem = malloc(sizeof(semp_t));
    semp_init(pAppDataTemp->m_pDecILComp->m_pPortSem, 0);

    pAppDataTemp->m_pScILComp = (COMPONENT_PRIVATE *)malloc(sizeof(COMPONENT_PRIVATE));
    memset(pAppDataTemp->m_pScILComp, 0x00, sizeof(COMPONENT_PRIVATE));

    pAppDataTemp->m_pScILComp->m_pEos = malloc(sizeof(semp_t));
    semp_init(pAppDataTemp->m_pScILComp->m_pEos, 0);

    pAppDataTemp->m_pScILComp->m_pDoneSem = malloc(sizeof(semp_t));
    semp_init(pAppDataTemp->m_pScILComp->m_pDoneSem, 0);

    pAppDataTemp->m_pScILComp->m_pPortSem = malloc(sizeof(semp_t));
    semp_init(pAppDataTemp->m_pScILComp->m_pPortSem, 0);

    pAppDataTemp->m_pNfILComp = (COMPONENT_PRIVATE *)malloc(sizeof(COMPONENT_PRIVATE));
    memset(pAppDataTemp->m_pNfILComp, 0x00, sizeof(COMPONENT_PRIVATE));

    pAppDataTemp->m_pNfILComp->m_pEos = malloc(sizeof(semp_t));
    semp_init(pAppDataTemp->m_pNfILComp->m_pEos, 0);

    pAppDataTemp->m_pNfILComp->m_pDoneSem = malloc(sizeof(semp_t));
    semp_init(pAppDataTemp->m_pNfILComp->m_pDoneSem, 0);

    pAppDataTemp->m_pNfILComp->m_pPortSem = malloc(sizeof(semp_t));
    semp_init(pAppDataTemp->m_pNfILComp->m_pPortSem, 0);

    pAppDataTemp->m_pDisILComp = (COMPONENT_PRIVATE *)malloc(sizeof(COMPONENT_PRIVATE));
    memset(pAppDataTemp->m_pDisILComp, 0x00, sizeof(COMPONENT_PRIVATE));

    pAppDataTemp->m_pDisILComp->m_pEos = malloc(sizeof(semp_t));
    semp_init(pAppDataTemp->m_pDisILComp->m_pEos, 0);

    pAppDataTemp->m_pDisILComp->m_pDoneSem = malloc(sizeof(semp_t));
    semp_init(pAppDataTemp->m_pDisILComp->m_pDoneSem, 0);

    pAppDataTemp->m_pDisILComp->m_pPortSem = malloc(sizeof(semp_t));
    semp_init(pAppDataTemp->m_pDisILComp->m_pPortSem, 0);

    pAppDataTemp->m_pDecILComp->m_nInports = 1;
    pAppDataTemp->m_pDecILComp->m_nOutports = 1;
    pAppDataTemp->m_pDecILComp->m_nStartOutportIndex = 1;
    pAppDataTemp->m_pScILComp->m_nInports = 1;
    pAppDataTemp->m_pScILComp->m_nOutports = 1;

    pAppDataTemp->m_pScILComp->m_nStartOutportIndex = OMX_VFPC_NUM_INPUT_PORTS;
    pAppDataTemp->m_pNfILComp->m_nInports = 1;
    pAppDataTemp->m_pNfILComp->m_nOutports = 1;
    pAppDataTemp->m_pNfILComp->m_nStartOutportIndex = OMX_VFPC_NUM_INPUT_PORTS;
    pAppDataTemp->m_pDisILComp->m_nInports = 1;

    pAppDataTemp->m_pDisILComp->m_nOutports = 0;
    pAppDataTemp->m_pDisILComp->m_nStartOutportIndex = 0;

    pAppDataTemp->m_pDecILComp->m_pInPortParams = malloc(sizeof(INPORT_PARAMS) * pAppDataTemp->m_pDecILComp->m_nInports);
    memset(pAppDataTemp->m_pDecILComp->m_pInPortParams, 0x00, sizeof(INPORT_PARAMS));

    pAppDataTemp->m_pDecILComp->m_pOutPortParams = malloc(sizeof(OUTPORT_PARAMS) * pAppDataTemp->m_pDecILComp->m_nOutports);
    memset(pAppDataTemp->m_pDecILComp->m_pOutPortParams, 0x00, sizeof(OUTPORT_PARAMS));

    pAppDataTemp->m_pScILComp->m_pInPortParams = malloc(sizeof(INPORT_PARAMS) * pAppDataTemp->m_pScILComp->m_nInports);
    memset(pAppDataTemp->m_pScILComp->m_pInPortParams, 0x00, sizeof(INPORT_PARAMS));

    pAppDataTemp->m_pScILComp->m_pOutPortParams = malloc(sizeof(OUTPORT_PARAMS) * pAppDataTemp->m_pScILComp->m_nOutports);
    memset(pAppDataTemp->m_pScILComp->m_pOutPortParams, 0x00, sizeof(OUTPORT_PARAMS));

    pAppDataTemp->m_pNfILComp->m_pInPortParams = malloc(sizeof(INPORT_PARAMS) * pAppDataTemp->m_pNfILComp->m_nInports);
    memset(pAppDataTemp->m_pNfILComp->m_pInPortParams, 0x00, sizeof(INPORT_PARAMS));

    pAppDataTemp->m_pNfILComp->m_pOutPortParams = malloc(sizeof(OUTPORT_PARAMS) * pAppDataTemp->m_pNfILComp->m_nOutports);
    memset(pAppDataTemp->m_pNfILComp->m_pOutPortParams, 0x00, sizeof(OUTPORT_PARAMS));

    pAppDataTemp->m_pDisILComp->m_pInPortParams = malloc(sizeof(INPORT_PARAMS) * pAppDataTemp->m_pDisILComp->m_nInports);
    memset(pAppDataTemp->m_pDisILComp->m_pInPortParams, 0x00, sizeof(INPORT_PARAMS));

    for(nCnt = 0; nCnt < pAppDataTemp->m_pDecILComp->m_nInports; nCnt++)
    {
        pInPortParams = pAppDataTemp->m_pDecILComp->m_pInPortParams + nCnt;
        pInPortParams->m_nBufferCountActual = DECODER_INPUT_BUFFER_COUNT;
        pInPortParams->m_nBufferSize = pAppDataTemp->m_nHeight * pAppDataTemp->m_nWidth;
        pipe((int *)pInPortParams->m_aInPortBufPipe);
    }

    for (nCnt = 0; nCnt < pAppDataTemp->m_pDecILComp->m_nOutports; nCnt++)
    {
        pOutPortParams = pAppDataTemp->m_pDecILComp->m_pOutPortParams + nCnt;
        pOutPortParams->m_nBufferCountActual = DECODER_OUTPUT_BUFFER_COUNT;
        pOutPortParams->m_nBufferSize = (UTIL_ALIGN((pAppDataTemp->m_nWidth + (2 * PADX)), 128) * ((((pAppDataTemp->m_nHeight + 15) & 0xfffffff0) + (4 * PADY))) * 3) >> 1;
        pipe((int *)pOutPortParams->m_aOutPortBufPipe);
    }
    pipe((int *)pAppDataTemp->m_pDecILComp->m_aLocalPipe);

    for (nCnt = 0; nCnt < pAppDataTemp->m_pScILComp->m_nInports; nCnt++)
    {
        pInPortParams = pAppDataTemp->m_pScILComp->m_pInPortParams + nCnt;
        pInPortParams->m_nBufferCountActual = SCALAR_INPUT_BUFFER_COUNT;
        pInPortParams->m_nBufferSize = (UTIL_ALIGN ((pAppDataTemp->m_nWidth + (2 * PADX)), 128) * ((pAppDataTemp->m_nHeight + (4 * PADY))) * 3) >> 1;
        pipe ((int *) pInPortParams->m_aInPortBufPipe);
    }

    for (nCnt = 0; nCnt < pAppDataTemp->m_pScILComp->m_nOutports; nCnt++)
    {
        pOutPortParams = pAppDataTemp->m_pScILComp->m_pOutPortParams + nCnt;
        pOutPortParams->m_nBufferCountActual = SCALAR_OUTPUT_BUFFER_COUNT;
        pOutPortParams->m_nBufferSize = pAppDataTemp->m_nHeight * ((pAppDataTemp->m_nWidth + 15) & 0xfffffff0) * 2;
        pipe((int *)pOutPortParams->m_aOutPortBufPipe);
    }
    pipe((int *)pAppDataTemp->m_pScILComp->m_aLocalPipe);

    for (nCnt = 0; nCnt < pAppDataTemp->m_pNfILComp->m_nInports; nCnt++)
    {
        pInPortParams = pAppDataTemp->m_pNfILComp->m_pInPortParams + nCnt;
        pInPortParams->m_nBufferCountActual = NF_INPUT_BUFFER_COUNT;
        pInPortParams->m_nBufferSize = SD_DISPLAY_HEIGHT * SD_DISPLAY_WIDTH  * 2;
        pipe((int *)pInPortParams->m_aInPortBufPipe);
    }

    for (nCnt = 0; nCnt < pAppDataTemp->m_pNfILComp->m_nOutports; nCnt++)
    {
        pOutPortParams = pAppDataTemp->m_pNfILComp->m_pOutPortParams + nCnt;
        pOutPortParams->m_nBufferCountActual = NF_OUTPUT_BUFFER_COUNT;
        pOutPortParams->m_nBufferSize = (SD_DISPLAY_HEIGHT * SD_DISPLAY_WIDTH  * 3) >> 1;
        pipe((int *)pOutPortParams->m_aOutPortBufPipe);
    }
    pipe((int *)pAppDataTemp->m_pNfILComp->m_aLocalPipe);

    for (nCnt = 0; nCnt < pAppDataTemp->m_pDisILComp->m_nInports; nCnt++)
    {
        pInPortParams = pAppDataTemp->m_pDisILComp->m_pInPortParams + nCnt;
        pInPortParams->m_nBufferCountActual = DECODE_DISPLAY_INPUT_BUFFER_COUNT;
        pInPortParams->m_nBufferSize = pAppDataTemp->m_nHeight * pAppDataTemp->m_nWidth * 2;
        pipe((int *)pInPortParams->m_aInPortBufPipe);
    }
    pipe((int *)pAppDataTemp->m_pDisILComp->m_aLocalPipe);

    *pAppData = pAppDataTemp;
}

static void DeinitDecoder(DECODER *pAppData)
{
    int nCnt;
    INPORT_PARAMS *pInPortParams;
    OUTPORT_PARAMS *pOutPortParams;

    close((int)pAppData->m_pDisILComp->m_aLocalPipe);

    for (nCnt = 0; nCnt < pAppData->m_pDisILComp->m_nInports; nCnt++)
    {
        pInPortParams = pAppData->m_pDisILComp->m_pInPortParams + nCnt;
        close((int)pInPortParams->m_aInPortBufPipe);
    }
    close((int)pAppData->m_pScILComp->m_aLocalPipe);

    for (nCnt = 0; nCnt < pAppData->m_pScILComp->m_nInports; nCnt++)
    {
        pInPortParams = pAppData->m_pScILComp->m_pInPortParams + nCnt;
        close((int)pInPortParams->m_aInPortBufPipe);
    }

    for (nCnt = 0; nCnt < pAppData->m_pScILComp->m_nOutports; nCnt++)
    {
        pOutPortParams = pAppData->m_pScILComp->m_pOutPortParams + nCnt;
        close((int)pOutPortParams->m_aOutPortBufPipe);
    }
    close((int)pAppData->m_pNfILComp->m_aLocalPipe);

    for (nCnt = 0; nCnt < pAppData->m_pNfILComp->m_nInports; nCnt++)
    {
        pInPortParams = pAppData->m_pNfILComp->m_pInPortParams + nCnt;
        close((int)pInPortParams->m_aInPortBufPipe);
    }

    for (nCnt = 0; nCnt < pAppData->m_pNfILComp->m_nOutports; nCnt++)
    {
        pOutPortParams = pAppData->m_pNfILComp->m_pOutPortParams + nCnt;
        close((int)pOutPortParams->m_aOutPortBufPipe);
    }
    close((int)pAppData->m_pDecILComp->m_aLocalPipe);

    for (nCnt = 0; nCnt < pAppData->m_pDecILComp->m_nInports; nCnt++)
    {
        pInPortParams = pAppData->m_pDecILComp->m_pInPortParams + nCnt;
        close((int)pInPortParams->m_aInPortBufPipe);
    }

    for (nCnt = 0; nCnt < pAppData->m_pDecILComp->m_nOutports; nCnt++)
    {
        pOutPortParams = pAppData->m_pDecILComp->m_pOutPortParams + nCnt;
        close((int)pOutPortParams->m_aOutPortBufPipe);
    }

    free(pAppData->m_pDisILComp->m_pInPortParams);

    free(pAppData->m_pScILComp->m_pInPortParams);

    free(pAppData->m_pScILComp->m_pOutPortParams);

    free(pAppData->m_pNfILComp->m_pInPortParams);

    free(pAppData->m_pNfILComp->m_pOutPortParams);  

    free(pAppData->m_pDecILComp->m_pInPortParams);

    free(pAppData->m_pDecILComp->m_pOutPortParams);

    semp_deinit(pAppData->m_pScILComp->m_pEos);
    free(pAppData->m_pScILComp->m_pEos);

    semp_deinit(pAppData->m_pScILComp->m_pDoneSem);
    free(pAppData->m_pScILComp->m_pDoneSem);

    semp_deinit(pAppData->m_pScILComp->m_pPortSem);
    free(pAppData->m_pScILComp->m_pPortSem);

    semp_deinit(pAppData->m_pNfILComp->m_pEos);
    free(pAppData->m_pNfILComp->m_pEos);

    semp_deinit(pAppData->m_pNfILComp->m_pDoneSem);
    free(pAppData->m_pNfILComp->m_pDoneSem);

    semp_deinit(pAppData->m_pNfILComp->m_pPortSem);
    free(pAppData->m_pNfILComp->m_pPortSem);

    semp_deinit(pAppData->m_pDisILComp->m_pEos);
    free(pAppData->m_pDisILComp->m_pEos);

    semp_deinit(pAppData->m_pDisILComp->m_pDoneSem);
    free(pAppData->m_pDisILComp->m_pDoneSem);

    semp_deinit(pAppData->m_pDisILComp->m_pPortSem);
    free(pAppData->m_pDisILComp->m_pPortSem);

    semp_deinit(pAppData->m_pDecILComp->m_pEos);
    free(pAppData->m_pDecILComp->m_pEos);

    semp_deinit(pAppData->m_pDecILComp->m_pDoneSem);
    free(pAppData->m_pDecILComp->m_pDoneSem);

    semp_deinit(pAppData->m_pDecILComp->m_pPortSem);
    free(pAppData->m_pDecILComp->m_pPortSem);

    free(pAppData->m_pDecILComp);

    free(pAppData->m_pScILComp);

    free(pAppData->m_pNfILComp);

    free(pAppData->m_pDisILComp);

    if (pAppData->m_eCompressionFormat == OMX_VIDEO_CodingAVC) {
        if(pAppData->m_stParsingCtx.readBuf) {
            free(pAppData->m_stParsingCtx.readBuf);
        }
    }

    free(pAppData);
}

static unsigned int FillBitStreamData(DECODER *pAppData, OMX_BUFFERHEADERTYPE *pBuf)
{
    unsigned int nDataRead = 0;
    int nFrameSize = 0;
    COMPONENT_PRIVATE *pDecILComp = NULL;

    pDecILComp = ((DECODER *)pAppData)->m_pDecILComp;
    pAppData->m_stParsingCtx.outBuf.ptr = pBuf->pBuffer;
    pAppData->m_stParsingCtx.outBuf.bufsize = pDecILComp->m_pInPortParams->m_nBufferSize;
    pAppData->m_stParsingCtx.outBuf.bufused = 0;
    nFrameSize = GetNextFrameSize(&pAppData->m_stParsingCtx);
    nDataRead = nFrameSize;

    pBuf->nFilledLen = nFrameSize;

    return nFrameSize;
}

static OMX_ERRORTYPE InitialInputResources(DECODER *pAppdata)
{
    OMX_ERRORTYPE eErr = OMX_ErrorNone;
    unsigned int nCnt = 0;
    int nFrameSize = 0;
    COMPONENT_PRIVATE *pDecILComp = NULL;
    pDecILComp = ((DECODER *)pAppdata)->m_pDecILComp;

    for (nCnt = 0; nCnt < pDecILComp->m_pInPortParams->m_nBufferCountActual; nCnt++) {
        pAppdata->m_stParsingCtx.outBuf.ptr = pDecILComp->m_pInPortParams->m_pInPortBuff[nCnt]->pBuffer;
        pAppdata->m_stParsingCtx.outBuf.bufsize = pDecILComp->m_pInPortParams->m_nBufferSize;
        pAppdata->m_stParsingCtx.outBuf.bufused = 0;
        nFrameSize = GetNextFrameSize(&pAppdata->m_stParsingCtx);
        if (!nFrameSize) {
            break;
        }

        pDecILComp->m_pInPortParams->m_pInPortBuff[nCnt]->nFilledLen = nFrameSize;
        pDecILComp->m_pInPortParams->m_pInPortBuff[nCnt]->nOffset = 0;
        pDecILComp->m_pInPortParams->m_pInPortBuff[nCnt]->nAllocLen = nFrameSize;
        pDecILComp->m_pInPortParams->m_pInPortBuff[nCnt]->nInputPortIndex = 0;

        eErr = OMX_EmptyThisBuffer(pDecILComp->m_handle, pDecILComp->m_pInPortParams->m_pInPortBuff[nCnt]);
    }
    return eErr;
}

static void ReceiveTask (void *pArg)
{
    unsigned int nDataRead = 0;
    OMX_ERRORTYPE eErr = OMX_ErrorNone;
    COMPONENT_PRIVATE *pDecILComp = NULL;
    OMX_BUFFERHEADERTYPE *pBufferIn = NULL;
    UART_REQUEST_MESSAGE stReq;
    int nReadSize = 0, nDataSize = 0, nCnt = 0;
    unsigned char aCmd[10] = {0,}, *pData;

    pDecILComp = ((DECODER *)pArg)->m_pDecILComp;

    eErr = InitialInputResources(pArg);

#if 0
    memset(&stReq, 0x00, sizeof(UART_REQUEST_MESSAGE));
    stReq.m_nStx = UART_PROTOCOL_START_FLAG;
    stReq.m_nLength = sizeof(UART_REQUEST_MESSAGE);
    stReq.m_nType = UART_PROTOCOL_REQ_TYPE;
    stReq.m_nMsgId = UART_PROTOCOL_ID_START;
    stReq.m_nData = 0x01;
    stReq.m_nNumberOfCarId = 0;
    stReq.m_nCheckSum += (
                          stReq.m_nStx+
                          stReq.m_nLength+
                          stReq.m_nType+
                          stReq.m_nMsgId+
                          stReq.m_nData+
                          stReq.m_nNumberOfCarId
                         ) & 0xff;
    stReq.m_nEtx = UART_PROTOCOL_END_FLAG;
    WriteUart(g_stManager.m_nUart, &stReq, stReq.m_nLength);
    
    while (1) {
        ReadUart(g_stManager.m_nUart, g_stManager.m_aUartBuffer);
        if (g_stManager.m_aUartBuffer[3] == UART_PROTOCOL_ID_START)
            break;
    }
    
    recv(g_stManager.m_nAlgorithmResultSocket, aCmd, sizeof(aCmd), 0);
    if (strcmp(aCmd, "LDW") == 0) {
        printf("LDW Start\n");
        g_stManager.m_nAlgorithmMode = ALGORITHM_MODE_LDW;
    }
    else if (strcmp(aCmd, "FCW") == 0) {
        printf("FCW Start\n");
        g_stManager.m_nAlgorithmMode = ALGORITHM_MODE_FCW;
    }
    stReq.m_nMsgId = g_stManager.m_nAlgorithmMode;
    stReq.m_nCheckSum = (
                        stReq.m_nStx+
                        stReq.m_nLength+
                        stReq.m_nType+
                        stReq.m_nMsgId+
                        stReq.m_nData
                        ) & 0xff;
    WriteUart(g_stManager.m_nUart, &stReq, stReq.m_nLength);
#endif

    while (1)
    {
        read(pDecILComp->m_pInPortParams->m_aInPortBufPipe[0], &pBufferIn, sizeof(pBufferIn));

        nDataRead = FillBitStreamData(pArg, pBufferIn);
        if (g_stManager.m_bEnd) {        
            stReq.m_nStx = UART_PROTOCOL_START_FLAG;
            stReq.m_nLength = sizeof(UART_REQUEST_MESSAGE);
            stReq.m_nType = UART_PROTOCOL_REQ_TYPE;
            stReq.m_nMsgId = UART_PROTOCOL_ID_END;
            stReq.m_nData = 0x01;
            stReq.m_nCheckSum = (UART_PROTOCOL_START_FLAG+sizeof(UART_REQUEST_MESSAGE)+UART_PROTOCOL_REQ_TYPE+UART_PROTOCOL_ID_END+0x01) & 0xff;
            stReq.m_nEtx = UART_PROTOCOL_END_FLAG;
            WriteUart(g_stManager.m_nUart, &stReq, stReq.m_nLength);
            pBufferIn->nFlags |= OMX_BUFFERFLAG_EOS;
            eErr = OMX_EmptyThisBuffer(pDecILComp->m_handle, pBufferIn);
            pthread_exit(pDecILComp);
            break;
        }

        if (nDataRead > 0) {
            eErr = OMX_EmptyThisBuffer(pDecILComp->m_handle, pBufferIn);
            if (OMX_ErrorNone != eErr) {
                stReq.m_nStx = UART_PROTOCOL_START_FLAG;
                stReq.m_nLength = sizeof(UART_REQUEST_MESSAGE);
                stReq.m_nType = UART_PROTOCOL_REQ_TYPE;
                stReq.m_nMsgId = UART_PROTOCOL_ID_END;
                stReq.m_nData = 0x01;
                stReq.m_nCheckSum = (UART_PROTOCOL_START_FLAG+sizeof(UART_REQUEST_MESSAGE)+UART_PROTOCOL_REQ_TYPE+UART_PROTOCOL_ID_END+0x01) & 0xff;
                stReq.m_nEtx = UART_PROTOCOL_END_FLAG;
                WriteUart(g_stManager.m_nUart, &stReq, stReq.m_nLength);
                write(pDecILComp->m_pInPortParams->m_aInPortBufPipe[1], &pBufferIn, sizeof(pBufferIn));
                pthread_exit(pDecILComp);
                break;
            }
        }
        else
        {
            while (1)
            {
                if (g_stManager.m_bEnd) {                
                    stReq.m_nStx = UART_PROTOCOL_START_FLAG;
                    stReq.m_nLength = sizeof(UART_REQUEST_MESSAGE);
                    stReq.m_nType = UART_PROTOCOL_REQ_TYPE;
                    stReq.m_nMsgId = UART_PROTOCOL_ID_END;
                    stReq.m_nData = 0x01;
                    stReq.m_nCheckSum = (0xf2+sizeof(UART_REQUEST_MESSAGE)+0x01+0x54+0x01) & 0xff;
                    stReq.m_nEtx = UART_PROTOCOL_END_FLAG;
                    WriteUart(g_stManager.m_nUart, &stReq, stReq.m_nLength);
                    pBufferIn->nFlags |= OMX_BUFFERFLAG_EOS;
                    eErr = OMX_EmptyThisBuffer(pDecILComp->m_handle, pBufferIn);
                    pthread_exit(pDecILComp);
                    return;
                }

                usleep(50000);
            }
        }

#if 0
        if (g_stManager.m_nUartOffset == 0)
            memset(g_stManager.m_aUartBuffer, 0x00, 255);

        if (g_stManager.m_nAlgorithmMode == ALGORITHM_MODE_LDW) {

            while (58 - g_stManager.m_nUartOffset) {
                
                nReadSize = read(g_stManager.m_nUart, &g_stManager.m_aUartBuffer[g_stManager.m_nUartOffset], 58-g_stManager.m_nUartOffset);
                if (nReadSize > 0) {

                    g_stManager.m_nUartOffset += nReadSize;
                } else
                    break;
            }
            if (g_stManager.m_nUartOffset == 58)
                g_stManager.m_nUartOffset = 0;
        } else if (g_stManager.m_nAlgorithmMode == ALGORITHM_MODE_FCW) {

            while (110 - g_stManager.m_nUartOffset) {
                
                nReadSize = read(g_stManager.m_nUart, &g_stManager.m_aUartBuffer[g_stManager.m_nUartOffset], 110-g_stManager.m_nUartOffset);
                if (nReadSize > 0) {

                    g_stManager.m_nUartOffset += nReadSize;
                } else
                    break;
            }
            if (g_stManager.m_nUartOffset == 110)
                g_stManager.m_nUartOffset = 0;
        }
        
        if (g_stManager.m_aUartBuffer[0] == UART_PROTOCOL_START_FLAG &&
            ((g_stManager.m_aUartBuffer[3] == UART_PROTOCOL_ID_LDW && g_stManager.m_aUartBuffer[57] == UART_PROTOCOL_END_FLAG) ||
             (g_stManager.m_aUartBuffer[3] == UART_PROTOCOL_ID_FCW && g_stManager.m_aUartBuffer[109] == UART_PROTOCOL_END_FLAG))) {

            pData = &g_stManager.m_aUartBuffer[4];
            nDataSize = g_stManager.m_aUartBuffer[1]-6;
        
            memset(aCmd, 0x00, 10);
            if (g_stManager.m_nAlgorithmMode == ALGORITHM_MODE_LDW)
                strcpy(aCmd, "LDW");
            else
                strcpy(aCmd, "FCW");
            send(g_stManager.m_nAlgorithmResultSocket, aCmd, 10, 0);
            memset(aCmd, 0x00, 10);
            sprintf(aCmd, "%d", nDataSize);
            send(g_stManager.m_nAlgorithmResultSocket, aCmd, 10, 0);
            send(g_stManager.m_nAlgorithmResultSocket, pData, nDataSize, 0);
#if 1
            for (nCnt=0; nCnt<g_stManager.m_aUartBuffer[1]; nCnt++) {
                printf("%02x ", g_stManager.m_aUartBuffer[nCnt]);
            }
            printf("\n");
#endif
            printf("Algorithm Info Send : %d\t %d\n", nDataSize, g_stManager.m_nFrameCount);

        }
#endif
    }
}

void UartDecoderWriteTask(void *pArg)
{
    int nCnt = 0;
    unsigned char aBuffer[256] = {0,};

    while (!g_stManager.m_bEnd) {

#if 0
        memset(aBuffer, 0x00, 256);
        aBuffer[0] = UART_PROTOCOL_START_FLAG;
        aBuffer[1] = ReadEthernet(g_stManager.m_nCarInfoSocket, &aBuffer[4]);
        aBuffer[1]+= 6;
        aBuffer[2] = 0x00;
        aBuffer[3] = UART_PROTOCOL_ID_CAR;
        aBuffer[aBuffer[1]-1] = UART_PROTOCOL_END_FLAG;
        for (nCnt=0; nCnt<(aBuffer[1]-2); nCnt++)
            aBuffer[aBuffer[1]-2] += aBuffer[nCnt];
        aBuffer[aBuffer[1]-2] &= 0xff;
        WriteUart(g_stManager.m_nUart, &aBuffer[0], aBuffer[1]);
#endif
        usleep(100000);
    }
}

#if 0
void UartDecoderReadTask(void *pArg)
{
    int nReadSize = 0, nDataSize = 0;
    unsigned char aCmd[10] = {0,}, *pData;
    UART_REQUEST_MESSAGE stReq;

    /* ECU Power Enable Delay */
    usleep(1000000);

    memset(&stReq, 0x00, sizeof(UART_REQUEST_MESSAGE));
    stReq.m_nStx = UART_PROTOCOL_START_FLAG;
    stReq.m_nLength = sizeof(UART_REQUEST_MESSAGE);
    stReq.m_nType = UART_PROTOCOL_REQ_TYPE;
    stReq.m_nMsgId = UART_PROTOCOL_ID_START;
    stReq.m_nData = 0x01;
    stReq.m_nNumberOfCarId = 0;
    stReq.m_nCheckSum += (
                          stReq.m_nStx+
                          stReq.m_nLength+
                          stReq.m_nType+
                          stReq.m_nMsgId+
                          stReq.m_nData+
                          stReq.m_nNumberOfCarId
                         ) & 0xff;
    stReq.m_nEtx = UART_PROTOCOL_END_FLAG;
    WriteUart(g_stManager.m_nUart, &stReq, stReq.m_nLength);

    while (1) {
        ReadUart(g_stManager.m_nUart, g_stManager.m_aUartBuffer);
        if (g_stManager.m_aUartBuffer[3] == UART_PROTOCOL_ID_START)
            break;
    }

    recv(g_stManager.m_nAlgorithmResultSocket, aCmd, sizeof(aCmd), 0);
    if (strcmp(aCmd, "LDW") == 0) {
        printf("LDW Start\n");
        g_stManager.m_nAlgorithmMode = ALGORITHM_MODE_LDW;
    }
    else if (strcmp(aCmd, "FCW") == 0) {
        printf("FCW Start\n");
        g_stManager.m_nAlgorithmMode = ALGORITHM_MODE_FCW;
    }
    stReq.m_nMsgId = g_stManager.m_nAlgorithmMode;
    stReq.m_nCheckSum = (
                        stReq.m_nStx+
                        stReq.m_nLength+
                        stReq.m_nType+
                        stReq.m_nMsgId+
                        stReq.m_nData
                        ) & 0xff;
    WriteUart(g_stManager.m_nUart, &stReq, stReq.m_nLength);

    while (!g_stManager.m_bEnd) {

        memset(g_stManager.m_aUartBuffer, 0x00, 255);
        //nReadSize = read(g_stManager.m_nUart, g_stManager.m_aUartBuffer, 255);
        ReadUart(g_stManager.m_nUart, g_stManager.m_aUartBuffer);

        if (g_stManager.m_aUartBuffer[0] == UART_PROTOCOL_START_FLAG &&
            (g_stManager.m_aUartBuffer[3] == UART_PROTOCOL_ID_LDW || g_stManager.m_aUartBuffer[3] == UART_PROTOCOL_ID_FCW)) {

            pData = &g_stManager.m_aUartBuffer[4];
            nDataSize = g_stManager.m_aUartBuffer[1]-6;

            memset(aCmd, 0x00, 10);
            if (g_stManager.m_nAlgorithmMode == ALGORITHM_MODE_LDW)
                strcpy(aCmd, "LDW");
            else
                strcpy(aCmd, "FCW");
            send(g_stManager.m_nAlgorithmResultSocket, aCmd, 10, 0);
            memset(aCmd, 0x00, 10);
            sprintf(aCmd, "%d", nDataSize);
            send(g_stManager.m_nAlgorithmResultSocket, aCmd, 10, 0);
            send(g_stManager.m_nAlgorithmResultSocket, pData, nDataSize, 0);
            printf("Algorithm Info Send : %d\t %d\n", nDataSize, g_stManager.m_nFrameCount);
        }

        usleep(20000);
    }

    stReq.m_nStx = UART_PROTOCOL_START_FLAG;
    stReq.m_nLength = sizeof(UART_REQUEST_MESSAGE);
    stReq.m_nType = UART_PROTOCOL_REQ_TYPE;
    stReq.m_nMsgId = UART_PROTOCOL_ID_END;
    stReq.m_nData = 0x01;
    stReq.m_nCheckSum = (0xf2+sizeof(UART_REQUEST_MESSAGE)+0x01+0x54+0x01) & 0xff;
    stReq.m_nEtx = UART_PROTOCOL_END_FLAG;
    WriteUart(g_stManager.m_nUart, &stReq, stReq.m_nLength);
}
#endif

void StreamDecoder()
{
    OMX_U32 nCnt;
    OMX_S32 nRetValue;
    DECODER *pAppData = NULL;
    OMX_ERRORTYPE eErr = OMX_ErrorNone;
    PIPE_MSG stPipeMsg;

    g_stManager.m_bEnd = 0;

    /* Video Indicator */
    GpioExport(DECODER_VIDEO_INDICATOR);
    GpioSetDirection(DECODER_VIDEO_INDICATOR, 1);
    GpioSetValue(DECODER_VIDEO_INDICATOR, 0);

    /* Default setting */
    GpioExport(51);
    GpioExport(52);
    GpioSetDirection(51, 1);
    GpioSetDirection(52, 1);

    /* Decoder setting */
    GpioSetValue(51, 1);
    GpioSetValue(52, 0);

    /* LDWS Enable */
    GpioExport(48);
    GpioSetDirection(48, 1);
    GpioSetValue(48, 0);

    /* ECU Power on */
    ECUInit();
    ECUPowerOn();

    /* Display setting */
    system("echo 0 > /sys/devices/platform/vpss/display0/enabled"); 
    system("echo 0 > /sys/devices/platform/vpss/display1/enabled");    
    system("echo 74250,1280/10/10/80,720/1/1/5,1 > /sys/devices/platform/vpss/display0/timings");
    system("echo 74250,1280/10/10/80,720/1/1/5,1 > /sys/devices/platform/vpss/display1/timings");
    system("echo triple,rgb888,0/0/1/1 > /sys/devices/platform/vpss/display0/output");
    system("echo double,yuv422spuv,0/0/1/1 > /sys/devices/platform/vpss/display1/output");
    system("echo 5 > /sys/devices/platform/vpss/system/tiedvencs");
    system("echo 2:hdmi,dvo2 > /sys/devices/platform/vpss/graphics0/nodes");
    system("echo vcompmux:hdmi,dvo2 > /sys/devices/platform/vpss/video0/nodes");

    /* ECU Power Enable Delay */
    usleep(1000000);


    InitDecoder(&pAppData);    
    ParserInit(&pAppData->m_stParsingCtx);
    pAppData->m_pCb.EventHandler = CbEventHandler;
    pAppData->m_pCb.EmptyBufferDone = CbEmptyBufferDone;
    pAppData->m_pCb.FillBufferDone = CbFillBufferDone;

    eErr = OMX_GetHandle(&pAppData->m_pDecHandle, (OMX_STRING)"OMX.TI.DUCATI.VIDDEC", pAppData->m_pDecILComp, &pAppData->m_pCb);
    if ((eErr != OMX_ErrorNone) || (pAppData->m_pDecHandle == NULL))
    {
        goto EXIT;
    }
    pAppData->m_pDecILComp->m_handle = pAppData->m_pDecHandle;
    SetDecodeParams(pAppData);
    
    eErr = OMX_GetHandle (&pAppData->m_pScHandle, (OMX_STRING)"OMX.TI.VPSSM3.VFPC.INDTXSCWB", pAppData->m_pScILComp, &pAppData->m_pCb);
    if ((eErr != OMX_ErrorNone) || (pAppData->m_pScHandle == NULL))
    {
        goto EXIT;
    }
    pAppData->m_pScILComp->m_handle = pAppData->m_pScHandle;
    SetScalarParams(pAppData);
    
    OMX_SendCommand(pAppData->m_pScHandle, OMX_CommandPortEnable, OMX_VFPC_INPUT_PORT_START_INDEX, NULL);
    semp_pend(pAppData->m_pScILComp->m_pPortSem);
    
    OMX_SendCommand (pAppData->m_pScHandle, OMX_CommandPortEnable, OMX_VFPC_OUTPUT_PORT_START_INDEX, NULL);
    semp_pend(pAppData->m_pScILComp->m_pPortSem);
    
    eErr = OMX_GetHandle (&pAppData->m_pDisHandle, "OMX.TI.VPSSM3.VFDC", pAppData->m_pDisILComp, &pAppData->m_pCb);
    if (eErr != OMX_ErrorNone)
    {
        goto EXIT;
    }
    pAppData->m_pDisILComp->m_handle = pAppData->m_pDisHandle;
    g_stManager.m_pDispComp = pAppData->m_pDisILComp;

    eErr = OMX_GetHandle(&pAppData->m_pCtrlHandle, "OMX.TI.VPSSM3.CTRL.DC", pAppData->m_pDisILComp, &pAppData->m_pCb);
    if (eErr != OMX_ErrorNone)
    {
        goto EXIT;
    }
    eErr = SetDecDisplayParams(pAppData);

    OMX_SendCommand(pAppData->m_pDisHandle, OMX_CommandPortEnable, OMX_VFDC_INPUT_PORT_START_INDEX, NULL);
    semp_pend(pAppData->m_pDisILComp->m_pPortSem);
    
    ConnectComponents(pAppData->m_pDecILComp,
        OMX_VIDDEC_OUTPUT_PORT,
        pAppData->m_pScILComp,
        OMX_VFPC_INPUT_PORT_START_INDEX);
    
    ConnectComponents(pAppData->m_pScILComp,
        OMX_VFPC_OUTPUT_PORT_START_INDEX,
        pAppData->m_pDisILComp,
        OMX_VFDC_INPUT_PORT_START_INDEX);

    eErr = OMX_SendCommand(pAppData->m_pDecHandle, OMX_CommandStateSet, OMX_StateIdle, NULL);
    if (eErr != OMX_ErrorNone)
    {
        goto EXIT;
    }
    
    for (nCnt = 0; nCnt < pAppData->m_pDecILComp->m_pInPortParams->m_nBufferCountActual; nCnt++)
    {
        eErr = OMX_AllocateBuffer(pAppData->m_pDecHandle,
            &pAppData->m_pDecILComp->m_pInPortParams->m_pInPortBuff[nCnt],
            OMX_VIDDEC_INPUT_PORT,
            pAppData,
            pAppData->m_pDecILComp->m_pInPortParams->m_nBufferSize);
        if (eErr != OMX_ErrorNone)
        {
            goto EXIT;
        }
    }
    printf("SYNERGYS DEBUG : VIDDEC Input buffer allocated\n");
    
    for (nCnt = 0; nCnt < pAppData->m_pDecILComp->m_pOutPortParams->m_nBufferCountActual; nCnt++)
    {
        eErr = OMX_AllocateBuffer(pAppData->m_pDecHandle,
            &pAppData->m_pDecILComp->m_pOutPortParams->m_pOutPortBuff[nCnt],
            OMX_VIDDEC_OUTPUT_PORT,
            pAppData,
            pAppData->m_pDecILComp->m_pOutPortParams->m_nBufferSize);
        if (eErr != OMX_ErrorNone)
        {
            goto EXIT;
        }
    }
    printf("SYNERGYS DEBUG : VIDDEC Output buffer allocated\n");
    semp_pend(pAppData->m_pDecILComp->m_pDoneSem);

    eErr = OMX_SendCommand(pAppData->m_pScHandle, OMX_CommandStateSet, OMX_StateIdle, NULL);
    if (eErr != OMX_ErrorNone)
    {
        printf("omx send command error\n");
        goto EXIT;
    }
    
    for (nCnt = 0; nCnt < pAppData->m_pDecILComp->m_pOutPortParams->m_nBufferCountActual; nCnt++)
    {    
        eErr = OMX_UseBuffer(pAppData->m_pScHandle,
            &pAppData->m_pScILComp->m_pInPortParams->m_pInPortBuff[nCnt],
            OMX_VFPC_INPUT_PORT_START_INDEX,
            pAppData->m_pScILComp,
            pAppData->m_pDecILComp->m_pOutPortParams->m_nBufferSize,
            pAppData->m_pDecILComp->m_pOutPortParams->m_pOutPortBuff[nCnt]->pBuffer);
        if (eErr != OMX_ErrorNone)
        {
            printf("Use Buffer error\n");
            goto EXIT;
        }
    }
    printf("SYNERGYS DEBUG : VFPC.INDTXSCWB Input buffer used\n");

    for (nCnt = 0; nCnt < pAppData->m_pScILComp->m_pOutPortParams->m_nBufferCountActual; nCnt++)
    {
        eErr = OMX_AllocateBuffer(pAppData->m_pScHandle,
            &pAppData->m_pScILComp->m_pOutPortParams->m_pOutPortBuff[nCnt],
            OMX_VFPC_OUTPUT_PORT_START_INDEX,
            pAppData,
            pAppData->m_pScILComp->m_pOutPortParams->m_nBufferSize);
        if (eErr != OMX_ErrorNone)
        {
            printf("Alloc Buffer error\n");
            goto EXIT;
        }
    }
    printf("SYNERGYS DEBUG : VFPC.INDTXSCWB Output buffer allocated\n");
    semp_pend(pAppData->m_pScILComp->m_pDoneSem);

    eErr = OMX_SendCommand(pAppData->m_pCtrlHandle, OMX_CommandStateSet, OMX_StateIdle, NULL);
    if (eErr != OMX_ErrorNone)
    {
        goto EXIT;
    }    
    semp_pend(pAppData->m_pDisILComp->m_pDoneSem);

    eErr = OMX_SendCommand(pAppData->m_pDisHandle, OMX_CommandStateSet, OMX_StateIdle, NULL);
    if (eErr != OMX_ErrorNone)
    {
        goto EXIT;
    }

    for (nCnt = 0; nCnt < pAppData->m_pScILComp->m_pOutPortParams->m_nBufferCountActual; nCnt++)
    {
        eErr = OMX_UseBuffer(pAppData->m_pDisHandle,
            &pAppData->m_pDisILComp->m_pInPortParams->m_pInPortBuff[nCnt],
            OMX_VFDC_INPUT_PORT_START_INDEX,
            pAppData->m_pDisILComp,
            pAppData->m_pScILComp->m_pOutPortParams->m_nBufferSize,
            pAppData->m_pScILComp->m_pOutPortParams->m_pOutPortBuff[nCnt]->pBuffer);
        if (eErr != OMX_ErrorNone)
        {
            goto EXIT;
        }
    }
    semp_pend(pAppData->m_pDisILComp->m_pDoneSem);

    eErr = OMX_SendCommand(pAppData->m_pCtrlHandle, OMX_CommandStateSet, OMX_StateExecuting, NULL);
    if (eErr != OMX_ErrorNone)
    {
        goto EXIT;
    }
    semp_pend(pAppData->m_pDisILComp->m_pDoneSem);

    eErr = OMX_SendCommand(pAppData->m_pDisHandle, OMX_CommandStateSet, OMX_StateExecuting, NULL);
    if (eErr != OMX_ErrorNone)
    {
        goto EXIT;
    }
    semp_pend(pAppData->m_pDisILComp->m_pDoneSem);

    eErr = OMX_SendCommand(pAppData->m_pScHandle, OMX_CommandStateSet, OMX_StateExecuting, NULL);
    if (eErr != OMX_ErrorNone)
    {
        goto EXIT;
    }
    semp_pend(pAppData->m_pScILComp->m_pDoneSem);
    
    eErr = OMX_SendCommand(pAppData->m_pDecHandle, OMX_CommandStateSet, OMX_StateExecuting, NULL);
    if (eErr != OMX_ErrorNone)
    {
        goto EXIT;
    }
    semp_pend(pAppData->m_pDecILComp->m_pDoneSem);
    
    pthread_attr_init(&pAppData->m_pDecILComp->m_stThreadAttr);
    if (0 != pthread_create(&pAppData->m_pDecILComp->m_nInDataStrmThrdId,
        &pAppData->m_pDecILComp->m_stThreadAttr,
        (FcnPtr)ReceiveTask,
        pAppData))
    {
        goto EXIT;
    }

    pthread_attr_init(&pAppData->m_pDecILComp->m_stThreadAttr);
    if (0 != pthread_create(&pAppData->m_pDecILComp->m_nConnDataStrmThrdId,
        &pAppData->m_pDecILComp->m_stThreadAttr,
        (FcnPtr)ConnInConnOutTask,
        pAppData->m_pDecILComp)) {
        goto EXIT;
    }
    
    pthread_attr_init(&pAppData->m_pScILComp->m_stThreadAttr);
    if (0 != pthread_create(&pAppData->m_pScILComp->m_nConnDataStrmThrdId,
        &pAppData->m_pScILComp->m_stThreadAttr,
        (FcnPtr)ConnInConnOutTask,
        pAppData->m_pScILComp)) {
        goto EXIT;
    }
    
    pthread_attr_init(&pAppData->m_pDisILComp->m_stThreadAttr);
    if (0 != pthread_create(&pAppData->m_pDisILComp->m_nConnDataStrmThrdId,
        &pAppData->m_pDisILComp->m_stThreadAttr,
        (FcnPtr)ConnInConnOutTask,
        pAppData->m_pDisILComp)) {
        goto EXIT;
    }

    semp_pend(pAppData->m_pDisILComp->m_pEos);

    stPipeMsg.m_eCmd = PIPE_CMD_EXIT;

    write(pAppData->m_pDecILComp->m_aLocalPipe[1], &stPipeMsg, sizeof(PIPE_MSG));
    
    write(pAppData->m_pScILComp->m_aLocalPipe[1], &stPipeMsg, sizeof(PIPE_MSG));

    write(pAppData->m_pDisILComp->m_aLocalPipe[1], &stPipeMsg, sizeof(PIPE_MSG));
    
    eErr = OMX_SendCommand(pAppData->m_pDecHandle, OMX_CommandStateSet, OMX_StateIdle, NULL);
    if (eErr != OMX_ErrorNone) {
        goto EXIT;
    }
    semp_pend (pAppData->m_pDecILComp->m_pDoneSem);
    
    eErr = OMX_SendCommand(pAppData->m_pScHandle, OMX_CommandStateSet, OMX_StateIdle, NULL);
    if (eErr != OMX_ErrorNone) {
        goto EXIT;
    }
    semp_pend(pAppData->m_pScILComp->m_pDoneSem);
        
    eErr = OMX_SendCommand(pAppData->m_pDisHandle, OMX_CommandStateSet, OMX_StateIdle, NULL);
    if (eErr != OMX_ErrorNone) {
        goto EXIT;
    }
    semp_pend (pAppData->m_pDisILComp->m_pDoneSem);
    
    eErr = OMX_SendCommand(pAppData->m_pCtrlHandle, OMX_CommandStateSet, OMX_StateIdle, NULL);
    if (eErr != OMX_ErrorNone) {
        goto EXIT;
    }    
    semp_pend (pAppData->m_pDisILComp->m_pDoneSem);
    
    eErr = OMX_SendCommand(pAppData->m_pDecHandle, OMX_CommandStateSet, OMX_StateLoaded, NULL);
    if (eErr != OMX_ErrorNone) {
        goto EXIT;
    }

    for (nCnt = 0; nCnt < pAppData->m_pDecILComp->m_pInPortParams->m_nBufferCountActual; nCnt++) {
        eErr = OMX_FreeBuffer(pAppData->m_pDecHandle, OMX_VIDDEC_INPUT_PORT, pAppData->m_pDecILComp->m_pInPortParams->m_pInPortBuff[nCnt]);
        if (eErr != OMX_ErrorNone) {
            goto EXIT;
        }
    }
    
    for (nCnt = 0; nCnt < pAppData->m_pDecILComp->m_pOutPortParams->m_nBufferCountActual; nCnt++) {
        eErr = OMX_FreeBuffer(pAppData->m_pDecHandle, OMX_VIDDEC_OUTPUT_PORT, pAppData->m_pDecILComp->m_pOutPortParams->m_pOutPortBuff[nCnt]);
        if (eErr != OMX_ErrorNone) {
            goto EXIT;
        }
    }
    semp_pend(pAppData->m_pDecILComp->m_pDoneSem);
    
    eErr = OMX_SendCommand(pAppData->m_pScHandle, OMX_CommandStateSet, OMX_StateLoaded, NULL);
    if (eErr != OMX_ErrorNone) {
        goto EXIT;
    }

    for (nCnt = 0; nCnt < pAppData->m_pScILComp->m_pInPortParams->m_nBufferCountActual; nCnt++) {
        eErr = OMX_FreeBuffer(pAppData->m_pScHandle, OMX_VFPC_INPUT_PORT_START_INDEX, pAppData->m_pScILComp->m_pInPortParams->m_pInPortBuff[nCnt]);
        if (eErr != OMX_ErrorNone) {
            goto EXIT;
        }
    }
    
    for (nCnt = 0; nCnt < pAppData->m_pScILComp->m_pOutPortParams->m_nBufferCountActual; nCnt++)
    {
        eErr = OMX_FreeBuffer(pAppData->m_pScHandle, OMX_VFPC_OUTPUT_PORT_START_INDEX, pAppData->m_pScILComp->m_pOutPortParams->m_pOutPortBuff[nCnt]);
        if (eErr != OMX_ErrorNone)
        {
            goto EXIT;
        }
    }    
    semp_pend(pAppData->m_pScILComp->m_pDoneSem);
    
    eErr = OMX_SendCommand(pAppData->m_pDisHandle, OMX_CommandStateSet, OMX_StateLoaded, NULL);
    if (eErr != OMX_ErrorNone)
    {
        goto EXIT;
    }

    for (nCnt = 0; nCnt < pAppData->m_pDisILComp->m_pInPortParams->m_nBufferCountActual; nCnt++)
    {
        eErr = OMX_FreeBuffer(pAppData->m_pDisHandle, OMX_VFDC_INPUT_PORT_START_INDEX, pAppData->m_pDisILComp->m_pInPortParams->m_pInPortBuff[nCnt]);
        if (eErr != OMX_ErrorNone)
        {
            goto EXIT;
        }
    }
    semp_pend(pAppData->m_pDisILComp->m_pDoneSem);
        
    eErr = OMX_SendCommand(pAppData->m_pCtrlHandle, OMX_CommandStateSet, OMX_StateLoaded, NULL);
    if (eErr != OMX_ErrorNone)
    {
        goto EXIT;
    }    
    semp_pend(pAppData->m_pDisILComp->m_pDoneSem);
    
    eErr = OMX_FreeHandle(pAppData->m_pDecHandle);
    if ((eErr != OMX_ErrorNone))
    {
        goto EXIT;
    }
    
    eErr = OMX_FreeHandle(pAppData->m_pScHandle);
    if ((eErr != OMX_ErrorNone))
    {
        goto EXIT;
    }
    
    eErr = OMX_FreeHandle(pAppData->m_pDisHandle);
    if ((eErr != OMX_ErrorNone))
    {
        goto EXIT;
    }
    g_stManager.m_pDispComp = NULL;
    
    eErr = OMX_FreeHandle(pAppData->m_pCtrlHandle);
    if ((eErr != OMX_ErrorNone))
    {
        goto EXIT;
    }

    pthread_join(pAppData->m_pDecILComp->m_nInDataStrmThrdId, (void **) &nRetValue);
    pthread_join(pAppData->m_pDecILComp->m_nConnDataStrmThrdId, (void **) &nRetValue);
    pthread_join(pAppData->m_pScILComp->m_nConnDataStrmThrdId, (void **) &nRetValue);
    pthread_join(pAppData->m_pDisILComp->m_nConnDataStrmThrdId, (void **) &nRetValue);

    DeinitDecoder(pAppData);

EXIT:
    return;
}
#endif

