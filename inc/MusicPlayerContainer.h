/*
 ============================================================================
 Name		 : MusicPlayerContainer.h
 Author	     : Florin Lohan
 Version	 : 1.0
 Copyright   : Florin Lohan
 Description : CMusicPlayerContainer declaration
 ============================================================================
 */

#ifndef MUSICPLAYERCONTAINER_H
#define MUSICPLAYERCONTAINER_H

// INCLUDES
#include <coecntrl.h>
#include "defs.h"

class CMusicPlayerView;
class CEikLabel;
class CPeriodic;
class CTheme;
class CMetadata;

//class CAknsBasicBackgroundControlContext;
// CLASS DECLARATION

/**
 *  CMusicPlayerContainer
 *
 */
class CMusicPlayerContainer : public CCoeControl
{
public:
	enum TKeyMode
	{
		EModeUnknown=0,
		EModeSeek,
		EModeNext,
		EModePrevious
	};
public:
	// Constructors and destructor

	/**
	 * Destructor.
	 */
	~CMusicPlayerContainer();

	/**
	 * Two-phased constructor.
	 */
	static CMusicPlayerContainer* NewL(const TRect& aRect, CMusicPlayerView *aMusicPlayerView);

	/**
	 * Two-phased constructor.
	 */
	static CMusicPlayerContainer* NewLC(const TRect& aRect, CMusicPlayerView *aMusicPlayerView);

public:  // Functions from base classes

    /**
    * From CCoeControl, Draw
    * Draw this CMusicPlayerContainer to the screen.
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
    
    void HandleResourceChange(TInt aType);
    
    //TTypeUid::Ptr MopSupplyObject(TTypeUid aId);
    
#ifdef __TOUCH_ENABLED__
    void HandlePointerEventL (const TPointerEvent &aPointerEvent);
#endif

    
public: //own functions
	
	RWindow& GetWindow();
	
	/*
	void SetMetadata(CMetadata &aMetadata,TTimeIntervalMicroSeconds &aCurrentPosition);
	TBool UpdatePlaybackPosition(TTimeIntervalMicroSeconds &aCurrentPosition, TBool aDraw=ETrue); //returns ETrue if the progress bar was updated
	void PeriodicUpdate(TTimeIntervalMicroSeconds &aCurrentPosition);
    */
private:

	/**
	 * Constructor for performing 1st stage construction
	 */
	CMusicPlayerContainer();

	/**
	 * EPOC default constructor for performing 2nd stage construction
	 */
	void ConstructL(const TRect& aRect);

private: //data
	CMusicPlayerView *iMusicPlayerView;//not owned, do not delete

	TInt iKeyMode;
	
	//skin background
	//CAknsBasicBackgroundControlContext* iBackground; // for skins support 
public:
	CTheme *iCurrentTheme;  //not owned
};

#endif // MUSICPLAYERCONTAINER_H
