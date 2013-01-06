/*
 ============================================================================
 Name		 : PreferencesView.h
 Author	     : Florin Lohan
 Version	 : 1.0
 Copyright   : Florin Lohan
 Description : CPreferencesView declaration
 ============================================================================
 */

#ifndef PREFERENCESFORM_H
#define PREFERENCESFORM_H

// INCLUDES
#include <aknview.h>

class CMLauncherPreferences;
class CMLauncherAppUi;

// CLASS DECLARATION

/**
 *  CPreferencesView
 * 
 */
class CPreferencesView : public CAknView
{
public:
	// Constructors and destructor

	/**
	 * Destructor.
	 */
	~CPreferencesView();

	/**
	 * Two-phased constructor.
	 */
	static CPreferencesView* NewL(TInt aFocusPage);

	/**
	 * Two-phased constructor.
	 */
	static CPreferencesView* NewLC(TInt aFocusPage);

private:

	/**
	 * Constructor for performing 1st stage construction
	 */
	CPreferencesView();

	/**
	 * EPOC default constructor for performing 2nd stage construction
	 */
	void ConstructL(TInt aFocusPage);
	
public: //from base class
	/**
	 * From CEikAppUi, HandleCommandL.
	 * Takes care of command handling.
	 * @param aCommand Command to be handled.
	 */
	void HandleCommandL( TInt aCommand );

	TUid Id() const;
	void DoActivateL(const TVwsViewId& aPrevViewId,TUid aCustomMessageId, const TDesC8& aCustomMessage);
	void DoDeactivate();
	
	//own
	CMLauncherAppUi* MyAppUi(){return (CMLauncherAppUi*)AppUi();};

private:
    CMLauncherPreferences *iPreferences; //not owned
    TInt iFocusPage;
};

#endif // PREFERENCESFORM_H
