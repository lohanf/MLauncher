/*
 ============================================================================
 Name		: MusicPlayerTelObs.cpp
 Author	  : Florin Lohan
 Version	 : 1.0
 Copyright   : Florin Lohan
 Description : CMusicPlayerTelObs implementation
 ============================================================================
 */

#include "MusicPlayerTelObs.h"
#include "MusicPlayerView.h"
#include "log.h"

CMusicPlayerTelObs::CMusicPlayerTelObs(CMusicPlayerView &aView) : CActive(EPriorityStandard), // Standard priority
	iLineStatusPckg(iLineStatus),iMusicPlayerView(aView)
{
	iLineStatus.iStatus = CTelephony::EStatusUnknown;
}

CMusicPlayerTelObs* CMusicPlayerTelObs::NewLC(CMusicPlayerView &aView)
{
	CMusicPlayerTelObs* self = new (ELeave) CMusicPlayerTelObs(aView);
	CleanupStack::PushL(self);
	self->ConstructL();
	return self;
}

CMusicPlayerTelObs* CMusicPlayerTelObs::NewL(CMusicPlayerView &aView)
{
	CMusicPlayerTelObs* self = CMusicPlayerTelObs::NewLC(aView);
	CleanupStack::Pop(); // self;
	return self;
}

void CMusicPlayerTelObs::ConstructL()
{
	CActiveScheduler::Add(this); // Add to scheduler
	iTelephony=CTelephony::NewL();
	iTelephony->NotifyChange(iStatus,CTelephony::EVoiceLineStatusChange,iLineStatusPckg);
	SetActive();
}

CMusicPlayerTelObs::~CMusicPlayerTelObs()
{
	Cancel(); // Cancel any request, if outstanding
	// Delete instance variables if any
	delete iTelephony;
}

void CMusicPlayerTelObs::DoCancel()
{
	iTelephony->CancelAsync( CTelephony::EVoiceLineStatusChangeCancel );
}

void CMusicPlayerTelObs::RunL()
{
	LOG0("CMusicPlayerTelObs::RunL line status=%d",iLineStatus.iStatus);
	if(iStatus==KErrNone)
	{
		if(iLineStatus.iStatus == CTelephony::EStatusIdle)
		{
			iMusicPlayerView.ResumePlayback();
			LOG0("CMusicPlayerTelObs::RunL: playback resumed (line status: idle)");
		}
		else if(iMusicPlayerView.iTrack)
		{
			//we have an ongoing call AND we are in playing state
			iMusicPlayerView.iTrack->iFlags|=CTrack::EKilledByTelephony;
			LOG0("CMusicPlayerTelObs::RunL: (line status changed: %d)",iLineStatus.iStatus);
		}
	}
	
	/* Request the next notification */
	iTelephony->NotifyChange( iStatus,CTelephony::EVoiceLineStatusChange,iLineStatusPckg );
	SetActive();
}

TInt CMusicPlayerTelObs::RunError(TInt aError)
{
	return aError;
}
