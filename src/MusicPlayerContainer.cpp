/*
 ============================================================================
 Name		: MusicPlayerContainer.cpp
 Author	  : Florin Lohan
 Version	 : 1.0
 Copyright   : Florin Lohan
 Description : CMusicPlayerContainer implementation
 ============================================================================
 */

#include <eiklabel.h>
#include <stringloader.h>
#include <aknenv.h>
#include <aknsutils.h> //loading icons
#include <aknsdrawutils.h> 
//#include <aknsbasicbackgroundcontrolcontext.h> 
//#include <coemain.h>
#include <MLauncher.rsg>
#include <MLauncher.mbg> //loading icons
#include <avkon.hrh>
#include "MusicPlayerContainer.h"
#include "MusicPlayerView.h"
#include "MusicPlayerThemes.h"
#include "MusicPlayerImgEngine.h"
#include "MLauncherPreferences.h"
#include "MLauncherAppUi.h"
#include "MLauncher.pan"
#include "MLauncher.hrh"
#include "defs.h"
#include "log.h"

/*
_LIT(KEmpty,"");
_LIT(KTimeFormat,"%d:%02d");
_LIT(KSpaceAfterTitle,"     ");
_LIT(KOwnIconFile, "\\resource\\apps\\MLauncher.mif");


const TInt KXMarginQVGA=3;
const TInt KYMarginQVGA=3;
const TInt KXMarginDouble=1;
const TInt KYMarginDouble=1;
const TInt KXMarginNHD=3;
const TInt KYMarginNHD=125;
const TRect KQVGAPortraitRect=TRect(0,0,240,292);
const TRect KDoublePortraitRect=TRect(0,352,352,376);
const TRect KnHDPortraitRect=TRect(0,0,360,578);

const TSize KLegacySize=TSize(176,176);
const TSize KDoubleSize=TSize(352,352);
const TSize KQVGASize=TSize(240,240);

const TRgb KRGBBlack=TRgb(0);
const TRgb KColorBackground(0);
const TRgb KColorText(0xFFFFFF);
const TUint32 KColorStartFillProgressBar(0x80FF);
//const TUint32 KColorStepFillProgressBar(0x00050A);
const TUint32 KColorStepFillProgressBar(0x000307);
const TRgb  KColorProgressBarBoundary(0xFF8000);
*/

const TRect KRectNHDportrait=TRect(0,0,360,640);
const TRect KRectNHDlandscape=TRect(0,0,640,360);

CMusicPlayerContainer::CMusicPlayerContainer()
{
	// No implementation required
}

CMusicPlayerContainer::~CMusicPlayerContainer()
{
	//delete iBackground;
}

CMusicPlayerContainer* CMusicPlayerContainer::NewLC(const TRect& aRect, CMusicPlayerView *aMusicPlayerView)
{
	CMusicPlayerContainer* self = new (ELeave) CMusicPlayerContainer();
	CleanupStack::PushL(self);
	self->iMusicPlayerView=aMusicPlayerView;
	self->ConstructL(aRect);
	return self;
}

CMusicPlayerContainer* CMusicPlayerContainer::NewL(const TRect& aRect, CMusicPlayerView *aMusicPlayerView)
{
	CMusicPlayerContainer* self = CMusicPlayerContainer::NewLC(aRect, aMusicPlayerView);
	CleanupStack::Pop(); // self;
	return self;
}

void CMusicPlayerContainer::ConstructL(const TRect& aRect)
{
	// Create a window for this application view
	CreateWindowL();

	// Set the windows size
	SetMopParent(iMusicPlayerView);
	/*
#ifdef __TOUCH_ENABLED__
	SetExtentToWholeScreen();
	if(aRect.Width()==360)
		iCurrentTheme=iMusicPlayerView->iThemeManager->GetThemeL(KRectNHDportrait,iCurrentTheme);
	else
		iCurrentTheme=iMusicPlayerView->iThemeManager->GetThemeL(KRectNHDlandscape,iCurrentTheme);
#else*/
	SetRect( aRect );
	iCurrentTheme=iMusicPlayerView->iThemeManager->GetThemeL(aRect,iCurrentTheme);
//#endif



	__ASSERT_ALWAYS(iCurrentTheme,Panic(EMusicPlayerNoTheme));
	
	
	iCurrentTheme->SetContainer(this);
	//iBackground = CAknsBasicBackgroundControlContext::NewL( KAknsIIDQsnBgAreaMain, aRect, EFalse );
	//SetExtent(aRect.iTl,aRect.Size());
	
#ifdef __TOUCH_ENABLED__
	EnableDragEvents();
#endif
	// Activate the window, which makes it ready to be drawn
	ActivateL();
}
/*
TTypeUid::Ptr CMusicPlayerContainer::MopSupplyObject(TTypeUid aId)
{
	if(aId.iUid == MAknsControlContext::ETypeId && iBackground)
	{
		return MAknsControlContext::SupplyMopObject( aId, iBackground);
	};

	return CCoeControl::MopSupplyObject( aId );
}
*/
// -----------------------------------------------------------------------------
// CFiletransferContainer::Draw()
// Draws the display.
// -----------------------------------------------------------------------------
//
void CMusicPlayerContainer::Draw( const TRect& aRect ) const
{
	CWindowGc& gc = SystemGc();
/*
	MAknsSkinInstance* skin = AknsUtils::SkinInstance();
	MAknsControlContext* cc = AknsDrawUtils::ControlContext(this);
	AknsDrawUtils::Background( skin, cc, this, gc, aRect);*/
	
	__ASSERT_ALWAYS(iCurrentTheme,Panic(EMusicPlayerNoTheme));
	iCurrentTheme->Draw(aRect,gc);
}

// -----------------------------------------------------------------------------
// CFiletransferContainer::SizeChanged()
// Called by framework when the view size is changed.
// -----------------------------------------------------------------------------
//
void CMusicPlayerContainer::SizeChanged()
{
	TRect rect = Rect();
	iCurrentTheme=iMusicPlayerView->iThemeManager->GetThemeL(rect,iCurrentTheme);
	__ASSERT_ALWAYS(iCurrentTheme,Panic(EMusicPlayerNoTheme));
	iCurrentTheme->SetContainer(this);
	//iCurrentTheme->ActivateL();
	/*
	if(iBackground)
	{
		iBackground->SetRect(rect);
		if ( &Window() )
		{
			iBackground->SetParentPos( PositionRelativeToScreen() );
		};
	};*/
}

TInt CMusicPlayerContainer::CountComponentControls() const
{
	__ASSERT_ALWAYS(iCurrentTheme,Panic(EMusicPlayerNoTheme));
	return iCurrentTheme->CountComponentControls();
}


CCoeControl* CMusicPlayerContainer::ComponentControl( TInt aIndex ) const
{
	__ASSERT_ALWAYS(iCurrentTheme,Panic(EMusicPlayerNoTheme));
	return iCurrentTheme->ComponentControl(aIndex);
}

TKeyResponse CMusicPlayerContainer::OfferKeyEventL( const TKeyEvent& aKeyEvent, TEventCode aType )
{
	//we do not handle other events than key presses at the moment
	LOG0("MusicPlayerContainer OfferKeyEvent: key=%d, code=%d",aKeyEvent.iCode,aType);

	if(aType==EEventKeyDown)
	{
		//clear the mode
		iKeyMode=EModeUnknown;
		return EKeyWasConsumed;
	}
	else if(aType==EEventKeyUp)
	{
		if(iKeyMode==EModeNext)
		{
			//do the "Next" operation
			iMusicPlayerView->NextPreviousL(1);
		}
		else if(iKeyMode==EModePrevious)
		{
			//do the "Previous" operation
			iMusicPlayerView->NextPreviousL(-1);
		}
		else if(iKeyMode==EModeSeek)
		{
			//start playing again
			iMusicPlayerView->SeekEnd();
		}
		//clear the mode
		iKeyMode=EModeUnknown;
		return EKeyWasConsumed;
	};

	//normal handling (EEventKey) starts here
	if(aKeyEvent.iCode==EKeyOK)
	{
		iMusicPlayerView->PlayPauseStop(EFalse); //EFalse means pause (if playing)
		return EKeyWasConsumed;
	}
	else if(aKeyEvent.iCode==EKeyUpArrow)
	{
		if(iMusicPlayerView->iPreferences->iCFlags&CMLauncherPreferences::EHwDeviceHasVolumeKeys)
		{
			//TODO: change the mode
		}
		else
		{
			//device does not have volume keys. Use up and down to change volume
			iMusicPlayerView->VolumeUpDown(ETrue);
		};
		return EKeyWasConsumed;
	}
	else if(aKeyEvent.iCode==EKeyDownArrow)
	{
		if(iMusicPlayerView->iPreferences->iCFlags&CMLauncherPreferences::EHwDeviceHasVolumeKeys)
		{
			//stop playback
			iMusicPlayerView->PlayPauseStop(ETrue); //ETrue means stop
		}
		else
		{
			//device does not have volume keys. Use up and down to change volume
			iMusicPlayerView->VolumeUpDown(EFalse);
		};
		return EKeyWasConsumed;
	}
	else if(aKeyEvent.iCode==EKeyLeftArrow)
	{
		//previous song or seek backwords
		TTimeIntervalMicroSeconds currentPosition;
		if(iKeyMode==EModeUnknown)
			iKeyMode=EModePrevious;
		else if(iKeyMode==EModePrevious)
		{
			//we are in seek mode
			iKeyMode=EModeSeek;
			TInt err=iMusicPlayerView->Seek(ETrue,EFalse,&currentPosition);
			//reset the progress bar
			
			if(!err)
			{
				LOG(ELogMusicPlayerDraw,0,"Seek left f: UpdatePlaybackPosition");
				iCurrentTheme->UpdatePlaybackPosition(currentPosition);
				LOG(ELogMusicPlayerDraw,0,"Seek left f: UpdatePlaybackPosition done");
			};
		}
		else
		{
			//we are continuing with seeking
			TInt err=iMusicPlayerView->Seek(EFalse,EFalse,&currentPosition);
			//reset the progress bar
			if(!err)
			{
				LOG(ELogMusicPlayerDraw,0,"Seek left: UpdatePlaybackPosition");
				iCurrentTheme->UpdatePlaybackPosition(currentPosition);
				LOG(ELogMusicPlayerDraw,0,"Seek left: UpdatePlaybackPosition done");
			};
		};
		return EKeyWasConsumed;
	}
	else if(aKeyEvent.iCode==EKeyRightArrow)
	{
		//next song or seek forward
		TTimeIntervalMicroSeconds currentPosition;
		if(iKeyMode==EModeUnknown)
			iKeyMode=EModeNext;
		else if(iKeyMode==EModeNext)
		{
			//we are in seek mode
			iKeyMode=EModeSeek;
			TInt err=iMusicPlayerView->Seek(ETrue,ETrue,&currentPosition);
			//reset the progress bar
			if(!err)
			{
				LOG(ELogMusicPlayerDraw,0,"Seek right f: UpdatePlaybackPosition");
				iCurrentTheme->UpdatePlaybackPosition(currentPosition);
				LOG(ELogMusicPlayerDraw,0,"Seek right f: UpdatePlaybackPosition done");
			};
		}
		else
		{
			//we are continuing with seeking
			TInt err=iMusicPlayerView->Seek(EFalse,ETrue,&currentPosition);
			//reset the progress bar
			if(!err)
			{
				LOG(ELogMusicPlayerDraw,0,"Seek right: UpdatePlaybackPosition");
				iCurrentTheme->UpdatePlaybackPosition(currentPosition);
				LOG(ELogMusicPlayerDraw,0,"Seek right: UpdatePlaybackPosition");
			};
		};
		return EKeyWasConsumed;
	}
	else if(aKeyEvent.iCode==42) // this is the * key
	{
		iMusicPlayerView->VolumeUpDown(EFalse);
	}
	else if(aKeyEvent.iCode==35) // this is the # key
	{
		iMusicPlayerView->VolumeUpDown(ETrue);
	}
	else if(aKeyEvent.iCode==0x31) // 1
	{
		iMusicPlayerView->HandleCommandL(ECommandMusicPlayerLoopOff);
		iMusicPlayerView->MyAppUi()->DisplayQueryOrNoteL(EMsgNotePersistent,R_MSG_LOOP_OFF,-1,R_MESSAGE_DLG_OK_EMPTY);
		return EKeyWasConsumed;
	}
	else if(aKeyEvent.iCode==0x32) // 2
	{
		iMusicPlayerView->HandleCommandL(ECommandMusicPlayerLoopPlaylist);
		iMusicPlayerView->MyAppUi()->DisplayQueryOrNoteL(EMsgNotePersistent,R_MSG_LOOP_PLAYLIST,-1,R_MESSAGE_DLG_OK_EMPTY);
		return EKeyWasConsumed;
	}
	else if(aKeyEvent.iCode==0x33) // 3
	{
		iMusicPlayerView->HandleCommandL(ECommandMusicPlayerLoopSong);
		iMusicPlayerView->MyAppUi()->DisplayQueryOrNoteL(EMsgNotePersistent,R_MSG_LOOP_SONG,-1,R_MESSAGE_DLG_OK_EMPTY);
		return EKeyWasConsumed;
	};

	if(aKeyEvent.iCode==EKeyNo) //the end/red key (EKeyPhoneEnd)
		iMusicPlayerView->HandleCommandL(EEikCmdExit);
	
	//if we are here ...
	return EKeyWasNotConsumed;
}

#ifdef __TOUCH_ENABLED__
void CMusicPlayerContainer::HandlePointerEventL (const TPointerEvent &aPointerEvent)
{
	CCoeControl::HandlePointerEventL(aPointerEvent);
	//LOG1("HandlePointerEvent, event: %d",aPointerEvent.iType);
	//LOG2("Coordinates: %d %d",aPointerEvent.iPosition.iX,aPointerEvent.iPosition.iY);
	__ASSERT_ALWAYS(iCurrentTheme,Panic(EMusicPlayerNoTheme));
	iCurrentTheme->HandlePointerEventL(aPointerEvent);
}
#endif

void CMusicPlayerContainer::HandleResourceChange(TInt aType)
{
	CCoeControl::HandleResourceChange(aType);
	if(aType == KEikDynamicLayoutVariantSwitch)
	{
		TRect r(iMusicPlayerView->ClientRect());
		if(r==iCurrentTheme->iMySize)
			return; //no point in going further, nothing will change (right?)
		
		LOG(ELogGeneral,1,"CMusicPlayerContainer::HandleResourceChange(): start");
		iMusicPlayerView->iThemeManager->iImgEngine->CancelRequest(iCurrentTheme);
		LOG0("ImgRequest canceled");
		iCurrentTheme->Deactivate();
		LOG0("Deactivation succeeded");
		
		
#ifdef __TOUCH_ENABLED__
		SetExtentToWholeScreen();
#else
		iMusicPlayerView->SetStatusPaneVisibility();
		r=iMusicPlayerView->ClientRect();
		LOG0("Setting new rect: %dx%d",r.Width(),r.Height());
		SetRect(r);
		LOG0("SetRect succeeded");
#endif
		if(iMusicPlayerView->iTrack && iMusicPlayerView->iTrack->iMetadata.iFileDirEntry)
		{
			iCurrentTheme->SetMetadataL(iMusicPlayerView->iTrack->iMetadata,iMusicPlayerView->iPlaybackPosition);
			LOG0("SetMetadata succeeded");
		};
		LOG0("Will activate the new theme");
		iCurrentTheme->ActivateL();
		LOG0("Theme activated successfully");
		DrawDeferred();
		LOG(ELogGeneral,-1,"CMusicPlayerContainer::HandleResourceChange(): end");
	};
}

RWindow& CMusicPlayerContainer::GetWindow()
{
	return Window();
};



/*
void CMusicPlayerContainer::SetMetadata(CMetadata &aMetadata,TTimeIntervalMicroSeconds &aCurrentPosition)
{
	if(iCurrentTheme)
	{
		iCurrentTheme->SetMetadataL(aMetadata,aCurrentPosition);
		DrawDeferred();
		return;
	};
}


TBool CMusicPlayerContainer::UpdatePlaybackPosition(TTimeIntervalMicroSeconds &aCurrentPosition, TBool aDraw)
{
	if(iCurrentTheme)
		return iCurrentTheme->UpdatePlaybackPosition(aCurrentPosition,aDraw);
	return EFalse;
}

void CMusicPlayerContainer::PeriodicUpdate(TTimeIntervalMicroSeconds &aCurrentPosition)
{
	if(iCurrentTheme)
		return iCurrentTheme->PeriodicUpdate(aCurrentPosition);
}
*/
