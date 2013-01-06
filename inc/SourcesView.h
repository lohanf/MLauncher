/*
 ============================================================================
 Name		: SourcesView.h
 Author	  : Florin Lohan
 Version	 : 1.0
 Copyright   : Florin Lohan
 Description : CSourcesView declaration
 ============================================================================
 */

#ifndef SOURCESVIEW_H
#define SOURCESVIEW_H

// INCLUDES
#include <aknview.h>
#include <maknfilefilter.h> 
#include <maknfileselectionobserver.h> 
#include "defs.h"


class CSourcesContainer;
class CMLauncherPreferences;
class CMLauncherAppUi;


// CLASS DECLARATION

/**
 *  CSourcesView
 *
 */
class CSourcesView : public CAknView, public MAknFileSelectionObserver
{
public:
	// Constructors and destructor

	/**
	 * Destructor.
	 */
	~CSourcesView();

	/**
	 * Two-phased constructor.
	 */
	static CSourcesView* NewL();

	/**
	 * Two-phased constructor.
	 */
	static CSourcesView* NewLC();

public: //from base class
	/**
	 * From CEikAppUi, HandleCommandL.
	 * Takes care of command handling.
	 * @param aCommand Command to be handled.
	 */
	void HandleCommandL( TInt aCommand );

	/**
	 *  HandleStatusPaneSizeChange.
	 *  Called by the framework when the application status pane
	 *  size is changed.
	 */
	void HandleStatusPaneSizeChange();

	TUid Id() const;
	void DoActivateL(const TVwsViewId& aPrevViewId,TUid aCustomMessageId, const TDesC8& aCustomMessage);
	void DoDeactivate();
	
	// from MEikMenuObserver
	void DynInitMenuPaneL(TInt aResourceId, CEikMenuPane *aMenuPane);
	
	//own functions 
	TUint *CurrentElementsTypeAndNrElements();
	        
#ifdef __MSK_ENABLED__
	void SetMSKUseIgnore(TBool aUsed, TBool aHasSelection=ETrue);
#endif
	
private:
	//from MAknFileSelectionObserver 
	TBool OkToExitL(const TDesC &aDriveAndPath, const TEntry &aEntry);
private:

	/**
	 * Constructor for performing 1st stage construction
	 */
	CSourcesView();

	/**
	 * EPOC default constructor for performing 2nd stage construction
	 */
	void ConstructL();
	
	CMLauncherAppUi* MyAppUi(){return (CMLauncherAppUi*)AppUi();};

private:

#ifdef __MSK_ENABLED__
	//MSK labels
	HBufC *iMskUse;
	HBufC *iMskIgnore;
#endif

public:
	CSourcesContainer *iSourcesContainer;
	TBool iChangesMade;
	CMLauncherPreferences *iPreferences; //not owned
};


#endif // SOURCESVIEW_H
