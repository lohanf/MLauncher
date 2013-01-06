/*
 ============================================================================
 Name		: EqualizerDlg.cpp
 Author	  : Florin Lohan
 Version	 : 1.0
 Copyright   : 
 Description : CEqualizerDlg implementation
 ============================================================================
 */
#include <aknlists.h> 

#include <akniconarray.h> //loading icons
#include <gulicon.h> //loading icons
#include <aknsutils.h> //loading icons
#include <avkon.mbg> //loading icons
#include <eikclbd.h> //loading icons

#include <MLauncher.rsg>
#include "EqualizerDlg.h"
#include "MusicPlayerView.h"
#include "MLauncher.hrh"
#include "log.h"


CEqualizerDlg::~CEqualizerDlg()
{
	delete iElementsArray;
}

TInt CEqualizerDlg::RunDlgLD(CDesCArrayFlat *aElementsArray, TInt aNrPresets, TInt aSelectedPreset, CMusicPlayerView *aView)
{
	CEqualizerDlg* dlg = new CEqualizerDlg();
	if(!dlg)
	{
		delete aElementsArray;
		User::Leave(KErrNoMemory);
	}
	dlg->iElementsArray=aElementsArray;
	dlg->iNrPresets=aNrPresets;
	dlg->iSelectedPreset=aSelectedPreset;
	dlg->iView=aView;
	return dlg->ExecuteLD(R_EQUALIZER_EFFECTS_DLG);
}

void CEqualizerDlg::PreLayoutDynInitL()
{
	LOG(ELogGeneral,1,"CEqualizerDlg::PreLayoutDynInitL++ (%x)",this);
	iEEList=static_cast<CAknSingleGraphicStyleListBox*>(Control(EEqualizerSelectionListControl));
	
	//iEEList->CreateScrollBarFrameL(ETrue);
	//iEEList->ScrollBarFrame()->SetScrollBarVisibilityL(CEikScrollBarFrame::EOff,CEikScrollBarFrame::EAuto);
	iEEList->Model()->SetItemTextArray(iElementsArray);
	iEEList->Model()->SetOwnershipType(ELbmDoesNotOwnItemArray); //because we will delete it by ourselves
		
	//adding icons
	CArrayPtr<CGulIcon>* iconList=new(ELeave)CAknIconArray(4);
	CleanupStack::PushL(iconList);

	TFileName avkonIconFile( AknIconUtils::AvkonIconFileName() );
	MAknsSkinInstance* skinInstance=AknsUtils::SkinInstance();
	CFbsBitmap* newIconBmp = NULL;
	CFbsBitmap* newIconBmpMask = NULL;
	CGulIcon* newIcon = NULL;

	//radio icons
	AknsUtils::CreateIconLC(skinInstance,KAknsIIDQgnPropRadiobuttOff, 
			newIconBmp,newIconBmpMask,avkonIconFile,
			EMbmAvkonQgn_prop_radiobutt_off,
			EMbmAvkonQgn_prop_radiobutt_off_mask);
	newIcon = CGulIcon::NewL(newIconBmp,newIconBmpMask);
	CleanupStack::Pop(2,newIconBmp);//newIconBmp & newIconBmpMask
	CleanupStack::PushL(newIcon);
	iconList->AppendL(newIcon);
	CleanupStack::Pop(newIcon);

	AknsUtils::CreateIconLC(skinInstance,KAknsIIDQgnPropRadiobuttOn, 
			newIconBmp,newIconBmpMask,avkonIconFile,
			EMbmAvkonQgn_prop_radiobutt_on,
			EMbmAvkonQgn_prop_radiobutt_on_mask);
	newIcon = CGulIcon::NewL(newIconBmp,newIconBmpMask);
	CleanupStack::Pop(2,newIconBmp);//newIconBmp & newIconBmpMask
	CleanupStack::PushL(newIcon);
	iconList->AppendL(newIcon);
	CleanupStack::Pop(newIcon);
	
	//checkbox icons
	AknsUtils::CreateIconLC(skinInstance,KAknsIIDQgnPropCheckboxOff, 
			newIconBmp,newIconBmpMask,avkonIconFile,
			EMbmAvkonQgn_prop_checkbox_off,
			EMbmAvkonQgn_prop_checkbox_off_mask);
	newIcon = CGulIcon::NewL(newIconBmp,newIconBmpMask);
	CleanupStack::Pop(2,newIconBmp);//newIconBmp & newIconBmpMask
	CleanupStack::PushL(newIcon);
	iconList->AppendL(newIcon);
	CleanupStack::Pop(newIcon);

	AknsUtils::CreateIconLC(skinInstance,KAknsIIDQgnPropCheckboxOn, 
			newIconBmp,newIconBmpMask,avkonIconFile,
			EMbmAvkonQgn_prop_checkbox_on,
			EMbmAvkonQgn_prop_checkbox_on_mask);
	newIcon = CGulIcon::NewL(newIconBmp,newIconBmpMask);
	CleanupStack::Pop(2,newIconBmp);//newIconBmp & newIconBmpMask
	CleanupStack::PushL(newIcon);
	iconList->AppendL(newIcon);
	CleanupStack::Pop(newIcon);
	
	//empty icon
	AknsUtils::CreateIconLC(skinInstance,KAknsIIDQgnPropEmpty, 
			newIconBmp,newIconBmpMask,avkonIconFile,
			EMbmAvkonQgn_prop_empty,
			EMbmAvkonQgn_prop_empty_mask);
	newIcon = CGulIcon::NewL(newIconBmp,newIconBmpMask);
	CleanupStack::Pop(2,newIconBmp);//newIconBmp & newIconBmpMask
	CleanupStack::PushL(newIcon);
	iconList->AppendL(newIcon);
	CleanupStack::Pop(newIcon);
	
	iEEList->ItemDrawer()->ColumnData()->SetIconArray(iconList);
	CleanupStack::Pop(iconList);
	
	//underline effects and presets
	CColumnListBoxItemDrawer *cdr=iEEList->ItemDrawer();
	TListItemProperties props=cdr->Properties(0);
	props.SetUnderlined(ETrue);
	cdr->SetPropertiesL(0,props);
	
	cdr=iEEList->ItemDrawer();
	props=cdr->Properties(iNrPresets+1);
	props.SetUnderlined(ETrue);
	cdr->SetPropertiesL(iNrPresets+1,props);
			
	iEEList->MinimumSize();
	iEEList->SetListBoxObserver(this);
	
	//enable scroll bars
	iEEList->CreateScrollBarFrameL(ETrue);
	iEEList->ScrollBarFrame()->SetScrollBarVisibilityL(CEikScrollBarFrame::EOff,CEikScrollBarFrame::EAuto);
	
	LOG(ELogGeneral,-1,"CEqualizerDlg::PreLayoutDynInitL--");
}

void CEqualizerDlg::ListItemClicked()
{
	FL_ASSERT(iEEList);
	TInt index=iEEList->CurrentItemIndex();
	LOG(ELogGeneral,1,"CEqualizerDlg::ListItemClicked++ (index=%d)",index);
	CTextListBoxModel *model=((CAknSingleGraphicPopupMenuStyleListBox*)iEEList)->Model();//we do not take ownership
	CDesCArray* list = STATIC_CAST(CDesCArray*, model->ItemTextArray());
	if(index<0 || index>=list->Count())return;

	TBuf<64> txt( (*list)[index]);
	if(txt[0]=='4' || txt[0]=='1')return; //not interesting
	TBool updateSelected(EFalse);

	//switch the picture
	switch(txt[0])
	{
	case '0':txt[0]='1';updateSelected=ETrue;iView->EqualizerChanged(index);break;
	case '2':txt[0]='3';iView->EqualizerChanged(index);break;
	case '3':txt[0]='2';iView->EqualizerChanged(index);break;
	}

	//update
	list->Delete(index);
	list->InsertL(index,txt);
	if(updateSelected)
	{
		txt.Copy((*list)[iSelectedPreset]);
		txt[0]='0';
		list->Delete(iSelectedPreset);
		list->InsertL(iSelectedPreset,txt);
		iSelectedPreset=index;
	}
	iEEList->HandleItemAdditionL();
	LOG(ELogGeneral,-1,"CEqualizerDlg::ListItemClicked--");
}

void CEqualizerDlg::HandleListBoxEventL(CEikListBox *aListBox, TListBoxEvent aEventType)
{
	if(aEventType!=EEventEnterKeyPressed && aEventType!=EEventItemDoubleClicked)return; //we are not interested in other events
	FL_ASSERT(iEEList==aListBox);
	ListItemClicked();
}

/*
TBool CEqualizerDlg::OkToExitL(TInt aButtonId)
{
    TBool exit=CAknDialog::OkToExitL(aButtonId);
    if(aButtonId==EAknSoftkeyOk || aButtonId==EAknSoftkeySelect)
    {
    	ListItemClicked();
        exit=EFalse;
    };
    return exit;
}*/

TKeyResponse CEqualizerDlg::OfferKeyEventL( const TKeyEvent& aKeyEvent, TEventCode aType )
{
	if(!iEEList)return EKeyWasNotConsumed;
	//we do not handle other events than key presses
	return iEEList->OfferKeyEventL(aKeyEvent,aType);
}
