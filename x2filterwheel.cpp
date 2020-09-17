#include "x2filterwheel.h"


X2FilterWheel::X2FilterWheel(const char* pszDriverSelection,
				const int& nInstanceIndex,
				SerXInterface					* pSerX, 
				TheSkyXFacadeForDriversInterface	* pTheSkyX, 
				SleeperInterface					* pSleeper,
				BasicIniUtilInterface			* pIniUtil,
				LoggerInterface					* pLogger,
				MutexInterface					* pIOMutex,
				TickCountInterface				* pTickCount)
{
    m_nPrivateMulitInstanceIndex    = nInstanceIndex;
    m_pSerX                            = pSerX;
    m_pTheSkyXForMounts                = pTheSkyX;
    m_pSleeper                        = pSleeper;
    m_pIniUtil                        = pIniUtil;
    m_pLogger                        = pLogger;
    m_pIOMutex                        = pIOMutex;
    m_pTickCount                    = pTickCount;

    m_bLinked = false;
    m_IFW.SetSerxPointer(pSerX);
    m_IFW.setSleeper(pSleeper);
    if (m_pIniUtil) {
       m_IFW.setHomeOnConnect(m_pIniUtil->readInt(PARENT_KEY, CHILD_KEY_AUTOHOME, 0) == 0?false:true );
    }
}

X2FilterWheel::~X2FilterWheel()
{
	if (m_pSerX)
		delete m_pSerX;
	if (m_pIniUtil)
		delete m_pIniUtil;
	if (m_pIOMutex)
		delete m_pIOMutex;
}

int X2FilterWheel::queryAbstraction(const char* pszName, void** ppVal)
{
    X2MutexLocker ml(GetMutex());

    *ppVal = NULL;

    if (!strcmp(pszName, ModalSettingsDialogInterface_Name))
        *ppVal = dynamic_cast<ModalSettingsDialogInterface*>(this);
    else if (!strcmp(pszName, X2GUIEventInterface_Name))
        *ppVal = dynamic_cast<X2GUIEventInterface*>(this);
    else if (!strcmp(pszName, SerialPortParams2Interface_Name))
        *ppVal = dynamic_cast<SerialPortParams2Interface*>(this);

    return SB_OK;
}



#pragma mark - UI binding

int X2FilterWheel::execModalSettingsDialog()
{
    int nErr = SB_OK;
    bool bPressedOK = false;
    std::string sTmp;
    int nbFilters = 0;
    bool bHomeOnConnect;
    m_bUiEnabled = false;

    X2ModalUIUtil uiutil(this, GetTheSkyXFacadeForDrivers());
    X2GUIInterface*                    ui = uiutil.X2UI();
    X2GUIExchangeInterface*            dx = NULL;//Comes after ui is loaded

    if (NULL == ui)
        return ERR_POINTER;

    if ((nErr = ui->loadUserInterface("IFW.ui", deviceType(), m_nPrivateMulitInstanceIndex)))
        return nErr;

    if (NULL == (dx = uiutil.X2DX()))
        return ERR_POINTER;

    //Intialize the user interface
    if(m_bLinked) {
        dx->setEnabled("pushButton", true);
        dx->setEnabled("pushButton_2", true);

        m_IFW.getFilterCount(nbFilters);
        if(nbFilters) {
            dx->setEnabled("label",true);
            m_IFW.getFilterName(0, sTmp);
            dx->setText("lineEdit", sTmp.c_str());
            
            dx->setEnabled("label_2",true);
            m_IFW.getFilterName(1, sTmp);
            dx->setText("lineEdit_2", sTmp.c_str());
            
            dx->setEnabled("label_3",true);
            m_IFW.getFilterName(2, sTmp);
            dx->setText("lineEdit_3", sTmp.c_str());
            
            dx->setEnabled("label_4",true);
            m_IFW.getFilterName(3, sTmp);
            dx->setText("lineEdit_4", sTmp.c_str());
            
            dx->setEnabled("label_5",true);
            m_IFW.getFilterName(4, sTmp);
            dx->setText("lineEdit_5", sTmp.c_str());
            if(nbFilters>5) {
                dx->setEnabled("label_6",true);
                m_IFW.getFilterName(5, sTmp);
                dx->setText("lineEdit_6", sTmp.c_str());

                dx->setEnabled("label_7",true);
                m_IFW.getFilterName(6, sTmp);
                dx->setText("lineEdit_7", sTmp.c_str());

                dx->setEnabled("label_8",true);
                m_IFW.getFilterName(7, sTmp);
                dx->setText("lineEdit_8", sTmp.c_str());
            }
        }
     }
    else {
        dx->setEnabled("pushButton", false);
        dx->setEnabled("pushButton_2", false);

        dx->setEnabled("label",false);
        dx->setEnabled("lineEdit",false);

        dx->setEnabled("label_2",false);
        dx->setEnabled("lineEdit_2",false);

        dx->setEnabled("label_3",false);
        dx->setEnabled("lineEdit_3",false);

        dx->setEnabled("label_4",false);
        dx->setEnabled("lineEdit_4",false);

        dx->setEnabled("label_5",false);
        dx->setEnabled("lineEdit_5",false);

        dx->setEnabled("label_6",false);
        dx->setEnabled("lineEdit_6",false);

        dx->setEnabled("label_7",false);
        dx->setEnabled("lineEdit_7",false);

        dx->setEnabled("label_8",false);
        dx->setEnabled("lineEdit_8",false);
   }

    bHomeOnConnect = m_IFW.getHomeOnConnect();
    dx->setChecked("checkBox", bHomeOnConnect?1:0);

    X2MutexLocker ml(GetMutex());

    //Display the user interface
    m_bUiEnabled = true;
    if ((nErr = ui->exec(bPressedOK)))
        return nErr;
    m_bUiEnabled = false;

    //Retreive values from the user interface
    if (bPressedOK) {
        bHomeOnConnect = (dx->isChecked("checkBox")!=0);
        printf("bHomeOnConnect = %s\n", bHomeOnConnect?"True":"False");
        m_IFW.setHomeOnConnect(bHomeOnConnect);
        m_pIniUtil->writeInt(PARENT_KEY, CHILD_KEY_AUTOHOME, bHomeOnConnect);
    }

    return nErr;
}

void X2FilterWheel::uiEvent(X2GUIExchangeInterface* uiex, const char* pszEvent)
{
    int nErr = SB_OK;
    char szTmp[9];
    int nbFilters;
    bool bComplete;
    char szErrorMessage[LOG_BUFFER_SIZE];

    
    if(!m_bLinked | !m_bUiEnabled)
        return;

    if (!strcmp(pszEvent, "on_timer")) {
        if(m_bHomingWheel) {
            bComplete = false;
            nErr = m_IFW.isHomingComplete(bComplete);
            if(nErr) {
                snprintf(szErrorMessage, LOG_BUFFER_SIZE, "Error homing wheel : Error %d", nErr);
                uiex->messageBox("IFW Homing", szErrorMessage);
                m_bHomingWheel = false;
                // enable buttons
                uiex->setEnabled("pushButton",true);
                uiex->setEnabled("pushButtonOK",true);
                return;
            }
            if(bComplete) {
                m_bHomingWheel = false;
                uiex->setEnabled("pushButton",true);
                uiex->setEnabled("pushButtonOK",true);
                return;
            }
            else {
                if(m_HomingTimer.GetElapsedSeconds()>30) { // timeout ?
                    uiex->messageBox("IFW Homing", "Timeout homming wheel");
                    m_bHomingWheel = false;
                    // enable buttons
                    uiex->setEnabled("pushButton",true);
                    uiex->setEnabled("pushButtonOK",true);
                    return;
                }
            }
        }
    }
    else if (!strcmp(pszEvent, "on_pushButton_clicked")) {
        m_IFW.homeWheel();
        m_bHomingWheel = true;
        // disable buttons
        uiex->setEnabled("pushButton",false);
        uiex->setEnabled("pushButtonOK",false);
        m_HomingTimer.Reset();
    }
    else if (!strcmp(pszEvent, "on_pushButton_2_clicked")) {
        m_IFW.getFilterCount(nbFilters);
        uiex->propertyString("lineEdit", "text", szTmp, 9);
        m_IFW.setFilterName(0, std::string(szTmp));

        uiex->propertyString("lineEdit_2", "text", szTmp, 9);
        m_IFW.setFilterName(1, std::string(szTmp));
        
        uiex->propertyString("lineEdit_3", "text", szTmp, 9);
        m_IFW.setFilterName(2, std::string(szTmp));
        
        uiex->propertyString("lineEdit_4", "text", szTmp, 9);
        m_IFW.setFilterName(3, std::string(szTmp));
        
        uiex->propertyString("lineEdit_5", "text", szTmp, 9);
        m_IFW.setFilterName(4, std::string(szTmp));
        if(nbFilters>5) {
            uiex->propertyString("lineEdit_6", "text", szTmp, 9);
            m_IFW.setFilterName(5, std::string(szTmp));
            
            uiex->propertyString("lineEdit_7", "text", szTmp, 9);
            m_IFW.setFilterName(0, std::string(szTmp));

            uiex->propertyString("lineEdit_8", "text", szTmp, 9);
            m_IFW.setFilterName(0, std::string(szTmp));
        }
        m_IFW.saveFilterNameToWheel();
    }

 }




#pragma mark - LinkInterface

int	X2FilterWheel::establishLink(void)
{
    int nErr;
    char szPort[DRIVER_MAX_STRING];

    X2MutexLocker ml(GetMutex());
    // get serial port device name
    portNameOnToCharPtr(szPort,DRIVER_MAX_STRING);
    nErr = m_IFW.Connect(szPort);
    if(nErr)
        m_bLinked = false;
    else
        m_bLinked = true;

    return nErr;
}

int	X2FilterWheel::terminateLink(void)
{
    X2MutexLocker ml(GetMutex());
    m_IFW.Disconnect();
    m_bLinked = false;
    return SB_OK;
}

bool X2FilterWheel::isLinked(void) const
{
    X2FilterWheel* pMe = (X2FilterWheel*)this;
    X2MutexLocker ml(pMe->GetMutex());
    return pMe->m_bLinked;
}

bool X2FilterWheel::isEstablishLinkAbortable(void) const	{

    return false;
}


#pragma mark - AbstractDriverInfo

#define DISPLAY_NAME "Optec IFW Filter Wheel"
void	X2FilterWheel::driverInfoDetailedInfo(BasicStringInterface& str) const
{
	str = DISPLAY_NAME;
}

double	X2FilterWheel::driverInfoVersion(void) const
{
	return 1.0;
}

void X2FilterWheel::deviceInfoNameShort(BasicStringInterface& str) const
{
    str = DISPLAY_NAME;
}

void X2FilterWheel::deviceInfoNameLong(BasicStringInterface& str) const
{
	str = DISPLAY_NAME;

}

void X2FilterWheel::deviceInfoDetailedDescription(BasicStringInterface& str) const
{
	str = "Optec IFW Filter Wheel PlugIn by Rodolphe Pineau";
	
}

void X2FilterWheel::deviceInfoFirmwareVersion(BasicStringInterface& str)
{
    if(m_bLinked) {
        X2MutexLocker ml(GetMutex());
        char cFirmware[SERIAL_BUFFER_SIZE];
        m_IFW.getFirmwareVersion(cFirmware, SERIAL_BUFFER_SIZE);
        str = cFirmware;
    }
    else
        str = "N/A";
}
void X2FilterWheel::deviceInfoModel(BasicStringInterface& str)				
{
    if(m_bLinked) {
        std::string sModel;
        X2MutexLocker ml(GetMutex());
        m_IFW.getModel(sModel);
        str = sModel.c_str();
    }
    else
        str = "N/A";
}

#pragma mark - FilterWheelMoveToInterface

int	X2FilterWheel::filterCount(int& nCount)
{
    int nErr = SB_OK;
    X2MutexLocker ml(GetMutex());
    nErr = m_IFW.getFilterCount(nCount);
    if(nErr) {
        nErr = ERR_CMDFAILED;
    }
    return nErr;
}

int	X2FilterWheel::defaultFilterName(const int& nIndex, BasicStringInterface& strFilterNameOut)
{
    std::string sName;
    int nErr = SB_OK;
	X2MutexLocker ml(GetMutex());

    nErr = m_IFW.getFilterName(nIndex, sName);

    strFilterNameOut = sName.c_str();

    return nErr;
}

int	X2FilterWheel::startFilterWheelMoveTo(const int& nTargetPosition)
{
    int nErr = SB_OK;
    
    if(m_bLinked) {
        X2MutexLocker ml(GetMutex());
        nErr = m_IFW.moveToFilterIndex(nTargetPosition);
        if(nErr)
            nErr = ERR_CMDFAILED;
    }
    return nErr;
}

int	X2FilterWheel::isCompleteFilterWheelMoveTo(bool& bComplete) const
{
    int nErr = SB_OK;

    if(m_bLinked) {
        X2FilterWheel* pMe = (X2FilterWheel*)this;
        X2MutexLocker ml(pMe->GetMutex());
        nErr = pMe->m_IFW.isMoveToComplete(bComplete);
        if(nErr)
            nErr = ERR_CMDFAILED;
    }
    return nErr;
}

int	X2FilterWheel::endFilterWheelMoveTo(void)
{
	X2MutexLocker ml(GetMutex());
	return SB_OK;
}

int	X2FilterWheel::abortFilterWheelMoveTo(void)
{
	X2MutexLocker ml(GetMutex());
	return SB_OK;
}

#pragma mark -  SerialPortParams2Interface

void X2FilterWheel::portName(BasicStringInterface& str) const
{
    char szPortName[DRIVER_MAX_STRING];

    portNameOnToCharPtr(szPortName, DRIVER_MAX_STRING);

    str = szPortName;

}

void X2FilterWheel::setPortName(const char* szPort)
{
    if (m_pIniUtil)
        m_pIniUtil->writeString(PARENT_KEY, CHILD_KEY_PORTNAME, szPort);

}


void X2FilterWheel::portNameOnToCharPtr(char* pszPort, const int& nMaxSize) const
{
    if (NULL == pszPort)
        return;

    snprintf(pszPort, nMaxSize,DEF_PORT_NAME);

    if (m_pIniUtil)
        m_pIniUtil->readString(PARENT_KEY, CHILD_KEY_PORTNAME, pszPort, pszPort, nMaxSize);
    
}




