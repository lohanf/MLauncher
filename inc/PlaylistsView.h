/*
 ============================================================================
 Name		: PlaylistsView.h
 Author	  : Florin Lohan
 Version	 : 1.0
 Copyright   : (C) 2010 Florin Lohan. All rights reserved.
 Description : CPlaylistsView declaration
 ============================================================================
 */

#ifndef PLAYLISTSVIEW_H
#define PLAYLISTSVIEW_H

// INCLUDES
#include <aknview.h>
#include "defs.h"

class CPlaylistsContainer;
class CMLauncherAppUi;

// CLASS DECLARATION

/**
 *  CPlaylistsView
 * 
 */
class CPlaylistsView : public CAknView
{
public:
	// Constructors and destructor
	~CPlaylistsView();
	static CPlaylistsView* NewL();
	static CPlaylistsView* NewLC();

private:
	CPlaylistsView();
	void ConstructL();

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
public:
	CMLauncherAppUi* MyAppUi(){return (CMLauncherAppUi*)AppUi();};
	
public:
	CPlaylistsContainer *iPlaylistsContainer;
	//CMLauncherPreferences *iPreferences; //not owned
	
};

#endif // PLAYLISTSVIEW_H
