
#include "manager.h"

int main(int nArg, char *pArg[])
{
    ConfigureUIA stCfg;

    printf("\nCamera Analyzer Application\n\n");

    system("ifconfig eth0 192.168.1.100 netmask 255.255.255.0");
    memset(&g_stManager, 0x00, sizeof(MANAGER));

    signal(SIGINT, SigIntHandler);

    OMX_Init();

    stCfg.enableAnalysisEvents = 0;
    stCfg.enableStatusLogger = 1;
    stCfg.debugLevel = OMX_DEBUG_LEVEL1;
    configureUiaLoggerClient(2, &stCfg);
    configureUiaLoggerClient(1, &stCfg);

    pthread_attr_init(&g_stManager.m_stThreadAttr);
    pthread_create(&g_stManager.m_nManagerThrdId, &g_stManager.m_stThreadAttr, (FcnPtr)ManagerTask, &g_stManager);

    while (1) {
    
        usleep(100000);
    }

    OMX_Deinit();

    exit(0);
}

