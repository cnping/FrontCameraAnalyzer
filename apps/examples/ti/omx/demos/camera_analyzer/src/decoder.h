
#ifndef __DECODER_H__
#define __DECODER_H__

#include "manager.h"

#define UTIL_ALIGN(a,b) ((((uint32_t)(a)) + (b)-1) & (~((uint32_t)((b)-1))))

#define HD_WIDTH        (1280)
#define HD_HEIGHT       (720)

#define CROP_START_X    0
#define CROP_START_Y    0

#define SD_DISPLAY_WIDTH    ((720 + 31) & 0xffffffe0)
#define SD_DISPLAY_HEIGHT   ((480 + 31) & 0xffffffe0)

#define PARSER_SUCCESS                 (0)
#define PARSER_ERR_NOT_SUPPORTED       (1)
#define PARSER_ERR_MALLOC_FAILED       (2)
#define PARSER_ERR_INTERNAL_ERROR      (3)
#define PARSER_ERR_INVALID_ARGS        (4)
#define PARSER_ERR_NO_OUTPUT_CHANNEL   (5)
#define PARSER_ERR_WRONG_STATE         (6)
#define PARSER_ERR_INSUFFICIENT_INPUT  (7)

#define H264_NAL_ACCESS_UNIT_CODEDSLICE_CODE_FOR_NONIDR     0x01
#define H264_NAL_ACCESS_UNIT_CODEDSLICE_CODE_FOR_IDR        0x05
#define H264_PPS_START_CODE                                 0x08
#define H264_SPS_START_CODE                                 0x07
#define H264_WORKING_WORD_INIT                              0xFFFFFFFF

#define READSIZE                        (1*1024*1024)
#define CHUNKSIZE                       (1*1024*1024)
#define CHUNK_TO_READ                   (64*16)

enum
{
    H264_ST_LOOKING_FOR_SPS,
    H264_ST_LOOKING_FOR_ZERO_SLICE,
    H264_ST_INSIDE_PICTURE,
    H264_ST_STREAM_ERR,
    H264_ST_HOLDING_SC
};

typedef struct
{
  unsigned char *ptr;
  int bufsize;
  int bufused;
} AVChunk_Buf;

typedef struct
{
  unsigned int workingWord;
  unsigned char fifthByte;
  unsigned char state;
} H264_ChunkingCtx;

typedef struct
{
  unsigned int frameNo;
  unsigned int frameSize;
  unsigned char *readBuf;
  unsigned char *chunkBuf;
  H264_ChunkingCtx ctx;
  AVChunk_Buf inBuf;
  AVChunk_Buf outBuf;
  unsigned int bytes;
  unsigned int tmp;
  unsigned char firstParse;
  unsigned int chunkCnt;
} H264_ParsingCtx;

typedef struct DECODER_T
{
    OMX_HANDLETYPE m_pDecHandle, m_pScHandle, m_pDisHandle, m_pCtrlHandle, m_pNfHandle;
    OMX_COMPONENTTYPE *m_pComponent;
    OMX_CALLBACKTYPE m_pCb;
    OMX_STATETYPE m_eState;
    OMX_U8 m_eCompressionFormat;
    OMX_VIDEO_PARAM_AVCTYPE *m_pH264;
    OMX_COLOR_FORMATTYPE m_eColorFormat;
    OMX_U32 m_nWidth;
    OMX_U32 m_nHeight;
    OMX_U32 m_nDecStride;
    OMX_U32 m_nFrameRate;
    H264_ParsingCtx m_stParsingCtx;
    COMPONENT_PRIVATE *m_pDecILComp;
    COMPONENT_PRIVATE *m_pScILComp;
    COMPONENT_PRIVATE *m_pDisILComp;
    COMPONENT_PRIVATE *m_pNfILComp;
    pthread_attr_t m_stThreadAttr;
} DECODER;

void UartDecoderWriteTask(void *pArg);
#if 0
void UartDecoderReadTask(void *pArg);
#endif
void StreamDecoder();

#endif /* __DECODER_H__ */

