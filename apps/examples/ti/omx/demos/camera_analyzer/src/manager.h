
#ifndef __MANAGER_H__
#define __MANAGER_H__

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <stdio.h>
#include <pthread.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <netinet/in.h>
#include <netdb.h>
#include <termios.h>
#include <fcntl.h>
#include <errno.h>
#include <poll.h>
#include <linux/fb.h>
#include <xdc/std.h>
#include <xdc/runtime/System.h>
#include <xdc/runtime/knl/Thread.h>
#include <OMX_Types.h>
#include <OMX_Core.h>
#include <OMX_Component.h>
#include <OMX_TI_Common.h>
#include <OMX_Video.h>
#include <OMX_TI_Video.h>
#include <OMX_TI_Index.h>
#include <omx_venc.h>
#include <omx_vdec.h>
#include <omx_vfpc.h>
#include <omx_vfdc.h>
#include <omx_ctrl.h>
#include <omx_vfcc.h>
#include "msgq.h"
#include "semp.h"
#include "videodev2.h"
#include "ti81xxfb.h"
#include "ti81xxhdmi.h"
#include "ti81xxvin.h"

#define MAX_NUMBER_OF_OUT_BUFFERS           32

#define IPADDRESS                           "192.168.1.100"
#define ETHERNET_PORT_CONTROL               1500
#define ETHERNET_PORT_VIDEO                 1501
#define ETHERNET_PORT_ALGORITHM_RESULT      1502
#define ETHERNET_PORT_CAR_INFOMATION        1503

#define UART_DEV_NAME                       "/dev/ttyO0"
#define SYSFS_GPIO_DIR                      "/sys/class/gpio"

#define ENCODER_VIDEO_INDICATOR             58
#define DECODER_VIDEO_INDICATOR             59

#define CAPTURE_OUTPUT_BUFFER_COUNT         (12)
#define DEI_INPUT_BUFFER_COUNT              CAPTURE_OUTPUT_BUFFER_COUNT
#define DEI_OUTPUT_BUFFER_COUNT             (8)
#define ENC_INPUT_BUFFER_COUNT              (8)
#define ENC_OUTPUT_BUFFER_COUNT             (8)
#define ENCODE_DISPLAY_INPUT_BUFFER_COUNT   DEI_OUTPUT_BUFFER_COUNT
#define DECODER_INPUT_BUFFER_COUNT          (4)
#define DECODER_OUTPUT_BUFFER_COUNT         (6)
#define SCALAR_INPUT_BUFFER_COUNT           DECODER_OUTPUT_BUFFER_COUNT
#define SCALAR_OUTPUT_BUFFER_COUNT          (6)
#define NF_INPUT_BUFFER_COUNT               SCALAR_OUTPUT_BUFFER_COUNT
#define NF_OUTPUT_BUFFER_COUNT              SCALAR_OUTPUT_BUFFER_COUNT
#define DECODE_DISPLAY_INPUT_BUFFER_COUNT   SCALAR_OUTPUT_BUFFER_COUNT
#define MAX_FILE_NAME_SIZE                  256

#define UART_PROTOCOL_START_FLAG            0xf2
#define UART_PROTOCOL_END_FLAG              0xe2
#define UART_PROTOCOL_REQ_TYPE              0x01
#define UART_PROTOCOL_RES_TYPE              0x00
#define UART_PROTOCOL_ID_START              0x51
#define UART_PROTOCOL_ID_END                0x54
#define UART_PROTOCOL_ID_LDW                0x52
#define UART_PROTOCOL_ID_FCW                0x53
#define UART_PROTOCOL_ID_CAR                0x55

#define PROCESS_MODE_START                  0x01
#define PROCESS_MODE_PAUSE                  0x02
#define PROCESS_MODE_END                    0x03
#define PROCESS_MODE_ENCODER                0x10
#define PROCESS_MODE_DECODER                0x20

#define ALGORITHM_MODE_LDW                  UART_PROTOCOL_ID_LDW
#define ALGORITHM_MODE_FCW                  UART_PROTOCOL_ID_FCW

#define OMX_INIT_PARAM(param)                                                  \
        {                                                                      \
            memset ((param), 0, sizeof (*(param)));                           \
            (param)->nSize = sizeof (*(param));                                \
            (param)->nVersion.s.nVersionMajor = 1;                             \
            (param)->nVersion.s.nVersionMinor = 1;                             \
        }

typedef void *(*FcnPtr) (void *);

typedef struct COMPONENT_PRIVATE_T  COMPONENT_PRIVATE;

typedef struct SNT_CONNECT_T
{
    OMX_U32 m_nRemotePort;
    OMX_S32 m_aRemotePipe[2];           /* For making ETB / FTB calls to connected comp  */
    COMPONENT_PRIVATE *m_pRemoteClient; /* For allocate / use buffer */
} SNT_CONNECT;

typedef struct INPORT_PARAMS_T
{
    SNT_CONNECT m_stConnInfo;  
    OMX_S32 m_aInPortBufPipe[2];
    OMX_BUFFERHEADERTYPE *m_pInPortBuff[MAX_NUMBER_OF_OUT_BUFFERS];
    OMX_U32 m_nBufferCountActual;
    OMX_U32 m_nBufferSize;
} INPORT_PARAMS;

typedef struct OUTPORT_PARAMS_T
{
    SNT_CONNECT m_stConnInfo;
    OMX_S32 m_aOutPortBufPipe[2];
    OMX_BUFFERHEADERTYPE *m_pOutPortBuff[MAX_NUMBER_OF_OUT_BUFFERS];
    OMX_U32 m_nBufferCountActual;
    OMX_U32 m_nBufferSize;
} OUTPORT_PARAMS;

typedef enum 
{
    INPUT_PORT,
    OUTPUT_PORT,
    MAX_PORT = 0X7FFFFFFF
} PORT;

typedef enum 
{
    PIPE_CMD_ETB,
    PIPE_CMD_FTB,
    PIPE_CMD_EBD,
    PIPE_CMD_FBD,
    PIPE_CMD_EXIT,
    PIPE_CommandMax = 0X7FFFFFFF
} PIPE_CMD;

typedef struct PIPE_MSG_T
{
    PIPE_CMD m_eCmd;
    OMX_BUFFERHEADERTYPE *m_pBufferHeader;  /* used for EBD/FBD  */
    OMX_BUFFERHEADERTYPE m_stBufferHeader;  /* used for ETB/FTB */
} PIPE_MSG;

struct COMPONENT_PRIVATE_T
{
    INPORT_PARAMS *m_pInPortParams;
    OUTPORT_PARAMS *m_pOutPortParams;
    OMX_U32 m_nInports;
    OMX_U32 m_nOutports;
    OMX_U32 m_nStartOutportIndex;
    OMX_S32 m_aLocalPipe[2];
    OMX_HANDLETYPE m_handle;
    pthread_t m_nInDataStrmThrdId;
    pthread_t m_nOutDataStrmThrdId;
    pthread_t m_nConnDataStrmThrdId;
    semp_t *m_pDoneSem;
    semp_t *m_pEos;
    semp_t *m_pPortSem;
    pthread_attr_t m_stThreadAttr;
};

typedef struct UART_REQUEST_MESSAGE_T
{
    unsigned char m_nStx;
    unsigned char m_nLength;
    unsigned char m_nType;
    unsigned char m_nMsgId;
    unsigned char m_nData;
    unsigned char m_nNumberOfCarId;
    unsigned char m_aCarId[20];
    unsigned char m_nCheckSum;
    unsigned char m_nEtx;
} UART_REQUEST_MESSAGE;

typedef struct CONTROL_MESSAGE_T
{
    unsigned char m_cStx;
    /*
    Message ID
    0x01 : Start Process
    0x02 : Pause Process
    0x03 : End Process
    0x10 : Encoder Mode
    0x20 : Decoder Mode
    */
    unsigned char m_cMsgId;
    unsigned char m_cData;
    unsigned char m_cEtx;
} CONTROL_MESSAGE;

typedef struct MANAGER_T
{
    struct sockaddr_in m_stServerAddr;
    struct sockaddr_in m_stClientAddr;
    struct termios m_stTermIO;
    int m_nControlSocket;
    int m_nVideoSocket;
    int m_nAlgorithmResultSocket;
    int m_nCarInfoSocket;
    int m_nUart;
    int m_bPause;
    int m_bEnd;
    int m_nProcessMode;
    int m_nAlgorithmMode;
    int m_nFrameCount;
    int m_nUartOffset;
    unsigned char m_aUartBuffer[255];
    pthread_t m_nManagerThrdId;
    pthread_t m_nProcessThrdId;
    pthread_t m_nUartWriteThrdId;
    pthread_attr_t m_stThreadAttr;
    COMPONENT_PRIVATE *m_pDispComp;
} MANAGER;

extern MANAGER g_stManager;

void ECUInit();
void ECUPowerOn();
void ECUPowerOff();
int GpioExport(unsigned int nGpio);
int GpioUnexport(unsigned int nGpio);
int GpioSetDirection(unsigned int nGpio, unsigned int nOutFlag);
int GpioSetValue(unsigned int nGpio, unsigned int nValue);
int GpioGetValue(unsigned int nGpio, unsigned int *nValue);
int GpioSetEdge(unsigned int nGpio, char *cEdge);
void WriteUart(int nDev, unsigned char *pBuffer, unsigned int nBufferSize);
void ReadUart(int nDev, unsigned char *pBuffer);
void WriteEthernet(int nSocket, unsigned char *pBuffer, unsigned int nBufferSize);
int  ReadEthernet(int nSocket, unsigned char *pBuffer);
void SigIntHandler(int nSignal);
OMX_ERRORTYPE CbEventHandler(
    OMX_HANDLETYPE hComponent,
    OMX_PTR pAppData,
    OMX_EVENTTYPE eEvent,
    OMX_U32 nData1,
    OMX_U32 nData2, 
    OMX_PTR pEventData);
OMX_ERRORTYPE CbEmptyBufferDone(
    OMX_HANDLETYPE hComponent,
    OMX_PTR pAppData,
    OMX_BUFFERHEADERTYPE *pBuffer);
OMX_ERRORTYPE CbFillBufferDone(
    OMX_HANDLETYPE hComponent,
    OMX_PTR pAppData,
    OMX_BUFFERHEADERTYPE *pBuffer);
OMX_ERRORTYPE ConnectComponents(
    COMPONENT_PRIVATE *pCompPrivA,
    unsigned int nCompAPortOut,
    COMPONENT_PRIVATE *pCompPrivB,
    unsigned int nCompBPortIn);
void ConnInConnOutTask(void *pArg);
void ManagerTask(void *pArg);

#endif /* __MANAGER_H__ */

