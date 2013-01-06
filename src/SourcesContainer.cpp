/*
 ============================================================================
 Name		: SourcesContainer.cpp
 Author	  : Florin Lohan
 Version	 : 1.0
 Copyright   : Florin Lohan
 Description : CSourcesContainer implementation
 ============================================================================
 */
#include <coemain.h>
#include <barsread.h>
#include <eikclbd.h>
#include <akniconarray.h> //loading icons
#include <gulicon.h> //loading icons
#include <MLauncher.rsg>
#include <MLauncher.mbg> //loading icons
#include "SourcesContainer.h"
#include "SourcesView.h"
#include "MLauncherPreferences.h"
#include "log.h"

CSourcesContainer::CSourcesContainer()
{
	// No implementation required
}

CSourcesContainer::~CSourcesContainer()
{
	delete iSourcesList;
}

CSourcesContainer* CSourcesContainer::NewLC( const TRect& aRect, CSourcesView *aSourcesView)
{
	CSourcesContainer* self = new (ELeave) CSourcesContainer();
	CleanupStack::PushL(self);
	self->iSourcesView=aSourcesView;
	self->ConstructL(aRect);
	return self;
}

CSourcesContainer* CSourcesContainer::NewL( const TRect& aRect, CSourcesView *aSourcesView)
{
	CSourcesContainer* self = CSourcesContainer::NewLC(aRect,aSourcesView);
	CleanupStack::Pop(); // self;
	return self;
}

void CSourcesContainer::ConstructL(const TRect& aRect)
{
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

void CSourcesContainer::Draw( const TRect& aRect) const
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
void CSourcesContainer::SizeChanged()
{
    //DrawNow();
    if(iSourcesList)
    {
    	iSourcesList->SetRect(Rect());
        //iListView->ResetViewL();
    };
}

TInt CSourcesContainer::CountComponentControls() const
{
    return 1;
}


CCoeControl* CSourcesContainer::ComponentControl( TInt aIndex ) const
{
    switch ( aIndex )
    {
    case 0:
        return iSourcesList;
    default:
        return NULL;
    }
}

TKeyResponse CSourcesContainer::OfferKeyEventL( const TKeyEvent& aKeyEvent, TEventCode aType )
{
	LOG0("CSourcesContainer::OfferKeyEventL");
	if(!iSourcesList)return EKeyWasNotConsumed;
	//we do not handle other events than key presses
	if(aType!=EEventKey)return iSourcesList->OfferKeyEventL ( aKeyEvent, aType );

	//normal handling starts here
#ifdef __MSK_ENABLED__

	if(aKeyEvent.iCode==EKeyOK)
	{
		TInt index=iSourcesList->CurrentItemIndex();
		if(index<0)return EKeyWasConsumed;
		CListBoxView *view=iSourcesList->View();
		view->ToggleItemL(index);
		view->DrawItem(index);
		iSourcesView->SetMSKUseIgnore(view->ItemIsSelected(index));
		return EKeyWasConsumed;
	}
	else if(aKeyEvent.iCode==EKeyUpArrow)
	{
		//go one step up
		TInt index=iSourcesList->CurrentItemIndex()-1;
		CListBoxView *view=iSourcesList->View();
		if(index<0)
		{
			CTextListBoxModel *model=iSourcesList->Model();//we do not take ownership
			index=model->NumberOfItems()-1;
		};
		iSourcesView->SetMSKUseIgnore(view->ItemIsSelected(index));
		return iSourcesList->OfferKeyEventL ( aKeyEvent, aType );
	}
	else if(aKeyEvent.iCode==EKeyDownArrow)
	{
		//go one step down
		TInt index=iSourcesList->CurrentItemIndex()+1;
		CListBoxView *view=iSourcesList->View();
		CTextListBoxModel *model=iSourcesList->Model();//we do not take ownership
		TInt count=model->NumberOfItems();
		if(index>=count)
			index=0;
		iSourcesView->SetMSKUseIgnore(view->ItemIsSelected(index));
		return iSourcesList->OfferKeyEventL ( aKeyEvent, aType );
	};
#endif
	//if we are here ...
	return iSourcesList->OfferKeyEventL ( aKeyEvent, aType );
}


void CSourcesContainer::CreateListL()
{
    //create list
	//iSourcesList=new(ELeave)CAknSingleGraphicStyleListBox();
	iSourcesList=new(ELeave)CAknSingleGraphicHeadingStyleListBox();
	iSourcesList->ConstructL(this,EAknListBoxMultiselectionList);
	iSourcesList->SetListBoxObserver(this);

    //create icons
    CreateIconsL(EFalse);

    //enable marquee
    CColumnListBoxData *tmp=iSourcesList->ItemDrawer()->ColumnData();
    //CFormattedCellListBoxData *tmp=iSourcesList->ItemDrawer()->ColumnData();
    tmp->EnableMarqueeL(ETrue);

    //enable scroll bars
    iSourcesList->CreateScrollBarFrameL(ETrue);
    iSourcesList->ScrollBarFrame()->SetScrollBarVisibilityL(CEikScrollBarFrame::EOff,CEikScrollBarFrame::EAuto);
}

void CSourcesContainer::CreateIconsL(TBool aUpdateIcons)
{
    CArrayPtr<CGulIcon>* icons;

    if(aUpdateIcons)
    {
        icons=iSourcesList->ItemDrawer()->ColumnData()->IconArray();
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

    //create icon 0 (ticked/blacklist)
    AknIconUtils::CreateIconLC(newIconBmp, newIconBmpMask, KOwnIconFile,EMbmMlauncherSources_ignored, EMbmMlauncherSources_ignored_mask);

    newIcon = CGulIcon::NewL(newIconBmp,newIconBmpMask);
    CleanupStack::Pop(2,newIconBmp);//newIconBmp & newIconBmpMask
    CleanupStack::PushL(newIcon);
    icons->AppendL(newIcon);
    CleanupStack::Pop(newIcon);
    
    //create icon 1 (ticked/source-music)
    //AknIconUtils::CreateIconLC(newIconBmp, newIconBmpMask, KOwnIconFile,EMbmMlauncherSources_used, EMbmMlauncherSources_used_mask);
    AknIconUtils::CreateIconLC(newIconBmp, newIconBmpMask, KOwnIconFile,EMbmMlauncherSources_music, EMbmMlauncherSources_music_mask);

    newIcon = CGulIcon::NewL(newIconBmp,newIconBmpMask);
    CleanupStack::Pop(2,newIconBmp);//newIconBmp & newIconBmpMask
    CleanupStack::PushL(newIcon);
    icons->AppendL(newIcon);
    CleanupStack::Pop(newIcon);

    //create icon 2 (ticked/source-audiobooks)
    AknIconUtils::CreateIconLC(newIconBmp, newIconBmpMask, KOwnIconFile,EMbmMlauncherSources_audiobooks, EMbmMlauncherSources_audiobooks_mask);

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
        iSourcesList->ItemDrawer()->ColumnData()->SetIconArray(icons);
    };

}

void CSourcesContainer::AddListElementsL(TBool aUpdate /*=EFalse*/)
{
    //get pointer to model
    CTextListBoxModel *model=iSourcesList->Model();//we do not take ownership
    CDesCArray* sources = STATIC_CAST(CDesCArray*, model->ItemTextArray());
    sources->Reset();//delete all elements
    CArrayFix<TInt> *selectionIndexes=CONST_CAST(CArrayFix<TInt>*,iSourcesList->SelectionIndexes());
    selectionIndexes->Reset();
    CMLauncherPreferences *preferences=iSourcesView->iPreferences;
    TInt i,nr_startingdirs=preferences->iStartingDirs.Count();

    //first, add the sources
    for(i=0;i<nr_startingdirs;i++)
    {
    	TFileName entry;
    	//the type
    	if(preferences->iStartingDirsNrFiles[i]&CMLauncherPreferences::EFlagSourceIsAudiobook)
    		entry.Copy(_L("2\t"));
    	else
    		entry.Copy(_L("1\t"));
    		
    	//number of elements
    	if((preferences->iStartingDirsNrFiles[i]&0xFFFFFF)==0xFFFFFF)
    		entry.Append(_L("[?]\t")); //unknown
    	else
    		entry.AppendFormat(_L("[%d]\t"),preferences->iStartingDirsNrFiles[i]&0xFFFFFF);
    	
    		
    	entry.Append(*preferences->iStartingDirs[i]);

    	sources->AppendL(entry);
    };

    //then add the blacklist
    for(i=0;i<preferences->iBlacklistDirs.Count();i++)
    {
    	TFileName entry;
    	//the type
    	if(preferences->iBlacklistDirsNrFiles[i]&CMLauncherPreferences::EFlagSourceIsAudiobook)
    		entry.Copy(_L("2\t"));
    	else
    		entry.Copy(_L("1\t"));

    	//number of elements
    	if((preferences->iBlacklistDirsNrFiles[i]&0xFFFFFF)==0xFFFFFF)
    		entry.Append(_L("[?]\t"));
    	else
    		entry.AppendFormat(_L("[%d]\t"),preferences->iBlacklistDirsNrFiles[i]&0xFFFFFF);
    	entry.Append(*preferences->iBlacklistDirs[i]);
    	
    	sources->AppendL(entry);
    	selectionIndexes->AppendL(nr_startingdirs+i);
    };

    //do update stuff
    if(aUpdate)
    {
    	iSourcesList->UpdateScrollBarsL();
    	iSourcesList->HandleItemAdditionL();
#ifdef __MSK_ENABLED__
    	TInt index=sources->Count()?0:-1;
    	if(index<0)iSourcesView->SetMSKUseIgnore(EFalse,EFalse);
#endif
    	//TODO: Here we suppose there is at least one source...
    };
}


void CSourcesContainer::StoreSelection()
{
	CTextListBoxModel *model=iSourcesList->Model();//we do not take ownership
	TInt nr_sources(model->NumberOfItems());
	if(nr_sources==0)return; //no selection to store

	const CListBoxView::CSelectionIndexArray* selectionIndexes=iSourcesList->SelectionIndexes();
	TInt nr_selectionIndexes(selectionIndexes->Count());
	TInt i,j;
	TInt nr_startingdirs(iSourcesView->iPreferences->iStartingDirs.Count());
	TInt nr_blacklists(iSourcesView->iPreferences->iBlacklistDirs.Count());
	CArrayFixFlat<TInt> *blacklistedSources=new(ELeave)CArrayFixFlat<TInt>(8);
	CArrayFixFlat<TInt> *sourcesPreviouslyBlacklisted=new(ELeave)CArrayFixFlat<TInt>(8);

	for(i=0;i<nr_startingdirs;i++)
	{
		//try to find this source in the selection indexes
		TBool found(EFalse);
		for(j=0;j<nr_selectionIndexes;j++)
			if((*selectionIndexes)[j]==i)
			{
				found=ETrue;
				break;
			};
		if(found)
		{
			//this source needs to be converted to a blacklist
			blacklistedSources->AppendL(i);
		};
	};
	for(i=0;i<nr_blacklists;i++)
	{
		//try to find this source in the selection indexes
		TBool found(EFalse);
		for(j=0;j<nr_selectionIndexes;j++)
			if((*selectionIndexes)[j]==i+nr_startingdirs)
			{
				found=ETrue;
				break;
			};
		if(!found)
		{
			//this blacklist needs to be converted to a source
			sourcesPreviouslyBlacklisted->AppendL(i);
		};
	};
	//do the exchange: first append for both, then delete
	for(i=0;i<blacklistedSources->Count();i++)
	{
		iSourcesView->iPreferences->iBlacklistDirs.AppendL(iSourcesView->iPreferences->iStartingDirs[(*blacklistedSources)[i]]);
		iSourcesView->iPreferences->iBlacklistDirsNrFiles.AppendL(iSourcesView->iPreferences->iStartingDirsNrFiles[(*blacklistedSources)[i]]);
	};
	for(i=0;i<sourcesPreviouslyBlacklisted->Count();i++)
	{
		iSourcesView->iPreferences->iStartingDirs.AppendL(iSourcesView->iPreferences->iBlacklistDirs[(*sourcesPreviouslyBlacklisted)[i]]);
		iSourcesView->iPreferences->iStartingDirsNrFiles.AppendL(iSourcesView->iPreferences->iBlacklistDirsNrFiles[(*sourcesPreviouslyBlacklisted)[i]]);
	};
	
	for(i=blacklistedSources->Count()-1;i>=0;i--)
	{
		iSourcesView->iPreferences->iStartingDirs.Remove((*blacklistedSources)[i]);
		iSourcesView->iPreferences->iStartingDirsNrFiles.Remove((*blacklistedSources)[i]);
	};
	for(i=sourcesPreviouslyBlacklisted->Count()-1;i>=0;i--)
	{
		iSourcesView->iPreferences->iBlacklistDirs.Remove((*sourcesPreviouslyBlacklisted)[i]);
		iSourcesView->iPreferences->iBlacklistDirsNrFiles.Remove((*sourcesPreviouslyBlacklisted)[i]);
	};

	if(blacklistedSources->Count() || sourcesPreviouslyBlacklisted->Count())
		iSourcesView->iChangesMade=ETrue;
	//clean
	delete blacklistedSources;
	delete sourcesPreviouslyBlacklisted;
}

void CSourcesContainer::HandleListBoxEventL (CEikListBox *aListBox, TListBoxEvent aEventType)
{
	if(aListBox==iSourcesList)
	{
		switch(aEventType)
		{
		case EEventEnterKeyPressed:
		case EEventItemDoubleClicked:
		{

			TInt index=iSourcesList->CurrentItemIndex();
			if(index>=0)
			{
				CListBoxView *view=iSourcesList->View();
				view->ToggleItemL(index);
				view->DrawItem(index);
				iSourcesView->iChangesMade=ETrue;
#ifdef __MSK_ENABLED__
				iSourcesView->SetMSKUseIgnore(view->ItemIsSelected(index),ETrue);
#endif
			};
		};break;
		default:break; //to avoid a warning
		};//switch
	}		
}

