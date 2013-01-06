/*
 ============================================================================
 Name		: StopwatchContainer.h
 Author	  : Florin Lohan
 Version	 : 1.0
 Copyright   : Florin Lohan
 Description : CStopwatchContainer declaration
 ============================================================================
 */

#ifndef STOPWATCHCONTAINER_H
#define STOPWATCHCONTAINER_H

// INCLUDES
#include <coecntrl.h>

// CLASS DECLARATION
class CStopwatchView;
class CEikLabel;

/**
 *  CStopwatchContainer
 *
 */
class CStopwatchContainer : public CCoeControl
{
public:
    // Constructors and destructor

    /**
     * Destructor.
     */
    ~CStopwatchContainer();

    /**
     * Two-phased constructor.
     */
    static CStopwatchContainer* NewL( const TRect& aRect, CStopwatchView *aStopwatchView);

    /**
     * Two-phased constructor.
     */
    static CStopwatchContainer* NewLC( const TRect& aRect, CStopwatchView *aStopwatchView);

private:

    /**
     * Constructor for performing 1st stage construction
     */
    CStopwatchContainer(CStopwatchView *aStopwatchView);

    /**
     * EPOC default constructor for performing 2nd stage construction
     */
    void ConstructL(const TRect& aRect);

public:  // Functions from base classes

    /**
    * From CCoeControl, Draw
    * Draw this CStopwatchContainer to the screen.
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

public: //own function

    void UpdateData(TInt aElapsedSeconds, TInt aTotalSeconds);

public: //data

    CStopwatchView *iStopwatchView;//not owned, do not delete

    CEikLabel *iLabelElapsedTime;
    CEikLabel *iLabelRemainingTime;
    TInt iElapsedHours;
    TInt iElapsedMinutes;
    TInt iElapsedSeconds;
    TInt iRemainingHours;
    TInt iRemainingMinutes;
    TInt iRemainingSeconds;

};

#endif // STOPWATCHCONTAINER_H
