/*
 ============================================================================
 Name		: MyFilesContainer.h
 Author	  : Florin Lohan
 Version	 : 1.0
 Copyright   : (C) 2011 Florin Lohan. All rights reserved.
 Description : CMyFilesContainer declaration
 ============================================================================
 */

#ifndef MYFILESCONTAINER_H
#define MYFILESCONTAINER_H

// INCLUDES
#include <coecntrl.h>
#include <aknlists.h>
#include <eiklbo.h>


class CMyFilesView;

// CLASS DECLARATION

/**
 *  CMyFilesContainer
 * 
 */
class CMyFilesContainer : public CCoeControl, MEikListBoxObserver
{
public:
	// Constructors and destructor
	~CMyFilesContainer();
	static CMyFilesContainer* NewL(const TRect& aRect, CMyFilesView *aMyFilesView);
	static CMyFilesContainer* NewLC(const TRect& aRect, CMyFilesView *aMyFilesView);

private:
	CMyFilesContainer();
	void ConstructL(const TRect& aRect, CMyFilesView *aMyFilesView);
	
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
	
	void AddListElementsL(TBool aUpdate=EFalse);

	TInt CurrentIndex();
private:
	//own functions:
	void CreateIconsL(TBool aUpdateIcons);

	void CreateListL();

private:
	CAknDoubleLargeStyleListBox *iMyFilesList;
	CMyFilesView *iMyFilesView; //not owned
};

#endif // MYFILESCONTAINER_H
