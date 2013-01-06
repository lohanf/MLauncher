#include <eikbtgpc.h>

#include "MLauncherAppUi.h"
#include "FiletransferView.h"
#include "FiletransferContainer.h"
#include "MLauncher.hrh"
#include <MLauncher.rsg>
#include <avkon.rsg>
#include "MLauncher.pan"
#include "log.h"

const TInt KUpdateTreshold=10240; //in bytes. We will only update data on screen for each KUpdateTreshold data transfered

void CFiletransferView::ConstructL()
{
    // Initialise app UI with standard value.
    BaseConstructL(R_FILETRANSFER_VIEW);
    // Create view object
    //iFiletransferContainer = CFiletransferContainer::NewL( ClientRect(), this);
}
// -----------------------------------------------------------------------------
// CFiletransferView::CFiletransferView()
// C++ default constructor can NOT contain any code, that might leave.
// -----------------------------------------------------------------------------
//
CFiletransferView::CFiletransferView()
{
    // No implementation required
}

// -----------------------------------------------------------------------------
// CFiletransferView::~CFiletransferView()
// Destructor.
// -----------------------------------------------------------------------------
//
CFiletransferView::~CFiletransferView()
{
    if ( iFiletransferContainer )
    {
        DoDeactivate();
    };
    iFileSizes.Close();
}

CFiletransferView* CFiletransferView::NewLC()
{
    CFiletransferView* self=new(ELeave)CFiletransferView;
    CleanupStack::PushL(self);
    self->ConstructL();
    return self;
}

// -----------------------------------------------------------------------------
//  Called by the framework when the application status pane
//  size is changed.  Passes the new client rectangle to the
//  AppView
// -----------------------------------------------------------------------------
//
void CFiletransferView::HandleStatusPaneSizeChange()
{
    if(iFiletransferContainer)
        iFiletransferContainer->SetRect( ClientRect() );
}

void CFiletransferView::DoDeactivate()
{
    if(iFiletransferContainer)
    {
        AppUi()->RemoveFromStack(iFiletransferContainer);
        delete iFiletransferContainer;
        iFiletransferContainer = NULL;
        //((CMLauncherAppUi*)AppUi())->ChangeExitWithNaviL(-1);
    }
}

void CFiletransferView::DoActivateL(const TVwsViewId& /*aPrevViewId*/,TUid /*aCustomMessageId*/, const TDesC8& /*aCustomMessage*/)
{
	StatusPane()->MakeVisible(ETrue);
	Cba()->MakeVisible(ETrue);
	if(!iFiletransferContainer)
    {
        iFiletransferContainer = CFiletransferContainer::NewL( ClientRect(), this);
    };
    iFiletransferContainer->SetMopParent(this);
    AppUi()->AddToStackL( *this, iFiletransferContainer );
}

TUid CFiletransferView::Id() const
{
    return TUid::Uid(KFiletransferViewId); //defined in hrh
}

void CFiletransferView::HandleCommandL( TInt aCommand )
{
    //try first to send it to the AppUi for handling
    if(((CMLauncherAppUi*)AppUi())->AppUiHandleCommandL(aCommand,this))
        return; //AppUiHandleCommandL returned TRUE, it handled the command

    //AppUiHandleCommandL did not handle the command
    switch(aCommand)
    {
    case ECommandCancelTransfer:
        if(iDirection==EDirectionSending)
            CCommSender::GetInstanceL()->CancelTransfer(ETrue);
        else
            CCommReceiver::GetInstanceL()->CancelTransfer(ETrue);

        //UpdateData(500);
        //((CMLauncherAppUi*)AppUi())->PlaylistTransferCompleteL(0);
        break;
    case ECommandSkipThisFile:
        if(iDirection==EDirectionSending)
            CCommSender::GetInstanceL()->CancelTransfer(EFalse);
        else
            CCommReceiver::GetInstanceL()->CancelTransfer(EFalse);
        break;
    default:
        Panic( EMLauncherUi );
    }
}

void CFiletransferView::NewTransferL(const TInt aNrFiles, const TInt aTotalSize, const RArray<TInt> &aFileSizes)
{
    iTotalFilesNo=aNrFiles;
    iTotalBytes2Transfer=aTotalSize;
    iStartTime.UniversalTime();
    iLastTime=iStartTime;
    iTotalBytesTransfered=0;
    iCurrentFileNo=0;
    iFileSizes=aFileSizes;
    //LOG0("Total nr of files 2 transfer: %d %d",iTotalFilesNo,iFileSizes.Count());
    __ASSERT_ALWAYS(iTotalFilesNo==iFileSizes.Count(),Panic(EBTWrongNumberOfFileSizes));
}

void CFiletransferView::NewFileL(const TInt aDirection,const TDes& aFilename,const TInt aSize)
{
    LOG0("Starting new file. Old file length: %d, transfered until now: %d",iBytesTransfered,iTotalBytesTransfered);
    iDirection=aDirection;
    iLabelTextFilename.Copy(aFilename);
    iCurrentFileSize=aSize;
    iBytesTransfered=0;
    iBytesTransferedLastTime=0;
    iCurrentFileNo++;
    iUpdateTreshold=KUpdateTreshold;

    //iXX=0;
    //
    if(iFiletransferContainer)
    {
        iFiletransferContainer->ResetLabelsL();
        iFiletransferContainer->SizeChanged();
        iFiletransferContainer->DrawNow();
    };
}

void CFiletransferView::UpdateData(const TInt aBytesTransfered)
{
    iBytesTransfered+=aBytesTransfered;
    iTotalBytesTransfered+=aBytesTransfered;
    iUpdateTreshold-=aBytesTransfered;
    //if(iUpdateTreshold>0)return;
    //if we are here, we update all things and then redraw
    //iUpdateTreshold=KUpdateTreshold;


}

void CFiletransferView::UpdateAllData()
{
    //update elapsed time
    TTime timeNow;
    timeNow.UniversalTime();
    TInt64 diff=timeNow.Int64()-iStartTime.Int64();
    iElapsedTime=diff/1000000;
    //update average speed (kbps)
    /*
	TInt64 bits=(TInt64)(iTotalBytesTransfered*7812.5);//7812.5=8*1000*1000/1024 (8=bits, 1024=kbits, 1000000 from microseconds)
	if(diff>0)
		iAverageSpeed=(TInt)(bits/diff);
	else
		iAverageSpeed=0;
     */

    TInt64 bitsx2=(TInt64)(iTotalBytesTransfered)*15625;//7812.5=8*1000*1000/1024 (8=bits, 1024=kbits, 1000000 from microseconds)
    if(diff>0)
    {
        iAverageSpeed=(TInt)(bitsx2/diff);
        iAverageSpeed>>=1; //divide by 2
    }
    else
        iAverageSpeed=0;
    //update instant speed
    diff=timeNow.Int64()-iLastTime.Int64();
    bitsx2=(TInt64)(iTotalBytesTransfered-iBytesTransferedLastTime)*15625;
    if(diff>0)
    {
        iInstantSpeed=(TInt)(bitsx2/diff);
        iInstantSpeed>>=1; //divide by 2
    }
    else
        iInstantSpeed=9999; //if diff is zero, the instant speed is too big
    //update estimated time (remaining bytes/average speed)
    if(iAverageSpeed>0)
        iEstimatedTime=(iTotalBytes2Transfer-iTotalBytesTransfered)/iAverageSpeed/128; //128=1024/8, transforming from bytes to kbps
    else
        iEstimatedTime=0;
    //init some variables
    iLastTime=timeNow;
    iBytesTransferedLastTime=iTotalBytesTransfered;

    /*
	if(iInstantSpeed<100)
	{
		LOG2("Speed: %d Instant speed: %d",iAverageSpeed,iInstantSpeed);
		LOG2("Time diff: %f Bytes (file): %d",(TReal)(diff/1000000.0),iBytesTransfered);
		LOG1("Bytes diff: %d",iBytesTransfered-iXX);
		iXX=iBytesTransfered;
	};
     */

    //done updating, now refresh screen
    /*
	if(iUpdateTreshold>0)return;
	iUpdateTreshold=KUpdateTreshold;
	if(iFiletransferContainer)
	{
		iFiletransferContainer->UpdateLabelsL();
		iFiletransferContainer->SizeChanged();//should optimize & calculate only for some fields
		iFiletransferContainer->DrawNow();
	};
     */
}
