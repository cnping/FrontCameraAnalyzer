
#include "manager.h"
#include "encoder.h"
#include "decoder.h"

MANAGER g_stManager;

static void InitUart();
static void DestroyUart();
#if 0
static void UartISR(int nStatus);
#endif
static void InitManager();
static void DestroyManager();
static OMX_ERRORTYPE InitialOutputResources(COMPONENT_PRIVATE *pComp);
static OMX_ERRORTYPE GetSelfBufHeader(COMPONENT_PRIVATE *pComp,
                                          OMX_U8 *pBuffer,
                                          PORT eType,
                                          OMX_U32 nPortIndex,
                                          OMX_BUFFERHEADERTYPE **pBufferOut);
static OMX_ERRORTYPE ProcessPipeCmdETB(COMPONENT_PRIVATE *pComp, PIPE_MSG *pPipeMsg);
static OMX_ERRORTYPE ProcessPipeCmdFTB(COMPONENT_PRIVATE *pComp, PIPE_MSG *pPipeMsg);
static OMX_ERRORTYPE ProcessPipeCmdEBD(COMPONENT_PRIVATE *pComp, PIPE_MSG *pPipeMsg);
static OMX_ERRORTYPE ProcessPipeCmdFBD(COMPONENT_PRIVATE *pComp, PIPE_MSG *pPipeMsg);
static void ProcessTask(void *pArg);

static void InitUart()
{
    struct termios stIO;
    struct sigaction stSAIO;
    sigset_t stSigSet;
    
    g_stManager.m_nUart = open(UART_DEV_NAME, O_RDWR | O_NOCTTY | O_NDELAY);
    if (g_stManager.m_nUart < 0) {
        printf("Uart Open Error\n");
        return;
    }

#if 0
    sigfillset(&stSigSet);
    sigprocmask(SIG_SETMASK, &stSigSet, NULL);
    sigdelset(&stSigSet, SIGIO);

    stSAIO.sa_handler = UartISR;
    stSAIO.sa_flags = 0;
    sigfillset(&stSAIO.sa_mask);
    sigaction(SIGIO, &stSAIO, NULL);

    fcntl(g_stManager.m_nUart, F_SETOWN, getpid());
    fcntl(g_stManager.m_nUart, F_SETFL, FASYNC);
    fcntl(g_stManager.m_nUart, F_SETFL, O_ASYNC ); 
#endif

    tcgetattr(g_stManager.m_nUart, &g_stManager.m_stTermIO);
    stIO.c_cflag = B115200|CS8|CLOCAL|CREAD;
    stIO.c_iflag = IGNPAR;
    stIO.c_oflag = 0;
    stIO.c_lflag = 0;
    stIO.c_cc[VTIME] = 0;
    stIO.c_cc[VMIN] = 0;
    tcflush(g_stManager.m_nUart, TCIFLUSH);
    tcsetattr(g_stManager.m_nUart, TCSANOW, &stIO);
}

static void DestroyUart()
{
    tcsetattr(g_stManager.m_nUart, TCSANOW, &g_stManager.m_stTermIO);
    if (g_stManager.m_nUart) {
        close(g_stManager.m_nUart);
        g_stManager.m_nUart = 0;
    }
}

#if 0
static void UartISR(int nStatus)
{
    char aCmd[10], *pData;
    int nSize = 0, nSocket = 0, nCnt = 0;
    UART_REQUEST_MESSAGE stReq;

    read(g_stManager.m_nUart, g_stManager.m_aUartBuffer, 1);

    if (g_stManager.m_aUartBuffer[0] == UART_PROTOCOL_START_FLAG) {

        read(g_stManager.m_nUart, &g_stManager.m_aUartBuffer[1], 1);

        if (g_stManager.m_aUartBuffer[0] <= 0) {
            printf("Fail\n");
            read(g_stManager.m_nUart, &g_stManager.m_aUartBuffer[1], 255);
            return;
        }

        for (nCnt = 0; nCnt<(g_stManager.m_aUartBuffer[1]-2); nCnt++) {
            read(g_stManager.m_nUart, &g_stManager.m_aUartBuffer[nCnt+2], 1);
        }
        pData = &g_stManager.m_aUartBuffer[4];
        nSize = g_stManager.m_aUartBuffer[1]-6;
        printf("Success\n");
    } else {

        printf("Fail\n");
        read(g_stManager.m_nUart, &g_stManager.m_aUartBuffer[1], 255);
        return;
    }

#if 1
    printf("Receive ");
    for (nCnt = 0; nCnt<g_stManager.m_aUartBuffer[1]; nCnt++) {
        
        printf("%02x ", g_stManager.m_aUartBuffer[nCnt]);
    }
    printf("\n");
#endif

#if 0
    nReadSize = read(g_stManager.m_nUart, g_stManager.m_aUartBuffer, 255);
    if (nReadSize <= 0)
        return;
    pData = &g_stManager.m_aUartBuffer[4];
    nSize = g_stManager.m_aUartBuffer[1]-6;

    if (g_stManager.m_nProcessMode == PROCESS_MODE_ENCODER && g_stManager.m_nCarInfoSocket) {

        pData = &g_stManager.m_aUartBuffer[4];
        nSize = g_stManager.m_aUartBuffer[1]-6;
        memset(aCmd, 0x00, 10);
        strcpy(aCmd, "CAN");
        send(g_stManager.m_nCarInfoSocket, aCmd, 10, 0);
        memset(aCmd, 0x00, 10);
        sprintf(aCmd, "%d", nSize);
        send(g_stManager.m_nCarInfoSocket, aCmd, 10, 0);
        send(g_stManager.m_nCarInfoSocket, pData, nSize, 0);
        printf("Car\n");
    }

    {
        int i;
        for (i=0; i<g_stManager.m_aUartBuffer[1]; i++) {
            printf("%02x ", g_stManager.m_aUartBuffer[i]);
        }
        printf("\n");
    }
#endif

    if (!g_stManager.m_bEnd && !g_stManager.m_bPause) {

        if (g_stManager.m_nProcessMode == PROCESS_MODE_ENCODER) {

            if (g_stManager.m_aUartBuffer[0] == UART_PROTOCOL_START_FLAG &&
                g_stManager.m_aUartBuffer[nSize+5] == UART_PROTOCOL_END_FLAG &&
                g_stManager.m_aUartBuffer[3] == UART_PROTOCOL_ID_CAR &&
                (g_stManager.m_nCarInfoSocket > 0)) {

                nSocket = g_stManager.m_nCarInfoSocket;

                memset(aCmd, 0x00, 10);
                strcpy(aCmd, "CAN");
                send(nSocket, aCmd, 10, 0);
                memset(aCmd, 0x00, 10);
                sprintf(aCmd, "%d", nSize);
                send(nSocket, aCmd, 10, 0);
                send(nSocket, pData, nSize, 0);
                printf("Can Send : %d\n", nSize);
            }
        } else {

            nSocket = g_stManager.m_nAlgorithmResultSocket;

            if (g_stManager.m_aUartBuffer[3] == UART_PROTOCOL_ID_LDW &&
                g_stManager.m_nAlgorithmMode == ALGORITHM_MODE_LDW &&
                (nSocket > 0)) {

                memset(aCmd, 0x00, 10);
                strcpy(aCmd, "LDW");
                send(nSocket, aCmd, 10, 0);
                memset(aCmd, 0x00, 10);
                sprintf(aCmd, "%d", nSize);
                send(nSocket, aCmd, 10, 0);
                send(nSocket, pData, nSize, 0);
                printf("Can Send : %d\n", nSize);
            } else if (g_stManager.m_aUartBuffer[3] == UART_PROTOCOL_ID_FCW &&
                       g_stManager.m_nAlgorithmMode == ALGORITHM_MODE_FCW &&
                       (nSocket > 0)) {
                
                memset(aCmd, 0x00, 10);
                strcpy(aCmd, "FCW");
                send(nSocket, aCmd, 10, 0);
                memset(aCmd, 0x00, 10);
                sprintf(aCmd, "%d", nSize);
                send(nSocket, aCmd, 10, 0);
                send(nSocket, pData, nSize, 0);
                printf("Can Send : %d\n", nSize);
            } else if (g_stManager.m_aUartBuffer[3] == UART_PROTOCOL_ID_START &&
                       g_stManager.m_aUartBuffer[2] == UART_PROTOCOL_RES_TYPE &&
                       (nSocket > 0)) {
                memset(&stReq, 0x00, sizeof(stReq));
                stReq.m_nStx = 0xf2;
                stReq.m_nLength = sizeof(UART_REQUEST_MESSAGE);
                stReq.m_nType = 0x01;
                stReq.m_nMsgId = g_stManager.m_nAlgorithmMode;
                stReq.m_nData = 0x01;
                stReq.m_nCheckSum = (0xf2+sizeof(UART_REQUEST_MESSAGE)+0x01+stReq.m_nMsgId+0x01) & 0xff;
                stReq.m_nEtx = 0xe2;
                WriteUart(g_stManager.m_nUart, &stReq, stReq.m_nLength);
            }
            memset(g_stManager.m_aUartBuffer, 0x00, 255);
        }
    }
}
#endif

static void InitManager()
{
    int nListenId, nClientSize;
    printf("InitManager\n");

    g_stManager.m_bEnd = 0;

    /* Control Socket */
    nListenId = socket(PF_INET, SOCK_STREAM, 0);
    if (nListenId < 0) {
        printf("Socket Create Error\n");
        return;
    }

    memset(&g_stManager.m_stServerAddr, 0x00, sizeof(g_stManager.m_stServerAddr));
    g_stManager.m_stServerAddr.sin_family = AF_INET;
    g_stManager.m_stServerAddr.sin_addr.s_addr = htonl(INADDR_ANY);
    g_stManager.m_stServerAddr.sin_port = htons(ETHERNET_PORT_CONTROL);
    if (bind(nListenId, (struct sockaddr *)&g_stManager.m_stServerAddr, sizeof(g_stManager.m_stServerAddr)) < 0) {
        printf("Bind Error\n");
        return;
    }

    if (listen(nListenId, SOMAXCONN) < 0) {
        printf("Listen Error\n");
        return;
    }

    nClientSize = sizeof(g_stManager.m_stClientAddr);
    g_stManager.m_nControlSocket = accept(nListenId, (struct sockaddr *)&g_stManager.m_stClientAddr, &nClientSize);
    if (g_stManager.m_nControlSocket < 0) {
        printf("Accept Error\n");
        return;
    }
    printf("Accept Control socket = %d\n", g_stManager.m_nControlSocket);
    close(nListenId);
    nListenId = 0;

    
    /* Video socket */
    nListenId = socket(PF_INET, SOCK_STREAM, 0);
    if (nListenId < 0) {
        printf("Socket create error\n");
        return;
    }
    printf("Socket create\n");

    memset(&g_stManager.m_stServerAddr, 0x00, sizeof(g_stManager.m_stServerAddr));  
    g_stManager.m_stServerAddr.sin_family = AF_INET;
    g_stManager.m_stServerAddr.sin_addr.s_addr = htonl(INADDR_ANY);
    g_stManager.m_stServerAddr.sin_port = htons(ETHERNET_PORT_VIDEO);
    if (bind(nListenId, (struct sockaddr *)&g_stManager.m_stServerAddr, sizeof(g_stManager.m_stServerAddr)) < 0) {
        printf("Bind error\n");
        return;
    }
    printf("Bind\n");

    if (listen(nListenId, SOMAXCONN) < 0) {
        printf("Listen error\n");
        return;
    }
    printf("Listen\n");

    nClientSize = sizeof(g_stManager.m_stClientAddr);
    g_stManager.m_nVideoSocket = accept(nListenId, (struct sockaddr *)&g_stManager.m_stClientAddr, &nClientSize);
    if (g_stManager.m_nVideoSocket < 0) {
        printf("Accept error\n");
        return;
    }
    printf("Accept Video socket = %d\n", g_stManager.m_nVideoSocket);
    close(nListenId);
    nListenId = 0;


    /* Algorithm Result socket */
    nListenId = socket(PF_INET, SOCK_STREAM, 0);
    if (nListenId < 0) {
        printf("Socket create error\n");
        return;
    }

    memset(&g_stManager.m_stServerAddr, 0x00, sizeof(g_stManager.m_stServerAddr));
    g_stManager.m_stServerAddr.sin_family = AF_INET;
    g_stManager.m_stServerAddr.sin_addr.s_addr = htonl(INADDR_ANY);
    g_stManager.m_stServerAddr.sin_port = htons(ETHERNET_PORT_ALGORITHM_RESULT);
    if (bind(nListenId, (struct sockaddr *)&g_stManager.m_stServerAddr, sizeof(g_stManager.m_stServerAddr)) < 0) {
        printf("Bind error\n");
        return;
    }
    printf("Bind\n");

    if (listen(nListenId, SOMAXCONN) < 0) {
        printf("Listen error\n");
        return;
    }
    printf("Listen\n");

    nClientSize = sizeof(g_stManager.m_stClientAddr);
    g_stManager.m_nAlgorithmResultSocket = accept(nListenId, (struct sockaddr *)&g_stManager.m_stClientAddr, &nClientSize);
    if (g_stManager.m_nAlgorithmResultSocket < 0) {
        printf("Accept error\n");
        return;
    }
    printf("Accept CAN socket = %d\n", g_stManager.m_nAlgorithmResultSocket);
    close(nListenId);
    nListenId = 0;
    

    /* Car Infomation socket */
    nListenId = socket(PF_INET, SOCK_STREAM, 0);
    if (nListenId < 0) {
        printf("Socket create error\n");
        return;
    }

    memset(&g_stManager.m_stServerAddr, 0x00, sizeof(g_stManager.m_stServerAddr));
    g_stManager.m_stServerAddr.sin_family = AF_INET;
    g_stManager.m_stServerAddr.sin_addr.s_addr = htonl(INADDR_ANY);
    g_stManager.m_stServerAddr.sin_port = htons(ETHERNET_PORT_CAR_INFOMATION);
    if (bind(nListenId, (struct sockaddr *)&g_stManager.m_stServerAddr, sizeof(g_stManager.m_stServerAddr)) < 0) {
        printf("Bind error\n");
        return;
    }
    printf("Bind\n");

    if (listen(nListenId, SOMAXCONN) < 0) {
        printf("Listen error\n");
        return;
    }
    printf("Listen\n");

    nClientSize = sizeof(g_stManager.m_stClientAddr);
    g_stManager.m_nCarInfoSocket = accept(nListenId, (struct sockaddr *)&g_stManager.m_stClientAddr, &nClientSize);
    if (g_stManager.m_nCarInfoSocket < 0) {
        printf("Accept error\n");
        return;
    }
    printf("Accept CAN socket = %d\n", g_stManager.m_nCarInfoSocket);
    close(nListenId);
    nListenId = 0;

    InitUart();
}

static void DestroyManager()
{
    int nRetValue;

    printf("DestroyManager\n");

#if 0
    if (g_stManager.m_nUartWriteThrdId)
        pthread_join(g_stManager.m_nUartWriteThrdId, (void **)&nRetValue);

    if (g_stManager.m_nUartReadThrdId)
        pthread_join(g_stManager.m_nUartReadThrdId, (void **)&nRetValue);
#endif

    if (g_stManager.m_nProcessThrdId)
        pthread_join(g_stManager.m_nProcessThrdId, (void **)&nRetValue);

    if (g_stManager.m_nUart)
        close(g_stManager.m_nUart);
    
    if (g_stManager.m_nCarInfoSocket)
        close(g_stManager.m_nCarInfoSocket);    

    if (g_stManager.m_nAlgorithmResultSocket)
        close(g_stManager.m_nAlgorithmResultSocket);

    if (g_stManager.m_nVideoSocket)
        close(g_stManager.m_nVideoSocket);

    if (g_stManager.m_nControlSocket)
        close(g_stManager.m_nControlSocket);

    g_stManager.m_nProcessMode = 0;
    g_stManager.m_bPause = 0;
    g_stManager.m_nProcessThrdId = 0;
    g_stManager.m_nUartWriteThrdId = 0;
    g_stManager.m_nUart = 0;
    g_stManager.m_nFrameCount = 0;
    g_stManager.m_nCarInfoSocket = 0;
    g_stManager.m_nAlgorithmResultSocket = 0;
    g_stManager.m_nVideoSocket = 0;
    g_stManager.m_nControlSocket = 0;
    g_stManager.m_nAlgorithmMode = 0;
    g_stManager.m_nUartOffset = 0;

    memset(g_stManager.m_aUartBuffer, 0x00, 255);
}

static OMX_ERRORTYPE InitialOutputResources(COMPONENT_PRIVATE *pComp)
{
    OMX_ERRORTYPE eErr = OMX_ErrorNone;
    unsigned int nCntI = 0, nCntJ;
    OUTPORT_PARAMS *pOutPortParams = NULL;
    OMX_PARAM_PORTDEFINITIONTYPE stParam;

    memset(&stParam, 0, sizeof(stParam));

    OMX_INIT_PARAM(&stParam);

    for (nCntJ = 0; nCntJ < pComp->m_nOutports; nCntJ++) {
        stParam.nPortIndex = nCntJ + pComp->m_nStartOutportIndex;
        OMX_GetParameter(pComp->m_handle, OMX_IndexParamPortDefinition, &stParam);
        pOutPortParams = pComp->m_pOutPortParams + nCntJ;

        if (OMX_TRUE == stParam.bEnabled) {
            if (pOutPortParams->m_stConnInfo.m_aRemotePipe[0] != 0) {
                for (nCntI = 0; nCntI < pComp->m_pOutPortParams->m_nBufferCountActual; nCntI++) {
                    eErr = OMX_FillThisBuffer(pComp->m_handle, pOutPortParams->m_pOutPortBuff[nCntI]);
                }
            }
        }
    }

    return eErr;
}

static OMX_ERRORTYPE GetSelfBufHeader(
    COMPONENT_PRIVATE *pComp,
    OMX_U8 *pBuffer,
    PORT eType,
    OMX_U32 nPortIndex,
    OMX_BUFFERHEADERTYPE **pBufferOut)
{
    int nCnt;
    INPORT_PARAMS *pInPortParams;
    OUTPORT_PARAMS *pOutPortParams;
    OMX_ERRORTYPE eErr = OMX_ErrorNone;

    if (eType == INPUT_PORT) {
        pInPortParams = pComp->m_pInPortParams + nPortIndex;
        for (nCnt = 0; nCnt < pInPortParams->m_nBufferCountActual; nCnt++) {
            if (pBuffer == pInPortParams->m_pInPortBuff[nCnt]->pBuffer) {
                *pBufferOut = pInPortParams->m_pInPortBuff[nCnt];
            }
        }
    } else {
        pOutPortParams = pComp->m_pOutPortParams + nPortIndex - pComp->m_nStartOutportIndex;
        for (nCnt = 0; nCnt < pOutPortParams->m_nBufferCountActual; nCnt++) {
            if (pBuffer == pOutPortParams->m_pOutPortBuff[nCnt]->pBuffer) {
                *pBufferOut = pOutPortParams->m_pOutPortBuff[nCnt];
            }
        }
  }

  return (eErr);
}

static OMX_ERRORTYPE ProcessPipeCmdETB(COMPONENT_PRIVATE *pComp, PIPE_MSG *pPipeMsg)
{
    OMX_ERRORTYPE eErr = OMX_ErrorNone;
    OMX_BUFFERHEADERTYPE *pBufferIn;
    unsigned int *pFrameCount = NULL;
    int nCnt;

    GetSelfBufHeader(pComp, pPipeMsg->m_stBufferHeader.pBuffer, INPUT_PORT, pPipeMsg->m_stBufferHeader.nInputPortIndex, &pBufferIn);

    pBufferIn->nFilledLen = pPipeMsg->m_stBufferHeader.nFilledLen;
    pBufferIn->nOffset = pPipeMsg->m_stBufferHeader.nOffset;
    pBufferIn->nTimeStamp = pPipeMsg->m_stBufferHeader.nTimeStamp;
    pBufferIn->nFlags = pPipeMsg->m_stBufferHeader.nFlags;
    pBufferIn->hMarkTargetComponent = pPipeMsg->m_stBufferHeader.hMarkTargetComponent;
    pBufferIn->pMarkData = pPipeMsg->m_stBufferHeader.pMarkData;
    pBufferIn->nTickCount = 0;

    if (g_stManager.m_nProcessMode == PROCESS_MODE_DECODER && pComp == g_stManager.m_pDispComp) {
        GpioSetValue(DECODER_VIDEO_INDICATOR, 0);
    }

    if (g_stManager.m_nProcessMode == PROCESS_MODE_DECODER && pComp == g_stManager.m_pDispComp) {
        //FILE *fOut = NULL;
        //char aFileName[256] = {0,};

        pFrameCount = pBufferIn->pBuffer;
        *pFrameCount = g_stManager.m_nFrameCount;
        printf("Frame Number : %d\n", *pFrameCount);

        //if (g_stManager.m_nFrameCount == 10) {
        //sprintf(aFileName, "frame%d.yuv", *pFrameCount);  
        //fOut = fopen(aFileName, "wb");
        //fwrite(pBufferIn->pBuffer, sizeof(char), pBufferIn->nFilledLen, fOut);
        //fclose(fOut);
        //}

        if (!(g_stManager.m_nFrameCount % 4)) {
            GpioSetValue(DECODER_VIDEO_INDICATOR, 1);
        }
        g_stManager.m_nFrameCount ++;
    }

    eErr = OMX_EmptyThisBuffer(pComp->m_handle, pBufferIn);

    return (eErr);
}

static OMX_ERRORTYPE ProcessPipeCmdFTB(COMPONENT_PRIVATE *pComp, PIPE_MSG *pPipeMsg)
{
    OMX_ERRORTYPE eErr = OMX_ErrorNone;
    OMX_BUFFERHEADERTYPE *pBufferOut;
    
    GetSelfBufHeader(pComp, pPipeMsg->m_stBufferHeader.pBuffer, OUTPUT_PORT, pPipeMsg->m_stBufferHeader.nOutputPortIndex, &pBufferOut);

    eErr = OMX_FillThisBuffer(pComp->m_handle, pBufferOut);

    return (eErr);
}

static OMX_ERRORTYPE ProcessPipeCmdEBD(COMPONENT_PRIVATE *pComp, PIPE_MSG *pPipeMsg)
{
    OMX_ERRORTYPE eErr = OMX_ErrorNone;
    OMX_BUFFERHEADERTYPE *pBufferIn;
    PIPE_MSG stRemotePipeMsg;
    INPORT_PARAMS *pInPortParams;
    int nRetValue = 0;

    pBufferIn = pPipeMsg->m_pBufferHeader;

    pInPortParams = pComp->m_pInPortParams + pBufferIn->nInputPortIndex;

    stRemotePipeMsg.m_eCmd = PIPE_CMD_FTB;
    stRemotePipeMsg.m_stBufferHeader.pBuffer = pBufferIn->pBuffer;
    stRemotePipeMsg.m_stBufferHeader.nOutputPortIndex = pInPortParams->m_stConnInfo.m_nRemotePort;

    nRetValue = write(pInPortParams->m_stConnInfo.m_aRemotePipe[1], &stRemotePipeMsg, sizeof(PIPE_MSG));
    if (sizeof(PIPE_MSG) != nRetValue) {
        printf("Error writing to remote Pipe!\n");
        eErr = OMX_ErrorNotReady;
        return eErr;
    }

    return (eErr);
}

static OMX_ERRORTYPE ProcessPipeCmdFBD(COMPONENT_PRIVATE *pComp, PIPE_MSG *pPipeMsg)
{
    OMX_ERRORTYPE eErr = OMX_ErrorNone;
    OMX_BUFFERHEADERTYPE *pBufferOut;
    PIPE_MSG stRemotePipeMsg;
    OUTPORT_PARAMS *pOutPortParams;
    int nRetValue = 0;

    pBufferOut = pPipeMsg->m_pBufferHeader;

    stRemotePipeMsg.m_eCmd = PIPE_CMD_ETB;
    stRemotePipeMsg.m_stBufferHeader.pBuffer = pBufferOut->pBuffer;

    pOutPortParams = pComp->m_pOutPortParams + (pBufferOut->nOutputPortIndex - pComp->m_nStartOutportIndex);

    stRemotePipeMsg.m_stBufferHeader.nFilledLen = pBufferOut->nFilledLen;
    stRemotePipeMsg.m_stBufferHeader.nOffset = pBufferOut->nOffset;
    stRemotePipeMsg.m_stBufferHeader.nTimeStamp = pBufferOut->nTimeStamp;
    stRemotePipeMsg.m_stBufferHeader.nFlags = pBufferOut->nFlags;
    stRemotePipeMsg.m_stBufferHeader.hMarkTargetComponent = pBufferOut->hMarkTargetComponent;
    stRemotePipeMsg.m_stBufferHeader.pMarkData = pBufferOut->pMarkData;
    stRemotePipeMsg.m_stBufferHeader.nTickCount = pBufferOut->nTickCount;
    stRemotePipeMsg.m_stBufferHeader.nInputPortIndex = pOutPortParams->m_stConnInfo.m_nRemotePort;

    nRetValue = write(pOutPortParams->m_stConnInfo.m_aRemotePipe[1], &stRemotePipeMsg, sizeof(PIPE_MSG));
    if (sizeof(PIPE_MSG) != nRetValue) {
        printf("Error writing to remote Pipe!\n");
        eErr = OMX_ErrorNotReady;
        return eErr;
    }

    return (eErr);
}

static void ProcessTask(void *pArg)
{
    MANAGER *pManager = (MANAGER *)pArg;

    switch (pManager->m_nProcessMode) {

    case PROCESS_MODE_ENCODER:
#if 0
        pthread_attr_init(&pManager->m_stThreadAttr);
        pthread_create(&pManager->m_nUartReadThrdId,
                                &pManager->m_stThreadAttr,
                                (FcnPtr)UartEncoderReadTask,
                                pManager);
#endif
        StreamEncoder();
        break;

    case PROCESS_MODE_DECODER:
#if 0
        pthread_attr_init(&pManager->m_stThreadAttr);
        pthread_create(&pManager->m_nUartReadThrdId,
                                &pManager->m_stThreadAttr,
                                (FcnPtr)UartDecoderWriteTask,
                                pManager);
        usleep(10000);
        pthread_attr_init(&pManager->m_stThreadAttr);
        pthread_create(&pManager->m_nUartReadThrdId,
                                &pManager->m_stThreadAttr,
                                (FcnPtr)UartDecoderReadTask,
                                pManager);
        usleep(10000);
#endif
        StreamDecoder();
        break;
    }
}

void ECUInit()
{
    GpioExport(60);
    GpioSetDirection(60, 1);

    ECUPowerOff();
}

void ECUPowerOn()
{
    GpioSetValue(60, 0);
}

void ECUPowerOff()
{
    GpioSetValue(60, 1);
}

int GpioExport(unsigned int nGpio)
{
    int nHandle, nLangth;
    char aBuffer[64];

    nHandle = open(SYSFS_GPIO_DIR "/export", O_WRONLY);
    if (nHandle < 0) {
        perror("gpio/export");
        return nHandle;
    }

    nLangth = snprintf(aBuffer, sizeof(aBuffer), "%d", nGpio);
    write(nHandle, aBuffer, nLangth);
    close(nHandle);

    return 0;
}

int GpioUnexport(unsigned int nGpio)
{
    int nHandle, nLangth;
    char aBuffer[64];

    nHandle = open(SYSFS_GPIO_DIR "/unexport", O_WRONLY);
    if (nHandle < 0) {
        perror("gpio/export");
        return nHandle;
    }

    nLangth = snprintf(aBuffer, sizeof(aBuffer), "%d", nGpio);
    write(nHandle, aBuffer, nLangth);
    close(nHandle);
    return 0;
}

int GpioSetDirection(unsigned int nGpio, unsigned int nOutFlag)
{
    int nHandle, nLangth;
    char aBuffer[64];

    nLangth = snprintf(aBuffer, sizeof(aBuffer), SYSFS_GPIO_DIR  "/gpio%d/direction", nGpio);

    nHandle = open(aBuffer, O_WRONLY);
    if (nHandle < 0) {
        perror("gpio/direction");
        return nHandle;
    }

    if (nOutFlag)
    write(nHandle, "out", 4);
    else
    write(nHandle, "in", 3);

    close(nHandle);
    return 0;
}

int GpioSetValue(unsigned int nGpio, unsigned int nValue)
{
    int nHandle, nLangth;
    char aBuffer[64];

    nLangth = snprintf(aBuffer, sizeof(aBuffer), SYSFS_GPIO_DIR "/gpio%d/value", nGpio);

    nHandle = open(aBuffer, O_WRONLY);
    if (nHandle < 0) {
        perror("gpio/set-value");
        return nHandle;
    }

    if (nValue)
        write(nHandle, "1", 2);
    else
        write(nHandle, "0", 2);

    close(nHandle);
    return 0;
}

int GpioGetValue(unsigned int nGpio, unsigned int *nValue)
{
    int nHandle, nLangth;
    char aBuffer[64];
    char cChar;

    nLangth = snprintf(aBuffer, sizeof(aBuffer), SYSFS_GPIO_DIR "/gpio%d/value", nGpio);

    nHandle = open(aBuffer, O_RDONLY);
    if (nHandle < 0) {
        perror("gpio/get-value");
        return nHandle;
    }

    read(nHandle, &cChar, 1);

    if (cChar != '0') {
        *nValue = 1;
    } else {
        *nValue = 0;
    }

    close(nHandle);
    return 0;
}

int GpioSetEdge(unsigned int nGpio, char *cEdge)
{
    int nHandle, nLangth;
    char aBuffer[64];

    nLangth = snprintf(aBuffer, sizeof(aBuffer), SYSFS_GPIO_DIR "/gpio%d/edge", nGpio);

    nHandle = open(aBuffer, O_WRONLY);
    if (nHandle < 0) {
        perror("gpio/set-edge");
        return nHandle;
    }

    write(nHandle, cEdge, strlen(cEdge) + 1); 
    close(nHandle);
    return 0;
}

void WriteUart(int nDev, unsigned char *pBuffer, unsigned int nBufferSize)
{
    int nCnt = 0;

    //printf("Send ");
    for (nCnt = 0; nCnt < nBufferSize; nCnt++) {
        write(nDev, &pBuffer[nCnt], 1);
        //printf("%x ", pBuffer[nCnt]);
    }
    //printf("\n");
}

void ReadUart(int nDev, unsigned char *pBuffer)
{
    int nCnt, nRecvSize = 0;

    nRecvSize = read(nDev, pBuffer, 1);
    if (pBuffer[0] == 0xf2) {
        read(nDev, &pBuffer[1], 1);
        for (nCnt=0; nCnt<pBuffer[1]-2; nCnt++) {

            read(nDev, &pBuffer[nCnt+2], 1);
        }

        printf("UART ID : %x\t SIZE : %d\n", pBuffer[3], pBuffer[1]);
#if 1
        for (nCnt=0; nCnt<pBuffer[1]; nCnt++) {
            printf("%02x ", pBuffer[nCnt]);
        }
        printf("\n");
#endif
    }
}

void WriteEthernet(int nSocket,unsigned char * pBuffer,unsigned int nBufferSize)
{
    unsigned char aCmd[10];

    memset(aCmd, 0x00, 10);
    if ((g_stManager.m_nFrameCount % 4) == 0)
        sprintf(aCmd, "1 %d", nBufferSize);
    else
        sprintf(aCmd, "0 %d", nBufferSize);
    send(nSocket, aCmd, 10, 0);
    send(nSocket, pBuffer, nBufferSize, 0);

    //printf("Video Frame : %d\n", g_stManager.m_nFrameCount);
    g_stManager.m_nFrameCount ++;
}

int ReadEthernet(int nSocket,unsigned char * pBuffer)
{
    int nRecvSize = 0, nDataSize = 0;

    nRecvSize= recv(nSocket, pBuffer, 10, 0);
    if (nRecvSize > 0 && nRecvSize <= 10) {
        int nOffset = 0;
        nDataSize = atoi(pBuffer);
        while (nDataSize - nOffset)
        {
            nRecvSize = recv(nSocket, pBuffer + nOffset, nDataSize - nOffset, 0);
            nOffset += nRecvSize;
        }

        return nDataSize;
    }

    return 0;
}

void SigIntHandler(int nSignal)
{
    g_stManager.m_bEnd = 1;
}

OMX_ERRORTYPE CbEventHandler(
    OMX_HANDLETYPE hComponent,
    OMX_PTR pAppData,
    OMX_EVENTTYPE eEvent,
    OMX_U32 nData1,
    OMX_U32 nData2, 
    OMX_PTR pEventData)
{
    COMPONENT_PRIVATE *pComp = (COMPONENT_PRIVATE *)pAppData;

    if (eEvent == OMX_EventCmdComplete) {
        if (nData1 == OMX_CommandStateSet) {
            printf("State changed to: ");
            switch ((int)nData2) {
            case OMX_StateInvalid:
                printf("OMX_StateInvalid \n");
                break;
            case OMX_StateLoaded:
                printf("OMX_StateLoaded \n");
                break;
            case OMX_StateIdle:
                printf("OMX_StateIdle \n");
                break;
            case OMX_StateExecuting:
                printf("OMX_StateExecuting \n");
                break;
            case OMX_StatePause:
                printf("OMX_StatePause\n");
                break;
            case OMX_StateWaitForResources:
                printf("OMX_StateWaitForResources\n");
                break;
            }
            semp_post(pComp->m_pDoneSem);
        } else if (OMX_CommandPortEnable || OMX_CommandPortDisable) {
            printf("Enable/Disable Event \n");
            semp_post(pComp->m_pPortSem);
        }
    } else if (eEvent == OMX_EventBufferFlag) {
        printf("OMX_EventBufferFlag \n");
        if ((int)nData2 == OMX_BUFFERFLAG_EOS) {
            printf("got EOS event \n");
            semp_post(pComp->m_pEos);
        }
    } else if (eEvent == OMX_EventError) {
        printf("Press a key to proceed\n");
    } else {
        printf("unhandled event, param1 = %nCntI, param2 = %nCntI \n", (int)nData1, (int)nData2);
    }

    return OMX_ErrorNone;
}

OMX_ERRORTYPE CbEmptyBufferDone(
    OMX_HANDLETYPE hComponent,
    OMX_PTR pAppData,
    OMX_BUFFERHEADERTYPE *pBuffer)
{
    COMPONENT_PRIVATE *pComp = (COMPONENT_PRIVATE *)pAppData;
    INPORT_PARAMS *pInPortParams;
    PIPE_MSG stPipeMsg;
    OMX_ERRORTYPE eErr = OMX_ErrorNone;
    int nRetValue = 0;

    pInPortParams = pComp->m_pInPortParams + pBuffer->nInputPortIndex;
    if (pInPortParams->m_stConnInfo.m_aRemotePipe[0] == NULL) {
        nRetValue = write(pInPortParams->m_aInPortBufPipe[1], &pBuffer, sizeof(pBuffer));

        if (sizeof(pBuffer) != nRetValue) {
            eErr = OMX_ErrorNotReady;
            return eErr;
        }
    } else {
        stPipeMsg.m_eCmd = PIPE_CMD_EBD;
        stPipeMsg.m_pBufferHeader = pBuffer;
        nRetValue = write(pComp->m_aLocalPipe[1], &stPipeMsg, sizeof(PIPE_MSG));
        if (sizeof(PIPE_MSG) != nRetValue) {
            eErr = OMX_ErrorNotReady;
            return eErr;
        }
    }

    return eErr;
}

OMX_ERRORTYPE CbFillBufferDone(
    OMX_HANDLETYPE hComponent,
    OMX_PTR pAppData,
    OMX_BUFFERHEADERTYPE *pBuffer)
{
    COMPONENT_PRIVATE *pComp = (COMPONENT_PRIVATE *)pAppData;
    OUTPORT_PARAMS *pOutPortParams;
    PIPE_MSG stPipeMsg;
    OMX_ERRORTYPE eErr = OMX_ErrorNone;
    int nRetValue = 0;

    pOutPortParams = pComp->m_pOutPortParams + (pBuffer->nOutputPortIndex - pComp->m_nStartOutportIndex);
    if (pOutPortParams->m_stConnInfo.m_aRemotePipe[0] == NULL) {
        nRetValue = write(pOutPortParams->m_aOutPortBufPipe[1], &pBuffer, sizeof(pBuffer));
        if (sizeof(pBuffer) != nRetValue) {
            eErr = OMX_ErrorNotReady;
            return eErr;
        }
    } else {
        stPipeMsg.m_eCmd = PIPE_CMD_FBD;
        stPipeMsg.m_pBufferHeader = pBuffer;
        nRetValue = write(pComp->m_aLocalPipe[1], &stPipeMsg, sizeof(PIPE_MSG));
        if (sizeof(PIPE_MSG) != nRetValue) {
            eErr = OMX_ErrorNotReady;
            return eErr;
        }
    }
    return eErr;
}

OMX_ERRORTYPE ConnectComponents(
    COMPONENT_PRIVATE *pCompPrivA,
    unsigned int nCompAPortOut,
    COMPONENT_PRIVATE *pCompPrivB,
    unsigned int nCompBPortIn)
{
    OMX_ERRORTYPE eErr = OMX_ErrorNone;
    OUTPORT_PARAMS *pOutPortParams = NULL;
    INPORT_PARAMS *pInPortParams = NULL;

    pOutPortParams = pCompPrivA->m_pOutPortParams + nCompAPortOut - pCompPrivA->m_nStartOutportIndex;
    pInPortParams = pCompPrivB->m_pInPortParams + nCompBPortIn;

    pInPortParams->m_stConnInfo.m_pRemoteClient = pCompPrivA;
    pInPortParams->m_stConnInfo.m_nRemotePort = nCompAPortOut;
    pInPortParams->m_stConnInfo.m_aRemotePipe[0] = pCompPrivA->m_aLocalPipe[0];
    pInPortParams->m_stConnInfo.m_aRemotePipe[1] = pCompPrivA->m_aLocalPipe[1];

    pOutPortParams->m_stConnInfo.m_pRemoteClient = pCompPrivB;
    pOutPortParams->m_stConnInfo.m_nRemotePort = nCompBPortIn;
    pOutPortParams->m_stConnInfo.m_aRemotePipe[0] = pCompPrivB->m_aLocalPipe[0];
    pOutPortParams->m_stConnInfo.m_aRemotePipe[1] = pCompPrivB->m_aLocalPipe[1];

  return eErr;
}

void ConnInConnOutTask(void *pArg)
{
    PIPE_MSG stPipeMsg;
    COMPONENT_PRIVATE *pComp = (COMPONENT_PRIVATE *)pArg;
    OMX_ERRORTYPE eErr = OMX_ErrorNone;

    InitialOutputResources(pComp);

    for (;;) {
        read(pComp->m_aLocalPipe[0], &stPipeMsg, sizeof(PIPE_MSG));

        switch (stPipeMsg.m_eCmd) {
        case PIPE_CMD_EXIT:
            printf("exiting thread\n");
            pthread_exit(pComp);
            break;
        case PIPE_CMD_ETB:
            eErr = ProcessPipeCmdETB(pComp, &stPipeMsg);
            if (OMX_ErrorNone != eErr) {
                write(pComp->m_aLocalPipe[1], &stPipeMsg, sizeof(PIPE_MSG));
                printf("ETB: wait \n");
                pthread_exit(pComp);
            }
            break;
        case PIPE_CMD_FTB:
            eErr = ProcessPipeCmdFTB(pComp, &stPipeMsg);
            if (OMX_ErrorNone != eErr) {
                write(pComp->m_aLocalPipe[1], &stPipeMsg, sizeof(PIPE_MSG));
                printf("FTB: wait \n");
                pthread_exit(pComp);
            }
            break;
        case PIPE_CMD_EBD:
            ProcessPipeCmdEBD(pComp, &stPipeMsg);
            break;
        case PIPE_CMD_FBD:
            ProcessPipeCmdFBD(pComp, &stPipeMsg);
            break;
        default:
            break;
        }
    }
}

void ManagerTask(void *pArg)
{
    MANAGER *pManager = (MANAGER *)pArg;
    CONTROL_MESSAGE stMsg;

    memset(&stMsg, 0x00, sizeof(CONTROL_MESSAGE));

    InitManager();

    while (1)
    {
        recv(pManager->m_nControlSocket, &stMsg, sizeof(CONTROL_MESSAGE), 0);
        if (stMsg.m_cStx = 0xAA && stMsg.m_cEtx == 0xFF)
        {
            switch (stMsg.m_cMsgId)
            {
            case PROCESS_MODE_START:
                pthread_attr_init(&pManager->m_stThreadAttr);
                pthread_create(&pManager->m_nProcessThrdId, &pManager->m_stThreadAttr, (FcnPtr)ProcessTask, pManager);
                printf("Start Process\n");
                break;

            case PROCESS_MODE_PAUSE:
                pManager->m_bPause = 1;
                printf("Pause Process\n");
                break;

            case PROCESS_MODE_END:
                pManager->m_bEnd = 1;
                usleep(1000000);
                DestroyManager();
                ECUPowerOff();
                InitManager();
                printf("End Process\n");
                break;

            /* Encoder Mode */
            case PROCESS_MODE_ENCODER:
                pManager->m_nProcessMode = stMsg.m_cMsgId;
                printf("Encoder Process\n");
                break;

            /* Decoder Mode */
            case PROCESS_MODE_DECODER:
                pManager->m_nProcessMode = stMsg.m_cMsgId;
                printf("Decoder Process\n");
                break;
            }
        }
        usleep(500000);
    }

    DestroyUart();
}

