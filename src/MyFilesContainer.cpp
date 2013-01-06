/*
 ============================================================================
 Name		: MyFilesContainer.cpp
 Author	  : Florin Lohan
 Version	 : 1.0
 Copyright   : (C) 2011 Florin Lohan. All rights reserved.
 Description : CMyFilesContainer implementation
 ============================================================================
 */
#include <eikclbd.h>
#include <akniconarray.h> //loading icons
#include <gulicon.h> //loading icons
#include <MLauncher.rsg>
#include <MLauncher.mbg> //loading icons
#include "MLauncher.hrh"
#include "MyFilesContainer.h"
#include "MyFilesView.h"
#include "log.h"

CMyFilesContainer::CMyFilesContainer()
{
	// No implementation required
}

CMyFilesContainer::~CMyFilesContainer()
{
	delete iMyFilesList;
}

CMyFilesContainer* CMyFilesContainer::NewLC(const TRect& aRect, CMyFilesView *aMyFilesView)
{
	CMyFilesContainer* self = new (ELeave) CMyFilesContainer();
	CleanupStack::PushL(self);
	self->ConstructL(aRect,aMyFilesView);
	return self;
}

CMyFilesContainer* CMyFilesContainer::NewL(const TRect& aRect, CMyFilesView *aMyFilesView)
{
	CMyFilesContainer* self = CMyFilesContainer::NewLC(aRect,aMyFilesView);
	CleanupStack::Pop(); // self;
	return self;
}

void CMyFilesContainer::ConstructL(const TRect& aRect, CMyFilesView *aMyFilesView)
{
	iMyFilesView=aMyFilesView;

	// Create a window for this application view
	CreateWindowL();

	//create the list (including icons)
	CreateListL();

	//add elements to the list
	AddListElementsL();

	// Set the windows size
	SetRect( aRect );

	// Activate the window, which makes it ready to be drawn
	ActivateL();

}

void CMyFilesContainer::Draw( const TRect& aRect) const
{
    // Get the standard graphics context
    CWindowGc& gc = SystemGc();
    // Clears the screen
    gc.Clear(aRect);
}

// -----------------------------------------------------------------------------
// CFFListContainer::SizeChanged()
// Called by framework when the view size is changed.
// -----------------------------------------------------------------------------
//
void CMyFilesContainer::SizeChanged()
{
    //DrawNow();
    if(iMyFilesList)
    {
    	iMyFilesList->SetRect(Rect());
    };
}

TInt CMyFilesContainer::CountComponentControls() const
{
    return 1;
}


CCoeControl* CMyFilesContainer::ComponentControl( TInt aIndex ) const
{
    switch ( aIndex )
    {
    case 0:
        return iMyFilesList;
    default:
        return NULL;
    }
}

TKeyResponse CMyFilesContainer::OfferKeyEventL( const TKeyEvent& aKeyEvent, TEventCode aType )
{
	LOG0("CMyFilesContainer::OfferKeyEventL");
	if(!iMyFilesList)return EKeyWasNotConsumed;
	//we do not handle other events than key presses
	
	return iMyFilesList->OfferKeyEventL ( aKeyEvent, aType );
}


void CMyFilesContainer::CreateListL()
{
    //create list
	//iPlaylistsList=new(ELeave)CAknSingleGraphicStyleListBox();
	iMyFilesList=new(ELeave)CAknDoubleLargeStyleListBox();
	iMyFilesList->ConstructL(this,EAknListBoxSelectionList);
	iMyFilesList->SetListBoxObserver(this);

    //create icons
    CreateIconsL(EFalse);

    //enable marquee
    //CColumnListBoxData *tmp=iPlaylistsList->ItemDrawer()->ColumnData();
    CFormattedCellListBoxData *tmp=iMyFilesList->ItemDrawer()->ColumnData();
    tmp->EnableMarqueeL(ETrue);

    //enable scroll bars
    iMyFilesList->CreateScrollBarFrameL(ETrue);
    iMyFilesList->ScrollBarFrame()->SetScrollBarVisibilityL(CEikScrollBarFrame::EOff,CEikScrollBarFrame::EAuto);
}

void CMyFilesContainer::CreateIconsL(TBool aUpdateIcons)
{
    CArrayPtr<CGulIcon>* icons;

    if(aUpdateIcons)
    {
        icons=iMyFilesList->ItemDrawer()->ColumnData()->IconArray();
        if(!icons)aUpdateIcons=EFalse;
        else icons->ResetAndDestroy();
    };
    if(!aUpdateIcons)
    {
        icons = new (ELeave) CAknIconArray(2);
        CleanupStack::PushL(icons);
    };


    _LIT(KOwnIconFile, "\\resource\\apps\\MLauncher.mif");
    //MAknsSkinInstance* skinInstance;
    CFbsBitmap* newIconBmp = NULL;
    CFbsBitmap* newIconBmpMask = NULL;
    CGulIcon* newIcon = NULL;

    //skinInstance = AknsUtils::SkinInstance();

    //creating icons
    //non-skined icons from avkon.mbg

    //create icon 0 (empty)
    /*
    AknIconUtils::CreateIconLC(newIconBmp, newIconBmpMask, KOwnIconFile,EMbmMlauncherEmpty, EMbmMlauncherEmpty_mask);
    newIcon = CGulIcon::NewL(newIconBmp,newIconBmpMask);
    CleanupStack::Pop(2,newIconBmp);//newIconBmp & newIconBmpMask
    CleanupStack::PushL(newIcon);
    icons->AppendL(newIcon);
    CleanupStack::Pop(newIcon);
    */
    
    //create icon 1 (selected)
    AknIconUtils::CreateIconLC(newIconBmp, newIconBmpMask, KOwnIconFile,EMbmMlauncherSel_pls, EMbmMlauncherSel_pls_mask);
    newIcon = CGulIcon::NewL(newIconBmp,newIconBmpMask);
    CleanupStack::Pop(2,newIconBmp);//newIconBmp & newIconBmpMask
    CleanupStack::PushL(newIcon);
    icons->AppendL(newIcon);
    CleanupStack::Pop(newIcon);

    //clean icon vars
    if(!aUpdateIcons)
    {
        CleanupStack::Pop(icons);
        //add icons to the list
        iMyFilesList->ItemDrawer()->ColumnData()->SetIconArray(icons);
    };
}

void CMyFilesContainer::AddListElementsL(TBool aUpdate /*=EFalse*/)
{
    //get pointer to model
    CTextListBoxModel *model=iMyFilesList->Model();//we do not take ownership
    CDesCArray* fileNames = STATIC_CAST(CDesCArray*, model->ItemTextArray());
    iMyFilesList->Reset();//delete all elements
    
    TInt i;
    for(i=0;i<iMyFilesView->iFiles.Count();i++)
    {
    	TFileName entry;
    	HBufC *f=iMyFilesView->iFiles[i];
    	entry.Copy(_L("\t"));
    	entry.Append(*f);
    	
    	entry.Append(_L("\tSomething"));
    	fileNames->AppendL(entry);
    };

    //do update stuff
    if(aUpdate)
    {
    	iMyFilesList->UpdateScrollBarsL();
    	iMyFilesList->HandleItemAdditionL();
    };
    DrawDeferred();
}


void CMyFilesContainer::HandleListBoxEventL (CEikListBox *aListBox, TListBoxEvent aEventType)
{
	if(aListBox==iMyFilesList)
	{
		switch(aEventType)
		{
		case EEventEnterKeyPressed:
		case EEventItemDoubleClicked:
		{
			iMyFilesView->HandleCommandL(ECommandMyFilesPlay);
		};break;
		default:break; //to avoid a warning
		};//switch
	}		
}

TInt CMyFilesContainer::CurrentIndex()
{
	return iMyFilesList->CurrentItemIndex();
}
