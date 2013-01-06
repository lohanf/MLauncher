/*
 ============================================================================
 Name		: PlaylistsContainer.h
 Author	  : Florin Lohan
 Version	 : 1.0
 Copyright   : (C) 2010 Florin Lohan. All rights reserved.
 Description : CPlaylistsContainer declaration
 ============================================================================
 */

#ifndef PLAYLISTSCONTAINER_H
#define PLAYLISTSCONTAINER_H

// INCLUDES
#include <coecntrl.h>
#include <aknlists.h>
#include <eiklbo.h>
#include "TrackInfo.h"

class CPlaylistsView;

// CLASS DECLARATION

/**
 *  CPlaylistsContainer
 * 
 */
class CPlaylistsContainer : public CCoeControl, MEikListBoxObserver, MPlaylistObserver
{
public:
	// Constructors and destructor
	~CPlaylistsContainer();
	static CPlaylistsContainer* NewL(const TRect& aRect, CPlaylistsView *aPlaylistsView);
	static CPlaylistsContainer* NewLC(const TRect& aRect, CPlaylistsView *aPlaylistsView);

private:
	CPlaylistsContainer();
	void ConstructL(const TRect& aRect, CPlaylistsView *aPlaylistsView);
	
public:
	/**
	 * From CCoeControl, Draw
	 * Draw this CFFListContainer to the screen.
	 * @param aRect the rectangle of this view that needs updating
	 */
	void Draw( const TRect& aRect ) const;

	/**
	 * From CoeControl, SizeChanged.
	 * Called by framework when the view size is changed.
	 */
	virtual void SizeChanged();

	/**
	 * From CCoeControl,CountComponentControls.
	 */
	TInt CountComponentControls() const;

	/**
	 * From CCoeControl,ComponentControl.
	 */
	CCoeControl* ComponentControl( TInt aIndex ) const;
	/**
	 * From CCoeControl,OfferKeyEventL.
	 */
	TKeyResponse OfferKeyEventL( const TKeyEvent& aKeyEvent, TEventCode aType );

public: // from MEikListBoxObserver
	virtual void  HandleListBoxEventL (CEikListBox *, TListBoxEvent );
	
	//from MPlaylistObserver
	virtual void PlaylistTrackAndPositionUpdated(CPlaylist *aPlaylist);
	
	//own functions:
	TInt CurrentIndex();
	
	void AddListElementsL(TBool aUpdate=EFalse);

private:
	//own functions:
	void CreateIconsL(TBool aUpdateIcons);

	void CreateListL();

	void AppendTimeInfo(TDes& aString, TInt aCurrentPosition, TInt aTotalDuration, TInt aTotalPlaylistDuration);
private:
	CAknDoubleLargeStyleListBox *iPlaylistsList;
	CPlaylistsView *iPlaylistsView;
	TInt iNrElementsOnScreen;
};

#endif // PLAYLISTSCONTAINER_H
