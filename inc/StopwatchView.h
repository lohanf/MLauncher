/*
 ============================================================================
 Name		: StopwatchView.h
 Author	  : Florin Lohan
 Version	 : 1.0
 Copyright   : Florin Lohan
 Description : CStopwatchView declaration
 ============================================================================
 */

#ifndef STOPWATCHVIEW_H
#define STOPWATCHVIEW_H

//#define FL_USE_ACCELEROMETER 

// INCLUDES
#include <aknview.h>
#include <MdaAudioSamplePlayer.h>
#include "StopwatchSettings.h" //TStopwatchData

#ifdef FL_USE_ACCELEROMETER
#include <sensrvchannel.h>
#include <sensrvdatalistener.h>
#endif

// CLASS DECLARATION
class CStopwatchContainer;
class CMdaAudioPlayerUtility;
/**
 *  CStopwatchView
 *
 */
class CStopwatchView : public CAknView, public MMdaAudioPlayerCallback
#ifdef FL_USE_ACCELEROMETER
, public MSensrvDataListener 
#endif
{
public:
    enum TPlayerStates
    {
        EStateNotInitialized=0,
        EStateInitializing,
        EStatePlaying,
        EStatePaused,
        EStateStopped
    };
public:
    // Constructors and destructor

    /**
     * Destructor.
     */
    ~CStopwatchView();

    /**
     * Two-phased constructor.
     */
    static CStopwatchView* NewL();

    /**
     * Two-phased constructor.
     */
    static CStopwatchView* NewLC();

private:

    /**
     * Constructor for performing 1st stage construction
     */
    CStopwatchView();

    /**
     * EPOC default constructor for performing 2nd stage construction
     */
    void ConstructL();

public: // from CAknView

    TUid Id() const;
    void DoActivateL(const TVwsViewId& aPrevViewId,TUid aCustomMessageId, const TDesC8& aCustomMessage);
    void DoDeactivate();

    /**
     * From CEikAppUi, HandleCommandL.
     * Takes care of command handling.
     * @param aCommand Command to be handled.
     */
    void HandleCommandL( TInt aCommand );

    // from MEikMenuObserver
    virtual void DynInitMenuPaneL(TInt aResourceId, CEikMenuPane *aMenuPane);

public: //from MMdaAudioPlayerCallback
    virtual void MapcInitComplete(TInt aError, const TTimeIntervalMicroSeconds &aDuration);

    virtual void MapcPlayComplete(TInt aError);
    
#ifdef FL_USE_ACCELEROMETER
public: //from MSensrvDataListener
    virtual void DataReceived( CSensrvChannel& aChannel, TInt aCount, TInt aDataLost );
    virtual void DataError( CSensrvChannel& aChannel, TSensrvErrorSeverity aError );
    virtual void GetDataListenerInterfaceL( TUid /*aInterfaceUid*/, TAny*& /*aInterface*/){};
#endif

public: //own
    void SoundTheAlarm();
    void StopAlarm();

    void StartCountdownTimer();

    static TInt PeriodicWorker(TAny *aInstance);

public: //data

    CStopwatchContainer *iStopwatchContainer;
    CMdaAudioPlayerUtility *iMdaPlayer;
    TPlayerStates iAlarmState;

    TStopwatchData iStopwatchData;

    TTime iStartTime;
    TTime iLastTime;
    CPeriodic *iPeriodic;
    TInt iElapsedSeconds;
    
    //sensor
#ifdef FL_USE_ACCELEROMETER
    CSensrvChannel *iSensrvChannel;
    TInt64 iStartTimestamp;
#endif
};

#endif // STOPWATCHVIEW_H
