/*
 ============================================================================
 Name        : MusicPlayerView.h
 Author	     : Florin Lohan
 Version	 : 1.0
 Copyright   : Florin Lohan
 Description : CMusicPlayerView declaration
 ============================================================================
 */

#ifndef MUSICPLAYERVIEW_H
#define MUSICPLAYERVIEW_H

// INCLUDES
#include <aknview.h>
#include <MdaAudioSamplePlayer.h>
#include <remconcoreapitargetobserver.h>
#include <remconcoreapi.h>
#include <hwrmlight.h>  //light manager
#include <aknkeylock.h>
#include <eiklbo.h> //list box observer
#include "TrackInfo.h"
#include "MLauncherAppUi.h"
#include "defs.h"

#ifdef __S60_32__
#include <maudiooutputobserver.h>
#endif
#ifdef __S60_50__
#include <accmonitor.h>
#endif

class CAudioEqualizerUtility;
class CBassBoost;
class CStereoWidening;
class CLoudness;

class CMusicPlayerContainer;
class CMLauncherPreferences;
class CRemConInterfaceSelector;
class CRemConCoreApiTarget;
class CMusicPlayerTelObs;
class CMusicPlayerView;
class CThemeManager;
class CTheme;

// CLASS DECLARATION

class CMetadata : public CBase
{
public:
	CMetadata();
	~CMetadata();
	void Recycle();
	void SetStringL(HBufC **aOwnString, const TDesC& aValue);
public:
	HBufC *iTitle;
	HBufC *iArtist;
	HBufC *iAlbum;
	HBufC *iGenre;
	TFileDirEntry *iFileDirEntry;//not owned
	TUint16 iDuration; //in seconds
	TUint16 iTrack;
	TUint16 iYear; 
	TUint16 iKbps;
	HBufC8 *iCover;
};

class CTrack : public CBase, public MMdaAudioPlayerCallback
#ifdef __S60_32__
, public MAudioOutputObserver
#endif
{
public:
	enum TPlayerStates
	{
		EStateNotInitialized=0,
		EStateInitializing,
		EStateInitialized, //or paused or stopped
		EStatePlaying,
		EStateInitializationFailed,
	};
	enum TFlags
	{
		EStartPlaying=1,
		EKilledByTelephony=2,
		ERegistered=4,
		EPauseNeededWhenSeeking=8,
		EVolumeAndEffectsApplied=16
	};

public:
	CTrack(CMusicPlayerView &aView);
	~CTrack();
	void Recycle(); 
	void InitializeL(TFileDirEntry *iEntry, TBool aStartPlaying);
	void StartPlaying(CTrack *aFadeOutTrack);
	void RestartPlaying();
	TBool HasFadeOutTrack(){return (TBool)iFadeOutTrack;}
	
	void Stop();
	void End(); //stops the playing, announces end of song
	TInt Pause();
	void SetPosition(const TTimeIntervalMicroSeconds &aPosition);
	TInt GetPosition(TTimeIntervalMicroSeconds &aPosition);
	const TTimeIntervalMicroSeconds &Duration();
	//TInt GetVolume(TInt &aVolume);
	//void SetVolume(TInt aVolume);
	void SetVolumeFromView();
	void ClearFadeOutTrackIfEqual(CTrack *aTrack){if(aTrack==iFadeOutTrack)iFadeOutTrack=NULL;}
	CMdaAudioPlayerUtility* MdaPlayer(){return iMdaPlayer;}
protected: //from MMdaAudioPlayerCallback
    virtual void MapcInitComplete(TInt aError, const TTimeIntervalMicroSeconds &aDuration);

    virtual void MapcPlayComplete(TInt aError);
    
#ifdef __S60_32__
private: //from MAudioOutputObserver + own
	void DefaultAudioOutputChanged(CAudioOutput& aAudioOutput, CAudioOutput::TAudioOutputPreference aNewDefault);
	//own:
	void RegisterAudioOutput(CMdaAudioPlayerUtility &aUtility);	
#endif
	
private:
	TInt GetMetadata();
	void Play();
public:
	//track info
	TUint iFlags;
	TPlayerStates iState;
	CMetadata iMetadata;
private:
	CTrack *iFadeOutTrack; //not owned
	CMdaAudioPlayerUtility *iMdaPlayer;
	
	CMusicPlayerView &iView;
	TInt iVolumeStep,iMaxVolume; //we compute them once
#ifdef __S60_32__
	CAudioOutput *iAudioOutput;
	CAudioOutput::TAudioOutputPreference iCurrentAudioOutput;
#endif
};

/**
 *  CMusicPlayerView
 *
 */
class CMusicPlayerView : public CAknView, public MRemConCoreApiTargetObserver/*, public MHWRMLightObserver*/
#ifdef __S60_50__
, public MAccMonitorObserver
#endif
{
public: //needed in the container as well
	enum TFlags
	{
		EIsVisible=1,
		//EGoToNextSong=2,  //play the next song when it is done initializing
		//EGoToPrevSong=4,  //play the previous song when it is done initializing
		ESongPreview=8,
		EIsPlaying=16,
		ESeekFirstPress=32,
		EWasPausedBeforeSeek=64,
		//EPeriodicWorkerStopDelayed=128,
		EAllowScrolling=256,
		ECrossfadingCannotBeUsed=512, //because we have BT headset/headphones and crossfading does not work with them
		EReactivateCIdleAfterImgProcessingEnds=1024,

		//last 8 bits used for typematic
		EVolumeUpPressed=0x01000000,
		EVolumeDownPressed=0x02000000,
		EFastForwardPressed=0x04000000,
		ERewindPressed=0x08000000
	};
	enum TIdleWorkerJobs
	{
		EJobConstruct2=1, 
		EJobPrepareNextSong=2, 
		EJobStartCrossfadingTimer=4,
		EJobWaitSomeSeconds=8,
		EJobAllowScrolling=16,
		EJobInitNextSong=32,
	};
	enum TEffects
	{
		EBassBoost=1,
		EStereoWidening,
		ELoudness
	};
public:
	// Constructors and destructor

	/**
	 * Destructor.
	 */
	~CMusicPlayerView();

	/**
	 * Two-phased constructor.
	 */
	static CMusicPlayerView* NewL();

	/**
	 * Two-phased constructor.
	 */
	static CMusicPlayerView* NewLC();

public: // from CAknView

    TUid Id() const;
    void DoActivateL(const TVwsViewId& aPrevViewId,TUid aCustomMessageId, const TDesC8& aCustomMessage);
    void DoDeactivate();

    // from MEikMenuObserver
    void DynInitMenuPaneL(TInt aResourceId, CEikMenuPane *aMenuPane);

	/**
     * From CEikAppUi, HandleCommandL.
     * Takes care of command handling.
     * @param aCommand Command to be handled.
     */
    void HandleCommandL(TInt aCommand);
    void HandleForegroundEventL(TBool aForeground); 
private: 
    //from MRemConCoreApiTargetObserver
	void MrccatoCommand(TRemConCoreApiOperationId aOperationId, TRemConCoreApiButtonAction aButtonAct);
	//from MHWRMLightObserver
	//void LightStatusChanged(TInt aTarget, CHWRMLight::TLightStatus aStatus);
	
public: //own
	CMLauncherAppUi* MyAppUi(){return (CMLauncherAppUi*)AppUi();};

	TInt StartPlaylistL(CPlaylist *aPlaylist, TBool aIsPreview, TBool aStartPlaying);

	TBool PlayPauseStop(TBool aStop); //aStop==ETrue if the command is to stop playback. Function returns ETrue if music is playing

	void NextPreviousL(TInt aJump, TBool aCrossfade=EFalse); //aJump should be either 1 or -1

	TInt Seek(TBool aFirstSeek,TBool aSeekForward, TTimeIntervalMicroSeconds *aPositionObserver=NULL);
	void SeekEnd();//restores the state (play/pause) from before seek started

	void ResumePlayback();

	void VolumeUpDown(TBool aUp);

	void StartTypematicL(TFlags aKeyPressed);
	void StopTypematic();
	static TInt TypematicCallback(TAny* aInstance);
	
	void TrackInitializationComplete(CTrack *aTrack);
	void TrackPlaybackComplete(CTrack *aTrack, TBool aEndedByItself);
	
	void PrepareNextSongL();
	
	void StartCrossfadingTimer();
	
	void CurrentTrackInitializationComplete(); //called by the current theme
	
	void SetStatusPaneVisibility();
	
	void SaveBeforeExit();
	TBool IsCrossfadingAllowed() const;
	TBool IsCrossfadingUsed() const;
	
	// Idle worker
	TInt IdleWorker();
	void ScheduleWorkerL(TInt aJobs);
	TBool IsImgProcessingOngoing();
	
	void ClearPlaylist();
	
	//equalizer & effects
	void EqualizerChanged(TInt aValue); //aValue can be a preset or an effect
	void ApplyEqualizerPresetAndEffectsL(CMdaAudioPlayerUtility *aCurrentTrack); 
	void DeleteEqualizerAndEffectsL(); //applied to current track (iEEcurrent)
	
	CDesCArrayFlat* GetEqualizerPresetsAndEffectsL(TInt aSelected, TBool aBassBoost, TBool aStereoWidening, TBool aLoudness);
	TInt SetEqualizerPresetL(); //the preset to select is in iPreferences
	TInt SetEffect(enum TEffects aEffect); //value taken from preferences
		
#ifdef __MSK_ENABLED__
	void SetMSKPlayPause(TBool aPlay);
#endif
	
#ifdef __S60_50__
private: //from MAccMonitorObserver + own
	void ConnectedL(CAccMonitorInfo* aAccessoryInfo);
	void DisconnectedL(CAccMonitorInfo* aAccessoryInfo);
	void AccMonitorObserverError(TInt /*aError*/){};
	
    void StartMonitoringAccessories();
#endif
	
private:
	//equalizer
    void SelectEqualizerPresetL(); //returns ETrue if the selection changed
	
	void JumpToSongL(TInt aJump, TBool aCrossfade);
	static TInt CrossfadingStarter(TAny* aInstance);
	static TInt WaitFuntion(TAny* aInstance);
	//void CrossfadeToNextSong();
	
	void StartPeriodicWorker();
	void StopPeriodicWorker();
	static TInt PeriodicFunction(TAny* aInstance);
	
private:

	/**
	 * Constructor for performing 1st stage construction
	 */
	CMusicPlayerView();

	/**
	 * EPOC default constructor for performing 2nd stage construction
	 */
	void ConstructL();
	
	void Construct2L();
	
private: // Data
	// idle worker
	CPeriodic *iIdleDelayer;
	TUint iJobsSaved;
	
	CMusicPlayerTelObs *iTelObs;
	//CHWRMLight *iLight; //this is in this class (and not in the container) so we do not initialize it each time we activate the container
	RAknKeylock2 iLock;
	CPeriodic *iTypematic;

#ifdef __MSK_ENABLED__
	//MSK labels
	HBufC *iMskPlay;
	HBufC *iMskPause;
#endif

	//TInt iIndex;

	//seek
	TInt iSeekIndex;

	//volume
	CRemConInterfaceSelector *iInterfaceSelector;
	CRemConCoreApiTarget *iCoreTarget;
	
	//next & previous track
	CTrack *iNextTrack;
	CTrack *iPrevTrack;

	//how many times in a row initialization failed
	TInt iNrFailedTracksInARow;
public:
	CPlaylist *iPlaylist; //not owned
	CTrack *iTrack;
	TTimeIntervalMicroSeconds iPlaybackPosition;
	TTimeIntervalMicroSeconds iTargetPlaybackPosition;
	CPeriodic *iPeriodic;
	TInt iStopDelay;
	//TTime iStartTime; //time when current track started (excluding pause)
	
	//crossfading
	CPeriodic *iCrossfadingTimer;

	TUint iFlags; //public, so the container can access them
	
	//volume
	//TInt *iVolume; //pointer to CMLauncherPreferences::iVolume, not owned
	/**
	 * The container
	 * Owned by us
	 */
	CMusicPlayerContainer* iMusicPlayerContainer;
	CMLauncherPreferences *iPreferences; //not owned, public so that the container can access them
	CThemeManager *iThemeManager;
#ifdef __S60_50__
	CAccMonitor *iAccMonitor;
#endif
	
	//periodic worker
	TUint iJobs; //jobs for the idle worker, see flags above
	
	//equalizer & effects
	CMdaAudioPlayerUtility *iEEcurrent;
	CAudioEqualizerUtility *iEqualizerUtility;
	TInt iNrPresets; //does NOT include the "equalizer off"
	CBassBoost *iBassBoost;
	CStereoWidening *iStereoWidening;
	CLoudness *iLoudness;
};

#endif // MUSICPLAYERVIEW_H
