/*
 ============================================================================
 Name        : SourcesContainer.h
 Author	     : Florin Lohan
 Version	 : 1.0
 Copyright   : Florin Lohan
 Description : CSourcesContainer declaration
 ============================================================================
 */

#ifndef SOURCESCONTAINER_H
#define SOURCESCONTAINER_H

// INCLUDES
#include <coecntrl.h>
#include <aknlists.h>
#include <eiklbo.h>

class CSourcesView;

// CLASS DECLARATION

/**
 *  CSourcesContainer
 *
 */
class CSourcesContainer : public CCoeControl, MEikListBoxObserver
{
public:
	// Constructors and destructor

	/**
	 * Destructor.
	 */
	~CSourcesContainer();

	/**
	 * Two-phased constructor.
	 */
	static CSourcesContainer* NewL( const TRect& aRect, CSourcesView *aSourcesView);

	/**
	 * Two-phased constructor.
	 */
	static CSourcesContainer* NewLC( const TRect& aRect, CSourcesView *aSourcesView);

private:

	/**
	 * Constructor for performing 1st stage construction
	 */
	CSourcesContainer();

	/**
	 * EPOC default constructor for performing 2nd stage construction
	 */
	void ConstructL(const TRect& aRect);

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

	//own functions:
	void CreateIconsL(TBool aUpdateIcons);
	void AddListElementsL(TBool aUpdate=EFalse);
	void StoreSelection();
	
private:
	void CreateListL();

public:
	//CAknSingleGraphicStyleListBox *iSourcesList;
	CAknSingleGraphicHeadingStyleListBox *iSourcesList;
	CSourcesView *iSourcesView;//not owned, do not delete
};

#endif // SOURCESCONTAINER_H
