/*
 ============================================================================
 Name		 : MusicPlayerTelObs.h
 Author	     : Florin Lohan
 Version	 : 1.0
 Copyright   : Florin Lohan
 Description : CMusicPlayerTelObs declaration
 ============================================================================
 */

#ifndef MUSICPLAYERTELOBS_H
#define MUSICPLAYERTELOBS_H

#include <etel3rdparty.h>

class CMusicPlayerView;

class CMusicPlayerTelObs : public CActive
{
public:
	// Cancel and destroy
	~CMusicPlayerTelObs();

	// Two-phased constructor.
	static CMusicPlayerTelObs* NewL(CMusicPlayerView &aView);

	// Two-phased constructor.
	static CMusicPlayerTelObs* NewLC(CMusicPlayerView &aView);

private:
	// C++ constructor
	CMusicPlayerTelObs(CMusicPlayerView &aView);

	// Second-phase constructor
	void ConstructL();

private:
	// From CActive
	// Handle completion
	void RunL();

	// How to cancel me
	void DoCancel();

	// Override to handle leaves from RunL(). Default implementation causes
	// the active scheduler to panic.
	TInt RunError(TInt aError);

private:
	CTelephony *iTelephony;
	CTelephony::TCallStatusV1 iLineStatus;
	CTelephony::TCallStatusV1Pckg iLineStatusPckg;
	CMusicPlayerView &iMusicPlayerView;
};

#endif // MUSICPLAYERTELOBS_H
