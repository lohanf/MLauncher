/*
 ============================================================================
 Name		: StopwatchContainer.cpp
 Author	  : Florin Lohan
 Version	 : 1.0
 Copyright   : Florin Lohan
 Description : CStopwatchContainer implementation
 ============================================================================
 */
#include <eiklabel.h>
#include <stringloader.h>
#include <MLauncher.rsg>
#include "StopwatchContainer.h"

_LIT(KTimeHMS,"%02d:%02d:%02d");
_LIT(KTimeMS,"%02d:%02d");

CStopwatchContainer::CStopwatchContainer(CStopwatchView *aStopwatchView) :
    iStopwatchView(aStopwatchView)
    {
    // No implementation required
    }

CStopwatchContainer::~CStopwatchContainer()
{
    if(iLabelElapsedTime)
        delete iLabelElapsedTime;
    if(iLabelRemainingTime)
        delete iLabelRemainingTime;
}

CStopwatchContainer* CStopwatchContainer::NewLC(const TRect& aRect, CStopwatchView *aStopwatchView)
{
    CStopwatchContainer* self = new (ELeave)CStopwatchContainer(aStopwatchView);
    CleanupStack::PushL(self);
    self->ConstructL(aRect);
    return self;
}

CStopwatchContainer* CStopwatchContainer::NewL(const TRect& aRect, CStopwatchView *aStopwatchView)
{
    CStopwatchContainer* self=CStopwatchContainer::NewLC(aRect,aStopwatchView);
    CleanupStack::Pop(); // self;
    return self;
}

void CStopwatchContainer::ConstructL(const TRect& aRect)
{
    // Create a window for this application view
    CreateWindowL();

    iLabelElapsedTime = new (ELeave) CEikLabel;
    iLabelElapsedTime->SetContainerWindowL(*this);
    iLabelRemainingTime = new (ELeave) CEikLabel;
    iLabelRemainingTime->SetContainerWindowL(*this);
    //UpdateData(0,0);

    // Set the windows size
    SetRect( aRect );

    // Activate the window, which makes it ready to be drawn
    ActivateL();
}

// -----------------------------------------------------------------------------
// CStopwatchContainer::Draw()
// Draws the display.
// -----------------------------------------------------------------------------
//
void CStopwatchContainer::Draw( const TRect& /*aRect*/ ) const
{
    // Get the standard graphics context
    CWindowGc& gc = SystemGc();

    // Clears the screen
    gc.Clear( Rect() );


}

// -----------------------------------------------------------------------------
// CStopwatchContainer::SizeChanged()
// Called by framework when the view size is changed.
// -----------------------------------------------------------------------------
//
void CStopwatchContainer::SizeChanged()
{
    TRect rect = Rect();
    //TSize labelSize;
    //TInt labelHeight;
    TInt margin=rect.Width()/10;
    TInt verticalDistance=rect.Height()/5; //

    //labelSize = iLabelElapsedTime->MinimumSize();
    //labelHeight=labelSize.iHeight;
    rect.iTl.iX=margin;
    rect.iTl.iY=verticalDistance*2;
    iLabelElapsedTime->SetRect(rect);

    //labelSize = iLabelRemainingTime->MinimumSize();
    //labelHeight=labelSize.iHeight;
    rect.iTl.iX=margin;
    rect.iTl.iY=verticalDistance*3;
    iLabelRemainingTime->SetRect(rect);
}

TInt CStopwatchContainer::CountComponentControls() const
{
    return 2;
}


CCoeControl* CStopwatchContainer::ComponentControl( TInt aIndex ) const
{
    switch(aIndex)
    {
    case 0:return iLabelElapsedTime;
    case 1:return iLabelRemainingTime;
    };
    return NULL;
}

void CStopwatchContainer::UpdateData(TInt aElapsedSeconds, TInt aTotalSeconds)
{
    //compute elapsed stuff
    iElapsedHours=aElapsedSeconds/3600;
    iElapsedMinutes=(aElapsedSeconds-60*iElapsedHours)/60;
    iElapsedSeconds=aElapsedSeconds%60;
    //normalise
    if(iElapsedHours<0)iElapsedHours=0;
    if(iElapsedMinutes<0)iElapsedMinutes=0;
    if(iElapsedSeconds<0)iElapsedSeconds=0;
    //compute remaining stuff
    TInt remainingSeconds=aTotalSeconds-aElapsedSeconds;
    iRemainingHours=remainingSeconds/3600;
    iRemainingMinutes=(remainingSeconds-60*iRemainingHours)/60;
    iRemainingSeconds=remainingSeconds%60;
    //normalise
    if(iRemainingHours<0)iRemainingHours=0;
    if(iRemainingMinutes<0)iRemainingMinutes=0;
    if(iRemainingSeconds<0)iRemainingSeconds=0;

    //update labels
    HBufC *elapsedTxt;
    TBuf<10> elapsedTime;
    if(iElapsedHours)
    {
        elapsedTime.Format(KTimeHMS,iElapsedHours,iElapsedMinutes,iElapsedSeconds);
    }
    else
    {
        elapsedTime.Format(KTimeMS,iElapsedMinutes,iElapsedSeconds);
    };
    elapsedTxt=StringLoader::LoadLC(R_COUNTDOWN_TIMER_ELAPSED,elapsedTime);
    iLabelElapsedTime->SetTextL(*elapsedTxt);
    CleanupStack::PopAndDestroy(elapsedTxt);

    HBufC *remainingTxt;
    TBuf<10> remainingTime;
    if(iRemainingHours)
    {
        remainingTime.Format(KTimeHMS,iRemainingHours,iRemainingMinutes,iRemainingSeconds);
    }
    else
    {
        remainingTime.Format(KTimeMS,iRemainingMinutes,iRemainingSeconds);
    };
    remainingTxt=StringLoader::LoadLC(R_COUNTDOWN_TIMER_REMAINING,remainingTime);
    iLabelRemainingTime->SetTextL(*remainingTxt);
    CleanupStack::PopAndDestroy(remainingTxt);
}
