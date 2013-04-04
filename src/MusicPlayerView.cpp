/*
 ============================================================================
 Name		: MusicPlayer.cpp
 Author	  : Florin Lohan
 Version	 : 1.0
 Copyright   : Florin Lohan
 Description : CMusicPlayerView implementation
 ============================================================================
 */
#include <stringloader.h>
#include <eikbtgpc.h>
#include <eikmenub.h>
#include <mmf\common\MmfMeta.h>
#include <remconcoreapitarget.h> //volume keys
#include <remconinterfaceselector.h> //volume keys
#include <audioequalizerutility.h>  //equalizer
#include <stereowideningbase.h> 
#include <bassboostbase.h> 
#include <loudnessbase.h> 

#include <aknutils.h> 
#include <apgcli.h> // For FF_FMTX
#include <aknnotewrappers.h> //CAknInformationNote
#include <MLauncher.rsg>
#include "MusicPlayerView.h"
#include "MusicPlayerContainer.h"
#include "MusicPlayerImgEngine.h"
#include "MusicPlayerTelObs.h"
#include "MusicPlayerThemes.h"
#include "MLauncherAppUi.h"
#include "MLauncherPreferences.h"
#include "EqualizerDlg.h"
//#include <aknquerydialog.h> //TODO: remove

#include "FFListView.h"
#include "MLauncher.hrh"
#include "MLauncher.pan"
#include "log.h"

const TInt KSeekStepsCount=6;
const TInt KSeekSteps[KSeekStepsCount]={3, 5, 7, 10, 15, 20};
const TInt KTypematicRepeatDelay=500000; //half a second
const TInt KTypematicRepeatRate=300000;

const TInt KPeriodicWorkerStartDelay=600000; //in micro seconds
const TInt KPeriodicWorkerPeriod=200000; //in microseconds, 5 fps //TODO: make this twice per second after creating the scroll timer in themes
const TInt KStopPeriodicWorkerDelayInSeconds=20;
const TInt KStopPeriodicWorkerDelay=KStopPeriodicWorkerDelayInSeconds*(1000000/KPeriodicWorkerPeriod);

const TSize KnHDPortraitSize=TSize(360,640);
const TSize KnHDLandscapeSize=TSize(640,360);

const TInt KCrossfadingMinTime2SleepWhenTooEarly=1000000; //in microseconds

const TInt KWaitingTime=3000000; //in microseconds

const TInt KVolumeRampFromPauseUs=300000;//in microseconds
const TUid KFmTxAppUid={0x10282BEF};
_LIT(KWmaExtension,".wma");
_LIT(KListItemFormat, "%d\t%S");

CMetadata::CMetadata() 
{}

CMetadata::~CMetadata()
{
	delete iTitle;
	delete iArtist;
	delete iAlbum;
	delete iGenre;
	delete iCover;
}

void CMetadata::Recycle()
{
	if(iTitle)iTitle->Des().SetLength(0);
	if(iArtist)iArtist->Des().SetLength(0);
	if(iAlbum)iAlbum->Des().SetLength(0);
	if(iGenre)iGenre->Des().SetLength(0);
	if(iCover){delete iCover; iCover=NULL;};
	iFileDirEntry=NULL;
	
	iDuration=0; //in seconds
	iTrack=0;
	iYear=0; 
	iKbps=0;
}

void CMetadata::SetStringL(HBufC **aOwnString, const TDesC& aValue)
{
	if(*aOwnString==NULL || (*aOwnString)->Des().MaxLength()<aValue.Length())
	{
		delete *aOwnString;
		*aOwnString=HBufC::NewL(aValue.Length());
	};
	(*aOwnString)->Des().Copy(aValue);
	(*aOwnString)->Des().TrimAll();
}

CTrack::CTrack(CMusicPlayerView &aView) : iState(EStateNotInitialized), iView(aView)
{
	//no implementation needed
}
	
CTrack::~CTrack()
{
#ifdef __S60_32__
    delete iAudioOutput;
#endif
    LOG0("CTrack::~CTrack: this=%x, iState=%d",iMdaPlayer,iState);
	if(iMdaPlayer && iState!=EStateNotInitialized)
		iMdaPlayer->Close();
	delete iMdaPlayer;
}

void CTrack::Recycle()
{
#ifdef __S60_32__
    delete iAudioOutput;
    iAudioOutput=NULL;
#endif
    LOG(ELogGeneral,1,"CTrack::Recycle++ (%x, state=%d)",this,this?iState:-1);
    if(iView.iEEcurrent==iMdaPlayer)
    {
    	LOG0("iEEcurrent==this, deleting equalizer & stuff");
    	iView.DeleteEqualizerAndEffectsL();
    }
    if(iMdaPlayer && iState!=EStateNotInitialized)
    {
    	iMdaPlayer->Stop();
    	LOG0("Stopped");
    	iMdaPlayer->Close();
    	LOG0("Closed");
    }
    else LOG0("CTrack::Recycle: iMdaPlayer=%x, iState=%d",(TInt)iMdaPlayer,iState);
    
	iFadeOutTrack=NULL;
	iMetadata.Recycle();
	iState=EStateNotInitialized;
	iFlags=0;
	LOG(ELogGeneral,-1,"CTrack::Recycle--");
}

void CTrack::InitializeL(TFileDirEntry *aEntry, TBool aStartPlaying)
{
	LOG(ELogGeneral,1,"CTrack::InitializeL++");
	Recycle();
	
	__ASSERT_ALWAYS(aEntry && !(aEntry->iFlags&TFileDirEntry::EDeleted),Panic(ETrackFileIsDeleted));
	iMetadata.iFileDirEntry=aEntry;
	
	TFileName fullPath;
	aEntry->GetFullPath(fullPath);
	__ASSERT_ALWAYS(fullPath.Right(2)!=KEOL, Panic(EMusicFilenameEndsWithEOL));
	
	if(!iMdaPlayer)
		iMdaPlayer=CMdaAudioPlayerUtility::NewL(*this);
		
	iMdaPlayer->OpenFileL(fullPath);
#ifdef _FLL_SDK_VERSION_50_
	/* In 5.0 the wma playback changed, and the track needs to be now paused before its position gets changed
	 * 
	 */
	TBuf<4> ext(fullPath.Right(4));
	ext.LowerCase();
	if(ext==KWmaExtension)
		iFlags|=EPauseNeededWhenSeeking;
#endif
	iState=EStateInitializing;
	if(aStartPlaying)
		iFlags|=EStartPlaying;
	else
		iFlags&=~EStartPlaying;
	
	LOG(ELogGeneral,-1,"CTrack::InitializeL--");
}


TInt CTrack::GetMetadata()
{
	TInt nrMetadata(0);
	TInt i;
	iMdaPlayer->GetNumberOfMetaDataEntries(nrMetadata);
	CMMFMetaDataEntry* entry(NULL);

	TRAPD(err,
	for(i=0;i<nrMetadata;i++)
	{
		entry=iMdaPlayer->GetMetaDataEntryL(i);
		if(entry && entry->Value().Length()>0)
		{
			//check this metadata entry
			if(entry->Name()==KMMFMetaEntrySongTitle)
				iMetadata.SetStringL(&iMetadata.iTitle,entry->Value());
			else if(entry->Name()==KMMFMetaEntryArtist)
				iMetadata.SetStringL(&iMetadata.iArtist,entry->Value());
			else if(entry->Name()==KMMFMetaEntryAlbum)
				iMetadata.SetStringL(&iMetadata.iAlbum,entry->Value());
			else if(entry->Name()==KMMFMetaEntryGenre)
				iMetadata.SetStringL(&iMetadata.iGenre,entry->Value());
			else if(entry->Name()==KMMFMetaEntryAlbumTrack)
			{
				TLex lex(entry->Value());
				lex.Val(iMetadata.iTrack,EDecimal);
			}
			else if(entry->Name()==KMMFMetaEntryYear)
			{
				TLex lex(entry->Value());
				lex.Val(iMetadata.iYear,EDecimal);
			}
			else if(entry->Name()==KMMFMetaEntryAPIC)
			{
				LOG0("We have an embedded picture, lets use it (size: %d)!",entry->Value().Length());
				if(iMetadata.iCover)delete iMetadata.iCover;
				iMetadata.iCover=HBufC8::NewL(entry->Value().Length());
				iMetadata.iCover->Des().Copy(entry->Value());
			}
			delete entry;
		};
	};
	);//TRAPD
	//bitrate
	TUint bitrate;
	err=iMdaPlayer->GetBitRate(bitrate);
	if(!err)
		iMetadata.iKbps=bitrate>>10;

	return KErrNone;
}

void CTrack::SetVolumeFromView()
{
	FL_ASSERT(iVolumeStep>0);
	FL_ASSERT(iMaxVolume>0);
	
	TInt myVolume,currentVolume;
	if(iView.iPreferences->iVolume<0)
	{
		//set volume to 25% of maximum volume
		LOG0("Preferences volume found to be invalid (%d)",iView.iPreferences->iVolume);
		myVolume=iMaxVolume>>2; 
		iView.iPreferences->iVolume=myVolume/iVolumeStep; 	
	}
	else myVolume=iVolumeStep*iView.iPreferences->iVolume;
	iMdaPlayer->GetVolume(currentVolume);
	if(myVolume!=currentVolume)
	{
		iMdaPlayer->SetVolume(myVolume);
		LOG0("SetVolumeFromView: %d (before: %d) (corresponding to %d/%d)",myVolume,currentVolume,iView.iPreferences->iVolume,iView.iPreferences->iMaxVolume);
	}
	else 
		LOG0("SetVolumeFromView: volume to set is same as current (%d) (corresponding to %d/%d)",myVolume,iView.iPreferences->iVolume,iView.iPreferences->iMaxVolume);
}
void CTrack::StartPlaying(CTrack *aFadeOutTrack)
{
	LOG(ELogGeneral,1,"CTrack::StartPlaying++");
	if(iState==EStateNotInitialized || iState==EStateInitializing)
	{
		iFlags|=EStartPlaying;
		LOG(ELogGeneral,-1,"CTrack::StartPlaying-- (not yet initialized, it will start playing when initialization finishes)");
		return;
	}
		
	//set the volume
	SetVolumeFromView();
	
	if(aFadeOutTrack)
		iMdaPlayer->SetVolumeRamp(iView.iPreferences->iCrossfadingTimeVolRampUpMs*1000);
	else 
		iMdaPlayer->SetVolumeRamp(KVolumeRampFromPauseUs);
	//start playing
	Play();
	iFadeOutTrack=aFadeOutTrack; //after issuing the Play, so not to affect the playback of the FadeOutTrack
	
	//equalizer and effects
	
	iFlags|=EVolumeAndEffectsApplied;
	LOG(ELogGeneral,-1,"CTrack::StartPlaying--");
}

void CTrack::MapcInitComplete(TInt aError, const TTimeIntervalMicroSeconds &aDuration)
{
	LOG(ELogGeneral,1,"CTrack::MapcInitComplete: start (received duration: %d)",(TInt)(aDuration.Int64()/1000000));
	if(aError)LOG0("MapcInitComplete error: %d state=%d",aError,iState);
	__ASSERT_DEBUG(iState==EStateInitializing,Panic(EMusicTrackWrongState));
	if(aError)
    {
    	//there was an error opening this file, try the next one
		iState=EStateInitializationFailed;
    	iView.TrackInitializationComplete(this);
    	LOG(ELogGeneral,-1,"CTrack::MapcInitComplete: end (in error) state=%d",iState);
    	return;
    };

    //if we are here, opening the file succeeded
#ifdef __S60_32__
	if(!(iFlags&ERegistered))
	{
		RegisterAudioOutput(*iMdaPlayer);
		iFlags|=ERegistered;
	};
#endif

	//compute the volume step used to change the volume up and down
	iMaxVolume=iMdaPlayer->MaxVolume();
	if(iMaxVolume<=iView.iPreferences->iMaxVolume)
		iVolumeStep=1;
	else
	{
		iVolumeStep=iMaxVolume/iView.iPreferences->iMaxVolume;
		if(iVolumeStep<1)iVolumeStep=1; //this should not be necessary
	};
	if(iView.iPreferences->iVolume*iVolumeStep>iMaxVolume)iView.iPreferences->iVolume=iMaxVolume/iVolumeStep;
	LOG0("Max volume: %d, volume step: %d, nr volume steps: %d",iMaxVolume,iVolumeStep,iView.iPreferences->iMaxVolume);

	
	SetVolumeFromView();
	
	//volume is set in StartPlaying
    
    if(iView.iTargetPlaybackPosition!=0)
    {
    	//set the position
    	iMdaPlayer->SetPosition(iView.iTargetPlaybackPosition);
    	iView.iTargetPlaybackPosition=0;
    };
    
    iState=EStateInitialized;
    if(iFlags&EStartPlaying)
    {
    	StartPlaying(NULL);
#ifdef __MSK_ENABLED__
    	if(iView.iTrack==this)
    		iView.SetMSKPlayPause(EFalse);
#endif
    }
#ifdef __MSK_ENABLED__
    else 
    {
    	if(iView.iTrack==this)
    		iView.SetMSKPlayPause(ETrue);
    }
#endif
    LOG0("CTrack::MapcInitComplete: state=%d",iState);

    //get the duration in seconds
    if(iMetadata.iFileDirEntry && iMetadata.iFileDirEntry->iNrMusicFiles>0)
    	iMetadata.iDuration=iMetadata.iFileDirEntry->iNrMusicFiles/1000; //this should be more accurate
    else
    	iMetadata.iDuration=aDuration.Int64()/1000000;
    //get metadata
    GetMetadata();
    
    iView.TrackInitializationComplete(this);
    LOG(ELogGeneral,-1,"CTrack::MapcInitComplete: end");
}

void CTrack::MapcPlayComplete(TInt aError)
{
	if(aError)
		LOG0("MapcPlayComplete error: %d",aError);
	iState=EStateInitialized;
	if(aError==KErrDied || aError==KErrInUse)
	{
		//we were killed by telephony, perhaps. If we were killed by telephony, then the telephony observer sets the "KilledByTelephony" flag
		//otherwise there is no flag, and we just stop playing (the user will need to manually restart later)
		//get our position, so we know where to resume playback
		iView.iTargetPlaybackPosition=iView.iPlaybackPosition;
		LOG0("Playback stopped with error (pos=%d), perhaps killed by telephony.",(TInt)(iView.iTargetPlaybackPosition.Int64()/1000000));
	}
	else
	{
		if(aError==KErrNone)
		{
			//store the duration of the song
			TInt duration=(TInt)(iMdaPlayer->Duration().Int64()/1000);
			if(iMetadata.iFileDirEntry && iMetadata.iFileDirEntry->iNrMusicFiles!=duration)
			{
				//update the duration
				iMetadata.iFileDirEntry->iNrMusicFiles=duration;
				iView.iPlaylist->MetadataUpdated(); //this will force a save of the pool's metadata
			};
		};
		if(iFadeOutTrack)iFadeOutTrack=NULL;
		iView.TrackPlaybackComplete(this,ETrue);
	}
}

#ifdef __S60_32__
void CTrack::RegisterAudioOutput(CMdaAudioPlayerUtility &aUtility)
{
	__ASSERT_DEBUG(iAudioOutput==NULL,Panic(EAudioOutputNotNull));
	iAudioOutput=CAudioOutput::NewL(aUtility);
	iAudioOutput->RegisterObserverL(*this);
	iCurrentAudioOutput=iAudioOutput->AudioOutput();
}

void CTrack::DefaultAudioOutputChanged(CAudioOutput& aAudioOutput, CAudioOutput::TAudioOutputPreference aNewDefault)
{
	LOG0("New audio output: %d, old audio output: %d",aNewDefault,iCurrentAudioOutput);
	__ASSERT_DEBUG(iAudioOutput== &aAudioOutput,Panic(EWrongAudioOutput));
	
	if(aNewDefault==CAudioOutput::EPublic && iState==CTrack::EStatePlaying && (iCurrentAudioOutput==CAudioOutput::EPrivate || iCurrentAudioOutput==CAudioOutput::ENoPreference))
	{
		iView.PlayPauseStop(EFalse);
		LOG0("Track paused");
	};
	iCurrentAudioOutput=aNewDefault;
}

#endif

void CTrack::End()
{
	if(iMdaPlayer)
	{
		iMdaPlayer->Stop();
		iState=EStateInitialized;
		LOG0("iMdaPlayer stopped");
	}
	iFadeOutTrack=NULL;
	iView.TrackPlaybackComplete(this,EFalse);
}

void CTrack::Stop()
{
	if(iMdaPlayer)iMdaPlayer->Stop();
	iState=EStateInitialized;
	
	if(iFadeOutTrack)
	{
		iFadeOutTrack->End();//we will not restart the fadeOutTrack
		FL_ASSERT(!iFadeOutTrack);
	}
	// we only cancel the iCrossfadingTimer if there is no fadeOutTrack, because it is going to be cancelled in the fadeOutTrack
	else if(iView.iCrossfadingTimer)iView.iCrossfadingTimer->Cancel();
}

void CTrack::Play()
{
	FL_ASSERT_TXT(iState==EStateInitialized,"State (%d) is not Initialized",iState); 
	FL_ASSERT(iMdaPlayer);
	
	//iMdaPlayer->SetVolumeRamp(0); -> this kills crossfading, why is it here?
	iMdaPlayer->Play();
	iState=EStatePlaying;
	//set the volume ramp-up to zero, so in case the song is stopped when effects are applied (e.g. BT headphones)
	//when it gets restarted there is no fading-in
	iMdaPlayer->SetVolumeRamp(0);
	
	if(iFadeOutTrack)
		iFadeOutTrack->Play();
	//else iView->StartCrossfadingTimer();
}

void CTrack::RestartPlaying()
{
	if(iFlags&EVolumeAndEffectsApplied)
	{
		if(iView.iTargetPlaybackPosition!=0)
		{
			//set the position
			iMdaPlayer->SetPosition(iView.iTargetPlaybackPosition);
			iView.iTargetPlaybackPosition=0;
			if(iFadeOutTrack)
				iFadeOutTrack->End();
			FL_ASSERT(!iFadeOutTrack);
		};
		Play();
	}
	else StartPlaying(NULL);
}

TInt CTrack::Pause()
{
	TInt err(KErrNotReady);
	if(iMdaPlayer)err=iMdaPlayer->Pause();
	iState=EStateInitialized;
	
	if(iFadeOutTrack)iFadeOutTrack->Pause();
	// we only cancel the iCrossfadingTimer if there is no fadeOutTrack, because it is going to be cancelled in the fadeOutTrack
	else if(iView.iCrossfadingTimer)iView.iCrossfadingTimer->Cancel();
	
	return err;
}

void CTrack::SetPosition(const TTimeIntervalMicroSeconds &aPosition)
{
	LOG(ELogGeneral,1,"CTrack::SetPosition: start (position: %d)",(TInt)aPosition.Int64());
	if(iFadeOutTrack)
	{
		//we will not restart the fadeOutTrack
		iFadeOutTrack->End();
		FL_ASSERT(!iFadeOutTrack);
	};
	if(iMdaPlayer)
	{
		//pause first, if needed
		TBool wasPlaying(ETrue);
		if(iFlags&EPauseNeededWhenSeeking)
		{
			if(iState!=EStatePlaying)
				wasPlaying=EFalse;
			else
				Pause();//stop playing
		};

		iMdaPlayer->SetPosition(aPosition);
		if((iFlags&EPauseNeededWhenSeeking) && wasPlaying)
			Play();
		
		if(iState==EStatePlaying)
			iView.ScheduleWorkerL(CMusicPlayerView::EJobStartCrossfadingTimer);
	};
	LOG(ELogGeneral,-1,"CTrack::SetPosition: end"); 
}
TInt CTrack::GetPosition(TTimeIntervalMicroSeconds &aPosition)
{
	if(iMdaPlayer)return iMdaPlayer->GetPosition(aPosition);
	else return KErrNotReady;
}
	
const TTimeIntervalMicroSeconds& CTrack::Duration()
{
	return iMdaPlayer->Duration();
}

/*
TInt CTrack::GetVolume(TInt &aVolume)
{
	if(iMdaPlayer)return iMdaPlayer->GetVolume(aVolume)/iVolumeStep;
	else return KErrNotReady;
}

void CTrack::SetVolume(TInt aVolume)
{
	if(iMdaPlayer)iMdaPlayer->SetVolume(aVolume*iVolumeStep);
}
*/
///////////////////////////////////////////////////////////////////////	

CMusicPlayerView::CMusicPlayerView()
{
	// No implementation required
}

CMusicPlayerView::~CMusicPlayerView()
{
	LOG(ELogGeneral,1,"CMusicPlayerView::~CMusicPlayerView++");
	//typematic
	if(iTypematic)
	{
		iTypematic->Cancel();
		delete iTypematic;
	};
	
	if(iPeriodic)
	{
		iPeriodic->Cancel();
		delete iPeriodic;
	};
	
	if(iCrossfadingTimer)
	{
		iCrossfadingTimer->Cancel();
		delete iCrossfadingTimer;
	};
	
	//equalizer and effects (BEFORE deleting tracks)
	LOG0("Before deleting equalizer and effects");
	DeleteEqualizerAndEffectsL();
	LOG0("After deleting equalizer and effects");
		
	//tracks
	delete iPrevTrack;
	delete iTrack;
	delete iNextTrack;
	LOG0("After deleting tracks");

	//delete iCurrentFilename;
#ifdef __MSK_ENABLED__
    delete iMskPlay;
    delete iMskPause;
#endif
    delete iTelObs;
    delete iThemeManager;

    //RemCon
    delete iInterfaceSelector;
    // Don't delete target, it is owned by the interface selector.
    //delete iCoreTarget;

    //light
    //delete iLight;
    iLock.Close();
    
    //idle worker
    if(iIdleDelayer)
    {
    	iIdleDelayer->Cancel();
    	delete iIdleDelayer;
    };
#ifdef __S60_50__
	delete iAccMonitor;
#endif
	LOG(ELogGeneral,-1,"CMusicPlayerView::~CMusicPlayerView--");
}

CMusicPlayerView* CMusicPlayerView::NewLC()
{
	CMusicPlayerView* self = new (ELeave) CMusicPlayerView();
	CleanupStack::PushL(self);
	self->ConstructL();
	return self;
}

CMusicPlayerView* CMusicPlayerView::NewL()
{
	CMusicPlayerView* self = CMusicPlayerView::NewLC();
	CleanupStack::Pop(); // self;
	return self;
}

void CMusicPlayerView::ConstructL()
{
	// construct from resource
	LOG(ELogGeneral,1,"CMusicPlayerView::ConstructL: start");
	BaseConstructL(R_MUSICPLAYER_VIEW_NAVI);
#ifdef __MSK_ENABLED__
	if(!iMskPlay)
	{
		iMskPlay=StringLoader::LoadL(R_MSK_PLAY);
		iMskPause=StringLoader::LoadL(R_MSK_PAUSE);
	};
#endif
	iPreferences=((CMLauncherDocument*)MyAppUi()->Document())->iPreferences;
	iThemeManager=CThemeManager::NewL(this,iEikonEnv);

	ScheduleWorkerL(EJobConstruct2);
	LOG(ELogGeneral,-1,"CMusicPlayerView::ConstructL: end");
}

void CMusicPlayerView::Construct2L()
{
	//telephony observer
	iTelObs=CMusicPlayerTelObs::NewL(*this);
	
	//RemCon (volume keys)
	iInterfaceSelector = CRemConInterfaceSelector::NewL();
	iCoreTarget = CRemConCoreApiTarget::NewL(*iInterfaceSelector, *this);
	iInterfaceSelector->OpenTargetL();

	//light stuff
	//iLight=CHWRMLight::NewL(this);
	
#ifdef __S60_50__
	//start monitoring accessories
	StartMonitoringAccessories();
#endif

	//TODO: disable key lock 
	iLock.Connect();
	iLock.DisableWithoutNote();
	
	//if we have one of the nHD themes, load the other one 
	if(iThemeManager->iThemes.Count()==1)
	{
		if(iThemeManager->iThemes[0]->iMySize==KnHDPortraitSize)
		{
			//instantiate the landscape theme
			/*CTheme *theme=*/iThemeManager->GetThemeL(TRect(TPoint(0,0),KnHDLandscapeSize),NULL);
		}
		else if(iThemeManager->iThemes[0]->iMySize==KnHDLandscapeSize)
		{
			//instantiate the portrait theme
			/*CTheme *theme=*/iThemeManager->GetThemeL(TRect(TPoint(0,0),KnHDPortraitSize),NULL);
		};
	};
}

#ifdef __MSK_ENABLED__
void CMusicPlayerView::SetMSKPlayPause(TBool aPlay)
{
	if(aPlay)
		Cba()->SetCommandL(EAknSoftkeyForwardKeyEvent,*iMskPlay);
	else
		Cba()->SetCommandL(EAknSoftkeyForwardKeyEvent,*iMskPause);
	Cba()->DrawDeferred();
}
#endif

#ifdef __S60_50__
void CMusicPlayerView::StartMonitoringAccessories()
{
	iAccMonitor=CAccMonitor::NewL();
	iAccMonitor->StartObservingL(this);
}

void CMusicPlayerView::ConnectedL(CAccMonitorInfo* aAccessoryInfo)
{
	TAccMonCapability cap(aAccessoryInfo->AccPhysicalConnection());
	if(cap==KAccMonBluetooth)
	{
		//we need to temporarily disable crossfading
		if(IsCrossfadingUsed())
		{
			HBufC *txt=StringLoader::LoadLC(R_INFO_CROSSFADING_DISABLED);
			CAknInformationNote *note=new (ELeave)CAknInformationNote;
			note->ExecuteLD(*txt);
			CleanupStack::PopAndDestroy(txt);
		}
		iFlags|=ECrossfadingCannotBeUsed;
		LOG0("BT accessory connected, disabling crossfading");
	}
}

void CMusicPlayerView::DisconnectedL(CAccMonitorInfo* aAccessoryInfo)
{
	TAccMonCapability cap(aAccessoryInfo->AccDeviceType());
	LOG0("Some device was disconnected: %d",cap);
	
	if(iTrack->iState==CTrack::EStatePlaying && (cap==KAccMonHeadset || cap==KAccMonCarKit || cap==KAccMonLoopset || cap==KAccMonAVDevice))
		PlayPauseStop(EFalse);

	cap=aAccessoryInfo->AccPhysicalConnection();
	if(cap==KAccMonBluetooth)
	{
		//we can enable crossfading again
		iFlags&=~ECrossfadingCannotBeUsed;
		LOG0("BT accessory disconnecting, allowing crossfading");
		if(IsCrossfadingUsed())
		{
			StartCrossfadingTimer();
			HBufC *txt=StringLoader::LoadLC(R_INFO_CROSSFADING_ENABLED);
			CAknInformationNote *note=new (ELeave)CAknInformationNote;
			note->ExecuteLD(*txt);
			CleanupStack::PopAndDestroy(txt);
		}
	}
}
#endif

void CMusicPlayerView::DoDeactivate()
{
	LOG(ELogGeneral,1,"CMusicPlayerView::DoDeactivate++");
	if(iMusicPlayerContainer)
    {
		iMusicPlayerContainer->iCurrentTheme->Deactivate();
		AppUi()->RemoveFromStack(iMusicPlayerContainer);
        delete iMusicPlayerContainer;
        iMusicPlayerContainer = NULL;
    };
	LOG(ELogGeneral,-1,"CMusicPlayerView::DoDeactivate--");
}

void CMusicPlayerView::DoActivateL(const TVwsViewId& /*aPrevViewId*/,TUid /*aCustomMessageId*/, const TDesC8& /*aCustomMessage*/)
{
	LOG(ELogGeneral,1,"CMusicPlayerView::DoActivateL++");
	SetStatusPaneVisibility();
	
	//we activate the container
    if(!iMusicPlayerContainer)
    {
    	
    	iMusicPlayerContainer = CMusicPlayerContainer::NewL( ClientRect(), this);
    	if(iTrack && iTrack->iMetadata.iFileDirEntry && (iTrack->iState==CTrack::EStateInitialized || iTrack->iState==CTrack::EStatePlaying))
    		iMusicPlayerContainer->iCurrentTheme->SetMetadataL(iTrack->iMetadata,iPlaybackPosition);
    };
    AppUi()->AddToStackL( *this, iMusicPlayerContainer );
    iMusicPlayerContainer->iCurrentTheme->ActivateL();
    LOG(ELogGeneral,-1,"CMusicPlayerView::DoActivateL--");
}

TUid CMusicPlayerView::Id() const
{
    return TUid::Uid(KMusicPlayerViewId); //defined in hrh
}

void CMusicPlayerView::SetStatusPaneVisibility()
{

#ifdef __TOUCH_ENABLED__
	CEikStatusPane *sp(StatusPane());
	if(sp)sp->MakeVisible(EFalse);
	CEikButtonGroupContainer *cba(Cba());
	if(cba)cba->MakeVisible(EFalse);
#else
	TRect clientRect(ClientRect());
	TInt height(clientRect.iBr.iY);
	CEikStatusPane *sp(StatusPane());
	if(sp && (clientRect.Width()<=height || (iPreferences->iCFlags&CMLauncherPreferences::EHwCanDoLandscapeWithoutTitlebar)))
		sp->MakeVisible(EFalse);
	else 
		sp->MakeVisible(ETrue);
#endif
}

/*
void CMusicPlayerView::HandleViewRectChange()
{
	LOG("CMusicPlayerView::HandleStatusPaneSizeChange(): start");
	
	if(iMusicPlayerContainer)
	{
		LOG("We have a container");
		iMusicPlayerContainer->iCurrentTheme->Deactivate();
		LOG("Deactivation succeeded");
		TRect r(ClientRect());
		TInt height(r.iBr.iY);
		LOG("Setting status pane");
		if(r.Width()<=height || iPreferences->iFlags&CMLauncherPreferences::EHwCanDoLandscapeWithoutTitlebar)
		{
			CEikStatusPane *statusPane=StatusPane();
			statusPane->MakeVisible(EFalse);
			r=ClientRect();
			LOG("Status pane not visible");
		}
		else
		{
			CEikStatusPane *statusPane=StatusPane();
			statusPane->MakeVisible(ETrue);
			r=ClientRect();
			LOG("Status pane visible");
		};
		LOG("Setting new rect: %dx%d",r.Width(),r.Height());
		iMusicPlayerContainer->SetRect(r);
		LOG("SetRect succeeded");
		if(iTrack && iTrack->iIndex>=0 && iTrack->iMetadata.iFilename)
		{
			iMusicPlayerContainer->iCurrentTheme->SetMetadataL(iTrack->iMetadata,iPlaybackPosition);
			LOG("SetMetadata succeeded");
		};
		LOG("Will activate the new theme");
		iMusicPlayerContainer->iCurrentTheme->ActivateL();
		LOG("Theme activated successfully");
		iMusicPlayerContainer->DrawDeferred();
	};
	LOG("CMusicPlayerView::HandleStatusPaneSizeChange(): end");
}
*/



void CMusicPlayerView::HandleCommandL( TInt aCommand )
{
    //try first to send it to the AppUi for handling
    if(MyAppUi()->AppUiHandleCommandL(aCommand,this))
        return; //AppUiHandleCommandL returned TRUE, it handled the command

    //AppUiHandleCommandL did not handle the command
    switch(aCommand)
    {
    case ECommandMusicPlayerStop:
    	PlayPauseStop(ETrue);
    	break;
    	
    case ECommandAudioSettingsEqualizerAndEffects:
    	SelectEqualizerPresetL();
    	break;
    case ECommandMusicPlayerAudioSettingsCrossfading:
    	LOG0("Flags before crossfading changed: %x",iPreferences->iPFlags);
    	FL_ASSERT(IsCrossfadingAllowed());
    	iPreferences->iPFlags^=CMLauncherPreferences::EPreferencesUseCrossfading;
    	LOG0("Flags _after crossfading changed: %x",iPreferences->iPFlags);
    	iPreferences->SavePreferences();
    	break;
    case ECommandMusicPlayerFmTx:
        {
        	TApaTaskList tasList( iCoeEnv->WsSession() );
        	TApaTask task = tasList.FindApp(KFmTxAppUid);
        	if ( task.Exists() )
        	{
        		task.BringToForeground();
        	}
        	else
        	{
        		TApaAppInfo appInfo;
        		RApaLsSession session;
        		if(!session.Connect())
        		{
        			CleanupClosePushL(session);
        			TThreadId threadId;
        			session.CreateDocument(KNullDesC, KFmTxAppUid, threadId);
        			CleanupStack::PopAndDestroy(&session);
        		};
        	};
        	break;
        };
    case ECommandMusicPlayerPlaylistRemoveCurrentItem:
    {
    	TBool isPlaying(iFlags&EIsPlaying);
    	PlayPauseStop(ETrue);//stop current track
    	CPlaylist *pls=iPlaylist;//save the current playlist
    	FL_ASSERT(pls);
    	pls->RemoveCurrentElement();
    	pls->SaveCurrentPlaylist(*(CMLauncherDocument*)AppUi()->Document());
    	StartPlaylistL(pls,iFlags&ESongPreview,isPlaying);
    };break;
    case ECommandMusicPlayerPlaylistReshuffleAll:
    {
    	TBool isPlaying(iFlags&EIsPlaying);
    	PlayPauseStop(ETrue);//stop current track
    	CPlaylist *pls=iPlaylist;//save the current playlist
    	FL_ASSERT(pls);
    	pls->Shuffle();
    	pls->SaveCurrentPlaylist(*(CMLauncherDocument*)AppUi()->Document());
    	StartPlaylistL(pls,iFlags&ESongPreview,isPlaying);
    };break;
    case ECommandMusicPlayerPlaylistReshuffleRemaining:
    	if(iTrack)
    		iTrack->ClearFadeOutTrackIfEqual(iNextTrack);
    	if(iNextTrack)
    		iNextTrack->Recycle();
    	 		
    	iPlaylist->Shuffle(ETrue);
    	iPlaylist->SaveCurrentPlaylist(*(CMLauncherDocument*)AppUi()->Document());
    	
    	ScheduleWorkerL(EJobWaitSomeSeconds|EJobStartCrossfadingTimer|EJobPrepareNextSong);
    	break;
    case ECommandMusicPlayerLoopOff:
    	//clear all loop flags
    	iPreferences->iPFlags&=~CMLauncherPreferences::EPreferencesPlaylistLoop;
    	iPreferences->iPFlags&=~CMLauncherPreferences::EPreferencesSongLoop;
    	iNextTrack->Recycle();
    	PrepareNextSongL();
    	break;
    case ECommandMusicPlayerLoopPlaylist:
    	iPreferences->iPFlags|=CMLauncherPreferences::EPreferencesPlaylistLoop;
    	if(iPreferences->iPFlags&CMLauncherPreferences::EPreferencesSongLoop)
    	{
    		iPreferences->iPFlags&=~CMLauncherPreferences::EPreferencesSongLoop;
    		iNextTrack->Recycle();
    		PrepareNextSongL();
    	};
    	break;
    case ECommandMusicPlayerLoopSong:
    	iPreferences->iPFlags&=~CMLauncherPreferences::EPreferencesPlaylistLoop;
    	iPreferences->iPFlags|=CMLauncherPreferences::EPreferencesSongLoop;
    	iNextTrack->Recycle();
    	PrepareNextSongL();
    	break;
    case EAknSoftkeyBack:
    {
    	//iTrack->Stop(); not needed any more
    	//iTrack->iState=CTrack::EStateInitialized; not needed any more
    	MyAppUi()->CloseMusicPlayerViewL();
    };break;
    case ECommandMusicPlayerHelpHowTo:
    {
    	//we display some help on how to use the player
#ifdef __TOUCH_ENABLED__
    	if(AknLayoutUtils::PenEnabled())
    		MyAppUi()->DisplayQueryOrNoteL(EMsgBigNote,R_MUSIC_PLAYER_HELP_TEXT_TOUCH,R_MUSIC_PLAYER_HELP_HEADER);//touch UI
    	else
#endif
    		if(iPreferences->iCFlags&CMLauncherPreferences::EHwDeviceHasVolumeKeys)
    			MyAppUi()->DisplayQueryOrNoteL(EMsgBigNote,R_MUSIC_PLAYER_HELP_TEXT_WITH_VOLUME_KEYS,R_MUSIC_PLAYER_HELP_HEADER);//non-touch, with volume keys
    		else //non-touch, without volume keys
    			MyAppUi()->DisplayQueryOrNoteL(EMsgBigNote,R_MUSIC_PLAYER_HELP_TEXT_WITHOUT_VOLUME_KEYS,R_MUSIC_PLAYER_HELP_HEADER);
    };break;
    case ECommandMusicPlayerHelpAlbumArt:
    	MyAppUi()->DisplayQueryOrNoteL(EMsgBigNote,R_MUSIC_PLAYER_ALBUMART_HOWTO_TEXT,R_MUSIC_PLAYER_ALBUMART_HOWTO_HEADER);
    	break;
    case ECommandMusicPlayerPreferencesButtonsInsteadOfDrag:
    	if(MyAppUi()->DisplayQueryOrNoteL(EMsgQueryYesNo,R_QUERY_BUTTONSDRAG))
    		iPreferences->iPFlags|=CMLauncherPreferences::EPreferencesPlayerUseIconsInsteadOfDrag;
    	else
    		iPreferences->iPFlags&=~CMLauncherPreferences::EPreferencesPlayerUseIconsInsteadOfDrag;
    	if(iMusicPlayerContainer)
    		iMusicPlayerContainer->DrawDeferred();
    	iPreferences->SavePreferences();
    	break;	
    case ECommandMusicPlayerPreferencesCoverHintFiles:
    	if(MyAppUi()->DisplayQueryOrNoteL(EMsgQueryYesNo,R_QUERY_COVERHINTFILES))
    		iPreferences->iPFlags|=CMLauncherPreferences::EPreferencesCreateCoverHintFiles;
    	else
    	{
    		iPreferences->iPFlags&=~CMLauncherPreferences::EPreferencesCreateCoverHintFiles;
    		//check for existing cover hint files
    		if(iThemeManager->iImgEngine->AreThereCoverHintFiles())
    			if(MyAppUi()->DisplayQueryOrNoteL(EMsgQueryYesNo,R_QUERY_DELETE_COVERHINTFILES))
    				iThemeManager->iImgEngine->AreThereCoverHintFiles(ETrue);
    	}
    	iPreferences->SavePreferences();
    	break;
    case EAknSoftkeyOptions:
    	{
    		//show the navi menu
    		CEikMenuBar *myMenuBar = MenuBar();
    		if(myMenuBar)
    		{
#ifdef _FLL_SDK_VERSION_30_		
    			myMenuBar->SetMenuTitleResourceId(R_MENUBAR_MUSICPLAYER);
#else
    			myMenuBar->SetContextMenuTitleResourceId(R_MENUBAR_MUSICPLAYER);
#endif
    			myMenuBar->TryDisplayMenuBarL();
    		};
    	};break;
    
    default:
    	if(iMusicPlayerContainer && iMusicPlayerContainer->iCurrentTheme)
    	{
    		iMusicPlayerContainer->iCurrentTheme->HandleCommandL(aCommand);
    		CTheme *otherTheme=iThemeManager->GetTheOtherTheme(iMusicPlayerContainer->iCurrentTheme);
    		if(otherTheme)
    			otherTheme->HandleCommandL(aCommand);
    	}
    	else
    		Panic( EMLauncherUi );
    }
}

void CMusicPlayerView::TrackInitializationComplete(CTrack *aTrack)
{
	LOG(ELogGeneral,1,"TrackInitializationComplete: start (state: %d)",aTrack->iState);
	if(aTrack==iTrack)
	{
		LOG0("TrackInitializationComplete: this is current track");
		//check for errors 
		if(iTrack->iState==CTrack::EStateInitializationFailed)
		{
			iNrFailedTracksInARow++;
			if(iPlaylist && iNrFailedTracksInARow>=iPlaylist->Count())
			{
				//all tracks in the current playlist have failed. Go to other view
				LOG0("Initialization failed for all tracks in the current playlist.");
				PlayPauseStop(ETrue); //really stop
				MyAppUi()->DisplayQueryOrNoteL(EMsgNotePersistent,R_MESSAGE_ALL_TRACKS_FAILED,-1,R_MESSAGE_DLG_OK_EMPTY);
				MyAppUi()->AppUiHandleCommandL(ECommandSwitchToFFList,this);
			}
			else
			{
				//update the number of song
				iMusicPlayerContainer->iCurrentTheme->SetMetadataL(iTrack->iMetadata,iPlaybackPosition);
				iMusicPlayerContainer->DrawDeferred();
				ScheduleWorkerL(EJobInitNextSong);
			}
		}
		else
		{
			//update the container
			iNrFailedTracksInARow=0;
			if(iMusicPlayerContainer)
			{
				iMusicPlayerContainer->iCurrentTheme->SetMetadataL(iTrack->iMetadata,iPlaybackPosition);
				iMusicPlayerContainer->DrawDeferred();
			};
			//update the playlist
			iPlaylist->SetDurationForCurrentTrack(iTrack->iMetadata.iDuration);
			
			//equalizer and presets
			if(!iEEcurrent)
				ApplyEqualizerPresetAndEffectsL(iTrack->MdaPlayer());
		};
	}
	else if(aTrack==iNextTrack && aTrack->iState==CTrack::EStateInitialized)
	{
		LOG0("TrackInitializationComplete: this is next track");
		//this was the preparation of the next track
		//prepare the image as well
		if(iMusicPlayerContainer)
		{
			iThemeManager->iImgEngine->PrepareAlbumArtL(iNextTrack->iMetadata,iMusicPlayerContainer->iCurrentTheme->iAlbumArtSize);
			CTheme *theOtherTheme(iThemeManager->GetTheOtherTheme(iMusicPlayerContainer->iCurrentTheme));
			if(theOtherTheme)
				iThemeManager->iImgEngine->PrepareAlbumArtL(iNextTrack->iMetadata,theOtherTheme->iAlbumArtSize);
		};
	};
	LOG(ELogGeneral,-1,"TrackInitializationComplete: end");
}

TBool CMusicPlayerView::IsCrossfadingAllowed() const
{
	if(iFlags&ECrossfadingCannotBeUsed)return EFalse;
	if(iPlaylist && iPlaylist->iIsAudiobook)return EFalse;
	return ETrue;
}

TBool CMusicPlayerView::IsCrossfadingUsed() const
{
	if(!IsCrossfadingAllowed())return EFalse;
	if(iPreferences->iPFlags&CMLauncherPreferences::EPreferencesUseCrossfading)return ETrue;
	return EFalse;
}

void CMusicPlayerView::TrackPlaybackComplete(CTrack *aTrack, TBool aEndedByItself)
{
	LOG(ELogGeneral,1,"TrackPlaybackComplete++");
	if(aTrack==iTrack && aEndedByItself)
	{
		LOG0("Current track ended by itself");
		if(iPreferences->iPFlags&CMLauncherPreferences::EPreferencesSongLoop)
			NextPreviousL(0);
		else
			NextPreviousL(1);
	}
	else if(iTrack)
	{
		iTrack->ClearFadeOutTrackIfEqual(aTrack);
		if(IsCrossfadingUsed())
		{
			if(aEndedByItself)
			{
				ScheduleWorkerL(EJobWaitSomeSeconds|EJobStartCrossfadingTimer|EJobPrepareNextSong|EJobAllowScrolling);
				LOG0("Crossfading ended. We scheduled starting the crossfading timer & preparing next song");
			}
			else //the previousTrack was ended "by force" by some code
				ScheduleWorkerL(EJobAllowScrolling);	
		};
	}

	//equalizer and presets
	if(aTrack->MdaPlayer()==iEEcurrent)
	{
		DeleteEqualizerAndEffectsL();
		if(iTrack!=aTrack)//apply the effects to the current track
			ApplyEqualizerPresetAndEffectsL(iTrack->MdaPlayer());
		else LOG0("NOT applying Equalizer and Effects to track (because ending track equal to current track)");
	}
	LOG(ELogGeneral,-1,"TrackPlaybackComplete--");
}

void CMusicPlayerView::StartCrossfadingTimer()
{
	if(!IsCrossfadingUsed())
	{
		LOG0("Crossfading not used");
		return; 
	};

	LOG(ELogGeneral,1,"StartCrossfadingTimer: start");
	//we have crossfading and previous song finished. We create the crossfading timer
	if(iCrossfadingTimer)iCrossfadingTimer->Cancel();
	else iCrossfadingTimer=CPeriodic::New(EPriorityLow);//can return NULL
	if(iCrossfadingTimer && iTrack && iTrack->iState==CTrack::EStatePlaying)
	{
		TTimeIntervalMicroSeconds position;
		TTimeIntervalMicroSeconds duration(iTrack->Duration());
		iTrack->GetPosition(position);
		TTimeIntervalMicroSeconds32 time2crossfading(duration.Int64()-position.Int64()-iPreferences->iCrossfadingTimeMs*1000);
		if(time2crossfading.Int()>0)
		{
			if(time2crossfading.Int()<KCrossfadingMinTime2SleepWhenTooEarly)
				time2crossfading=KCrossfadingMinTime2SleepWhenTooEarly;
			TCallBack cb(&CrossfadingStarter,this);
			iCrossfadingTimer->Start(time2crossfading,time2crossfading,cb);
			LOG0("Crossfading timer started, time2crossfading=%d",time2crossfading.Int());
		}
	};
	LOG(ELogGeneral,-1,"StartCrossfadingTimer: end");
}

TInt CMusicPlayerView::CrossfadingStarter(TAny* aInstance)
{
	LOG(ELogGeneral,1,"CrossfadingStarter++");
	CMusicPlayerView *playerView((CMusicPlayerView*)aInstance);
	playerView->iCrossfadingTimer->Cancel();
	
	//check if we have crossfading
	if(!playerView->IsCrossfadingUsed())
		LOGret(0,"CrossfadingStarter-- (crossfading NOT used,l nothing started)");

	//check if the next song is initialized 
	if(!playerView->iNextTrack || playerView->iNextTrack->iState!=CTrack::EStateInitialized)
	{
		LOG0("Crossfading starter: next track not initialized, so we do not crossfade.");
		if(!playerView->iNextTrack)
			LOG0("Next track is NULL")
		else
			LOG0("Next track state: %d",playerView->iNextTrack->iState);
		return 0;
	};
	//check if the crossfading time is close to what it should be
	TTimeIntervalMicroSeconds position;
	playerView->iTrack->GetPosition(position);
	TTimeIntervalMicroSeconds duration(playerView->iTrack->Duration());
	TUint time2play(duration.Int64()-position.Int64());
	if(time2play>playerView->iPreferences->iCrossfadingTimeMs*1000+KCrossfadingMinTime2SleepWhenTooEarly)
	{
		//too early to start crosfading. Sleep some more
		LOG0("Too early to crossfade, will still sleep (remaining time2play=%d, crossfading time=%d",time2play,playerView->iPreferences->iCrossfadingTimeMs*1000);
		playerView->StartCrossfadingTimer();
	}
	else
	{
		LOG0("Crossfading started...");
		//cancel text scrolling in Container/Theme
		playerView->iFlags&=~EAllowScrolling;
		if(playerView->iMusicPlayerContainer)
			playerView->iMusicPlayerContainer->iCurrentTheme->AllowScrolling(EFalse);
		if(playerView->iPreferences->iPFlags&CMLauncherPreferences::EPreferencesSongLoop)
			playerView->NextPreviousL(0,ETrue);
		else
			playerView->NextPreviousL(1,ETrue);
	};
	LOG(ELogGeneral,-1,"CrossfadingStarter--");
	return 0; //no more work to do
}

void CMusicPlayerView::CurrentTrackInitializationComplete() //called by the current theme
{
	LOG(ELogGeneral,1,"CMusicPlayerView::CurrentTrackInitializationComplete: start");
	//if we have crossfading, we delay the initialization of the next song until the crossfading is over
	if(!iTrack->HasFadeOutTrack())
	{
		//cancel text scrolling in Container/Theme
		iFlags&=~EAllowScrolling;
		if(iMusicPlayerContainer)
			iMusicPlayerContainer->iCurrentTheme->AllowScrolling(EFalse);
		ScheduleWorkerL(EJobStartCrossfadingTimer|EJobPrepareNextSong|EJobAllowScrolling);
		LOG0("CurrentTrackInitializationComplete: No crossfading, prepare next song");
	}
	else
		LOG0("CurrentTrackInitializationComplete: we have crossfading, prepare next song deferred after crossfading");
	//else, the song will be prepared after crossfading
	LOG(ELogGeneral,-1,"CMusicPlayerView::CurrentTrackInitializationComplete: end");
}


void CMusicPlayerView::ClearPlaylist()
{
	DeleteEqualizerAndEffectsL();
	iEEcurrent=NULL;
	if(iTrack)iTrack->Recycle();
	if(iNextTrack)iNextTrack->Recycle();
	if(iPrevTrack)iPrevTrack->Recycle();
	iPlaylist=NULL;
}

TInt CMusicPlayerView::StartPlaylistL(CPlaylist *aPlaylist, TBool aIsPreview, TBool aStartPlaying) 
{
	LOG(ELogGeneral,1,"CMusicPlayerView::StartPlaylistL start");
	//do some cleaning
	ClearPlaylist();
	iNrFailedTracksInARow=0;
	iPlaylist=aPlaylist;
	iTargetPlaybackPosition=(TInt64)iPlaylist->iCurrentPlaybackPosition*1000000;
	if(aIsPreview)
		iFlags|=ESongPreview;
	else
		iFlags&=~ESongPreview;
	//TODO: check if the playlist is empty and display a notification (from AppUi)
	if(iPlaylist->Count()==0)
	{
		LOG(ELogGeneral,-1,"CMusicPlayerView::StartPlaylistL end (no current elements)");
		return KErrUnderflow;
	};
	TBool noValidElements(EFalse);
	iPlaylist->IncrementCurrent(0,ETrue,NULL,&noValidElements);
	if(noValidElements)
	{
		LOG(ELogGeneral,-1,"CMusicPlayerView::StartPlaylistL end (no valid elements)");
		return KErrNotFound;
	}
	//initialize current track
	if(!iTrack)iTrack=new (ELeave) CTrack(*this);
	iTrack->InitializeL(iPlaylist->GetCurrentEntry(ETrue),aStartPlaying);
	if(aStartPlaying)iFlags|=EIsPlaying;
	else iFlags&=~EIsPlaying;
#ifndef __TOUCH_ENABLED__
	if(aIsPreview)
	{
		//set the right SK to back
		Cba()->SetCommandSetL(R_SOFTKEYS_OPTIONS_BACK__PAUSE);
	}
#endif
	
	if(IsCrossfadingUsed())
		ScheduleWorkerL(EJobWaitSomeSeconds|EJobStartCrossfadingTimer|EJobPrepareNextSong/*|EJobAllowScrolling*/);
	LOG(ELogGeneral,-1,"CMusicPlayerView::StartPlaylistL end");
	return KErrNone;
}

void CMusicPlayerView::ApplyEqualizerPresetAndEffectsL(CMdaAudioPlayerUtility *aCurrentTrack)
{
	LOG(ELogGeneral,1,"ApplyEqualizerPresetAndEffectsL++ (iEEcurrent=%x (should be null), aCurrentTrack=%x)",iEEcurrent,aCurrentTrack);
	FL_ASSERT(!iEEcurrent);
	iEEcurrent=aCurrentTrack;
	TInt err;
	if(iPreferences->iAudioEqualizerPreset>=0)
	{
		TRAP(err,iEqualizerUtility=CAudioEqualizerUtility::NewL(*iEEcurrent));
		if(!err)
		{
			//check how many presets do we have
			if(iNrPresets<=0)//get the number of presets
				iNrPresets=iEqualizerUtility->Presets().Count();
			if(iPreferences->iAudioEqualizerPreset<iNrPresets)
			{
				iEqualizerUtility->ApplyPresetL(iPreferences->iAudioEqualizerPreset);
				LOG0("Audio preset %d applied",iPreferences->iAudioEqualizerPreset);
			}
			else LOG0("Selected audio preset (%d) is higher than the number of presets available (%d)",iPreferences->iAudioEqualizerPreset,iNrPresets);
		}
		else LOG0("Instantiating an audioEqualizerUtility Left with err=%d",err);
	}
	else LOG0("No audio preset.");

	if(iPreferences->iPFlags&CMLauncherPreferences::EPreferencesUseBassBoost)
	{
		LOG0("Applying Bass Boost");
		TRAP(err,iBassBoost=CBassBoost::NewL(*iEEcurrent,ETrue));
		if(!err)
		{
			TRAP(err,iBassBoost->ApplyL());
			if(err){LOG0("Bass Boost error: %d",err);}
			else LOG0("Bass Boost applied successfully");
		}
		else LOG0("Instantiating Bass Boost Left with err=%d",err);
	}
	if(iPreferences->iPFlags&CMLauncherPreferences::EPreferencesUseStereoWidening)
	{
		LOG0("Applying Stereo Widening");
		TRAP(err,iStereoWidening=CStereoWidening::NewL(*iEEcurrent,ETrue));
		if(!err)
		{
			TRAP(err,iStereoWidening->ApplyL());
			if(err){LOG0("Stereo Widening error: %d",err);}
			else LOG0("Stereo Widening applied successfully");
		}
		else LOG0("Instantiating Stereo Widening Left with err=%d",err);
	}
	if(iPreferences->iPFlags&CMLauncherPreferences::EPreferencesUseLoudness)
	{
		LOG0("Applying Loudness");
		TRAP(err,iLoudness=CLoudness::NewL(*iEEcurrent,ETrue));
		if(!err)
		{
			TRAP(err,iLoudness->ApplyL());
			if(err){LOG0("Loudness error: %d",err);}
			else LOG0("Loudness applied successfully");
		}
		else LOG0("Instantiating Stere Widening Left with err=%d",err);
	}
	LOG(ELogGeneral,-1,"ApplyEqualizerPresetAndEffectsL--");
}

void CMusicPlayerView::DeleteEqualizerAndEffectsL()
{
	LOG(ELogGeneral,1,"DeleteEqualizerAndEffectsL++ (iEEcurrent=%x iLoudness=%x iStereoWidening=%x iBassBoost=%x iEqualizer=%x)",iEEcurrent,iLoudness,iStereoWidening,iBassBoost,iEqualizerUtility);
	if(iEEcurrent)
	{
		delete iLoudness;iLoudness=NULL;
		LOG0("Loudness deleted");
		delete iStereoWidening;iStereoWidening=NULL;
		LOG0("Stereo widening deleted");
		delete iBassBoost;iBassBoost=NULL;
		LOG0("Bass boost deleted");
		delete iEqualizerUtility;iEqualizerUtility=NULL;
		LOG0("Equalizer deleted");
		iEEcurrent=NULL;
	}
	else
		FL_ASSERT(!(iLoudness || iStereoWidening || iBassBoost || iEqualizerUtility));
	LOG(ELogGeneral,-1,"DeleteEqualizerAndEffectsL--");
}


CDesCArrayFlat* CMusicPlayerView::GetEqualizerPresetsAndEffectsL(TInt aSelected, TBool aBassBoost, TBool aStereoWidening, TBool aLoudness)
{
	if(!iEEcurrent)return NULL;
	if(!iEqualizerUtility)
	{
		//we have to create it
		iEqualizerUtility=CAudioEqualizerUtility::NewL(*iEEcurrent);
	}
	//we should have an equalizer here
	TArray<TEfAudioEqualizerUtilityPreset> allPresets = iEqualizerUtility->Presets();
	
	iNrPresets=allPresets.Count();
	CDesCArrayFlat* presetsArray = new(ELeave)CDesCArrayFlat(iNrPresets+1+3+2); //+1=equalizer off, +3=nr of the effects, +2=nr of the titles
	CleanupStack::PushL(presetsArray);
	TBuf<64> item;
	HBufC *presetName;
	
	//first, add the "presets" empty element
	presetName=StringLoader::LoadLC(R_LIST_ENTRY_PRESETS);
	item.Format(KListItemFormat,4,presetName);
	presetsArray->AppendL(item);
	CleanupStack::PopAndDestroy(presetName);
	
	presetName=StringLoader::LoadLC(R_LIST_ENTRY_EQUALIZER_OFF);
	if(aSelected<0)item.Format(KListItemFormat,1,presetName);
	else item.Format(KListItemFormat,0,presetName);
	presetsArray->AppendL(item);
	CleanupStack::PopAndDestroy(presetName);
	
	for(TInt i=0;i<allPresets.Count();i++)
	{
		if(aSelected==i)item.Format(KListItemFormat,1,&allPresets[i].iPresetName);
		else item.Format(KListItemFormat,0,&allPresets[i].iPresetName);
		presetsArray->AppendL(item);
	}
	
	//effects
	presetName=StringLoader::LoadLC(R_LIST_ENTRY_EFFECTS);
	item.Format(KListItemFormat,4,presetName);
	presetsArray->AppendL(item);
	CleanupStack::PopAndDestroy(presetName);
		
	//Bass Boost
	presetName=StringLoader::LoadLC(R_LIST_ENTRY_EFFECT_BASS_BOOST);
	item.Format(KListItemFormat,aBassBoost?3:2,presetName);
	CleanupStack::PopAndDestroy(presetName);
	presetsArray->AppendL(item);
		
	//Stereo widening
	presetName=StringLoader::LoadLC(R_LIST_ENTRY_EFFECT_STEREO_WIDENING);
	item.Format(KListItemFormat,aStereoWidening?3:2,presetName);
	CleanupStack::PopAndDestroy(presetName);
	presetsArray->AppendL(item);
	
	//Loudness
	presetName=StringLoader::LoadLC(R_LIST_ENTRY_EFFECT_LOUDNESS);
	item.Format(KListItemFormat,aLoudness?3:2,presetName);
	CleanupStack::PopAndDestroy(presetName);
	presetsArray->AppendL(item);
		
	CleanupStack::Pop(presetsArray);
	return presetsArray;
}

TInt CMusicPlayerView::SetEqualizerPresetL()
{
	LOG(ELogGeneral,1,"SetEqualizerPreset++ (%d)",iPreferences->iAudioEqualizerPreset);
	if(!iEEcurrent)
	{
		LOG(ELogGeneral,-1,"SetEqualizerPreset-- (no iMdaPlayer, exiting early ...)");
		return KErrNotFound;
	}
	if(!iEqualizerUtility)
		iEqualizerUtility=CAudioEqualizerUtility::NewL(*iEEcurrent);
	LOG0("We have iEqualizerUtility");
	
	TInt err;
	if(iPreferences->iAudioEqualizerPreset>=0)
	{
		//check how many presets do we have
		if(iNrPresets<=0)//get the number of presets
			iNrPresets=iEqualizerUtility->Presets().Count();
		if(iPreferences->iAudioEqualizerPreset<iNrPresets)
		{	
			LOG0("Applying preset...");
			TRAP(err,iEqualizerUtility->ApplyPresetL(iPreferences->iAudioEqualizerPreset));
			if(err){LOG0("Applying preset Left with err=%d",err);}
			else LOG0("Audio preset %d applied",iPreferences->iAudioEqualizerPreset);
		}
		else LOG0("Selected audio preset (%d) is higher than the number of presets available (%d)",iPreferences->iAudioEqualizerPreset,iNrPresets);
	}
	else 
	{
		LOG0("Disabling preset...");
		TRAP(err,iEqualizerUtility->DisableEqualizerL());
		if(err){LOG0("Disabling preset left with err=%d.",err);}
		else LOG0("Audio preset disabled.");
	}
	LOG(ELogGeneral,-1,"SetEqualizerPreset--");
	return 0;
}

TInt CMusicPlayerView::SetEffect(enum TEffects aEffect) //value taken from preferences
{
	CAudioEffect **effect(NULL);
	TBool enabled(EFalse);
	TInt err=0;
	
	if(!iEEcurrent)
	{
		LOG0("SetEffect++/-- (no iMdaPlayer, exiting early, and not doing anything)");
		return KErrNotFound;
	}
	
	switch(aEffect)
	{
	case EBassBoost:
		enabled=iPreferences->iPFlags&CMLauncherPreferences::EPreferencesUseBassBoost;
		LOG(ELogGeneral,1,"SetEffect++ (BassBoost, enabled: %d)",enabled);
		if(!iBassBoost)
			TRAP(err,iBassBoost=CBassBoost::NewL(*iEEcurrent,enabled));
		effect=(CAudioEffect**)&iBassBoost;
		break;
	case EStereoWidening:
		enabled=iPreferences->iPFlags&CMLauncherPreferences::EPreferencesUseStereoWidening;
		LOG(ELogGeneral,1,"SetEffect++ (StereoWidening, enabled: %d)",enabled);
		if(!iStereoWidening)
			TRAP(err,iStereoWidening=CStereoWidening::NewL(*iEEcurrent,enabled));
		effect=(CAudioEffect**)&iStereoWidening;
		break;
	case ELoudness:
		enabled=iPreferences->iPFlags&CMLauncherPreferences::EPreferencesUseLoudness;
		LOG(ELogGeneral,1,"SetEffect++ (Loudness, enabled: %d)",enabled);
		if(!iLoudness)
			TRAP(err,iLoudness=CLoudness::NewL(*iEEcurrent,enabled));
		effect=(CAudioEffect**)&iLoudness;
		break;
	default: //not handled
		FL_ASSERT(0);
	}//switch

	if(err)
		LOGret(err,"SetEffect-- (Creating effect instance Left with err=%d)",err);
	
	//if here, we have an instance that we need to enable or disable
	if(enabled){TRAP(err,(*effect)->EnableL());}
	else {TRAP(err,(*effect)->DisableL());}
	if(err)
	{
		delete *effect;
		*effect=NULL;
		LOGret(err,"SetEffect-- (Enabling/Disabling effect Left with err=%d)",err);
	}
	
	//if here, we need to apply the modification
	TRAP(err,(*effect)->ApplyL());
	if(err)
	{
		delete *effect;
		*effect=NULL;
		LOGret(err,"SetEffect-- (Applying effect Left with err=%d)",err);
	}
	LOGret(0,"SetEffect--");
}

void CMusicPlayerView::PrepareNextSongL()
{
	LOG(ELogGeneral,1,"PrepareNextSong++");
	
	//check if the device is too slow to play and prepare next song at the same time
	if(iPreferences->iCFlags&CMLauncherPreferences::EHwDeviceIsSlow)
	{
		LOG(ELogGeneral,-1,"PrepareNextSong: device is slow, not preparing next song");
		return;
	};
	
	//first we check if it is not already prepared
	
	TInt index(iPlaylist->GetCurrentIndex());
	TInt nextIndex(index);
	if(!(iPreferences->iPFlags&CMLauncherPreferences::EPreferencesSongLoop)) //if we need to loop the current song, we do not increment
		iPlaylist->IncrementIndex(nextIndex,1,ETrue,NULL,NULL);
	if(nextIndex<0)
	{
		LOG(ELogGeneral,-1,"PrepareNextSong-- (nextIndex==-1, there are no more valid entries in the playlist)");
		return;
	}

	if(iNextTrack && iNextTrack->iMetadata.iFileDirEntry==iPlaylist->GetEntry(nextIndex,ETrue) && (iNextTrack->iState==CTrack::EStateInitialized || iNextTrack->iState==CTrack::EStateInitializing))
	{
		LOG0("Next track prepared or preparing (index=%d, current index=%d",nextIndex,index);
		LOG(ELogGeneral,-1,"PrepareNextSong--");
		return; //next track already prepared, or preparing
	};
	
	//here we should prepare the next track
	LOG0("Current index: %d, next index: %d",index,nextIndex);
	
	TFileDirEntry *nextEntry=iPlaylist->GetEntry(nextIndex,ETrue);
	//initialize next track
	if(!iNextTrack)iNextTrack=new (ELeave) CTrack(*this);
	iNextTrack->InitializeL(nextEntry,EFalse);
	LOG(ELogGeneral,-1,"PrepareNextSong: end");
}

//aStop==ETrue if the command is to stop playback. Function returns ETrue if music is playing
TBool CMusicPlayerView::PlayPauseStop(TBool aStop)
{
	LOG(ELogGeneral,1,"PlayPauseStop++ (aStop=%d)",aStop);
	if(aStop)
	{
		if(iTrack->iState==CTrack::EStateInitialized)
		{
			LOG(ELogGeneral,-1,"PlayPauseStop-- (already stopped, returning)");
			return EFalse;
		}
		iTrack->iFlags&=~CTrack::EStartPlaying;
		iTrack->Stop();
		iFlags&=~EIsPlaying;
#ifdef __MSK_ENABLED__
		Cba()->SetCommandL(EAknSoftkeyForwardKeyEvent,*iMskPlay);
		Cba()->DrawDeferred();
#endif
		if(iMusicPlayerContainer)
			iMusicPlayerContainer->DrawDeferred();
		LOG(ELogGeneral,-1,"PlayPauseStop-- (stopped)");
		return EFalse;
	}
	else if(iTrack->iState==CTrack::EStatePlaying)
	{
		iTrack->Pause();
		iTrack->iFlags&=~CTrack::EStartPlaying;
		iTrack->iState=CTrack::EStateInitialized;
		iFlags&=~EIsPlaying;
#ifdef __MSK_ENABLED__
		Cba()->SetCommandL(EAknSoftkeyForwardKeyEvent,*iMskPlay);
		Cba()->DrawDeferred();
#endif
		if(iMusicPlayerContainer)
			iMusicPlayerContainer->DrawDeferred();
		LOG(ELogGeneral,-1,"PlayPauseStop-- (paused)");
		return EFalse;
	}
	else
	{
		//we are stoped/paused and should start playing
		if(iTrack->iFlags&CTrack::EKilledByTelephony)
		{
			//we are in the middle of a call and the user is impatient
			//display a note
			TRAP_IGNORE(
					HBufC* textResource=StringLoader::LoadLC(R_CANNOT_PLAY_RIGHT_NOW);
			        CAknInformationNote* informationNote=new(ELeave)CAknInformationNote;
			        informationNote->ExecuteLD(*textResource);
			        CleanupStack::PopAndDestroy(textResource);
					);
			LOG(ELogGeneral,-1,"PlayPauseStop-- (request ignored, cannot play right now)");
			return EFalse; //because we ignore the request
		}
		iTrack->RestartPlaying();
		iFlags|=EIsPlaying;
#ifdef __MSK_ENABLED__
		Cba()->SetCommandL(EAknSoftkeyForwardKeyEvent,*iMskPause);
		Cba()->DrawDeferred();
#endif
		if(iMusicPlayerContainer)
			iMusicPlayerContainer->DrawDeferred();
		LOG(ELogGeneral,-1,"PlayPauseStop-- (playback restarted)");
		return ETrue;
	}
}

void CMusicPlayerView::NextPreviousL(TInt aJump, TBool aCrossfade/*=EFalse*/) //aJump should be 1, 0 or -1
{
	if(iFlags&ESongPreview)aJump=0; //we will restart the song
	
	//check for some particular cases
	LOG(ELogGeneral,1,"NextPreviousL++ (jump: %d, crossfade: %d)",aJump,aCrossfade);
	TBool wentOverTheTop,noValidElementsFound;
	
	//compute the next song's index
	TInt nextSongIndex=iPlaylist->GetCurrentIndex();
	iPlaylist->IncrementIndex(nextSongIndex,aJump,ETrue,&wentOverTheTop,&noValidElementsFound);
	
	//check for valid elements
	if(noValidElementsFound)
	{
		LOG(ELogGeneral,-1,"NextPreviousL-- (NO VALID ELEMENTS FOUND IN CURRENT PLAYLIST)");
		return;
	}
	
	if(iPreferences->iPFlags&CMLauncherPreferences::EPreferencesSongLoop && iNextTrack)
		iNextTrack->Recycle(); //this track is not good, because it is actually the same, current track
			
	if(aCrossfade)
	{
		//aJump could be 1 or perhaps even 0 (if we loop the song)
		if(!(iPreferences->iPFlags&CMLauncherPreferences::EPreferencesPlaylistLoop) && aJump==1 && wentOverTheTop)
		{
			LOG(ELogGeneral,-1,"NextPreviousL-- (no next element, in crossfading mode, waiting for current song to end)");
			return;
		}			
		//if we are here, we can jump to the "next" element
	}
	else
	{
		//no crossfading, stop the current element
		LOG0("Stopping the current element");
		iTrack->End();
		LOG0("Done stopping");
		if(!(iPreferences->iPFlags&CMLauncherPreferences::EPreferencesPlaylistLoop) && aJump==1 && wentOverTheTop)
		{
			//we will pause playing
			iFlags&=~EIsPlaying;
		}
	}
	
	//increment the current playlist
	iPlaylist->IncrementCurrent(aJump,ETrue,&wentOverTheTop,&noValidElementsFound);
	TFileDirEntry* nextEntry=iPlaylist->GetCurrentEntry(ETrue);
	FL_ASSERT(nextEntry);
	TBool startTrack(ETrue);
	
	if(aJump==1 || (aJump==0 && aCrossfade))
	{
		//switch tracks
		CTrack *temp=iPrevTrack;
		iPrevTrack=iTrack;
		iTrack=iNextTrack;
		iNextTrack=temp;
		if(iNextTrack)iNextTrack->Recycle();
	}
	else if(aJump==0)
	{
		FL_ASSERT(!aCrossfade);
		//we restart the current track
		iTargetPlaybackPosition=0;
	}
	else
	{
		FL_ASSERT(aJump==-1 && !aCrossfade);
		//switch tracks
		CTrack *temp=iNextTrack;
		iNextTrack=iTrack;
		iTrack=iPrevTrack;
		iPrevTrack=temp;
		if(iPrevTrack)iPrevTrack->Recycle();	
	}
	if(startTrack)
	{
		//we need to cancel any image request
		if(iMusicPlayerContainer)
			iThemeManager->iImgEngine->CancelRequest(iMusicPlayerContainer->iCurrentTheme);
			
		//check if iTrack has the valid element
		if(!iTrack)iTrack=new(ELeave)CTrack(*this);
		LOG0("We have a new current track. Check its state (%d) to see what to do next",iTrack->iState);
		switch(iTrack->iState)
		{
		case CTrack::EStateNotInitialized:
			iTrack->InitializeL(nextEntry,iFlags&EIsPlaying);
			break;
		case CTrack::EStateInitializing:
			if(iFlags&EIsPlaying) iTrack->iFlags|=CTrack::EStartPlaying;
			else iTrack->iFlags&=~CTrack::EStartPlaying;
			break;
		case CTrack::EStateInitialized:
			if(iFlags&EIsPlaying)
			{
				iTrack->StartPlaying(aCrossfade?iPrevTrack:NULL);
				if(!iEEcurrent)//check if we need to apply equalizer & effects
					ApplyEqualizerPresetAndEffectsL(iTrack->MdaPlayer());
			}
			//we also need to update the container
			if(iMusicPlayerContainer)
			{
				iMusicPlayerContainer->iCurrentTheme->SetMetadataL(iTrack->iMetadata,iPlaybackPosition);
				iMusicPlayerContainer->DrawDeferred();
			};
			//update the playlist
			iPlaylist->SetDurationForCurrentTrack(iTrack->iMetadata.iDuration);
			break;
		case CTrack::EStatePlaying:
			break; //nothing to be done I guess
		case CTrack::EStateInitializationFailed:
			LOG0("State is InitializationFailed, jump to next track");
			NextPreviousL(1,EFalse);
			break;
		}//switch
	}
		
#ifdef __MSK_ENABLED__
	if(iFlags&EIsPlaying)
		Cba()->SetCommandL(EAknSoftkeyForwardKeyEvent,*iMskPause);
	else
		Cba()->SetCommandL(EAknSoftkeyForwardKeyEvent,*iMskPlay);
	Cba()->DrawDeferred();
#endif
	
	LOG(ELogGeneral,-1,"NextPreviousL--");
}



void CMusicPlayerView::VolumeUpDown(TBool aUp)
{
	//sets volume up one step
	TBool changed(EFalse);
	if(iTrack->iState==CTrack::EStateInitialized || iTrack->iState==CTrack::EStatePlaying)
	{
		if(aUp && iPreferences->iVolume<iPreferences->iMaxVolume)
		{
			changed=ETrue;
			iPreferences->iVolume++;
		}
		if(!aUp && iPreferences->iVolume>0)
		{
			changed=ETrue;
			iPreferences->iVolume--;
		}
		if(changed)
		{
			iTrack->SetVolumeFromView();
			iPreferences->iCFlags|=CMLauncherPreferences::ESavePreferences;
			//update volume on screen
			if(iMusicPlayerContainer)
				iMusicPlayerContainer->iCurrentTheme->UpdateVolumeOnScreen();
		}
	};
}

void CMusicPlayerView::MrccatoCommand(TRemConCoreApiOperationId aOperationId, TRemConCoreApiButtonAction aButtonAct)
{
	TRequestStatus status;
	TInt err(KErrNone);
	LOG0("RemCon in MusicPlayerView: operation=%d buttonAct=%d",aOperationId,aButtonAct);
	switch( aOperationId )
	{
	case ERemConCoreApiPausePlayFunction:
	{
		switch (aButtonAct)
		{
		case ERemConCoreApiButtonPress:break;//keep it to avoid a warning
		case ERemConCoreApiButtonRelease:break;//keep it to avoid a warning
		case ERemConCoreApiButtonClick:
			// Play/Pause button clicked
			PlayPauseStop(EFalse);
			break;
		/*default:
			// Play/Pause unknown action
			break;*/
		}
		//Send the response back to Remcon server
		iCoreTarget->PausePlayFunctionResponse(status, KErrNone);
		User::WaitForRequest(status);
		break;
	}

	case ERemConCoreApiPause:
	{
		switch (aButtonAct)
		{
		case ERemConCoreApiButtonPress:break;//keep it to avoid a warning
		case ERemConCoreApiButtonRelease:break;//keep it to avoid a warning
		case ERemConCoreApiButtonClick:
			// Pause button clicked
			if(iTrack->iState==CTrack::EStatePlaying)
				PlayPauseStop(EFalse);
			break;
			/*default:
				// Play/Pause unknown action
				break;*/
		}
		//Send the response back to Remcon server
		iCoreTarget->PauseResponse(status, KErrNone);
		User::WaitForRequest(status);
		break;
	}
	
	case ERemConCoreApiPlay:
	{
		switch (aButtonAct)
		{
		case ERemConCoreApiButtonPress:break;//keep it to avoid a warning
		case ERemConCoreApiButtonRelease:break;//keep it to avoid a warning
		case ERemConCoreApiButtonClick:
			// Pause button clicked
			if(iTrack->iState==CTrack::EStateInitialized)
				PlayPauseStop(EFalse);
			break;
			/*default:
					// Play/Pause unknown action
					break;*/
		}
		//Send the response back to Remcon server
		iCoreTarget->PlayResponse(status, KErrNone);
		User::WaitForRequest(status);
		break;
	}

	case ERemConCoreApiStop:
	{
		switch (aButtonAct)
		{
		case ERemConCoreApiButtonPress:break;//keep it to avoid a warning
		case ERemConCoreApiButtonRelease:break;//keep it to avoid a warning
		case ERemConCoreApiButtonClick:
			// Stop button clicked
			PlayPauseStop(ETrue);
			break;
		}
		iCoreTarget->StopResponse(status, KErrNone);
		User::WaitForRequest(status);
		break;
	}
	case ERemConCoreApiBackward:
	{
		switch (aButtonAct)
		{
		case ERemConCoreApiButtonPress:break;//keep it to avoid a warning
		case ERemConCoreApiButtonRelease:break;//keep it to avoid a warning
		case ERemConCoreApiButtonClick:
			// Previous button clicked
			NextPreviousL(-1);
			break;
		}
		iCoreTarget->RewindResponse(status, KErrNone);
		User::WaitForRequest(status);
		break;
	}
	case ERemConCoreApiForward:
	{
		switch (aButtonAct)
		{
		case ERemConCoreApiButtonPress:break;//keep it to avoid a warning
		case ERemConCoreApiButtonRelease:break;//keep it to avoid a warning
		case ERemConCoreApiButtonClick:
			// Next button clicked
			NextPreviousL(1);
			break;
		}
		iCoreTarget->ForwardResponse(status, KErrNone);
		User::WaitForRequest(status);
		break;
	}
	case ERemConCoreApiVolumeUp:
	{
		switch (aButtonAct)
		{
		case ERemConCoreApiButtonPress:
			//sometimes we do not get the ERemConCoreApiButtonClick event, so we need to simulate clicks
			StartTypematicL(EVolumeUpPressed);
			break;
		case ERemConCoreApiButtonRelease:
			if(iTypematic->IsActive())
			{
				//there was no click, so we would need to issue the volume up command
				StopTypematic();
				VolumeUpDown(ETrue);
			};
			break;
		case ERemConCoreApiButtonClick:
			// Volume Up button clicked
			StopTypematic();//we get clicks, we do not need typematic
			VolumeUpDown(ETrue);
			break;
		}
		iCoreTarget->VolumeUpResponse(status, err);
		User::WaitForRequest(status);
		break;
	}
	case ERemConCoreApiVolumeDown:
	{
		switch (aButtonAct)
		{
		case ERemConCoreApiButtonPress:
			//sometimes we do not get the ERemConCoreApiButtonClick event, so we need to simulate clicks
			StartTypematicL(EVolumeDownPressed);
			break;
		case ERemConCoreApiButtonRelease:
			if(iTypematic->IsActive())
			{
				//there was no click, so we would need to issue the volume down command
				StopTypematic();
				VolumeUpDown(EFalse);
			};
			break;
		case ERemConCoreApiButtonClick:
			// Volume Down button clicked
			StopTypematic();//we get clicks, we do not need typematic
			VolumeUpDown(EFalse);
			break;
		}
		iCoreTarget->VolumeDownResponse(status, err);
		User::WaitForRequest(status);
		break;
	}
	case ERemConCoreApiFastForward:
	{
		//we only get button pressed and button released, AFAIK
		switch (aButtonAct)
		{
		case ERemConCoreApiButtonPress:
			if(!iTypematic || !iTypematic->IsActive())
			{
				iFlags|=ESeekFirstPress;
				StartTypematicL(EFastForwardPressed);
			};
			break;
		case ERemConCoreApiButtonRelease:
			StopTypematic();
			SeekEnd();
			break;
		case ERemConCoreApiButtonClick:
		{
			//seek forward
			TInt err;
			if(iFlags&ESeekFirstPress)
			{
				err=Seek(ETrue,ETrue);
				iFlags&=~ESeekFirstPress;
			}
			else err=Seek(EFalse,ETrue);
			if(!err && iMusicPlayerContainer)
				iMusicPlayerContainer->iCurrentTheme->UpdatePlaybackPosition(iPlaybackPosition);
			if(!err && iPlaylist)
				iPlaylist->UpdatePosition(iPlaybackPosition);
		};break;
		}//switch
		iCoreTarget->FastForwardResponse(status, KErrNone);
		User::WaitForRequest(status);
		break;
	}
	case ERemConCoreApiRewind:
	{
		switch (aButtonAct)
		{
		case ERemConCoreApiButtonPress:
			if(!iTypematic || !iTypematic->IsActive())
			{
				iFlags|=ESeekFirstPress;
				StartTypematicL(ERewindPressed);
			};
			break;
		case ERemConCoreApiButtonRelease:
			StopTypematic();
			SeekEnd();
			break;
		case ERemConCoreApiButtonClick:
		{
			//seek backward
			TInt err;
			if(iFlags&ESeekFirstPress)
			{
				err=Seek(ETrue,EFalse);
				iFlags&=~ESeekFirstPress;
			}
			else err=Seek(EFalse,EFalse);
			if(!err && iMusicPlayerContainer)
				iMusicPlayerContainer->iCurrentTheme->UpdatePlaybackPosition(iPlaybackPosition);
			if(!err && iPlaylist)
				iPlaylist->UpdatePosition(iPlaybackPosition);
		};break;
		}
		iCoreTarget->BackwardResponse(status, KErrNone);
		User::WaitForRequest(status);
		break;
	}
	default:
		break;
	}
}

TInt CMusicPlayerView::Seek(TBool aFirstSeek, TBool aSeekForward, TTimeIntervalMicroSeconds *aPositionObserver/*=NULL*/)
{
	if(aFirstSeek)
	{
		iSeekIndex=0;
		if(iTrack->iState!=CTrack::EStatePlaying)
			iFlags|=EWasPausedBeforeSeek;
		//stop playing
		iTrack->Pause();
	}
	else
	{
		iSeekIndex++;
		if(iSeekIndex>=KSeekStepsCount)
			iSeekIndex=KSeekStepsCount-1;
	}

	TInt err=iTrack->GetPosition(iPlaybackPosition);
	if(err)return err;
	if(aSeekForward)iPlaybackPosition=iPlaybackPosition.Int64()+KSeekSteps[iSeekIndex]*1000000;
	else iPlaybackPosition=iPlaybackPosition.Int64()-KSeekSteps[iSeekIndex]*1000000;

	if(iPlaybackPosition<0)iPlaybackPosition=0;

	iTrack->SetPosition(iPlaybackPosition);
	if(aPositionObserver)
		*aPositionObserver=iPlaybackPosition;
	return KErrNone;
}

void CMusicPlayerView::SeekEnd()
{
	if(iFlags&EWasPausedBeforeSeek)
		iFlags&=~EWasPausedBeforeSeek;
	else
		iTrack->RestartPlaying();
}

void CMusicPlayerView::ResumePlayback()
{
	LOG0("CMusicPlayerView::ResumePlayback")
	if(iTrack->iFlags&CTrack::EKilledByTelephony)
	{
		//we were killed by telephony and now we need to resume playback
		LOG0("Resuming playback from position %d",(TInt)(iTargetPlaybackPosition.Int64()/1000000));
		SetStatusPaneVisibility();
		iTrack->iFlags&=~CTrack::EKilledByTelephony;
		if(iFlags&EIsPlaying)
			PlayPauseStop(EFalse);
	};
}
/*
void CMusicPlayerView::LightStatusChanged(TInt aTarget, CHWRMLight::TLightStatus aStatus)
{
	//if the light goes off, we close the periodic function in the container
	LOG0("Light status changed: target=%d, status=%d",aTarget,aStatus);
	if(aTarget & CHWRMLight::EPrimaryDisplay )
	{
		if(aStatus==CHWRMLight::ELightOff ) //light went off, delete the periodic worker
		{
			if(iFlags&ELightIsOn)
			{
				iFlags&=~ELightIsOn;
				if(iMusicPlayerContainer)
				{
					//stop the container's periodic worker in KDelay seconds
					iFlags|=EPeriodicWorkerStopDelayed;
					iStopDelay=KStopPeriodicWorkerDelay;
				};
				LOG0("Screen is off now");
			};
		}
		else //lights are on
		{
			if(!(iFlags&ELightIsOn))
			{
				iFlags|=ELightIsOn;
				StartPeriodicWorker();
				LOG0("Screen is on now");
			};
		};
	}
}*/

void CMusicPlayerView::HandleForegroundEventL(TBool aForeground)
{
	if(aForeground)
	{
		iFlags|=EIsVisible;
		StartPeriodicWorker();
		if(iMusicPlayerContainer && iMusicPlayerContainer->iCurrentTheme)
			iMusicPlayerContainer->iCurrentTheme->AllowScrolling(ETrue);
		LOG0("CMusicPlayerView in Foreground");
	}
	else
	{
		iFlags&=~EIsVisible;
		StopPeriodicWorker();
		if(iMusicPlayerContainer && iMusicPlayerContainer->iCurrentTheme)
			iMusicPlayerContainer->iCurrentTheme->AllowScrolling(EFalse);
		LOG0("CMusicPlayerView in Background");
	}
}
	
void CMusicPlayerView::StartPeriodicWorker()
{
	if(iFlags & EIsVisible)
	{
		//start the alarm
		if(iPeriodic)iPeriodic->Cancel();
		else iPeriodic = CPeriodic::New(CActive::EPriorityLow);

		if( iPeriodic )
		{
			//alarm should start so that startTime+n*Period do not make exact seconds
			//otherwise the time will not be displayed nicely (seconds not incremented regularly)
			TCallBack cb(&PeriodicFunction,this);
			iPeriodic->Start(KPeriodicWorkerStartDelay,KPeriodicWorkerPeriod,cb);
			//iFlags&=~EPeriodicWorkerStopDelayed;
		};
		PeriodicFunction(this);//we need an update now
	};
}

void CMusicPlayerView::StopPeriodicWorker()
{
	if(iPeriodic)iPeriodic->Cancel();
}
	
TInt CMusicPlayerView::PeriodicFunction(TAny* aInstance)
{
	CMusicPlayerView *self=(CMusicPlayerView*)aInstance;
	if(!self->iTrack || self->iTrack->iState==CTrack::EStateNotInitialized || self->iTrack->iState==CTrack::EStateInitializing || self->iTrack->iState==CTrack::EStateInitializationFailed)return 1;
	
	TInt err=self->iTrack->GetPosition(self->iPlaybackPosition);
	if(err)self->iPlaybackPosition=0;
		
	if(self->iMusicPlayerContainer)
		self->iMusicPlayerContainer->iCurrentTheme->PeriodicUpdate(self->iPlaybackPosition);
	
	if(self->iPlaylist)
		self->iPlaylist->UpdatePosition(self->iPlaybackPosition);
	
	/*
	if(self->iFlags&EPeriodicWorkerStopDelayed)
	{
		self->iStopDelay--;
		if(self->iStopDelay<0)
		{
			LOG0("Stopping the periodic timer");
			self->iFlags&=~EPeriodicWorkerStopDelayed;
			self->iPeriodic->Cancel();
			return 0; //this will stop the periodic worker
		};
	};*/

	return 1;
}
	
void CMusicPlayerView::DynInitMenuPaneL(TInt aResourceId, CEikMenuPane *aMenuPane)
{
    if(aResourceId==R_MENU_MUSICPLAYER)
    {
        if(iPreferences->iCFlags&CMLauncherPreferences::EHwDeviceHasVolumeKeys)
        {
        	//device has volume keys, we will hide the stop command
        	aMenuPane->SetItemDimmed(ECommandMusicPlayerStop,ETrue);
        };
        if(iMusicPlayerContainer)
        	iMusicPlayerContainer->iCurrentTheme->AddMenuItemL(aMenuPane,ECommandMusicPlayerAudioSettings);
    }
    else if(aResourceId==R_MENU_MUSIC_PLAYER_PREFERENCES)
    {
    	//if this is a non-touch device, dimm the ECommandMusicPlayerPreferencesButtonsInsteadOfDrag menu
    	if(!(iPreferences->iCFlags&CMLauncherPreferences::EHwTouchDevice))
    		aMenuPane->SetItemDimmed(ECommandMusicPlayerPreferencesButtonsInsteadOfDrag,ETrue);
    }
    else if(aResourceId==R_MENU_MUSIC_PLAYER_LOOP)
    {
    	if(iPreferences->iPFlags&CMLauncherPreferences::EPreferencesPlaylistLoop)
    		aMenuPane->SetItemButtonState(ECommandMusicPlayerLoopPlaylist,EEikMenuItemSymbolOn);
    	else if(iPreferences->iPFlags&CMLauncherPreferences::EPreferencesSongLoop)
    		aMenuPane->SetItemButtonState(ECommandMusicPlayerLoopSong,EEikMenuItemSymbolOn );
    }
    else if(aResourceId==R_MENU_AUDIO_SETTINGS)
    {
    	if(IsCrossfadingAllowed())
    	{
    		if(iPreferences->iPFlags&CMLauncherPreferences::EPreferencesUseCrossfading)
    			aMenuPane->SetItemButtonState(ECommandMusicPlayerAudioSettingsCrossfading,EEikMenuItemSymbolOn);
    	}
    	else aMenuPane->SetItemDimmed(ECommandMusicPlayerAudioSettingsCrossfading,ETrue);
    	//FmTx
    	if(!(iPreferences->iCFlags&CMLauncherPreferences::EHwHasFmTx))
    	{
    		//device does not have FmTx
    		aMenuPane->SetItemDimmed(ECommandMusicPlayerFmTx,ETrue);
    	};
    }
    else if(aResourceId==R_MENU_COLOR_SCHEMES)
    {
    	LOG0("DynInitMenuPaneL for color scheme menu++");
    	if(iPreferences->iPFlags&CMLauncherPreferences::EPreferencesMatchColors2AlbumArt)
    		aMenuPane->SetItemButtonState(ECommandThemeMatchAlbumArt,EEikMenuItemSymbolOn);
    	else if(iMusicPlayerContainer && iMusicPlayerContainer->iCurrentTheme)
    	{
    		LOG0("Not matching the album art ...");
    		TUint colors=((CColoredTheme*)iMusicPlayerContainer->iCurrentTheme)->GetColorScheme();
    		switch(colors)
    		{
    		case (CColoredTheme::EBgColorBlack|CColoredTheme::EColorGreen):
    				aMenuPane->SetItemButtonState(ECommandThemeBlackAndGreen,EEikMenuItemSymbolOn);break;
    		case (CColoredTheme::EBgColorBlack|CColoredTheme::EColorBlue):
    		    	aMenuPane->SetItemButtonState(ECommandThemeBlackAndBlue,EEikMenuItemSymbolOn);break;
    		case (CColoredTheme::EBgColorBlack|CColoredTheme::EColorOrange):
    		        aMenuPane->SetItemButtonState(ECommandThemeBlackAndOrange,EEikMenuItemSymbolOn);break;
    		case (CColoredTheme::EBgColorBlack|CColoredTheme::EColorRed):
    		        aMenuPane->SetItemButtonState(ECommandThemeBlackAndRed,EEikMenuItemSymbolOn);break;
    		case (CColoredTheme::EBgColorBlack|CColoredTheme::EColorViolet):
    		        aMenuPane->SetItemButtonState(ECommandThemeBlackAndViolet,EEikMenuItemSymbolOn);break;
    		
    		case (CColoredTheme::EBgColorWhite|CColoredTheme::EColorGreen):
    		    	aMenuPane->SetItemButtonState(ECommandThemeWhiteAndGreen,EEikMenuItemSymbolOn);break;
    		case (CColoredTheme::EBgColorWhite|CColoredTheme::EColorBlue):
    		    	aMenuPane->SetItemButtonState(ECommandThemeWhiteAndBlue,EEikMenuItemSymbolOn);break;
    		case (CColoredTheme::EBgColorWhite|CColoredTheme::EColorOrange):
    		    	aMenuPane->SetItemButtonState(ECommandThemeWhiteAndOrange,EEikMenuItemSymbolOn);break;
    		case (CColoredTheme::EBgColorWhite|CColoredTheme::EColorRed):
    		    	aMenuPane->SetItemButtonState(ECommandThemeWhiteAndRed,EEikMenuItemSymbolOn);break;
    		case (CColoredTheme::EBgColorWhite|CColoredTheme::EColorViolet):
    		    	aMenuPane->SetItemButtonState(ECommandThemeWhiteAndViolet,EEikMenuItemSymbolOn);break;
    		}
    	}
    	LOG0("DynInitMenuPaneL for color scheme menu--");
    }
}

void CMusicPlayerView::StartTypematicL(TFlags aKeyPressed)
{
	if(iTypematic)iTypematic->Cancel();
	else iTypematic=CPeriodic::NewL(CActive::EPriorityUserInput);
	TCallBack cb(&TypematicCallback,this);
	iTypematic->Start(KTypematicRepeatDelay,KTypematicRepeatRate,cb);
	iFlags|=aKeyPressed;
}
void CMusicPlayerView::StopTypematic()
{
	if(iTypematic)iTypematic->Cancel();
	//clear typematic range
	iFlags&=0x00FFFFFF;
}


TInt CMusicPlayerView::TypematicCallback(TAny* aInstance)
{
	LOG0("In Typematic callback");
	CMusicPlayerView *self=(CMusicPlayerView*)aInstance;
	if(self->iFlags&EVolumeUpPressed)
		self->VolumeUpDown(ETrue);
	else if(self->iFlags&EVolumeDownPressed)
		self->VolumeUpDown(EFalse);
	else if(self->iFlags&EFastForwardPressed)
	{
		TInt err;
		if(self->iFlags&ESeekFirstPress)
		{
			err=self->Seek(ETrue,ETrue);
			self->iFlags&=~ESeekFirstPress;
		}
		else err=self->Seek(EFalse,ETrue);
		if(!err && self->iMusicPlayerContainer)
			self->iMusicPlayerContainer->iCurrentTheme->UpdatePlaybackPosition(self->iPlaybackPosition);
		if(!err && self->iPlaylist)
			self->iPlaylist->UpdatePosition(self->iPlaybackPosition);
	}
	else if(self->iFlags&ERewindPressed)
	{
		TInt err;
		if(self->iFlags&ESeekFirstPress)
		{
			err=self->Seek(ETrue,EFalse);
			self->iFlags&=~ESeekFirstPress;
		}
		else err=self->Seek(EFalse,EFalse);
		if(!err && self->iMusicPlayerContainer)
			self->iMusicPlayerContainer->iCurrentTheme->UpdatePlaybackPosition(self->iPlaybackPosition);
		if(!err && self->iPlaylist)
			self->iPlaylist->UpdatePosition(self->iPlaybackPosition);
	};
	return 1;
}

void CMusicPlayerView::ScheduleWorkerL(TInt aJobs)
{
	iJobs|=aJobs;
	MyAppUi()->ScheduleWorkerL(0);
}

TInt CMusicPlayerView::IdleWorker()
{
    LOG(ELogGeneral,1,"CMusicPlayerView::IdleWorker: start (jobs=%d)",iJobs);
    
    //do stuff
    if(iJobs == EJobConstruct2) //we do this only when it is the only job left
    {
    	Construct2L();
    	iJobs-=EJobConstruct2;
    }
    else if(iJobs & EJobInitNextSong)
    {
    	NextPreviousL(1);
    	iJobs-=EJobInitNextSong;
    }
    else if(iJobs & EJobWaitSomeSeconds)
    {
    	//we sleep sone time before continuing
    	//we will use the crossfading timer for the wait job
    	if(iIdleDelayer)iIdleDelayer->Cancel();
    	else iIdleDelayer=CPeriodic::New(EPriorityLow);//can return NULL
    	if(iIdleDelayer)
    	{
    		TCallBack cb(&WaitFuntion,this);
    		iIdleDelayer->Start(KWaitingTime,KWaitingTime,cb);
    		LOG(ELogGeneral,-1,"CMusicPlayerView::IdleWorker: Waiting for some seconds before resuming");
    		//no more stuff to do before Wait function ends
    		iJobsSaved=iJobs;
    		iJobs=0;
    		return 0; //no more work to do
    	};
    }
    else if(iJobs & EJobStartCrossfadingTimer)
    {
    	if(iTrack->iState==CTrack::EStatePlaying)
    		StartCrossfadingTimer();
    	iJobs-=EJobStartCrossfadingTimer;
    }
    else if(iJobs & EJobPrepareNextSong)
    {
    	PrepareNextSongL();
    	iJobs-=EJobPrepareNextSong;
    }
    else if(iJobs & EJobAllowScrolling)
    {
    	iFlags|=EAllowScrolling;
    	if(iMusicPlayerContainer)
    		iMusicPlayerContainer->iCurrentTheme->AllowScrolling(ETrue);
    	iJobs-=EJobAllowScrolling;
    }


    //done
    if(iJobs)
    {
        LOG(ELogGeneral,-1,"CMusicPlayerView::IdleWorker: function end, still stuff to do");
        return 1; //more stuff to do
    }
    else
    {
        LOG(ELogGeneral,-1,"CMusicPlayerView::IdleWorker: end, nothing to do");
        return 0; //no more work to do
    };
}

TBool CMusicPlayerView::IsImgProcessingOngoing()
{
	return iThemeManager->iImgEngine->IsActive();
}

TInt CMusicPlayerView::WaitFuntion(TAny* aInstance)
{
	CMusicPlayerView *musicPlayerView=(CMusicPlayerView*)aInstance;
	LOG(ELogGeneral,1,"CMusicPlayerView::WaitFuntion: start");
	
	//we need to resume CIdle
	musicPlayerView->ScheduleWorkerL(musicPlayerView->iJobsSaved&~EJobWaitSomeSeconds);
	
	//... and cancel crossfading timer
	musicPlayerView->iIdleDelayer->Cancel();
	LOG(ELogGeneral,-1,"CMusicPlayerView::WaitFuntion: end");
	return 0; //no more stuff to do
}


void CMusicPlayerView::SelectEqualizerPresetL()
{
	LOG(ELogGeneral,1,"SelectEqualizerPresetL++ ");
	TInt selectedPreset(iPreferences->iAudioEqualizerPreset);
	//we need to get a list of presets
	CDesCArrayFlat* presetsArray(NULL);
	if(!iTrack || !(presetsArray=GetEqualizerPresetsAndEffectsL(selectedPreset,iPreferences->iPFlags&CMLauncherPreferences::EPreferencesUseBassBoost,iPreferences->iPFlags&CMLauncherPreferences::EPreferencesUseStereoWidening,iPreferences->iPFlags&CMLauncherPreferences::EPreferencesUseLoudness)))
	{
		LOG(ELogGeneral,-1,"SelectEqualizerPresetL-- There is no current track (%x) or getting the list of presets failed!",iTrack);
		//TODO: announce the user
		return;
	}
	
	LOG0("Presets: %d (including Equalizer Off), effects: %d",iNrPresets+1,presetsArray->Count()-(iNrPresets+1)-2); //2=the titles ("Presets", "Effects")
	if(selectedPreset<0)selectedPreset=-1;
	selectedPreset+=2;
	CEqualizerDlg::RunDlgLD(presetsArray,iNrPresets+1,selectedPreset,this);
			
	LOG(ELogGeneral,-1,"SelectEqualizerPresetL-- (dialog launched)");
}

void CMusicPlayerView::EqualizerChanged(TInt aValue)
{
	aValue--; //the first is the title
	if(aValue<=iNrPresets)
	{
		//this is one of the presets
		iPreferences->iAudioEqualizerPreset=aValue-1;
		iPreferences->SavePreferences();
		SetEqualizerPresetL();
	}
	else
	{
		//this is one of the effects
		aValue-=iNrPresets+2; //one is the second title, the other one is the "equalizer off"
		switch(aValue)
		{
		case 0: //bass boost
			iPreferences->iPFlags^=CMLauncherPreferences::EPreferencesUseBassBoost;
			iPreferences->SavePreferences();
			SetEffect(EBassBoost);
			break;
		case 1:
			iPreferences->iPFlags^=CMLauncherPreferences::EPreferencesUseStereoWidening;
			iPreferences->SavePreferences();
			SetEffect(EStereoWidening);
			break;
		case 2:
			iPreferences->iPFlags^=CMLauncherPreferences::EPreferencesUseLoudness;
			iPreferences->SavePreferences();
			SetEffect(ELoudness);
			break;
		default:FL_ASSERT(0);
		}//switch
	}//else
}
