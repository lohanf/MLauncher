/*
 ============================================================================
 Name		: PlaylistsContainer.cpp
 Author	  : Florin Lohan
 Version	 : 1.0
 Copyright   : (C) 2010 Florin Lohan. All rights reserved.
 Description : CPlaylistsContainer implementation
 ============================================================================
 */
//#include <coemain.h>
//#include <barsread.h>
#include <eikclbd.h>
#include <akniconarray.h> //loading icons
#include <gulicon.h> //loading icons
#include <MLauncher.rsg>
#include <MLauncher.mbg> //loading icons
#include "PlaylistsContainer.h"
#include "PlaylistsView.h"
#include "TrackInfo.h"
#include "MLauncherAppUi.h"
#include "MLauncher.hrh"
#include "log.h"

_LIT(KNoName,"(no name)");
_LIT(KSongTimeFormat,"%d:%02d/%d:%02d, total: "); 
_LIT(KSongTimeFormatHMS,"%d:%02d:%02d/%d:%02d:%02d, total: "); 

_LIT(KPlsTimeFormatMS,"%d:%02d)");
_LIT(KPlsTimeFormatHMS,"%d:%02d:%02d)");

CPlaylistsContainer::CPlaylistsContainer()
{
	// No implementation required
}

CPlaylistsContainer::~CPlaylistsContainer()
{
	delete iPlaylistsList;
	
	//remove ourselves as observer for the playlists
	TInt i;
	RPointerArray<CPlaylist> &pls=((CMLauncherDocument*)iPlaylistsView->MyAppUi()->Document())->iPlaylists;
	for(i=0;i<pls.Count();i++)
		pls[i]->RemoveObserver(this);
}

CPlaylistsContainer* CPlaylistsContainer::NewLC(const TRect& aRect, CPlaylistsView *aPlaylistsView)
{
	CPlaylistsContainer* self = new (ELeave) CPlaylistsContainer();
	CleanupStack::PushL(self);
	self->ConstructL(aRect,aPlaylistsView);
	return self;
}

CPlaylistsContainer* CPlaylistsContainer::NewL(const TRect& aRect, CPlaylistsView *aPlaylistsView)
{
	CPlaylistsContainer* self = CPlaylistsContainer::NewLC(aRect,aPlaylistsView);
	CleanupStack::Pop(); // self;
	return self;
}

void CPlaylistsContainer::ConstructL(const TRect& aRect, CPlaylistsView *aPlaylistsView)
{
	iPlaylistsView=aPlaylistsView;
	
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
	
	//add ourselves as observer for the playlists
	TInt i;
	RPointerArray<CPlaylist> &pls=((CMLauncherDocument*)iPlaylistsView->MyAppUi()->Document())->iPlaylists;
	for(i=0;i<pls.Count();i++)
		pls[i]->AddObserver(this);
}

void CPlaylistsContainer::Draw( const TRect& aRect) const
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
void CPlaylistsContainer::SizeChanged()
{
    //DrawNow();
    if(iPlaylistsList)
    {
    	iPlaylistsList->SetRect(Rect());
    };
}

TInt CPlaylistsContainer::CountComponentControls() const
{
    return 1;
}


CCoeControl* CPlaylistsContainer::ComponentControl( TInt aIndex ) const
{
    switch ( aIndex )
    {
    case 0:
        return iPlaylistsList;
    default:
        return NULL;
    }
}

TKeyResponse CPlaylistsContainer::OfferKeyEventL( const TKeyEvent& aKeyEvent, TEventCode aType )
{
	LOG0("CSourcesContainer::OfferKeyEventL");
	if(!iPlaylistsList)return EKeyWasNotConsumed;
	//we do not handle other events than key presses
	
	return iPlaylistsList->OfferKeyEventL ( aKeyEvent, aType );
}


void CPlaylistsContainer::CreateListL()
{
    //create list
	//iPlaylistsList=new(ELeave)CAknSingleGraphicStyleListBox();
	iPlaylistsList=new(ELeave)CAknDoubleLargeStyleListBox();
	iPlaylistsList->ConstructL(this,EAknListBoxSelectionList);
	iPlaylistsList->SetListBoxObserver(this);

    //create icons
    CreateIconsL(EFalse);

    //enable marquee
    //CColumnListBoxData *tmp=iPlaylistsList->ItemDrawer()->ColumnData();
    CFormattedCellListBoxData *tmp=iPlaylistsList->ItemDrawer()->ColumnData();
    tmp->EnableMarqueeL(ETrue);

    //enable scroll bars
    iPlaylistsList->CreateScrollBarFrameL(ETrue);
    iPlaylistsList->ScrollBarFrame()->SetScrollBarVisibilityL(CEikScrollBarFrame::EOff,CEikScrollBarFrame::EAuto);
}

void CPlaylistsContainer::CreateIconsL(TBool aUpdateIcons)
{
    CArrayPtr<CGulIcon>* icons;

    if(aUpdateIcons)
    {
        icons=iPlaylistsList->ItemDrawer()->ColumnData()->IconArray();
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
    AknIconUtils::CreateIconLC(newIconBmp, newIconBmpMask, KOwnIconFile,EMbmMlauncherEmpty, EMbmMlauncherEmpty_mask);
    newIcon = CGulIcon::NewL(newIconBmp,newIconBmpMask);
    CleanupStack::Pop(2,newIconBmp);//newIconBmp & newIconBmpMask
    CleanupStack::PushL(newIcon);
    icons->AppendL(newIcon);
    CleanupStack::Pop(newIcon);
    
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
        iPlaylistsList->ItemDrawer()->ColumnData()->SetIconArray(icons);
    };
}

void CPlaylistsContainer::AddListElementsL(TBool aUpdate /*=EFalse*/)
{
    //get pointer to model
    CTextListBoxModel *model=iPlaylistsList->Model();//we do not take ownership
    CDesCArray* playlists = STATIC_CAST(CDesCArray*, model->ItemTextArray());
    playlists->Reset();//delete all elements
    
    TInt i;
    RPointerArray<CPlaylist> &pls=((CMLauncherDocument*)iPlaylistsView->MyAppUi()->Document())->iPlaylists;
    //first, add the sources
    iNrElementsOnScreen=pls.Count();
    for(i=iNrElementsOnScreen-1;i>=0;i--)
    {
    	TFileName entry;
    	CPlaylist *p=pls[i];
    	//entry.Copy(_L("1\t"));
    	if(p->IsCurrent())
    		entry.Copy(_L("1\t"));
    	else
    		entry.Copy(_L("0\t"));
    	if(p->iName)
    		entry.Append(*p->iName);
    	else
    		entry.Append(KNoName);
    	//TInt index=p->GetCurrentIndex()+1;  	
    	entry.AppendFormat(_L("\t(%d/%d, "),p->GetCurrentIndex()+1,p->NrElementsWhenLoaded());

    	AppendTimeInfo(entry,p->iCurrentPlaybackPosition,p->CurrentTrackDuration(),(TInt)(p->iTotalDurationInMiliseconds/1000));
    	
    	playlists->AppendL(entry);
    };

    //do update stuff
    if(aUpdate)
    {
    	iPlaylistsList->UpdateScrollBarsL();
    	iPlaylistsList->HandleItemAdditionL();
    };
    DrawDeferred();
}


void CPlaylistsContainer::HandleListBoxEventL (CEikListBox *aListBox, TListBoxEvent aEventType)
{
	if(aListBox==iPlaylistsList)
	{
		switch(aEventType)
		{
		case EEventEnterKeyPressed:
		case EEventItemDoubleClicked:
		{
			iPlaylistsView->HandleCommandL(ECommandPlaylistPlay);
		};break;
		default:break; //to avoid a warning
		};//switch
	}		
}

TInt CPlaylistsContainer::CurrentIndex()
{
	return iNrElementsOnScreen-1-iPlaylistsList->CurrentItemIndex();
}

void CPlaylistsContainer::PlaylistTrackAndPositionUpdated(CPlaylist */*aPlaylist*/)
{
	AddListElementsL();
}

void CPlaylistsContainer::AppendTimeInfo(TDes& aString, TInt aCurrentPosition, TInt aTotalDuration, TInt aTotalPlaylistDuration)
{
	if(aTotalDuration>=3600)
	{
		TInt h(aCurrentPosition/3600);
		TInt m((aCurrentPosition-h*3600)/60);
		TInt h2(aTotalDuration/3600);
		TInt m2((aTotalDuration-h2*3600)/60);
		aString.AppendFormat(KSongTimeFormatHMS,h,m,aCurrentPosition%60,h2,m2,aTotalDuration%60);
	}
	else
		aString.AppendFormat(KSongTimeFormat,aCurrentPosition/60,aCurrentPosition%60,aTotalDuration/60,aTotalDuration%60);
	
	if(aTotalPlaylistDuration>=3600)
	{
		TInt h(aTotalPlaylistDuration/3600);
		TInt m((aTotalPlaylistDuration-h*3600)/60);
		aString.AppendFormat(KPlsTimeFormatHMS,h,m,aTotalPlaylistDuration%60);
	}
	else
		aString.AppendFormat(KPlsTimeFormatMS,aTotalPlaylistDuration/60,aTotalPlaylistDuration%60);
}
