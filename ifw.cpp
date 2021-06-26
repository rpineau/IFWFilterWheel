//
//  xagyl.cpp
//  XagylFilterWheel
//
//  Created by Rodolphe Pineau on 26/10/16.
//  Copyright © 2016 RTI-Zone. All rights reserved.
//

#include "ifw.h"

CIFW::CIFW()
{
    m_bIsConnected = false;
    m_nCurentFilterSlot = -1;
    m_nTargetFilterSlot = 0;
    m_nNbSlot = 0;
    m_pSleeper = NULL;
    m_pSerx = NULL;
    m_bMoving = false;
    m_bFilterNameLoaded = false;
    m_bHomeOnConnect = false;
    
#ifdef PLUGIN_DEBUG
#if defined(SB_WIN_BUILD)
    m_sLogfilePath = getenv("HOMEDRIVE");
    m_sLogfilePath += getenv("HOMEPATH");
    m_sLogfilePath += "\\IFWLog.txt";
#elif defined(SB_LINUX_BUILD)
    m_sLogfilePath = getenv("HOME");
    m_sLogfilePath += "/IFWLog.txt";
#elif defined(SB_MAC_BUILD)
    m_sLogfilePath = getenv("HOME");
    m_sLogfilePath += "/IFWLog.txt";
#endif
    Logfile = fopen(m_sLogfilePath.c_str(), "w");
#endif
    
#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
    ltime = time(NULL);
    timestamp = asctime(localtime(&ltime));
    timestamp[strlen(timestamp) - 1] = 0;
    fprintf(Logfile, "[%s] [CIFW::CIFW] New Constructor Called\n", timestamp);
    fprintf(Logfile, "[%s] [CIFW::CIFW] Version %3.2f build 2020_09_16_1755.\n", timestamp, DRIVER_VERSION);
    fflush(Logfile);
#endif
    memset(&m_filterNames, ' ', 8*9);
}

CIFW::~CIFW()
{

}

int CIFW::Connect(const char *szPort)
{
    int nErr = PLUGIN_OK;
    bool bHomingComplete = false;
    int timeout = 0;
#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
    ltime = time(NULL);
    timestamp = asctime(localtime(&ltime));
    timestamp[strlen(timestamp) - 1] = 0;
    fprintf(Logfile, "[%s] [CIFW::Connect] Called, connecting to %s\n", timestamp, szPort);
    fflush(Logfile);
#endif

    if(!m_pSerx)
        return ERR_COMMNOLINK;
        
    // 9600 8N1
    if(m_pSerx->open(szPort, 19200, SerXInterface::B_NOPARITY, "-DTR_CONTROL 1") == 0)
        m_bIsConnected = true;
    else
        m_bIsConnected = false;

    if(!m_bIsConnected)
        return ERR_COMMNOLINK;
    
#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
    ltime = time(NULL);
    timestamp = asctime(localtime(&ltime));
    timestamp[strlen(timestamp) - 1] = 0;
    fprintf(Logfile, "[%s] [Connect::Connect] Connected.\n", timestamp);
    fflush(Logfile);
#endif

    nErr = initComs();
    if(nErr) {
#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
        ltime = time(NULL);
        timestamp = asctime(localtime(&ltime));
        timestamp[strlen(timestamp) - 1] = 0;
        fprintf(Logfile, "[%s] [Connect::Connect] Error initiating comunication : %d\n", timestamp, nErr);
        fflush(Logfile);
#endif
        m_pSerx->purgeTxRx();
        m_pSerx->close();
        m_bIsConnected = false;
        m_bMoving = false;
        return ERR_COMMNOLINK;
    }
    
    // if any of this fails we're not properly connected or there is a hardware issue.
    nErr = getFirmwareVersion(m_szFirmwareVersion, SERIAL_BUFFER_SIZE);
    if(nErr) {
#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
        ltime = time(NULL);
        timestamp = asctime(localtime(&ltime));
        timestamp[strlen(timestamp) - 1] = 0;
        fprintf(Logfile, "[%s] [Connect::Connect] Error Getting Firmware : %d\n", timestamp, nErr);
        fflush(Logfile);
#endif
        m_bIsConnected = false;
        m_bMoving = false;
        m_pSerx->close();
        return FIRMWARE_NOT_SUPPORTED;
    }

    
#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
    ltime = time(NULL);
    timestamp = asctime(localtime(&ltime));
    timestamp[strlen(timestamp) - 1] = 0;
    fprintf(Logfile, "[%s] [Connect::Connect] Connected.\n", timestamp);
    fflush(Logfile);
#endif

    nErr |= loadFilterNameFromWheel();
    
    if(m_bHomeOnConnect) {
        nErr = homeWheel();
        if(nErr) {
    #if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
            ltime = time(NULL);
            timestamp = asctime(localtime(&ltime));
            timestamp[strlen(timestamp) - 1] = 0;
            fprintf(Logfile, "[%s] [Connect::Connect] Error homing filter wheel : %d\n", timestamp, nErr);
            fflush(Logfile);
    #endif
            m_bIsConnected = false;
            m_bMoving = false;
            m_pSerx->close();
            return ERR_COMMNOLINK;
        }
        do {
            #if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
                ltime = time(NULL);
                timestamp = asctime(localtime(&ltime));
                timestamp[strlen(timestamp) - 1] = 0;
                fprintf(Logfile, "[%s] [Connect::Connect] waiting for wheel to home.\n", timestamp);
                fflush(Logfile);
            #endif
            if(m_pSleeper)
                m_pSleeper->sleep(1000);
            isHomingComplete(bHomingComplete);
            timeout++;
            if(timeout>30) // 30 seconds is max , should never take that long.
                return ERR_COMMNOLINK;
        } while(!bHomingComplete);

#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
        ltime = time(NULL);
        timestamp = asctime(localtime(&ltime));
        timestamp[strlen(timestamp) - 1] = 0;
        fprintf(Logfile, "[%s] [Connect::Connect] Wheel is at home.\n", timestamp);
        fflush(Logfile);
    #endif
    }
    
    nErr = getCurrentSlot(m_nCurentFilterSlot);
    m_nTargetFilterSlot = m_nCurentFilterSlot;
    return nErr;
}



void CIFW::Disconnect()
{
     char szResp[SERIAL_BUFFER_SIZE];
    
#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
    ltime = time(NULL);
    timestamp = asctime(localtime(&ltime));
    timestamp[strlen(timestamp) - 1] = 0;
    fprintf(Logfile, "[%s] [CIFW::Disconnect] Called\n", timestamp);
    fflush(Logfile);
#endif

    if(m_bIsConnected) {
        filterWheelCommand("WEXITS\r\n", strlen("WEXITS\r\n"), szResp, SERIAL_BUFFER_SIZE);
        m_pSerx->purgeTxRx();
        m_pSerx->close();
    }
    m_bIsConnected = false;
    m_bMoving = false;
    
}


#pragma mark - communication functions
int CIFW::readResponse(char *szRespBuffer, int nBufferLen, int nTimeout)
{
    int nErr = PLUGIN_OK;
    unsigned long ulBytesRead = 0;
    unsigned long ulTotalBytesRead = 0;
    char *szBufPtr;
    
    memset(szRespBuffer, 0, (size_t) nBufferLen);
    szBufPtr = szRespBuffer;

    do {
        nErr = m_pSerx->readFile(szBufPtr, 1, ulBytesRead, nTimeout);
        if(nErr) {
#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
            ltime = time(NULL);
            timestamp = asctime(localtime(&ltime));
            timestamp[strlen(timestamp) - 1] = 0;
            fprintf(Logfile, "[%s] [CIFW::readResponse] readFile error.\n", timestamp);
            fflush(Logfile);
#endif
            return nErr;
        }

#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 3
        ltime = time(NULL);
        timestamp = asctime(localtime(&ltime));
        timestamp[strlen(timestamp) - 1] = 0;
        fprintf(Logfile, "[%s] [CIFW::readResponse] respBuffer = %s\n", timestamp, szRespBuffer);
        fflush(Logfile);
#endif

        if (ulBytesRead !=1) {// timeout
#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
            ltime = time(NULL);
            timestamp = asctime(localtime(&ltime));
            timestamp[strlen(timestamp) - 1] = 0;
            fprintf(Logfile, "[%s] [CIFW::readResponse] readFile Timeout\n", timestamp);
            fflush(Logfile);
#endif
            nErr = PLUGIN_RESP_TIMEOUT;
            break;
        }
        ulTotalBytesRead += ulBytesRead;
#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 3
        ltime = time(NULL);
        timestamp = asctime(localtime(&ltime));
        timestamp[strlen(timestamp) - 1] = 0;
        fprintf(Logfile, "[%s] [CIFW::readResponse] ulBytesRead = %lu\n", timestamp, ulBytesRead);
        fprintf(Logfile, "[%s] [CIFW::readResponse] ulTotalBytesRead = %lu\n", timestamp, ulTotalBytesRead);
        fflush(Logfile);
#endif

    } while (*szBufPtr++ != '\r' && ulTotalBytesRead < (unsigned long)nBufferLen );

    if(ulTotalBytesRead>1)
        *(szBufPtr-1) = 0; //remove the CR

    return nErr;
}


int CIFW::filterWheelCommand(const char *szCmd, int nbByteToWrite, char *szResult, int nResultMaxLen, int nTimeout)
{
    int nErr = PLUGIN_OK;
    char szResp[SERIAL_BUFFER_SIZE];
    unsigned long  ulBytesWrite;

    m_pSerx->purgeTxRx();
    
#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
    ltime = time(NULL);
    timestamp = asctime(localtime(&ltime));
    timestamp[strlen(timestamp) - 1] = 0;
    fprintf(Logfile, "[%s] [CIFW::filterWheelCommand] Sending %s\n", timestamp, szCmd);
    fflush(Logfile);
#endif

    nErr = m_pSerx->writeFile((void *)szCmd, nbByteToWrite, ulBytesWrite);
    m_pSerx->flushTx();

    if(nErr){
#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
        ltime = time(NULL);
        timestamp = asctime(localtime(&ltime));
        timestamp[strlen(timestamp) - 1] = 0;
        fprintf(Logfile, "[%s] [CIFW::filterWheelCommand] writeFile error.\n", timestamp);
        fflush(Logfile);
#endif
        return nErr;
    }

    if(szResult) {
        // read response
#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
        ltime = time(NULL);
        timestamp = asctime(localtime(&ltime));
        timestamp[strlen(timestamp) - 1] = 0;
        fprintf(Logfile, "[%s] [CIFW::filterWheelCommand] Getting response.\n", timestamp);
        fflush(Logfile);
#endif
        nErr = readResponse(szResp, nResultMaxLen, nTimeout);
        if(nErr){
            m_bMoving = false;
#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
            ltime = time(NULL);
            timestamp = asctime(localtime(&ltime));
            timestamp[strlen(timestamp) - 1] = 0;
            fprintf(Logfile, "[%s] [CIFW::filterWheelCommand] readResponse error : %d\n", timestamp, nErr);
            fflush(Logfile);
            return nErr;
#endif
        }
#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
        ltime = time(NULL);
        timestamp = asctime(localtime(&ltime));
        timestamp[strlen(timestamp) - 1] = 0;
        fprintf(Logfile, "[%s] [CIFW::filterWheelCommand] response = %s\n", timestamp, szResp);
        fflush(Logfile);
#endif
        strncpy(szResult, szResp, nResultMaxLen);
    }
    return nErr;
    
}

int CIFW::initComs()
{
    int nErr = 0;
    char szResp[SERIAL_BUFFER_SIZE];

#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
    ltime = time(NULL);
    timestamp = asctime(localtime(&ltime));
    timestamp[strlen(timestamp) - 1] = 0;
    fprintf(Logfile, "[%s] [CIFW::initComs] Called\n", timestamp);
    fflush(Logfile);
#endif

    if(!m_bIsConnected) {
        return PLUGIN_NOT_CONNECTED;
    }

    m_bMoving = false;
    nErr = filterWheelCommand("WSMODE\r\n", strlen("WSMODE\r\n"), szResp, SERIAL_BUFFER_SIZE);
    if(nErr) {
#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
        ltime = time(NULL);
        timestamp = asctime(localtime(&ltime));
        timestamp[strlen(timestamp) - 1] = 0;
        fprintf(Logfile, "[%s] [CIFW::initComs] Error Getting response from filterWheelCommand : %d\n", timestamp, nErr);
        fflush(Logfile);
#endif
        return nErr;
    }
    
    if(szResp[0] != '!') {
        nErr = PLUGIN_COMMAND_FAILED;
    }
    
    return nErr;

}

#pragma mark - Filter Wheel info commands

int CIFW::loadFilterNameFromWheel()
{
    int nErr = PLUGIN_OK;
    char szResp[SERIAL_BUFFER_SIZE];
    int i = 0;
#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
    ltime = time(NULL);
    timestamp = asctime(localtime(&ltime));
    timestamp[strlen(timestamp) - 1] = 0;
    fprintf(Logfile, "[%s] [CIFW::loadFilterNameFromWheel] Called\n", timestamp);
    fflush(Logfile);
#endif

    if(!m_bIsConnected) {
        return PLUGIN_NOT_CONNECTED;
    }

    if(m_bMoving) {
        return ERR_COMMANDINPROGRESS;
    }

    if(!m_nNbSlot)
        nErr = getModel(m_sModel);

    nErr = filterWheelCommand("WREAD\r\n", strlen("WREAD\r\n"), szResp, SERIAL_BUFFER_SIZE);
    if(nErr) {
#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
        ltime = time(NULL);
        timestamp = asctime(localtime(&ltime));
        timestamp[strlen(timestamp) - 1] = 0;
        fprintf(Logfile, "[%s] [CIFW::loadFilterNameFromWheel] Error Getting response from filterWheelCommand : %d\n", timestamp, nErr);
        fflush(Logfile);
#endif
        return nErr;
    }

    m_bFilterNameLoaded = true;
    // copy each name to the array
    for(i=0; i<m_nNbSlot; i++) {
        memcpy(m_filterNames[i], szResp+(i*8), 8);
        m_filterNames[i][8] = 0;    // terminate string.
    }
    return nErr;
}

int CIFW::saveFilterNameToWheel()
{
    int nErr = PLUGIN_OK;
    char szCmd[SERIAL_BUFFER_SIZE];
    char szResp[SERIAL_BUFFER_SIZE];
#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
    unsigned char cHexMessage[256];
#endif
    int nbBytes = 0;
    unsigned long ulBytesWrite;
    int i;
    
    if(m_bMoving)
        return ERR_COMMANDINPROGRESS;
    
    snprintf(szCmd,SERIAL_BUFFER_SIZE, "WLOAD%c*", m_cWheelID);

    nErr = filterWheelCommand(szCmd, int(strlen(szCmd)), NULL, 0);

    for(i=0; i<m_nNbSlot; i++) {
        memcpy(szCmd+(i*8), m_filterNames[i], 8);
    }
    nbBytes = (m_nNbSlot*8);

#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
    ltime = time(NULL);
    timestamp = asctime(localtime(&ltime));
    timestamp[strlen(timestamp) - 1] = 0;
    fprintf(Logfile, "[%s] [CIFW::saveFilterNameToWheel] Saving filter name to filter wheel : nbBytes = %d\n", timestamp, nbBytes);
    hexdump((unsigned char*)szCmd, cHexMessage, nbBytes, 256);
    fprintf(Logfile, "[%s] [CIFW::saveFilterNameToWheel] Saving filter name data :\n=============\n%s\n=============\n", timestamp, cHexMessage);
    fflush(Logfile);
#endif
    // sends bytes one by one with 25ms inter char minimum as this writes to EEPROM
    for(i=0; i<nbBytes; i++ ) {
        nErr = m_pSerx->writeFile((void *)(szCmd+i), 1, ulBytesWrite);
        if(nErr) {
            return nErr;
        }
        m_pSerx->flushTx();
        m_pSleeper->sleep(25);
    }

    nErr = filterWheelCommand("\r\n", 2, szResp, SERIAL_BUFFER_SIZE);
    if(nErr) {
#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
        ltime = time(NULL);
        timestamp = asctime(localtime(&ltime));
        timestamp[strlen(timestamp) - 1] = 0;
        fprintf(Logfile, "[%s] [CIFW::saveFilterNameToWheel] Error saving filter name to filter wheel : %d\n", timestamp, nErr);
        fflush(Logfile);
#endif
    }

    return nErr;
}


int CIFW::getFirmwareVersion(char *szVersion, int nStrMaxLen)
{
    int nErr = 0;
    std::string sDummy;
#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
    ltime = time(NULL);
    timestamp = asctime(localtime(&ltime));
    timestamp[strlen(timestamp) - 1] = 0;
    fprintf(Logfile, "[%s] [CIFW::getFirmwareVersion] Called\n", timestamp);
    fflush(Logfile);
#endif

    if(!m_bIsConnected) {
        return PLUGIN_NOT_CONNECTED;
    }

    if(m_bMoving) {
        return ERR_COMMANDINPROGRESS;
    }

    nErr = getModel(m_sModel);
    strncpy(szVersion, m_sModel.c_str(), nStrMaxLen);

#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
    ltime = time(NULL);
    timestamp = asctime(localtime(&ltime));
    timestamp[strlen(timestamp) - 1] = 0;
    fprintf(Logfile, "[%s] [CIFW::getFirmwareVersion] Firmware '%s'\n", timestamp, szVersion);
    fflush(Logfile);
#endif

    
    return nErr;
}

int CIFW::getModel(std::string &sModel)
{
    int nErr = 0;
    char szResp[SERIAL_BUFFER_SIZE];
    int nNbFilters;
    std::string sSize;

#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
    ltime = time(NULL);
    timestamp = asctime(localtime(&ltime));
    timestamp[strlen(timestamp) - 1] = 0;
    fprintf(Logfile, "[%s] [CIFW::getModel] Called\n", timestamp);
    fflush(Logfile);
#endif

    if(!m_bIsConnected) {
        return PLUGIN_NOT_CONNECTED;
    }

    if(m_bMoving) {
        return ERR_COMMANDINPROGRESS;
    }

    // get Wheel ID
    nErr = filterWheelCommand("WIDENT\r\n", strlen("WIDENT\r\n"), szResp, SERIAL_BUFFER_SIZE);
    if(nErr) {
#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
        ltime = time(NULL);
        timestamp = asctime(localtime(&ltime));
        timestamp[strlen(timestamp) - 1] = 0;
        fprintf(Logfile, "[%s] [CIFW::getModel] Error Getting response from filterWheelCommand : %d\n", timestamp, nErr);
        fflush(Logfile);
#endif
        return nErr;
    }
    
    m_cWheelID = szResp[0];
    
    // we need to know the length of the data for the filters to properly identify the wheel size and slot numbers
    nErr = filterWheelCommand("WREAD\r\n", strlen("WREAD\r\n"), szResp, SERIAL_BUFFER_SIZE);
    if(nErr) {
#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
        ltime = time(NULL);
        timestamp = asctime(localtime(&ltime));
        timestamp[strlen(timestamp) - 1] = 0;
        fprintf(Logfile, "[%s] [CIFW::getModel] Error Getting response from filterWheelCommand : %d\n", timestamp, nErr);
        fflush(Logfile);
#endif
        return nErr;
    }

    m_nNbSlot = int(strlen(szResp))/8;

    switch(m_cWheelID) {
        case 'A':
        case 'B':
            if(m_nNbSlot == 9)
                sSize = "3\"";
            else
                sSize = "2\"";

        case 'C':
        case 'D':
        case 'E':
            if(m_nNbSlot == 6)
                sSize = "3\"";
            else
                sSize = "2\"";
            break;
        case 'F':
        case 'G':
        case 'H':
            if(m_nNbSlot == 5)
                sSize = "3\"";
            else
                sSize = "2\"";
            break;
        case 'I':
        case 'J':
        case 'K':
            sSize = "2\"";
            break;
        default :
            sSize = "2\"";
            break;
    }

    sModel = std::string("IFW ") + sSize + std::string(" model ") + m_cWheelID + " " + std::to_string(m_nNbSlot) + " Slots";
    m_sModel.assign(sModel);
    
#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
    ltime = time(NULL);
    timestamp = asctime(localtime(&ltime));
    timestamp[strlen(timestamp) - 1] = 0;
    fprintf(Logfile, "[%s] [CIFW::getModel] Model '%s'\n", timestamp, m_sModel.c_str());
    fprintf(Logfile, "[%s] [CIFW::getModel] Wheel ID '%c'\n", timestamp, m_cWheelID);
    fflush(Logfile);
#endif

    
    return nErr;
}

#pragma mark - Filter Wheel move commands

int CIFW::moveToFilterIndex(int nTargetPosition)
{
    int nErr = 0;
    char szCmd[SERIAL_BUFFER_SIZE];
    
#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
    ltime = time(NULL);
    timestamp = asctime(localtime(&ltime));
    timestamp[strlen(timestamp) - 1] = 0;
    fprintf(Logfile, "[%s] [CIFW::moveToFilterIndex] Called, movig to filter %d\n", timestamp, nTargetPosition);
    fprintf(Logfile, "[%s] [CIFW::moveToFilterIndex] m_nCurentFilterSlot = %d\n", timestamp, m_nCurentFilterSlot);
    fflush(Logfile);
#endif

    if(m_bMoving) {
        return ERR_COMMANDINPROGRESS;
    }
    
    snprintf(szCmd,SERIAL_BUFFER_SIZE, "WGOTO%d\r\n", nTargetPosition+1);
    nErr = filterWheelCommand(szCmd, int(strlen(szCmd)), NULL, SERIAL_BUFFER_SIZE);
    if(nErr) {
#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
        ltime = time(NULL);
        timestamp = asctime(localtime(&ltime));
        timestamp[strlen(timestamp) - 1] = 0;
        fprintf(Logfile, "[%s] [CIFW::moveToFilterIndex] Error Getting response from filterWheelCommand : %d\n", timestamp, nErr);
        fflush(Logfile);
#endif
        return nErr;
    }
    m_nTargetFilterSlot = nTargetPosition;
    m_bMoving = true;
    
#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
    ltime = time(NULL);
    timestamp = asctime(localtime(&ltime));
    timestamp[strlen(timestamp) - 1] = 0;
    fprintf(Logfile, "[%s] [CIFW::moveToFilterIndex] m_nCurentFilterSlot = %d\n", timestamp, m_nCurentFilterSlot);
    fprintf(Logfile, "[%s] [CIFW::moveToFilterIndex] m_nTargetFilterSlot = %d\n", timestamp, m_nTargetFilterSlot);
    fflush(Logfile);
#endif

    return nErr;
}

int CIFW::isMoveToComplete(bool &bComplete)
{
    int nErr = PLUGIN_OK;
    char szResp[SERIAL_BUFFER_SIZE];

    bComplete = false;

    if(m_nCurentFilterSlot == m_nTargetFilterSlot) {
        bComplete = true;
        m_bMoving = false;
        return nErr;
    }
    
    if(!m_bMoving) {
        bComplete = true;
        return nErr;
    }
    
    nErr = readResponse(szResp, SERIAL_BUFFER_SIZE, 250); // 250ms timeout
    if(nErr && nErr == PLUGIN_RESP_TIMEOUT)
        return PLUGIN_OK;   // still moving to selected slot
    else if(nErr) {
#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
        ltime = time(NULL);
        timestamp = asctime(localtime(&ltime));
        timestamp[strlen(timestamp) - 1] = 0;
        fprintf(Logfile, "[%s] [CIFW::isMoveToComplete] Error Getting response from readResponse : %d\n", timestamp, nErr);
        fflush(Logfile);
#endif
        return nErr;
    }

    if(szResp[0]=='*') {
        bComplete = true;
        m_nCurentFilterSlot = m_nTargetFilterSlot;
        m_bMoving = false;
    }

#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
    ltime = time(NULL);
    timestamp = asctime(localtime(&ltime));
    timestamp[strlen(timestamp) - 1] = 0;
    fprintf(Logfile, "[%s] [CIFW::isMoveToComplete] Done, nErr =  %d\n", timestamp, nErr);
    fprintf(Logfile, "[%s] [CIFW::isMoveToComplete] m_bMoving = %s\n", timestamp, m_bMoving?"Yes":"No");
    fprintf(Logfile, "[%s] [CIFW::isMoveToComplete] bComplete = %s\n", timestamp, bComplete?"Yes":"No");
    fflush(Logfile);
#endif

    return nErr;
}

int CIFW::homeWheel(void)
{
    int nErr = 0;
    
#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
    ltime = time(NULL);
    timestamp = asctime(localtime(&ltime));
    timestamp[strlen(timestamp) - 1] = 0;
    fprintf(Logfile, "[%s] [CIFW::homeWheel] Called\n", timestamp);
    fflush(Logfile);
#endif

    if(!m_bIsConnected)
        return PLUGIN_NOT_CONNECTED;

    nErr = filterWheelCommand("WHOME\r\n", strlen("WHOME\r\n"),  NULL, SERIAL_BUFFER_SIZE); // it can take up to 30 seconds to home
    if(nErr) {
#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
        ltime = time(NULL);
        timestamp = asctime(localtime(&ltime));
        timestamp[strlen(timestamp) - 1] = 0;
        fprintf(Logfile, "[%s] [CIFW::homeWheel] Error Getting response from filterWheelCommand : %d\n", timestamp, nErr);
        fflush(Logfile);
#endif
        return nErr;
    }
    m_bMoving = true;
    
    return nErr;

}

int CIFW::isHomingComplete(bool &bComplete)
{
    int nErr = PLUGIN_OK;
    char szResp[SERIAL_BUFFER_SIZE];

    bComplete = false;

    #if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
        ltime = time(NULL);
        timestamp = asctime(localtime(&ltime));
        timestamp[strlen(timestamp) - 1] = 0;
        fprintf(Logfile, "[%s] [CIFW::isHomingComplete] m_bMoving = %s\n", timestamp, m_bMoving?"Yes":"No");
        fflush(Logfile);
    #endif

    if(!m_bMoving) {
        bComplete = true;
        return nErr;
    }
    
    nErr = readResponse(szResp, SERIAL_BUFFER_SIZE, 250); // 250ms timeout
    if(nErr && nErr == PLUGIN_RESP_TIMEOUT)
        return PLUGIN_OK;   // still moving to selected slot
    else if(nErr) {
#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
        ltime = time(NULL);
        timestamp = asctime(localtime(&ltime));
        timestamp[strlen(timestamp) - 1] = 0;
        fprintf(Logfile, "[%s] [CIFW::isHomingComplete] Error Getting response from readResponse : %d\n", timestamp, nErr);
        fflush(Logfile);
#endif
        return nErr;
    }

    if(szResp[0]==m_cWheelID) {
        bComplete = true;
        m_bMoving = false;
    }

#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
    ltime = time(NULL);
    timestamp = asctime(localtime(&ltime));
    timestamp[strlen(timestamp) - 1] = 0;
    fprintf(Logfile, "[%s] [CIFW::isHomingComplete] nErr =  %d\n", timestamp, nErr);
    fprintf(Logfile, "[%s] [CIFW::isHomingComplete] szResp =  %s\n", timestamp, szResp);
    fprintf(Logfile, "[%s] [CIFW::isHomingComplete] m_bMoving = %s\n", timestamp, m_bMoving?"Yes":"No");
    fprintf(Logfile, "[%s] [CIFW::isHomingComplete] bComplete = %s\n", timestamp, bComplete?"Yes":"No");
    fflush(Logfile);
#endif

    return nErr;
}


#pragma mark - filters and device params functions
int CIFW::getFilterCount(int &nCount)
{
    nCount = m_nNbSlot;
    return PLUGIN_OK;
}


int CIFW::getCurrentSlot(int &nSlot)
{
    int nErr = PLUGIN_OK;
    char szResp[SERIAL_BUFFER_SIZE];


    if(m_bMoving) {
        nSlot = m_nCurentFilterSlot;
        return nErr;
    }

#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
    ltime = time(NULL);
    timestamp = asctime(localtime(&ltime));
    timestamp[strlen(timestamp) - 1] = 0;
    fprintf(Logfile, "[%s] [CIFW::getCurrentSlot] Called\n", timestamp);
    fflush(Logfile);
#endif

    nErr = filterWheelCommand("WFILTR\r\n", strlen("WFILTR\r\n"), szResp, SERIAL_BUFFER_SIZE);
    if(nErr) {
#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
        ltime = time(NULL);
        timestamp = asctime(localtime(&ltime));
        timestamp[strlen(timestamp) - 1] = 0;
        fprintf(Logfile, "[%s] [CIFW::getCurrentSlot] Error Getting response from filterWheelCommand : %d\n", timestamp, nErr);
        fflush(Logfile);
#endif
        return nErr;
    }

    nSlot = atoi(szResp) - 1;
    m_nCurentFilterSlot = nSlot;

#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
    ltime = time(NULL);
    timestamp = asctime(localtime(&ltime));
    timestamp[strlen(timestamp) - 1] = 0;
    fprintf(Logfile, "[%s] [CIFW::getCurrentSlot] m_nCurentFilterSlot = %d\n", timestamp, m_nCurentFilterSlot);
    fflush(Logfile);
#endif

    
    return nErr;
}

int CIFW::getFilterName(const int nIndex, std::string &szName)
{
    int nErr = PLUGIN_OK;
    
    if(!m_bFilterNameLoaded)
        szName = "Filter " + std::to_string(nIndex);
    else
        szName.assign(m_filterNames[nIndex]);
    szName = trim(szName," ");
    return nErr;
}

int CIFW::setFilterName(const int nIndex, std::string szName)
{
    int nErr = PLUGIN_OK;
    if(szName.size() <8)
        szName.append(8-szName.size(),' '); // fill to the end with spaces
    strncpy(m_filterNames[nIndex], szName.c_str(),8);

#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
    ltime = time(NULL);
    timestamp = asctime(localtime(&ltime));
    timestamp[strlen(timestamp) - 1] = 0;
    fprintf(Logfile, "[%s] [CIFW::setFilterName] set filter %d name to '%s'\n", timestamp, nIndex, m_filterNames[nIndex]);
    fflush(Logfile);
#endif
    
    return nErr;
}

void CIFW::setHomeOnConnect(const bool bEnable)
{
    m_bHomeOnConnect = bEnable;
#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
    ltime = time(NULL);
    timestamp = asctime(localtime(&ltime));
    timestamp[strlen(timestamp) - 1] = 0;
    fprintf(Logfile, "[%s] [CIFW::setHomeOnConnect] m_bHomeOnConnect = %s\n", timestamp,m_bHomeOnConnect?"True":"False");
    fflush(Logfile);
#endif
}

bool CIFW::getHomeOnConnect()
{
#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
    ltime = time(NULL);
    timestamp = asctime(localtime(&ltime));
    timestamp[strlen(timestamp) - 1] = 0;
    fprintf(Logfile, "[%s] [CIFW::getHomeOnConnect] m_bHomeOnConnect = %s\n", timestamp,m_bHomeOnConnect?"True":"False");
    fflush(Logfile);
#endif

    return m_bHomeOnConnect;
}



std::string& CIFW::trim(std::string &str, const std::string& filter )
{
    return ltrim(rtrim(str, filter), filter);
}

std::string& CIFW::ltrim(std::string& str, const std::string& filter)
{
    str.erase(0, str.find_first_not_of(filter));
    return str;
}

std::string& CIFW::rtrim(std::string& str, const std::string& filter)
{
    str.erase(str.find_last_not_of(filter) + 1);
    return str;
}


void CIFW::hexdump(const unsigned char* pszInputBuffer, unsigned char* pszOutputBuffer, unsigned int nInputBufferSize, unsigned int nOutpuBufferSize)
{
    unsigned char *pszBuf = pszOutputBuffer;
    unsigned int nIdx=0;

    memset(pszOutputBuffer, 0, nOutpuBufferSize);
    for(nIdx=0; nIdx < nInputBufferSize && pszBuf < (pszOutputBuffer + nOutpuBufferSize -3); nIdx++){
        snprintf((char *)pszBuf,4,"%02X ", pszInputBuffer[nIdx]);
        pszBuf+=3;
    }
}
