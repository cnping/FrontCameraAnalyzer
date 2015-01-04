
#ifndef __ENCODER_H__
#define __ENCODER_H__

#include "manager.h"

typedef struct COMPONENT_V4L2_T
{
    int m_handle;
    struct v4l2_capability m_stCap;
    struct v4l2_format m_stFmt;
    struct v4l2_dv_preset m_stDvPreset;
    struct v4l2_requestbuffers m_stReqBuffers;
    int m_nBuffers;
    struct v4l2_buffer m_stBuffer;
    struct ti81xxvin_overflow_status m_stOverflowStatus;
} COMPONENT_V4L2;

typedef struct ENCODER_T
{

    OMX_HANDLETYPE m_pCapHandle, m_pDeiHandle, m_pDisHandle, m_pCtrlHandle, m_pEncHandle;
    OMX_COMPONENTTYPE *m_pComponent;
    OMX_CALLBACKTYPE m_pCb;
    OMX_STATETYPE m_eState;
    OMX_U8 m_eCompressionFormat;
    OMX_COLOR_FORMATTYPE m_eColorFormat;
    OMX_U32 m_nWidth;
    OMX_U32 m_nHeight;
    OMX_U32 m_nDecStride;
    OMX_U32 m_nFrameRate;
    OMX_U32 m_nBitRate;
    COMPONENT_PRIVATE *m_pEncILComp;
    COMPONENT_PRIVATE *m_pDeiILComp;
    COMPONENT_PRIVATE *m_pCapILComp;
    COMPONENT_PRIVATE *m_pDisILComp;
    COMPONENT_V4L2 m_stCaptComp;
    pthread_attr_t m_stThreadAttr;
} ENCODER;

#if 0
void UartEncoderReadTask(void *pArg);
#endif
void StreamEncoder();

#endif /* __ENCODER_H__ */

