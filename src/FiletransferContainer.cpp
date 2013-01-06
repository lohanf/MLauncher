#include <eikenv.h>
#include <aknenv.h>
#include <eiklabel.h>
#include <stringloader.h>

#include "FiletransferContainer.h"
#include "FiletransferView.h"
#include <MLauncher.rsg>
#include <Avkon.hrh>
#include "MLauncherBT.h"
#include "log.h"

const TInt KMarginRatio=100; //how much of the horizontal length we leave out as "margin"

// ============================ MEMBER FUNCTIONS ===============================

// -----------------------------------------------------------------------------
// CMLauncherAppView::NewL()
// Two-phased constructor.
// -----------------------------------------------------------------------------
//
CFiletransferContainer* CFiletransferContainer::NewL( const TRect& aRect, CFiletransferView *aFiletransferView)
    {
	CFiletransferContainer* self = CFiletransferContainer::NewLC( aRect, aFiletransferView);
    CleanupStack::Pop( self );
    return self;
    }

// -----------------------------------------------------------------------------
// CFiletransferContainer::NewLC()
// Two-phased constructor.
// -----------------------------------------------------------------------------
//
CFiletransferContainer* CFiletransferContainer::NewLC( const TRect& aRect, CFiletransferView *aFiletransferView)
    {
    CFiletransferContainer* self = new ( ELeave ) CFiletransferContainer(aFiletransferView);
    CleanupStack::PushL( self );
    self->ConstructL( aRect );
    return self;
    }

// -----------------------------------------------------------------------------
// CFiletransferContainer::ConstructL()
// Symbian 2nd phase constructor can leave.
// -----------------------------------------------------------------------------
//
void CFiletransferContainer::ConstructL( const TRect& aRect)
    {
    // Create a window for this application view
    CreateWindowL();

    UpdateDimensions(aRect);

    ResetLabelsL();

    // Set the windows size
    SetRect( aRect );

    // Activate the window, which makes it ready to be drawn
    ActivateL();
    }

// -----------------------------------------------------------------------------
// CFiletransferContainer::CFiletransferContainer()
// C++ default constructor can NOT contain any code, that might leave.
// -----------------------------------------------------------------------------
//
CFiletransferContainer::CFiletransferContainer(CFiletransferView *aFiletransferView) :
	iFileDetailsLabels(6), iFiletransferView(aFiletransferView)
    {
    // No implementation required
    }


// -----------------------------------------------------------------------------
// CFiletransferContainer::~CFiletransferContainer()
// Destructor.
// -----------------------------------------------------------------------------
//
CFiletransferContainer::~CFiletransferContainer()
{
	iFileDetailsLabels.ResetAndDestroy();
	iFileDetailsLabels.Close();
	iRects.ResetAndDestroy();
	iRects.Close();

	if(iAlarm)
	{
	    iAlarm->Cancel();
	    delete iAlarm;
	}
}


// -----------------------------------------------------------------------------
// CFiletransferContainer::Draw()
// Draws the display.
// -----------------------------------------------------------------------------
//
void CFiletransferContainer::Draw( const TRect& /*aRect*/ ) const
{
    // Get the standard graphics context
    CWindowGc& gc = SystemGc();

    // Clears the screen
    gc.Clear( Rect() );

    //LOG("CFiletransferContainer::Draw: start");

    gc.SetPenColor(AKN_LAF_COLOR(185));//green
	//gc.SetPenColor(AKN_LAF_COLOR(35));//red
    gc.UseBrushPattern(&iFill);
    gc.SetBrushStyle(CGraphicsContext::EPatternedBrush);
    gc.SetBrushColor(AKN_LAF_COLOR(215));
    gc.SetPenStyle(CGraphicsContext::ENullPen);
    TInt i;
    for(i=0;i<iRects.Count();i++)
    {
    	if(i==iFiletransferView->iCurrentFileNo-1 || iDistanceBetweenRectangles<0)
    	{
    		gc.SetBrushStyle(CGraphicsContext::ENullBrush);
    		gc.SetPenStyle(CGraphicsContext::ESolidPen);
    	};
    	gc.DrawRoundRect(*iRects[i],TSize(iRound,iRound));

    };
    //fill the rect up to now
    TRect fRect;
    TReal hrzCurrent;

    if(iDistanceBetweenRectangles>=0)
    {
    	//we have one rectangle for each file
    	fRect=*iRects[iFiletransferView->iCurrentFileNo-1];
    	hrzCurrent=fRect.Width();
        hrzCurrent=iFiletransferView->iBytesTransfered*(hrzCurrent-iDistanceBetweenRectangles)/iFiletransferView->iCurrentFileSize;
    }
    else
    {
    	//we have one rectangle overall
		fRect=*iRects[0];
		hrzCurrent=fRect.Width();
		hrzCurrent=iFiletransferView->iTotalBytesTransfered*hrzCurrent/iFiletransferView->iTotalBytes2Transfer;
    };

    gc.UseBrushPattern(&iFill);
    gc.SetBrushStyle(CGraphicsContext::EPatternedBrush);
    gc.SetBrushColor(AKN_LAF_COLOR(215));
    gc.SetPenStyle(CGraphicsContext::ENullPen);

    TInt delta=iRound-(TInt)hrzCurrent;
    if(delta<0)delta=0;
	fRect.SetWidth((TInt)hrzCurrent);
	fRect.Grow(0,-delta);
	gc.DrawRoundRect(fRect,TSize(iRound-delta,iRound-delta));
	//LOG("CFiletransferContainer::Draw: end");
}

// -----------------------------------------------------------------------------
// CFiletransferContainer::SizeChanged()
// Called by framework when the view size is changed.
// -----------------------------------------------------------------------------
//
void CFiletransferContainer::SizeChanged()
{
	TRect rect = Rect();
	TSize labelSize;
	TInt labelHeight;
	if(rect!=iRect)
		UpdateDimensions(rect);

	//label 0: Sending/Receiving
	//rect=Rect();
	labelSize = iFileDetailsLabels[0]->MinimumSize();
	labelHeight=labelSize.iHeight+6;
	rect.iTl.iX=iMargin;
	rect.iTl.iY=iVerticalDistance-labelHeight;
	//rect.iBr = rect.iTl + labelSize;
	iFileDetailsLabels[0]->SetRect(rect);

	//label 1: Current filename
	//rect=Rect();
	labelSize = iFileDetailsLabels[1]->MinimumSize();
	rect.iTl.iX=iMargin;
	rect.iTl.iY=iVerticalDistance*2-labelHeight;
	rect.iBr = rect.iTl + labelSize;
	iFileDetailsLabels[1]->SetRect(rect);
	iFileDetailsLabels[1]->UseLogicalToVisualConversion(EFalse);

	//label 2: total KB sent and %
	rect=Rect();
	//labelSize = iFileDetailsLabels[2]->MinimumSize();
	rect.iTl.iX=iMargin;
	rect.iTl.iY=(TInt)(iVerticalDistance*3.75-labelHeight);
	//rect.iBr = rect.iTl + labelSize;
	iFileDetailsLabels[2]->SetRect(rect);

	//label 3: KB remaining to be transmitted
	//rect=Rect();
	labelSize = iFileDetailsLabels[3]->MinimumSize();
	//iFileDetailsLabels[3]->SetLabelAlignment(ELayoutAlignRight);
	rect.iTl.iX=iWidth-iMargin-labelSize.iWidth;
	rect.iTl.iY=(TInt)(iVerticalDistance*3.75-labelHeight);
	//rect.iBr = rect.iTl + labelSize;
	iFileDetailsLabels[3]->SetRect(rect);

	//label 4: elapsed time
	//rect=Rect();
	labelSize = iFileDetailsLabels[4]->MinimumSize();
	rect.iTl.iX=iMargin;
	rect.iTl.iY=(TInt)(iVerticalDistance*5.75-labelHeight);
	//rect.iBr = rect.iTl + labelSize;
	iFileDetailsLabels[4]->SetRect(rect);

	//label 5: estimated time
	//rect=Rect();
	labelSize = iFileDetailsLabels[5]->MinimumSize();
	//iFileDetailsLabels[5]->SetLabelAlignment(ELayoutAlignRight);
	rect.iTl.iX=iWidth-iMargin-labelSize.iWidth;
	rect.iTl.iY=(TInt)(iVerticalDistance*5.75-labelHeight);
	//rect.iBr = rect.iTl + labelSize;
	iFileDetailsLabels[5]->SetRect(rect);

	//label 6: Link
	//rect=Rect();
	//labelSize = iFileDetailsLabels[6]->MinimumSize();
	rect.iTl.iX=iMargin;
	rect.iTl.iY=iVerticalDistance*7-labelHeight;
	//rect.iBr = rect.iTl + labelSize;
	iFileDetailsLabels[6]->SetRect(rect);

	//label 7: estimated time
	//rect=Rect();
	labelSize = iFileDetailsLabels[7]->MinimumSize();
	//iFileDetailsLabels[7]->SetLabelAlignment(ELayoutAlignRight);
	rect.iTl.iX=iWidth-iMargin-labelSize.iWidth;
	rect.iTl.iY=iVerticalDistance*7-labelHeight;
	//rect.iBr = rect.iTl + labelSize;
	iFileDetailsLabels[7]->SetRect(rect);

	//
	if( !iAlarm )
	{
		//start the alarm
		iAlarm = CPeriodic::New(EPriorityLow);
		if( iAlarm )
		{
			//alarm should start so that startTime+n*Period do not make exact seconds
			//otherwise the time will not be displayed nicely (seconds not incremented regularly)
		    TCallBack cb(&MoveLabel,this);
			iAlarm->Start(300000,200000,cb);
		};
	};
}

TInt CFiletransferContainer::CountComponentControls() const
{
	return iFileDetailsLabels.Count();
}


CCoeControl* CFiletransferContainer::ComponentControl( TInt aIndex ) const
{
	if( aIndex < iFileDetailsLabels.Count() )
		return iFileDetailsLabels[aIndex];
	return NULL;
}

void CFiletransferContainer::ResetLabelsL()
{
	CEikLabel *l;
	iFileDetailsLabels.ResetAndDestroy();
	HBufC* txt;
	TInt percent;

	//label 0: Sending/Receiving
	l = new (ELeave) CEikLabel;
	l->SetContainerWindowL(*this);
	//create the nr of files array
	CArrayFixFlat<TInt> *nrFiles=new(ELeave)CArrayFixFlat<TInt>(2);
	CleanupStack::PushL(nrFiles);
	nrFiles->AppendL(iFiletransferView->iCurrentFileNo);
	nrFiles->AppendL(iFiletransferView->iTotalFilesNo);
	//load label text

	if(iFiletransferView->iDirection==CFiletransferView::EDirectionSending)
		txt=StringLoader::LoadLC(R_FILETRANSFER_LABEL_SENDING_FILE,*nrFiles);
	else
		txt=StringLoader::LoadLC(R_FILETRANSFER_LABEL_RECEIVING_FILE,*nrFiles);
	//set the label and clean
	l->SetTextL(*txt);
	CleanupStack::PopAndDestroy(txt);
	CleanupStack::PopAndDestroy(nrFiles);
	iFileDetailsLabels.Append(l);

	//label 1: filename
	l = new (ELeave) CEikLabel;
	l->SetContainerWindowL(*this);
	iLabelTextFilename.Copy(iFiletransferView->iLabelTextFilename);
	iLabelTextFilename.Append(_L("     ")); //5 white spaces
	l->SetTextL(iLabelTextFilename);
	iFileDetailsLabels.Append(l);

	//label 2: KB sent/received + percent
	l = new (ELeave) CEikLabel;
	l->SetContainerWindowL(*this);
	//create the KB array
	CArrayFixFlat<TInt> *transferedKB=new(ELeave)CArrayFixFlat<TInt>(2);
	CleanupStack::PushL(transferedKB);
	transferedKB->AppendL(iFiletransferView->iTotalBytesTransfered/1024);
	percent=(TInt)(iFiletransferView->iTotalBytesTransfered*100.0/iFiletransferView->iTotalBytes2Transfer);
	transferedKB->AppendL(percent);
	//load label text
	txt=StringLoader::LoadLC(R_FILETRANSFER_LABEL_KB_TRANSFERED,*transferedKB);
	//set the label and clean
	l->SetTextL(*txt);
	CleanupStack::PopAndDestroy(txt);
	CleanupStack::PopAndDestroy(transferedKB);
	iFileDetailsLabels.Append(l);

	//label 3: KB remaining to sent/received
	l = new (ELeave) CEikLabel;
	l->SetContainerWindowL(*this);
	//
	TInt KB2Transfer=(iFiletransferView->iTotalBytes2Transfer-iFiletransferView->iTotalBytesTransfered)/1024;
	//load label text
	txt=StringLoader::LoadLC(R_FILETRANSFER_LABEL_KB_2TRANSFER,KB2Transfer);
	//set the label and clean
	l->SetTextL(*txt);
	CleanupStack::PopAndDestroy(txt);
	iFileDetailsLabels.Append(l);

	//label 4: Elapsed time (m,s):
	l = new (ELeave) CEikLabel;
	l->SetContainerWindowL(*this);
	//create the array
	CArrayFixFlat<TInt> *time=new(ELeave)CArrayFixFlat<TInt>(2);
	CleanupStack::PushL(time);
	time->AppendL(iFiletransferView->iElapsedTime/60); //minutes
	time->AppendL(iFiletransferView->iElapsedTime%60); //seconds
	//load label text
	txt=StringLoader::LoadLC(R_FILETRANSFER_LABEL_ELAPSED_TIME,*time);
	//set the label and clean
	l->SetTextL(*txt);
	CleanupStack::PopAndDestroy(txt);
	CleanupStack::PopAndDestroy(time);
	iFileDetailsLabels.Append(l);

	//label 5: Estimated time (m, s):
	l = new (ELeave) CEikLabel;
	l->SetContainerWindowL(*this);
	//create the array
	time=new(ELeave)CArrayFixFlat<TInt>(2);
	CleanupStack::PushL(time);
	time->AppendL(iFiletransferView->iEstimatedTime/60); //minutes
	time->AppendL(iFiletransferView->iEstimatedTime%60); //seconds
	//load label text
	txt=StringLoader::LoadLC(R_FILETRANSFER_LABEL_ESTIMATED_TIME,*time);
	//set the label and clean
	l->SetTextL(*txt);
	CleanupStack::PopAndDestroy(txt);
	CleanupStack::PopAndDestroy(time);
	iFileDetailsLabels.Append(l);

	//label 6: Link
	l = new (ELeave) CEikLabel;
	l->SetContainerWindowL(*this);
	//load label text
	txt=StringLoader::LoadLC(R_FILETRANSFER_LABEL_LINK);
	//set the label and clean
	l->SetTextL(*txt);
	CleanupStack::PopAndDestroy(txt);
	iFileDetailsLabels.Append(l);

	//label 7: Speed (avrg, kbps):
	l = new (ELeave) CEikLabel;
	l->SetContainerWindowL(*this);
	//load label text
	txt=StringLoader::LoadLC(R_FILETRANSFER_LABEL_SPEED,iFiletransferView->iAverageSpeed);
	//set the label and clean
	l->SetTextL(*txt);
	CleanupStack::PopAndDestroy(txt);
	iFileDetailsLabels.Append(l);
}

void CFiletransferContainer::UpdateLabelsL()
{
	//we update labels 2,3,4, 5 and 7
	CEikLabel *l;
	HBufC* txt;
	TInt percent;

	//label 2: KB sent/received + percent
	l = iFileDetailsLabels[2];
	//l->SetContainerWindowL(*this);
	//create the KB array
	CArrayFixFlat<TInt> *transferedKB=new(ELeave)CArrayFixFlat<TInt>(2);
	CleanupStack::PushL(transferedKB);
	transferedKB->AppendL(iFiletransferView->iTotalBytesTransfered/1024);
	percent=(TInt)(iFiletransferView->iTotalBytesTransfered*100.0/iFiletransferView->iTotalBytes2Transfer);
	transferedKB->AppendL(percent);
	//load label text
	txt=StringLoader::LoadLC(R_FILETRANSFER_LABEL_KB_TRANSFERED,*transferedKB);
	//set the label and clean
	l->SetTextL(*txt);
	CleanupStack::PopAndDestroy(txt);
	CleanupStack::PopAndDestroy(transferedKB);

	//label 3: KB remaining to sent/received
	l = iFileDetailsLabels[3];
	//l->SetContainerWindowL(*this);
	//
	TInt KB2Transfer=(iFiletransferView->iTotalBytes2Transfer-iFiletransferView->iTotalBytesTransfered)/1024;
	//load label text
	txt=StringLoader::LoadLC(R_FILETRANSFER_LABEL_KB_2TRANSFER,KB2Transfer);
	//set the label and clean
	l->SetTextL(*txt);
	CleanupStack::PopAndDestroy(txt);

	//label 4: Elapsed time (m,s):
	l = iFileDetailsLabels[4];
	//l->SetContainerWindowL(*this);
	//create the array
	CArrayFixFlat<TInt> *time=new(ELeave)CArrayFixFlat<TInt>(2);
	CleanupStack::PushL(time);
	time->AppendL(iFiletransferView->iElapsedTime/60); //minutes
	time->AppendL(iFiletransferView->iElapsedTime%60); //seconds
	//load label text
	txt=StringLoader::LoadLC(R_FILETRANSFER_LABEL_ELAPSED_TIME,*time);
	//set the label and clean
	l->SetTextL(*txt);
	CleanupStack::PopAndDestroy(txt);
	CleanupStack::PopAndDestroy(time);

	//label 5: Estimated time (m, s):
	l = iFileDetailsLabels[5];
	//l->SetContainerWindowL(*this);
	//create the array
	time=new(ELeave)CArrayFixFlat<TInt>(2);
	CleanupStack::PushL(time);
	time->AppendL(iFiletransferView->iEstimatedTime/60); //minutes
	time->AppendL(iFiletransferView->iEstimatedTime%60); //seconds
	//load label text
	txt=StringLoader::LoadLC(R_FILETRANSFER_LABEL_ESTIMATED_TIME,*time);
	//set the label and clean
	l->SetTextL(*txt);
	CleanupStack::PopAndDestroy(txt);
	CleanupStack::PopAndDestroy(time);

	//label 7: Speed (avrg, kbps):
	l = iFileDetailsLabels[7];
	//l->SetContainerWindowL(*this);
	//load label text
	txt=StringLoader::LoadLC(R_FILETRANSFER_LABEL_SPEED,iFiletransferView->iAverageSpeed);
	//set the label and clean
	l->SetTextL(*txt);
	CleanupStack::PopAndDestroy(txt);
}

TInt CFiletransferContainer::MoveLabel(TAny *aInstance )
{
	CFiletransferContainer *self = (CFiletransferContainer*)aInstance;
	TRect rect = self->Rect();
	TInt screenSize=rect.Width();

	//self->iFiletransferView->UpdateData(10000); // for debug only

	if( self->iFileDetailsLabels.Count() < 2)
		return 1;

	//if we are here, we make the label move....
	TSize labelSize = self->iFileDetailsLabels[1]->MinimumSize();
	if(labelSize.iWidth >= screenSize)
	{
		//if we are here, we have to move the label
		TChar first=self->iLabelTextFilename[0];
		self->iLabelTextFilename.Delete(0,1);
		self->iLabelTextFilename.Append(first);

		self->iFileDetailsLabels[1]->SetTextL(self->iLabelTextFilename);
	};
	
	self->iTick++;

	if(self->iTick>=4)
	{
		self->iTick=0;
		self->iFiletransferView->UpdateAllData();
		self->UpdateLabelsL();
		self->SizeChanged();

		if(self->iBytesLastTime==self->iFiletransferView->iTotalBytesTransfered)
			self->iTimesSame++;
		else self->iTimesSame=0;
		
		if(self->iTimesSame==5)
		{
			//we have a problem. Seems the sending/receiving is blocked
			//CCommSender::GetInstance()->iSocket->CancelAll();
			LOG0("FileTransfer::MoveLabel: No bytes transfered since last time (%d)",self->iBytesLastTime);

			if(self->iFiletransferView->iDirection==CFiletransferView::EDirectionSending)
			{
				//sender
				CCommSender::GetInstanceL()->CancelTransferTimeout();
			}
			else
			{
				//receiver
				CCommReceiver::GetInstanceL()->CancelTransferTimeout();
			};

		};
		self->iBytesLastTime=self->iFiletransferView->iTotalBytesTransfered;
		LOG(ELogBT,0,"BytesLastTime updated (%d)",self->iBytesLastTime);
	};
    self->DrawNow();

	return 1;
}

void CFiletransferContainer::UpdateDimensions(const TRect& aRect)
{
	iRect=aRect;
	iWidth=aRect.Width();
	iMargin=iWidth/KMarginRatio;
	iVerticalDistance=aRect.Height()/7;
	if(iVerticalDistance>50)
		iVerticalDistance=50;
	iRound=iVerticalDistance/5; //was 3 before
	iHorizontalDistance=iWidth-iMargin-iMargin;
	iVD4=iVerticalDistance*4;
	iVD5=iVerticalDistance*5;

    iFill.Create(TSize(1,iVerticalDistance),EColor16MU);
    TBitmapUtil bu(&iFill);
    bu.Begin(TPoint(0,0));
    TInt i;
    for(i=0;i<iVerticalDistance;i++)
    {
    	bu.SetPixel(0x00FF00-0x500*i);
    	bu.IncYPos();
    };

    //rectangles
    TInt totHrz=iMargin;
    TReal hrz=0;
    TRect *rect;
    iRects.ResetAndDestroy(); //we will append new elements

    //compute the distance between rectangles
    TInt averageRectangleLength=iHorizontalDistance/iFiletransferView->iTotalFilesNo;

    if(averageRectangleLength<5)
    {
    	//we should have a single rectangle, and not one rectangle for each file
    	//iRects.Count() will be one, but iDistanceBetweenRectangles is negative
    	iDistanceBetweenRectangles=-1;
    	//
    	rect=new(ELeave) TRect(totHrz,iVD4,totHrz+iHorizontalDistance,iVD5);
    	iRects.Append(rect);
    }
    else
    {
    	iDistanceBetweenRectangles=2; //this is what we will use if there are not so many rectangles
    	if(averageRectangleLength<20)iDistanceBetweenRectangles=1;
        if(averageRectangleLength<10)iDistanceBetweenRectangles=0;
        TInt remainingBytes=iFiletransferView->iTotalBytes2Transfer;
        TInt remainingHorizontalDistance=iHorizontalDistance;

        for(i=0;i<iFiletransferView->iTotalFilesNo;i++)
        {
        	totHrz+=(TInt)hrz;//hrz was computed in the last cycle

        	//hrz=iFiletransferView->iFileSizes[i]/(TReal)iFiletransferView->iTotalBytes2Transfer*iHorizontalDistance;
        	hrz=iFiletransferView->iFileSizes[i]/(TReal)remainingBytes*remainingHorizontalDistance;
        	remainingBytes-=iFiletransferView->iFileSizes[i];
        	remainingHorizontalDistance-=(TInt)hrz;

        	rect=new(ELeave) TRect(totHrz,iVD4,totHrz+(TInt)hrz-iDistanceBetweenRectangles,iVD5);
        	iRects.Append(rect);
        };
    }





}

/*
TKeyResponse CFiletransferContainer::OfferKeyEventL( const TKeyEvent& aKeyEvent, TEventCode aType )
{
	return EKeyWasNotConsumed;
}
*/

