/*
 ============================================================================
 Name		: CStopwatchView.cpp
 Author	  : Florin Lohan
 Version	 : 1.0
 Copyright   : Florin Lohan
 Description : CStopwatchView implementation
 ============================================================================
 */
#include <eikmenup.h>
#include <avkon.rsg>
#include <MLauncher.rsg>
#include "StopwatchView.h"
#include "StopwatchContainer.h"
#include "MLauncherAppUi.h"
#include "MLauncher.hrh"
#include "MLauncher.pan"
#include "log.h"

#ifdef FL_USE_ACCELEROMETER
#include <sensrvchannelfinder.h> 
#include <sensrvaccelerometersensor.h>
#endif

const TInt KOneSecondMs=1000000;
#ifdef __WINS__
_LIT(KAlarmFile,"z:\\system\\sounds\\simple\\soa.mp3");
#else
_LIT(KAlarmFile,"z:\\system\\sounds\\simple\\alarm.rng");
#endif

CStopwatchView::CStopwatchView() : iAlarmState(EStateNotInitialized)
{
    // No implementation required
}

CStopwatchView::~CStopwatchView()
{
    if(iPeriodic)
    {
        iPeriodic->Cancel();
        delete iPeriodic;
    };
    delete iMdaPlayer;
    
#ifdef FL_USE_ACCELEROMETER
    if(iSensrvChannel)
    {
    	iSensrvChannel->StopDataListening(); 
    	iSensrvChannel->CloseChannel();
    	delete iSensrvChannel;
    	iSensrvChannel=NULL;
    }
#endif
}

CStopwatchView* CStopwatchView::NewLC()
{
    CStopwatchView* self = new (ELeave)CStopwatchView();
    CleanupStack::PushL(self);
    self->ConstructL();
    return self;
}

CStopwatchView* CStopwatchView::NewL()
{
    CStopwatchView* self=CStopwatchView::NewLC();
    CleanupStack::Pop(); // self;
    return self;
}

void CStopwatchView::ConstructL()
{
    // construct from resource
    BaseConstructL(R_STOPWATCH_VIEW);
}

void CStopwatchView::DoDeactivate()
{
    if(iStopwatchContainer)
    {
        AppUi()->RemoveFromStack(iStopwatchContainer);
        delete iStopwatchContainer;
        iStopwatchContainer = NULL;
        //((CMLauncherAppUi*)AppUi())->ChangeExitWithNaviL(-1);
    }
}

void CStopwatchView::DoActivateL(const TVwsViewId& /*aPrevViewId*/,TUid /*aCustomMessageId*/, const TDesC8& /*aCustomMessage*/)
{
    //we activate the container
	CEikStatusPane *sp=StatusPane();
	if(sp)sp->MakeVisible(ETrue);
	if(!iStopwatchContainer)
    {
        iStopwatchContainer = CStopwatchContainer::NewL( ClientRect(), this);
        iStopwatchContainer->UpdateData(iElapsedSeconds,iStopwatchData.iDuration.Int());
    };
    iStopwatchContainer->SetMopParent(this);
    AppUi()->AddToStackL( *this, iStopwatchContainer );
}

TUid CStopwatchView::Id() const
{
    return TUid::Uid(KStopwatchViewId); //defined in hrh
}

void CStopwatchView::HandleCommandL( TInt aCommand )
{
    //try first to send it to the AppUi for handling
    if(((CMLauncherAppUi*)AppUi())->AppUiHandleCommandL(aCommand,this))
        return; //AppUiHandleCommandL returned TRUE, it handled the command

    //AppUiHandleCommandL did not handle the command
    switch(aCommand)
    {
    case ECommandStopAlarm:
        StopAlarm();
        break;
    case ECommandStartCountdown:
        StartCountdownTimer();
        break;
    case ECommandCountdownSettings:
        CAknForm* dlg = CStopwatchForm::NewL(iStopwatchData);
        dlg->ExecuteLD(R_STOPWATCH_FORM_DIALOG);
        iStopwatchContainer->UpdateData(iElapsedSeconds,iStopwatchData.iDuration.Int());
        iStopwatchContainer->DrawNow();
        break;
    case ECommandExitCountdown:
        StopAlarm();
        ((CMLauncherAppUi*)AppUi())->StopwatchDoneL();
        break;
    default:
        Panic( EMLauncherUi );
    }
}

void CStopwatchView::DynInitMenuPaneL(TInt aResourceId, CEikMenuPane *aMenuPane)
{
    if(aResourceId==R_MENU_STOPWATCH)
        if(iAlarmState==EStatePlaying)
        {
            //dimme something
        }
        else
        {
            //dimme alarm stop
            aMenuPane->SetItemDimmed(ECommandStopAlarm,ETrue);
        };
}

void CStopwatchView::MapcInitComplete(TInt aError, const TTimeIntervalMicroSeconds &aDuration)
{
    if(!aError)
    {
        iAlarmState=EStatePlaying;
        iMdaPlayer->Play();
    }
    else
        iAlarmState=EStateNotInitialized;
}

void CStopwatchView::MapcPlayComplete(TInt /*aError*/)
{
    delete iMdaPlayer;
    iMdaPlayer=NULL;
    iAlarmState=EStateNotInitialized;
}


void CStopwatchView::SoundTheAlarm()
{
    //
    iMdaPlayer=CMdaAudioPlayerUtility::NewFilePlayerL(KAlarmFile,*this);
    iAlarmState=EStateInitializing;
    
#ifdef FL_USE_ACCELEROMETER
    //stop sensor thingie
    iSensrvChannel->StopDataListening(); 
    iSensrvChannel->CloseChannel();
    delete iSensrvChannel;
    iSensrvChannel=NULL;
#endif
}

void CStopwatchView::StopAlarm()
{
    if(iAlarmState==EStatePlaying)
    {
        iMdaPlayer->Stop();
        //mapc play complete will not be called, so we call it ourselves
        MapcPlayComplete(KErrNone);
    }
}

void CStopwatchView::StartCountdownTimer()
{
    //initialize the data
    iStartTime.UniversalTime();
    iElapsedSeconds=0;

    //start the alarm
    if(!iPeriodic)
    {
        iPeriodic = CPeriodic::NewL(CActive::EPriorityStandard);
        TCallBack cb(&PeriodicWorker,this);
        iPeriodic->Start(KOneSecondMs,KOneSecondMs,cb);
    };
    
#ifdef FL_USE_ACCELEROMETER
    //senzor stuff
    CSensrvChannelFinder* sensrvChannelFinder=CSensrvChannelFinder::NewLC(); 
    RSensrvChannelInfoList channelInfoList;
    CleanupClosePushL(channelInfoList);
     
    TSensrvChannelInfo searchConditions; // none, so matches all.
    searchConditions.iChannelType=KSensrvChannelTypeIdAccelerometerXYZAxisData;
    
    sensrvChannelFinder->FindChannelsL(channelInfoList,searchConditions);
    if(channelInfoList.Count())
    {
    	FL_ASSERT(channelInfoList.Count()==1);//at most one channel
    	iStartTimestamp=0;
    	iSensrvChannel=CSensrvChannel::NewL(channelInfoList[0]);
    	iSensrvChannel->OpenChannelL();
    	iSensrvChannel->StartDataListeningL(this,1,1,0);
    }
    else iSensrvChannel=NULL;
     
    channelInfoList.Close();
    CleanupStack::Pop(&channelInfoList);
    CleanupStack::PopAndDestroy(sensrvChannelFinder);
#endif
}

TInt CStopwatchView::PeriodicWorker(TAny *aInstance)
{
    //LOG(ELogGeneral,1,"CMLauncherStopwatchView::PeriodicWorker: start");
    CStopwatchView *stopwatchView=(CStopwatchView*)aInstance;
    TInt ret(1); //more work to do

    stopwatchView->iElapsedSeconds++;
    if(stopwatchView->iStopwatchContainer)
    {
        stopwatchView->iStopwatchContainer->UpdateData(stopwatchView->iElapsedSeconds,stopwatchView->iStopwatchData.iDuration.Int());
        stopwatchView->iStopwatchContainer->DrawNow();
    };
    if(stopwatchView->iElapsedSeconds >= stopwatchView->iStopwatchData.iDuration.Int())
    {
        //we should sound the alarm
        stopwatchView->SoundTheAlarm();
        delete stopwatchView->iPeriodic;
        stopwatchView->iPeriodic=NULL;
        ret=0; //we are done
    };

    //LOG(ELogGeneral,-1,"CMLauncherStopwatchView::PeriodicWorker: end");
    return ret;
}

#ifdef FL_USE_ACCELEROMETER
void CStopwatchView::DataReceived(CSensrvChannel& aChannel, TInt aCount, TInt aDataLost)
 {
    FL_ASSERT(aChannel.GetChannelInfo().iChannelType==KSensrvChannelTypeIdAccelerometerXYZAxisData);

    TPckgBuf<TSensrvAccelerometerAxisData> dataBuf;
    TInt i;
    for(i=0;i<aCount;i++ )
    {
    	aChannel.GetData(dataBuf);
    	const TSensrvAccelerometerAxisData &data=dataBuf();
    	if(!iStartTimestamp)iStartTimestamp=data.iTimeStamp.Int64();
    	LOG0("Accelerometer: %Ld %d %d %d",data.iTimeStamp.Int64()-iStartTimestamp,data.iAxisX,data.iAxisY,data.iAxisZ);	
    }
    if(aDataLost)
    	LOG0("AccelerometerDataLost: %d",aDataLost);
 }

void CStopwatchView::DataError(CSensrvChannel& aChannel, TSensrvErrorSeverity aError)
{
	switch(aError)
	{
	case ESensrvErrorSeverityNotDefined:LOG0("AccelerometerError: unknown severity");break;
	case ESensrvErrorSeverityMinor:LOG0("AccelerometerError: minor");break;
	case ESensrvErrorSeverityFatal:LOG0("AccelerometerError: fatal");break;
	default:LOG0("AccelerometerError: ??? (%d)",aError);break;
	}
}
#endif
