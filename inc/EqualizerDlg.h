/*
 ============================================================================
 Name		: EqualizerDlg.h
 Author	  : Florin Lohan
 Version	 : 1.0
 Copyright   : 
 Description : CEqualizerDlg declaration
 ============================================================================
 */

#ifndef EQUALIZERDLG_H
#define EQUALIZERDLG_H

// INCLUDES
#include <akndialog.h> 

class CMusicPlayerView;
class CAknSingleGraphicStyleListBox;

class CEqualizerDlg: public CAknDialog, public MEikListBoxObserver 
{
public:
	~CEqualizerDlg();
	
	static TInt RunDlgLD(CDesCArrayFlat *aElementsArray, TInt aNrPresets, TInt aSelectedPreset, CMusicPlayerView *aView);
private:
	void PreLayoutDynInitL();
	//from MEikListBoxObserver 
	void HandleListBoxEventL (CEikListBox *aListBox, TListBoxEvent aEventType); 
	//
	TBool OkToExitL(TInt aButtonId);
	TKeyResponse OfferKeyEventL(const TKeyEvent& aKeyEvent, TEventCode aType);
	
	//own
	void ListItemClicked();
private:
	CDesCArrayFlat *iElementsArray;
	TInt iNrPresets;
	TInt iSelectedPreset;
	CMusicPlayerView *iView; //not owned
	CAknSingleGraphicStyleListBox *iEEList; //not owned, constructed by the framework
};

#endif // EQUALIZERDLG_H
