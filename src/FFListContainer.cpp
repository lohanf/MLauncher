/*
============================================================================
 Name        : FFListContainer.cpp
 Author      : Florin Lohan
 Copyright   : Florin Lohan
 Description : Application view implementation
============================================================================
 */

// INCLUDE FILES
#include <coemain.h>
#include <barsread.h>
#include <akniconarray.h> //loading icons
#include <gulicon.h> //loading icons
#include <aknsutils.h> //loading icons
#include <avkon.mbg> //loading icons
#include <eikclbd.h> //loading icons
#include <aknsfld.h>

#include <stringloader.h>
#include <aknquerydialog.h>
#include <MLauncher.rsg>
#include <MLauncher.mbg> //loading icons
#include "FFListContainer.h"
#include "FFListView.h"
#include "MLauncherAppUi.h"
#include "TrackInfo.h"
#include "MLauncherPreferences.h"
#include "MLauncher.hrh"
#include "MLauncher.pan"
#include "log.h"

#ifdef __TOUCH_ENABLED__
//const TInt KMoveRightSpace=100;
const TInt KDragFactor=5;
const TInt KDragMinimumDistance=30;
#endif

const TInt KProgressBarRound=5;

// ============================ MEMBER FUNCTIONS ===============================

// -----------------------------------------------------------------------------
// CFFListContainer::NewL()
// Two-phased constructor.
// -----------------------------------------------------------------------------
//
CFFListContainer* CFFListContainer::NewL( const TRect& aRect, CFileDirPool *aPool, CFFListView *aListView)
{
    CFFListContainer* self = CFFListContainer::NewLC( aRect, aPool, aListView);
    CleanupStack::Pop( self );
    return self;
}

// -----------------------------------------------------------------------------
// CFFListContainer::NewLC()
// Two-phased constructor.
// -----------------------------------------------------------------------------
//
CFFListContainer* CFFListContainer::NewLC( const TRect& aRect, CFileDirPool *aPool, CFFListView *aListView)
{
    CFFListContainer* self = new ( ELeave ) CFFListContainer;
    self->iListView=aListView;
    CleanupStack::PushL( self );
    self->ConstructL( aRect, aPool );
    return self;
}

// -----------------------------------------------------------------------------
// CFFListContainer::ConstructL()
// Symbian 2nd phase constructor can leave.
// -----------------------------------------------------------------------------
//
void CFFListContainer::ConstructL( const TRect& aRect, CFileDirPool *aPool)
{
    // Create a window for this application view
    CreateWindowL();

    //create the list (including icons)
    CreateListL();
    CreatePopupFindL();

    //add elements to the list
    AddListElementsL(*aPool);
    
    // Set the windows size
    SetRect( aRect );

    // Activate the window, which makes it ready to be drawn
    ActivateL();
}

// -----------------------------------------------------------------------------
// CFFListContainer::CFFListContainer()
// C++ default constructor can NOT contain any code, that might leave.
// -----------------------------------------------------------------------------
//
CFFListContainer::CFFListContainer()
{
    // No implementation required
}


// -----------------------------------------------------------------------------
// CFFListContainer::~CFFListContainer()
// Destructor.
// -----------------------------------------------------------------------------
//
CFFListContainer::~CFFListContainer()
{
    delete iFileList;
    delete iFindBox;
}


// -----------------------------------------------------------------------------
// CFFListContainer::Draw()
// Draws the display.
// -----------------------------------------------------------------------------
//
void CFFListContainer::Draw( const TRect& aRect ) const
{
    // Get the standard graphics context
    CWindowGc& gc = SystemGc();

    // Gets the control's extent
    //TRect drawRect( Rect());

    // Clears the screen
    gc.SetBrushColor(0);
    gc.Clear( aRect );
    CFileDirPool *filesTree=iListView->MyDoc()->iFilesTree;
    if(iListView->iFlags&CFFListView::EContainerHasProgressBar && (filesTree->iCurrentParent->iFlags&TFileDirEntry::EIsMetadata))
    {
    	//we draw the progress bar
    	TRect r(Rect());
    	if(r.Intersects(aRect))
    	{
    		//makes sense to draw the progress bar
    		r.iBr.iY-=2;
    		r.iTl.iY=r.iBr.iY-iFontHeight;
    		//draw the full bar
    		gc.SetPenColor(0x0000FF);
    		gc.SetBrushStyle(CGraphicsContext::ENullBrush);
    		gc.SetPenStyle(CGraphicsContext::ESolidPen);
    		gc.DrawRoundRect(r,TSize(KProgressBarRound,KProgressBarRound));
    		
    		TInt rectangleWidth(r.Width()*filesTree->iCurrentFileNr/filesTree->iTotalNrFiles);
    		if(rectangleWidth>2) //no sese to draw anything for less than 2
    		{
    			//gc.UseBrushPattern(ESolidBrush);
    			//gc.SetBrushStyle(CGraphicsContext::EPatternedBrush);			
    			gc.SetBrushStyle(CGraphicsContext::ESolidBrush);
    			gc.SetBrushColor(0x0000FF);
    			gc.SetPenSize(TSize(0,0));
    			gc.SetBrushOrigin(r.iTl);
    			
    			r.SetWidth(rectangleWidth);
    			gc.DrawRoundRect(r,TSize(KProgressBarRound,KProgressBarRound));
    		};
    		
    		//draw text
    		TBuf<128> txt;
    		TTime now;
    		TTimeIntervalSeconds s;
    		now.UniversalTime();
    		now.SecondsFrom(iListView->iMetadataParseStart,s);
    		txt.Format(_L("Metadata: (%d/%d) [%ds]"),filesTree->iCurrentFileNr,filesTree->iTotalNrFiles,s.Int());
    		gc.UseFont(iFont);
    		gc.SetPenColor(0xFFFFFF);
    		gc.DrawText(txt,TPoint(r.iTl.iX+5,r.iTl.iY+iFont->AscentInPixels()+2));
    	};
    };
    
    if(iListView->iFlags&CFFListView::EWaitForFirstDraw)
    	iListView->FirstDrawDone();
}

// -----------------------------------------------------------------------------
// CFFListContainer::SizeChanged()
// Called by framework when the view size is changed.
// -----------------------------------------------------------------------------
//
void CFFListContainer::SizeChanged()
{
	LOG(ELogGeneral,1,"CFFListContainer::SizeChanged() start");
	if(iFileList)
	{
		TRect rect(Rect());
		if(iListView->iFlags&CFFListView::EContainerHasProgressBar && iListView->MyDoc()->iFilesTree->iCurrentParent->iFlags&TFileDirEntry::EIsMetadata)
		{
			//we give the lower part to the progress bar
			if(!iFont)
			{
				//get the font we want to use
				iFont=iEikonEnv->DenseFont();
				iFontHeight=iFont->AscentInPixels()+iFont->DescentInPixels()+2;
			};
			rect.iBr.iY-=iFontHeight+4;
			iFileList->SetRect(rect);
		}
		else if(iFindBox)
			AknFind::HandlePopupFindSizeChanged(this,iFileList,iFindBox);
		else
			iFileList->SetRect(Rect());

		iListView->ResetViewL();
	};
	LOG(ELogGeneral,-1,"CFFListContainer::SizeChanged() end");
}

TInt CFFListContainer::CountComponentControls() const
{
	return 2;
	//return 1;
}


CCoeControl* CFFListContainer::ComponentControl( TInt aIndex ) const
{
    __ASSERT_ALWAYS(aIndex<2,Panic(ECoeControlIndexTooBig));
	//__ASSERT_ALWAYS(aIndex<1,Panic(ECoeControlIndexTooBig));
    __ASSERT_ALWAYS(iFileList!=0,Panic(ECoeControlIsNull));
	switch ( aIndex )
    {
    case 0:
        return iFileList;
    case 1:
    	return iFindBox;
    default:
    	Panic(ECoeControlIndexTooBig);
    };
	return NULL; //we should not get here because of the Panic, but this line avoids a compile warning
}

TKeyResponse CFFListContainer::OfferKeyEventL( const TKeyEvent& aKeyEvent, TEventCode aType )
{
	if(!iFileList)return EKeyWasNotConsumed;

	if(iFindBox && iFindBox->IsVisible())
	{
		TBool needsRefresh(EFalse);
		TKeyResponse rsp=AknFind::HandleFindOfferKeyEventL(aKeyEvent,aType,this,iFileList,iFindBox,ETrue,needsRefresh);
		if(needsRefresh)
		{              
			AknFind::HandlePopupFindSizeChanged(this,iFileList,iFindBox);
			DrawDeferred();
		};
		if(rsp==EKeyWasConsumed)return rsp;
		//else we will handle the event by ourselves
	};   

	
	//we do not handle other events than key presses
	if(aType!=EEventKey)return iFileList->OfferKeyEventL ( aKeyEvent, aType );

	//normal handling starts here
	TBool moveMode=iListView->iMovingFiles.Count();
#ifdef __MSK_ENABLED__
	if(moveMode)
	{
		if(aKeyEvent.iCode==EKeyRightArrow)
		{
			//go one step to the right
			TInt index=iFileList->CurrentItemIndex();
			iListView->MoveRightL(index);
			return EKeyWasConsumed;
		}
		else if(aKeyEvent.iCode==EKeyLeftArrow)
		{
			//go one step to the left
			iListView->MoveLeftL();
			return EKeyWasConsumed;
		}
		else //let the list handle it
			return iFileList->OfferKeyEventL ( aKeyEvent, aType );
	}
	else
	{
		if(aKeyEvent.iCode==EKeyOK)
		{
			TInt index=iFileList->CurrentItemIndex();
			if(index>=0)
			{
				CListBoxView *view=iFileList->View();
				iListView->SetMSKSelectUnselect(!view->ItemIsSelected(index));
			}
			/*
			if(index<0)return EKeyWasConsumed;
			CListBoxView *view=iFileList->View();
			view->ToggleItemL(index);
			view->DrawItem(index);
			iListView->SetMSKSelectUnselect(view->ItemIsSelected(index));
			return EKeyWasConsumed;*/
		}
		else if(aKeyEvent.iCode==EKeyUpArrow)
		{
			//go one step up
			TInt index=iFileList->CurrentItemIndex()-1;
			CListBoxView *view=iFileList->View();
			if(index<0)
			{
				CTextListBoxModel *model=iFileList->Model();//we do not take ownership
				index=model->NumberOfItems()-1;
			};
			iListView->SetMSKSelectUnselect(view->ItemIsSelected(index));
			return iFileList->OfferKeyEventL ( aKeyEvent, aType );
		}
		else if(aKeyEvent.iCode==EKeyDownArrow)
		{
			//go one step down
			TInt index=iFileList->CurrentItemIndex()+1;
			CListBoxView *view=iFileList->View();
			CTextListBoxModel *model=iFileList->Model();//we do not take ownership
			TInt count=model->NumberOfItems();
			if(index>=count)
				index=0;
			iListView->SetMSKSelectUnselect(view->ItemIsSelected(index));
			return iFileList->OfferKeyEventL ( aKeyEvent, aType );
		};
	};
#endif
	/*if(aKeyEvent.iCode==EKeyOK && moveMode)
	{
		//we are in move mode, we can not select, but we should open a context menu
		CEikMenuBar *menu=iListView->MenuBar();
		if(menu)
		{
			//here we would change the menu ID with SetMenuTitleResourceId(), but in this case it is the same
			menu->TryDisplayMenuBarL();
			//here we would change back the menu ID (same function)
		};
		return EKeyWasConsumed;
	}
	else*/ if(aKeyEvent.iCode==EKeyRightArrow)
	{
		//go one step to the right
		TInt index=iFileList->CurrentItemIndex();
		iListView->MoveRightL(index);
		return EKeyWasConsumed;
	}
	else if(aKeyEvent.iCode==EKeyLeftArrow)
	{
		//go one step to the left
		iListView->MoveLeftL();
		return EKeyWasConsumed;
	}/*
	else if(aKeyEvent.iCode==0x30) // 0
	{
		//CreatePopupFindL();
		iFindBox->MakeVisible(ETrue);
		iFindBox->SetFocus(ETrue);
		//SizeChanged();
		return EKeyWasConsumed;
	}*/
	else if(aKeyEvent.iCode==0x31) // 1
	{
		iListView->HandleCommandL(ECommandPlay);
		return EKeyWasConsumed;
	}
	else if(aKeyEvent.iCode==0x32) // 2
	{
		iListView->HandleCommandL(ECommandShufflePlay);
		return EKeyWasConsumed;
	}
	/*
	else if(aKeyEvent.iCode==0x33) // 3
	{
		iListView->HandleCommandL(ECommandPlaylist);
		return EKeyWasConsumed;
	}*/
	else if(aKeyEvent.iCode==0x34) // 4
	{
		iListView->HandleCommandL(ECommandSelectAll);
		return EKeyWasConsumed;
	}
	else if(aKeyEvent.iCode==0x35) // 5
	{
		iListView->HandleCommandL(ECommandUnselectAll);
		return EKeyWasConsumed;
	}
	else if(aKeyEvent.iCode==0x36) // 6
	{
		iListView->HandleCommandL(ECommandSaveSelection);
		return EKeyWasConsumed;
	}
	else if(aKeyEvent.iCode==0x37) // 7
	{
		iListView->HandleCommandL(ECommandSendPlaylistBTDiscover);
		return EKeyWasConsumed;
	}
	else if(aKeyEvent.iCode==0x38) // 8
	{
		iListView->HandleCommandL(ECommandSendApplicationBTDiscover);
		return EKeyWasConsumed;
	}
	else if(aKeyEvent.iCode==0x39) // 9
	{
		iListView->HandleCommandL(ECommandCheckUpdates);
		return EKeyWasConsumed;
	}
	else return iFileList->OfferKeyEventL ( aKeyEvent, aType );

}

void CFFListContainer::HandleResourceChange(TInt aType)
{
	CCoeControl::HandleResourceChange(aType);
	if(aType == KEikDynamicLayoutVariantSwitch)
		SetRect( iListView->ClientRect() );
}

#ifdef __TOUCH_ENABLED__
void CFFListContainer::HandlePointerEventL (const TPointerEvent &aPointerEvent)
{
	//if(iFindBox)iFindBox->HandlePointerEventL(aPointerEvent);
	CCoeControl::HandlePointerEventL(aPointerEvent);
	/*
	if(!iFileList)return;
	if(aPointerEvent.iType==TPointerEvent::EButton1Down)
	{
		iDragStart=aPointerEvent.iPosition;
		iDragSelectionIndex=iFileList->CurrentItemIndex();
	}
	else if(aPointerEvent.iType==TPointerEvent::EDrag)
		iDragOperation=ETrue;
	if(aPointerEvent.iType==TPointerEvent::EButton1Up)
	{
		//was this a drag operation?
		if(iDragOperation)
		{
			//yes it was.
			TInt dx=aPointerEvent.iPosition.iX-iDragStart.iX;
			TInt dy=aPointerEvent.iPosition.iY-iDragStart.iY;
			if(Abs(dy)*KDragFactor<Abs(dx) && Abs(dx)>=KDragMinimumDistance)
			{
				//this is the right pattern to move either to the left or right
				if(dx>0)
					iListView->MoveRightL(iDragSelectionIndex);
				else
					iListView->MoveLeftL();
			};
			iDragOperation=EFalse;
		}
		else
		{
			//no drag operation, so at most a selection

			TInt selectedItem=iFileList->CurrentItemIndex();
			if(selectedItem==iLastSelectionIndex)
			{
				//select the item
				CListBoxView *view=iFileList->View();
				view->ToggleItemL(selectedItem);
				view->DrawItem(selectedItem);
			};
		};
		iLastSelectionIndex=iFileList->CurrentItemIndex();
	}; 
	*/
}
#endif

void CFFListContainer::CreateListL()
{
    //create list
    iFileList=new(ELeave)CAknSingleGraphicStyleListBox();
    iFileList->ConstructL(this,EAknListBoxMultiselectionList);
    //iFileList->SetContainerWindowL(*this);
    iFileList->SetListBoxObserver(this);
    iFileList->SetObserver(this);

    //construct list from resource
    /*
    TResourceReader reader;
    iEikonEnv->CreateResourceReaderLC(reader,R_MULTISELECTION_LIST);
    iFileList->ConstructFromResourceL(reader);
    CleanupStack::PopAndDestroy();//resource reader
     */
    //create the list's empty text
    SetListEmptyTextL();
    
    //create icons
    CreateIconsL(EFalse);

    //enable marquee
    CColumnListBoxData *tmp=iFileList->ItemDrawer()->ColumnData();
    tmp->EnableMarqueeL(ETrue);

    //enable scroll bars
    iFileList->CreateScrollBarFrameL(ETrue);
    iFileList->ScrollBarFrame()->SetScrollBarVisibilityL(CEikScrollBarFrame::EOff,CEikScrollBarFrame::EAuto);
}

void CFFListContainer::SetListEmptyTextL()
{
	LOG(ELogGeneral,1,"SetListEmptyText++ (%d)",iListView->MyDoc()->iPreferences->iCFlags&CMLauncherPreferences::EMusicSearchInProgress);
	HBufC *et(NULL);
	if(iListView->MyDoc()->iPreferences->iCFlags&CMLauncherPreferences::EMusicSearchInProgress)
		et=StringLoader::LoadLC(R_FFLIST_EMPTY_SEARCHINGMUSIC);
	else
		et=StringLoader::LoadLC(R_FFLIST_EMPTY_NOMUSIC);
	if(iFileList->View())
		iFileList->View()->SetListEmptyTextL(*et);
	CleanupStack::PopAndDestroy(et);
	DrawDeferred();
	LOG(ELogGeneral,-1,"SetListEmptyText--");
}

void CFFListContainer::CreatePopupFindL()
{
	CAknFilteredTextListBoxModel *model(NULL);
	if(iFileList)model=static_cast<CAknFilteredTextListBoxModel*>(iFileList->Model());
	if(model)
	{
		//iFindBox=CAknSearchField::NewL(*this,CAknSearchField::EPopupAdaptiveSearch,NULL,KMaxTexLengthInSelectionText);
		iFindBox=CAknSearchField::NewL(*this,CAknSearchField::EPopup,NULL,KMaxTexLengthInSelectionText);
		model->CreateFilterL(iFileList,iFindBox);
		model->Filter()->HandleOfferkeyEventL();
		//iFindBox->AddAdaptiveSearchTextObserverL(this);
#ifdef __TOUCH_ENABLED__
		iFindBox->MakeVisible(ETrue);
#else
		iFindBox->MakeVisible(EFalse);
#endif
		//iFindBox->SetFocus(EFalse);
	}
	else iFindBox=NULL;
}


void CFFListContainer::ShowFindPopup()
{
	if(iListView->iFlags&CFFListView::EContainerHasProgressBar)return;
	/*
	CAknFilteredTextListBoxModel *model=static_cast<CAknFilteredTextListBoxModel*>(iFileList->Model());
	CAknListBoxFilterItems *filter=model->Filter();
	filter->UpdateSelectionIndexesL();
	const CListBoxView::CSelectionIndexArray* selectionIndexes=filter->SelectionIndexes();
	LOG0("NR Selection indexes: %d",selectionIndexes->Count());
		*/
	iFindBox->MakeVisible(ETrue);
#ifndef __TOUCH_ENABLED__
	//there is a bug when trying to use EPopup:
	// http://www.developer.nokia.com/Community/Wiki/KIS001345_-_Input_problems_when_using_popup_search_field_in_a_dialog
	iFindBox->SetFocus(ETrue);
#endif
	//iFindBox->SetFocus(ETrue);
	SizeChanged();
}

void CFFListContainer::CreateIconsL(TBool aUpdateIcons)
{
    CArrayPtr<CGulIcon>* icons;
    
    LOG(ELogGeneral,1,"CreateIconsL++");

    if(aUpdateIcons)
    {
        icons=iFileList->ItemDrawer()->ColumnData()->IconArray();
        if(!icons)aUpdateIcons=EFalse;
        else icons->ResetAndDestroy();
    };
    if(!aUpdateIcons)
    {
        icons = new (ELeave) CAknIconArray(8);
        CleanupStack::PushL(icons);
    };

    TFileName avkonIconFile( AknIconUtils::AvkonIconFileName() );
    _LIT(KOwnIconFile, "\\resource\\apps\\MLauncher.mif");
    MAknsSkinInstance* skinInstance;
    CFbsBitmap* newIconBmp = NULL;
    CFbsBitmap* newIconBmpMask = NULL;
    CGulIcon* newIcon = NULL;

    skinInstance = AknsUtils::SkinInstance();

    TBool usePartialCheck=iListView->MyDoc()->iPreferences->iPFlags&CMLauncherPreferences::EPreferencesUsePartialCheck;
    //creating icons
    //get skined icons from aknsconstants.h
    //non-skined icons from avkon.mbg

    //create icon 0 (ticked checkbox)
    if(usePartialCheck)
    {
        //AknIconUtils::CreateIconLC(newIconBmp, newIconBmpMask, KOwnIconFile,EMbmMlauncherB_checked_svgt, EMbmMlauncherB_checked_svgt_mask);
        AknIconUtils::CreateIconLC(newIconBmp, newIconBmpMask, KOwnIconFile,EMbmMlauncherCheckbox_checked, EMbmMlauncherCheckbox_checked_mask);
    }
    else
    {
        AknsUtils::CreateIconLC(skinInstance,KAknsIIDQgnPropCheckboxOn,
                newIconBmp,newIconBmpMask,avkonIconFile,
                EMbmAvkonQgn_prop_checkbox_on,
                EMbmAvkonQgn_prop_checkbox_on_mask);
    };
    newIcon = CGulIcon::NewL(newIconBmp,newIconBmpMask);
    CleanupStack::Pop(2,newIconBmp);//newIconBmp & newIconBmpMask
    CleanupStack::PushL(newIcon);
    icons->AppendL(newIcon);
    CleanupStack::Pop(newIcon);

    //create icon 1 (non-ticked checkbox)
    if(usePartialCheck)
    {
        //AknIconUtils::CreateIconLC(newIconBmp, newIconBmpMask, KOwnIconFile,EMbmMlauncherB_empty_svgt, EMbmMlauncherB_empty_svgt_mask);
        AknIconUtils::CreateIconLC(newIconBmp, newIconBmpMask, KOwnIconFile,EMbmMlauncherCheckbox_empty, EMbmMlauncherCheckbox_empty_mask);
    }
    else
    {
        AknsUtils::CreateIconLC(skinInstance,KAknsIIDQgnPropCheckboxOff,
                newIconBmp,newIconBmpMask,avkonIconFile,
                EMbmAvkonQgn_prop_checkbox_off,
                EMbmAvkonQgn_prop_checkbox_off_mask);
    };
    newIcon = CGulIcon::NewL(newIconBmp,newIconBmpMask);
    CleanupStack::Pop(2,newIconBmp);//newIconBmp & newIconBmpMask
    CleanupStack::PushL(newIcon);
    icons->AppendL(newIcon);
    CleanupStack::Pop(newIcon);

    //create icon 2 (partially-ticked checkbox)
    if(usePartialCheck)
    {
        //AknIconUtils::CreateIconLC(newIconBmp, newIconBmpMask, KOwnIconFile,EMbmMlauncherB_partially_checked_svgt, EMbmMlauncherB_partially_checked_svgt_mask);
        AknIconUtils::CreateIconLC(newIconBmp, newIconBmpMask, KOwnIconFile,EMbmMlauncherCheckbox_partially_checked, EMbmMlauncherCheckbox_partially_checked_mask);

        newIcon = CGulIcon::NewL(newIconBmp,newIconBmpMask);
        CleanupStack::Pop(2,newIconBmp);//newIconBmp & newIconBmpMask
        CleanupStack::PushL(newIcon);
        icons->AppendL(newIcon);
        CleanupStack::Pop(newIcon);
    };

    //create icon 2+usePartialCheck (folder)
    /*
        AknsUtils::CreateIconLC(skinInstance,KAknsIIDQgnPropFolderMedium,
                               newIconBmp,newIconBmpMask,iconFile,
                               EMbmAvkonQgn_prop_folder_medium,
                               EMbmAvkonQgn_prop_folder_medium_mask);
     */
    AknsUtils::CreateIconLC(skinInstance,KAknsIIDQgnPropFolderSmall,
            newIconBmp,newIconBmpMask,avkonIconFile,
            EMbmAvkonQgn_prop_folder_small,
            EMbmAvkonQgn_prop_folder_small_mask);

    newIcon = CGulIcon::NewL(newIconBmp,newIconBmpMask);
    CleanupStack::Pop(2,newIconBmp);//newIconBmp & newIconBmpMask
    CleanupStack::PushL(newIcon);
    icons->AppendL(newIcon);
    CleanupStack::Pop(newIcon);

    //create icon 3+usePartialCheck (music file)
    AknsUtils::CreateIconLC(skinInstance,KAknsIIDQgnIndiAiNtMusic,
            newIconBmp,newIconBmpMask,avkonIconFile,
            EMbmAvkonQgn_indi_attach_audio,
            EMbmAvkonQgn_indi_attach_audio_mask);
    //EMbmAvkonQgn_prop_nrtyp_video,
    //EMbmAvkonQgn_prop_nrtyp_video_mask);

    newIcon = CGulIcon::NewL(newIconBmp,newIconBmpMask);
    CleanupStack::Pop(2,newIconBmp);//newIconBmp & newIconBmpMask
    CleanupStack::PushL(newIcon);
    icons->AppendL(newIcon);
    CleanupStack::Pop(newIcon);

    //create icon 4+usePartialCheck (folder with subfolders)
    AknsUtils::CreateIconLC(skinInstance,KAknsIIDQgnPropFolderSubSmall,
            newIconBmp,newIconBmpMask,avkonIconFile,
            EMbmAvkonQgn_prop_folder_sub_small,
            EMbmAvkonQgn_prop_folder_sub_small_mask);

    newIcon = CGulIcon::NewL(newIconBmp,newIconBmpMask);
    CleanupStack::Pop(2,newIconBmp);//newIconBmp & newIconBmpMask
    CleanupStack::PushL(newIcon);
    icons->AppendL(newIcon);
    CleanupStack::Pop(newIcon);

    //create icon 5+usePartialCheck (folder with media)
    AknsUtils::CreateIconLC(skinInstance,KAknsIIDQgnPropFolderSounds,
            newIconBmp,newIconBmpMask,avkonIconFile,
            EMbmAvkonQgn_prop_folder_small,
            EMbmAvkonQgn_prop_folder_small_mask);

    newIcon = CGulIcon::NewL(newIconBmp,newIconBmpMask);
    CleanupStack::Pop(2,newIconBmp);//newIconBmp & newIconBmpMask
    CleanupStack::PushL(newIcon);
    icons->AppendL(newIcon);
    CleanupStack::Pop(newIcon);

    /*
        //create icon 4 (<)
        AknsUtils::CreateIconLC(skinInstance,KAknsIIDQgnIndiNaviArrowLeft,
                               newIconBmp,newIconBmpMask,iconFile,
                               EMbmAvkonQgn_indi_navi_arrow_left,
                               EMbmAvkonQgn_indi_navi_arrow_left_mask);

        newIcon = CGulIcon::NewL(newIconBmp,newIconBmpMask);
        CleanupStack::Pop(2);//newIconBmp & newIconBmpMask
        CleanupStack::PushL(newIcon);
        icons->AppendL(newIcon);
        CleanupStack::Pop(newIcon);

        //create icon 5 (>)
        AknsUtils::CreateIconLC(skinInstance,KAknsIIDQgnIndiNaviArrowRight,
                               newIconBmp,newIconBmpMask,iconFile,
                               EMbmAvkonQgn_indi_navi_arrow_right,
                               EMbmAvkonQgn_indi_navi_arrow_right_mask);

        newIcon = CGulIcon::NewL(newIconBmp,newIconBmpMask);
        CleanupStack::Pop(2);//newIconBmp & newIconBmpMask
        CleanupStack::PushL(newIcon);
        icons->AppendL(newIcon);
        CleanupStack::Pop(newIcon);

     */




    //clean icon vars
    if(!aUpdateIcons)
    {
        CleanupStack::Pop(icons);
        //add icons to the list
        iFileList->ItemDrawer()->ColumnData()->SetIconArray(icons);
    };
    LOG(ELogGeneral,-1,"CreateIconsL--");
}

void CFFListContainer::AddListElementsL(CFileDirPool& aPool, TBool aUpdate)
{
	LOG(ELogGeneral,1,"AddListElementsL++ (update: %d)",aUpdate);
	TBool usePartialCheck=iListView->MyDoc()->iPreferences->iPFlags&CMLauncherPreferences::EPreferencesUsePartialCheck;
    //get pointer to model
    //CTextListBoxModel *model=iFileList->Model();//we do not take ownership
    CAknFilteredTextListBoxModel *model=(CAknFilteredTextListBoxModel*)iFileList->Model();//we do not take ownership
    CDesCArray* files = STATIC_CAST(CDesCArray*, model->ItemTextArray());
    files->Reset();//delete all elements

    TInt i;
    TBool isDir,isMusicFile;
    TInt selection; //0=not selected, 1=partially selected, 2=selected (fully)
    //CArrayFix<TInt> *selectionIndexes=CONST_CAST(CArrayFix<TInt>*,iFileList->SelectionIndexes());
    model->Filter()->ResetFilteringL();
    CArrayFix<TInt> *selectionIndexes=model->Filter()->SelectionIndexes();
    
    selectionIndexes->Reset();
    
    if(!(aPool.iCurrentParent->iFlags&TFileDirEntry::EWasExpanded))
    	aPool.MakeChildren(aPool.iCurrentParent);
    if(iHasSpecialElements)
    {
    	iFileList->ItemDrawer()->ClearAllPropertiesL();
    	iHasSpecialElements=EFalse;
    }
    //create the list out of the children
    LOG0("Current parent has %d children (iRoot=%x, iCurrentParent=%x)",aPool.iCurrentParent->ChildrenCount(),aPool.iRoot,aPool.iCurrentParent);
    for(i=0;i<aPool.iCurrentParent->ChildrenCount();i++)
    {
    	TFileName entry;
        TFileDirEntry &currentChild=(*aPool.iCurrentParent->iChildren)[i];
        
        __ASSERT_DEBUG(!(currentChild.iFlags & TFileDirEntry::EIsSelected && currentChild.iFlags & TFileDirEntry::EIsPartiallySelected),Panic(EBothIsSelectedAndIsPartiallySelected));
        /*
        if((currentChild->iFlags & CFileDirEntry::EIsSelected) &&
                !currentChild->AllChildrenAndSubchildrenSelected())
        {
            currentChild->iFlags-=CFileDirEntry::EIsSelected;
            currentChild->iFlags|=CFileDirEntry::EIsPartiallySelected;
        }*/

        if(currentChild.iFlags & TFileDirEntry::EIsSelected)
        {
            selection=2;
            entry.Copy(_L("1\t"));
            //LOG0("Element %d/%d, is selected.",i+1,aPool.iCurrentParent->ChildrenCount());
        }
        else if(currentChild.iFlags & TFileDirEntry::EIsPartiallySelected)
        {
        	//LOG0("Element %d/%d, is partially selected.",i+1,aPool.iCurrentParent->ChildrenCount());
            if(usePartialCheck)
            {
                selection=1;
                entry.Copy(_L("2\t"));
            }
            else
            {
                selection=2;
                entry.Copy(_L("1\t"));
            };
        }
        else
        {
        	//LOG0("Element %d/%d, is NOT selected.",i+1,aPool.iCurrentParent->ChildrenCount());
        	selection=0;
            entry.Copy(_L("1\t"));
        };
        if(currentChild.iFlags & TFileDirEntry::EIsDir){isDir=ETrue;isMusicFile=EFalse;}
        else {isDir=EFalse;isMusicFile=ETrue;};

        if(currentChild.iFlags&TFileDirEntry::EIsMetadata && 
        		currentChild.iParent!=iListView->MyDoc()->iFilesTree->iRoot)//this is the Artists holder, displayed as it is
        	currentChild.AppendMetadataDisplayName(entry);
        
        TPtr name(currentChild.iName,currentChild.iNameLength,currentChild.iNameLength);
        if(currentChild.iGlobalDisplayNameIndex!=255)
        {
        	//we are using one of the global display names
        	entry.Append(*iListView->MyDoc()->iGlobalDisplayNames[i]);
        }
        else if(currentChild.iDisplayNameRight)
        {
            //first part
        	if(currentChild.iDisplayNameLeft>0)
        		entry.Append(name.Left(currentChild.iDisplayNameLeft));
        	entry.Append(_L(" "));
        	//second part
        	entry.Append(name.Right(currentChild.iDisplayNameRight));
        }
        else entry.Append(name); //we do not have a "display name", we are using the entire name
       
        if(isDir)
        {
            if(usePartialCheck)
            {
                if(currentChild.iFlags & TFileDirEntry::EHasSubdirs)
                    entry.Append(_L("\t5"));//folder with subfolders icon
                else if(currentChild.iFlags & TFileDirEntry::EHasMedia)
                    entry.Append(_L("\t6"));//folder with media icon
                else
                    entry.Append(_L("\t3"));//folder icon
            }
            else
            {
                if(currentChild.iFlags & TFileDirEntry::EHasSubdirs)
                    entry.Append(_L("\t4"));//folder with subfolders icon
                else if(currentChild.iFlags & TFileDirEntry::EHasMedia)
                    entry.Append(_L("\t5"));//folder with media icon
                else
                    entry.Append(_L("\t2"));//folder icon
            };
        }
        else
        {
            if(usePartialCheck)
                entry.Append(_L("\t4"));//music file icon
            else
                entry.Append(_L("\t3"));//music file icon
        };
        files->AppendL(entry);
        //LOG1("List entry: %S",&entry);
        if(selection==2)
        {
            selectionIndexes->AppendL(i);
            //LOG0("Child selection appended");
        }
        iFileList->HandleItemAdditionL();
        
        //check if this is a special directory
        if(currentChild.iFlags & TFileDirEntry::EIsSpecial)
        {
        	//mark this entry as special
            CColumnListBoxItemDrawer *cdr=iFileList->ItemDrawer();
            TListItemProperties props=cdr->Properties(i);
            props.SetUnderlined(ETrue);
            cdr->SetPropertiesL(i,props);
            iHasSpecialElements=ETrue;
        }
    };
    
    //check for empty list
    /*
    if(!files->Count())
    {
    	//there is nothing to show to the user in this folder!!
    	if(aPool.iCurrentParent->ChildrenCount()==0)
    	{
    		if(aPool.iCurrentParent==aPool.iRoot)
    		{
    			//we are in the root view and there is no stuff to display.
    			//HBufC *et=StringLoader::LoadLC(,)
    			iFileList->View()->SetListEmptyTextL(_L("cucu"));
    		}
    	}
    }*/
    
    //model->Filter()->UpdateSelectionIndexesL();
    model->Filter()->HandleItemArrayChangeL();
    //do update stuff
    if(aUpdate)
    {
    	SetCurrentItemAndMSK(*aPool.iCurrentParent);
    	iFileList->UpdateScrollBarsL();
    };
    DrawDeferred();
    LOG(ELogGeneral,-1,"AddListElementsL--");
}

void CFFListContainer::SetCurrentItemAndMSK(TFileDirEntry& aDirEntry)
{
	LOG(ELogGeneral,1,"CFFListContainer::SetCurrentItemAndMSK start");
	TInt itemsCount = iFileList->Model()->ItemTextArray()->MdcaCount();
	TInt index=itemsCount?0:-1;
	if(itemsCount && aDirEntry.iSelectedPosition>=0 && aDirEntry.iSelectedPosition<itemsCount)
	{
		index=aDirEntry.iSelectedPosition;
		LOG0("Setting current item index: %d (max=%d)",index,itemsCount);
		//iFileList->SetCurrentItemIndexAndDraw(index);
		iFileList->SetCurrentItemIndex(index);
		//iFileList->View()->SetCurrentItemIndex(index);
	};

#ifdef __MSK_ENABLED__
	TBool moveMode=iListView->iMovingFiles.Count();
	if(!moveMode)
	{
		if(index>=0)
		{
			if((*aDirEntry.iChildren)[index].iFlags & TFileDirEntry::EIsSelected)
				iListView->SetMSKSelectUnselect(ETrue);
			else
				iListView->SetMSKSelectUnselect(EFalse);
		}
		else
			iListView->SetMSKSelectUnselect(EFalse,EFalse);

	};
#endif
	LOG(ELogGeneral,-1,"CFFListContainer::SetCurrentItemAndMSK end");
}

void CFFListContainer::StoreSelection(CFileDirPool *aPool, TBool aDeleteDisplayName)
{
	TInt nr_children(aPool->iCurrentParent->ChildrenCount());
	if(nr_children==0)return; //no selection to store

	LOG(ELogGeneral,1,"CFFListContainer::StoreSelection++");
	//const CListBoxView::CSelectionIndexArray* selectionIndexes=iFileList->SelectionIndexes();
	CAknFilteredTextListBoxModel *model=static_cast<CAknFilteredTextListBoxModel*>(iFileList->Model());
	CAknListBoxFilterItems *filter=model->Filter();
	filter->UpdateSelectionIndexesL();
	const CListBoxView::CSelectionIndexArray* selectionIndexes=filter->SelectionIndexes();
    TBool usePartialCheck=iListView->MyDoc()->iPreferences->iPFlags&CMLauncherPreferences::EPreferencesUsePartialCheck;
    TInt nr_selectionIndexes(selectionIndexes->Count());
    TInt i;
    //TInt flags;
    LOG0("Nr selection indexes: %d",nr_selectionIndexes);

    //get the position of the current selection
    aPool->iCurrentParent->iSelectedPosition=iFileList->CurrentItemIndex();
    //reset all the selected flags
    for(i=0;i<nr_children;i++)
    {
        TFileDirEntry &child=(*aPool->iCurrentParent->iChildren)[i];
    	if(aDeleteDisplayName && !(child.iFlags & TFileDirEntry::EIsSpecial))
            child.iDisplayNameLeft=child.iDisplayNameRight=0;

        if(child.iFlags & TFileDirEntry::EIsSelected)
        {
        	child.iFlags -= TFileDirEntry::EIsSelected;
            //mark the element that it was selected
        	child.iFlags |= TFileDirEntry::EWasSelected;
        };
        if(!usePartialCheck && (child.iFlags & TFileDirEntry::EIsPartiallySelected))
        {
        	child.iFlags -= TFileDirEntry::EIsPartiallySelected;
            //mark the element that it was selected
        	child.iFlags |= TFileDirEntry::EWasSelected;
        };
    };
    //select the selected indexes
    for(i=0;i<nr_selectionIndexes;i++)
    {
    	TFileDirEntry &child=(*aPool->iCurrentParent->iChildren)[(*selectionIndexes)[i]];
    	
    	child.iFlags |= TFileDirEntry::EIsSelected;
        if(! (child.iFlags & TFileDirEntry::EWasSelected))
        {
            //the child was not selected before. We need to select all its children, if it has any
        	child.SetSelection4Subtree(ETrue);
        };
        //clear the partially selected mark, if it exists
        child.iFlags &= ~TFileDirEntry::EIsPartiallySelected;
    };
    //unselect all the grandchildren for the children that were selected and now are not selected any more
    for(i=0;i<nr_children;i++)
    {
    	TFileDirEntry &child=(*aPool->iCurrentParent->iChildren)[i];
    	if((child.iFlags & TFileDirEntry::EWasSelected) &&
    			(!(child.iFlags & TFileDirEntry::EIsSelected)) )
    	{
    		//clear the EWasSelected mark
    		child.iFlags -= TFileDirEntry::EWasSelected;
    		//unselect the child and its children
    		child.SetSelection4Subtree(EFalse);
    	};
    };
    //mark the parent and the ancestors that some/all/none of the children are selected
    aPool->SetSelection(aPool->iCurrentParent,nr_selectionIndexes);
    LOG(ELogGeneral,-1,"CFFListContainer::StoreSelection--");
}


void CFFListContainer::HandleListBoxEventL (CEikListBox *aListBox, TListBoxEvent aEventType)
{
    if(aListBox==iFileList)
    {
    	switch(aEventType)
    	{
    	case EEventEnterKeyPressed:
    	case EEventItemDoubleClicked:
    	{
    		TBool moveMode=iListView->iMovingFiles.Count();

    		if(!moveMode)
    		{
    			TInt index=iFileList->CurrentItemIndex();
    			if(index>=0)
    			{
    				CListBoxView *view=iFileList->View();
    				view->ToggleItemL(index);
    				view->DrawItem(index);
    				    
#ifdef __MSK_ENABLED__
    				iListView->SetMSKSelectUnselect(view->ItemIsSelected(index));
#endif
    			};
    		};

    		if(moveMode)
    		{
    			//we are in move mode, we can not select, but we should open a context menu
    			CEikMenuBar *menu=iListView->MenuBar();
    			if(menu)
    			{
    				//here we would change the menu ID with SetMenuTitleResourceId(), but in this case it is the same
    				menu->TryDisplayMenuBarL();
    				//here we would change back the menu ID (same function)
    			};
    		}
    		    		
    	};break;
    	default:break; //to avoid a warning
    	};//switch
    }		
}

void CFFListContainer::HandleControlEventL(CCoeControl *aControl, TCoeEvent aEventType)
{
	if(aControl==iFileList && aEventType==EEventStateChanged)
	{
		iListView->CurrentItemChanged(iFileList->CurrentItemIndex());
	}
}

void CFFListContainer::UpdateProgressBar()
{
	DrawDeferred();
}


// End of File
