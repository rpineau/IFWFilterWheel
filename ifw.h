//
//  ifw.h
//  IFW FilterWheel
//
//  Created by Rodolphe Pineau on 2020/09/15.
//  Copyright © 2020 RTI-Zone. All rights reserved.
//

#ifndef ifw_h
#define ifw_h
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#include <memory.h>
#include <time.h>
#ifdef SB_MAC_BUILD
#include <unistd.h>
#endif

#include <string>


#include "../../licensedinterfaces/sberrorx.h"
#include "../../licensedinterfaces/serxinterface.h"
#include "../../licensedinterfaces/sleeperinterface.h"

#define PLUGIN_DEBUG 3

#define DRIVER_VERSION      1.0

#define SERIAL_BUFFER_SIZE 256
#define MAX_TIMEOUT 1000            // in miliseconds
#define MAX_FILTER_CHANGE_TIMEOUT 25 // in seconds
#define LOG_BUFFER_SIZE 256

enum IFWFilterWheelErrors { PLUGIN_OK=SB_OK, PLUGIN_NOT_CONNECTED, PLUGIN_CANT_CONNECT, PLUGIN_BAD_CMD_RESPONSE, PLUGIN_RESP_TIMEOUT, PLUGIN_COMMAND_FAILED};
enum IFWStatus {IDLE=0, MOVING};

class CIFW
{
public:
    CIFW();
    ~CIFW();

    int             Connect(const char *szPort);
    void            Disconnect(void);
    bool            IsConnected(void) { return m_bIsConnected; };

    void            SetSerxPointer(SerXInterface *p) { m_pSerx = p; };
    void            setSleeper(SleeperInterface *pSleeper) { m_pSleeper = pSleeper; };

    // filter wheel communication
    int             filterWheelCommand(const char *szCmd, int nbByteToWrite, char *szResult, int nResultMaxLen, int nTimeout=MAX_TIMEOUT);
    int             readResponse(char *szRespBuffer, int nBufferLen, int nTimeout=MAX_TIMEOUT);

    // Filter Wheel commands
    int             getFirmwareVersion(char *szVersion, int nStrMaxLen);

    int             moveToFilterIndex(int nTargetPosition);
    int             isMoveToComplete(bool &bComplete);
    int             isHomingComplete(bool &bComplete);
    
    int             getFilterCount(int &nCount);
    int             getCurrentSlot(int &nSlot);
    int             getModel(std::string &szModel);
    
    int             getFilterName(const int nIndex, std::string &szName);
    int             setFilterName(const int nIndex, std::string szName);

    void            setHomeOnConnect(const bool bEnable);
    bool            getHomeOnConnect();

    int             loadFilterNameFromWheel();
    int             saveFilterNameToWheel();
    int             homeWheel(void);

protected:
    int             initComs(void);

    SerXInterface   *m_pSerx;
    SleeperInterface    *m_pSleeper;
    bool            m_bIsConnected;
    char            m_szFirmwareVersion[SERIAL_BUFFER_SIZE];

    std::string     m_sModel;
    int             m_nCurentFilterSlot;
    int             m_nTargetFilterSlot;
    
    int             m_nNbSlot;
    bool            m_bMoving;
    char            m_cWheelID;
    
    char            m_filterNames[8][9];
    bool            m_bFilterNameLoaded;
    bool            m_bHomeOnConnect;
    
    
    std::string&    trim(std::string &str, const std::string &filter );
    std::string&    ltrim(std::string &str, const std::string &filter);
    std::string&    rtrim(std::string &str, const std::string &filter);

    void            hexdump(const unsigned char* pszInputBuffer, unsigned char* pszOutputBuffer, unsigned int nInputBufferSize, unsigned int nOutpuBufferSize);
    
#ifdef PLUGIN_DEBUG
    std::string m_sLogfilePath;
    // timestamp for logs
    char *timestamp;
    time_t ltime;
    FILE *Logfile;      // LogFile
#endif

};
#endif /* ifw_h */
