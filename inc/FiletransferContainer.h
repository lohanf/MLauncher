#ifndef FILETRANSFERCONTAINER_H_
#define FILETRANSFERCONTAINER_H_

#include <coecntrl.h>
#include <coeccntx.h>

// FORWARD DECLARATIONS
class CEikLabel;
class CFiletransferView;

class CFiletransferContainer : public CCoeControl
{
public: // New methods

    static CFiletransferContainer* NewL( const TRect& aRect, CFiletransferView *aFiletransferView);
    static CFiletransferContainer* NewLC( const TRect& aRect, CFiletransferView *aFiletransferView);

    virtual ~CFiletransferContainer();

private: // Constructors

    void ConstructL(const TRect& aRect);
    CFiletransferContainer(CFiletransferView *aFiletransferView);

public:  // Functions from base classes

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
    //TKeyResponse OfferKeyEventL( const TKeyEvent& aKeyEvent, TEventCode aType );

public:

	static TInt MoveLabel(TAny* aInstance);

	void ResetLabelsL();
	void UpdateLabelsL();

	void UpdateDimensions(const TRect& aRect);
private:
	// pointer of file details labels
	RPointerArray<CEikLabel> iFileDetailsLabels;
	TFileName iLabelTextFilename;
	RPointerArray<TRect> iRects;
	TInt iDistanceBetweenRectangles;

	//filename scroll
	CPeriodic *iAlarm;
	TInt iTick;
	TInt iBytesLastTime;
	TInt iTimesSame;

	// the brush style
	//CWindowGc::TBrushStyle iBrushStyle;
	// the brush color
	//TRgb iBrushColor;
	TRect iRect;
	TInt iWidth;
	TInt iMargin;
	TInt iRound;
	TInt iVerticalDistance;
	TInt iVD4,iVD5;
	TInt iHorizontalDistance;
	CFbsBitmap iFill;

	CFiletransferView *iFiletransferView;//not owned, do not delete
};
#endif /*FILETRANSFERCONTAINER_H_*/
