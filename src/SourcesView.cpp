/*
 ============================================================================
 Name		: SourcesView.cpp
 Author	  : Florin Lohan
 Version	 : 1.0
 Copyright   : Florin Lohan
 Description : CSourcesView implementation
 ============================================================================
 */
#include <stringloader.h>
#include <akncommondialogs.h> 
//#include <caknfileselectiondialog.h>
//#include <caknmemoryselectiondialog.h>
//#include <pathinfo.h> 
#include "MLauncherAppUi.h"
#include "MLauncherDocument.h"
#include "TrackInfo.h"
#include "SourcesView.h"
#include "SourcesContainer.h"

#include "FFListView.h"
#include "MLauncherPreferences.h"
#include "MLauncher.hrh"
#include <MLauncher.rsg>
//#include <avkon.rsg>
#include "MLauncher.pan" 



CSourcesView::CSourcesView()
{
	// No implementation required
}

CSourcesView::~CSourcesView()
{
	if ( iSourcesContainer )
	{
		DoDeactivate();
	};
#ifdef __MSK_ENABLED__
    delete iMskUse;
    delete iMskIgnore;
#endif
}

CSourcesView* CSourcesView::NewLC()
{
	CSourcesView* self = new (ELeave) CSourcesView();
	CleanupStack::PushL(self);
	self->ConstructL();
	return self;
}

CSourcesView* CSourcesView::NewL()
{
	CSourcesView* self = CSourcesView::NewLC();
	CleanupStack::Pop(); // self;
	return self;
}

void CSourcesView::ConstructL()
{
	BaseConstructL(R_SOURCES_VIEW);
	iPreferences=((CMLauncherDocument*)MyAppUi()->Document())->iPreferences;
}

// -----------------------------------------------------------------------------
//  Called by the framework when the application status pane
//  size is changed.  Passes the new client rectangle to the
//  AppView
// -----------------------------------------------------------------------------
//
void CSourcesView::HandleStatusPaneSizeChange()
{
    if(iSourcesContainer)
    	iSourcesContainer->SetRect( ClientRect() );
}

void CSourcesView::DoDeactivate()
{
    if(iSourcesContainer)
    {
        AppUi()->RemoveFromStack(iSourcesContainer);
        delete iSourcesContainer;
        iSourcesContainer = NULL;
    }
}

void CSourcesView::DoActivateL(const TVwsViewId& /*aPrevViewId*/,TUid /*aCustomMessageId*/, const TDesC8& /*aCustomMessage*/)
{
    if(!iSourcesContainer)
    {
    	iSourcesContainer = CSourcesContainer::NewL( ClientRect(), this);
    };
    iSourcesContainer->SetMopParent(this);
    AppUi()->AddToStackL( *this, iSourcesContainer );
    iChangesMade=EFalse;
}

TUid CSourcesView::Id() const
{
    return TUid::Uid(KSourcesViewId); //defined in hrh
}

TUint *CSourcesView::CurrentElementsTypeAndNrElements()
{
	//get the current index
	TInt index=iSourcesContainer->iSourcesList->CurrentItemIndex();
	if(index<0)return NULL;
	
	
	if(index<iPreferences->iStartingDirs.Count())
	{
		//we have a source
		return &iPreferences->iStartingDirsNrFiles[index];
	}
	else if(index-iPreferences->iStartingDirs.Count()<iPreferences->iBlacklistDirs.Count())
	{
		//we have a blacklisted source
		index-=iPreferences->iStartingDirs.Count();
		return &iPreferences->iBlacklistDirsNrFiles[index];
	}
	return NULL; //this is just to avoid a warning
}

void CSourcesView::DynInitMenuPaneL(TInt aResourceId, CEikMenuPane *aMenuPane)
{
    if(aResourceId!=R_MENU_SOURCES || !aMenuPane)
    	return;
    
    TUint *type=CurrentElementsTypeAndNrElements();
    if(type)
    {
    	if((*type)&CMLauncherPreferences::EFlagSourceIsAudiobook)
    	{
    		//current element is an audiobook
    		aMenuPane->SetItemDimmed(ECommandSourceToAudioBook,ETrue);
    		aMenuPane->SetItemDimmed(ECommandSourceToMusic,EFalse);
    	}
    	else
    	{
    		aMenuPane->SetItemDimmed(ECommandSourceToAudioBook,EFalse);
    		aMenuPane->SetItemDimmed(ECommandSourceToMusic,ETrue);
    	}
    	//check for zero elements
    	if((*type)&0xFFFFFF)
    		aMenuPane->SetItemDimmed(ECommandDeleteSource,ETrue); //the item has non-zero nr of elements, we cannot delete it
    	else
    		aMenuPane->SetItemDimmed(ECommandDeleteSource,EFalse); //element has zero elements, can be deleted
    }
    else
    {
    	//there is no current element, dimm both 
    	aMenuPane->SetItemDimmed(ECommandSourceToAudioBook,ETrue);
    	aMenuPane->SetItemDimmed(ECommandSourceToMusic,ETrue);
    }
}

void CSourcesView::HandleCommandL( TInt aCommand )
{
    //try first to send it to the AppUi for handling
    if(MyAppUi()->AppUiHandleCommandL(aCommand,this))
        return; //AppUiHandleCommandL returned TRUE, it handled the command

    LOG(ELogGeneral,1,"CSourcesView::HandleCommandL++ (command: %d)",aCommand);
    //AppUiHandleCommandL did not handle the command
    switch(aCommand)
    {
    case EAknSoftkeyBack:
    {
    	iSourcesContainer->StoreSelection();
    	if(iChangesMade)
    	{
    		LOG0("CSourcesView::HandleCommandL back: changes were made.");
    		iPreferences->SavePreferences();
    		MyAppUi()->MyDoc().SourcesChanged();
    	}
    	MyAppUi()->SourcesViewDoneL();
    };break;
    case ECommandSourceToAudioBook:
    {
    	//set this source type to audiobook
    	TUint *type=CurrentElementsTypeAndNrElements();
    	__ASSERT_ALWAYS(type,Panic(EMLauncherUi));

    	(*type)|=CMLauncherPreferences::EFlagSourceIsAudiobook;
    	iChangesMade=ETrue;
    	iSourcesContainer->AddListElementsL(ETrue);    	
    };break;
    case ECommandSourceToMusic:
    {
    	//set this source type to music
    	TUint *type=CurrentElementsTypeAndNrElements();
    	__ASSERT_ALWAYS(type,Panic(EMLauncherUi));

    	(*type)&=~CMLauncherPreferences::EFlagSourceIsAudiobook;
    	iChangesMade=ETrue;
    	iSourcesContainer->AddListElementsL(ETrue); 
    };break;
    case ECommandDeleteSource:
    {
    	//get the current index
    	TInt index=iSourcesContainer->iSourcesList->CurrentItemIndex();
    	if(index>=0)
    	{
    		if(index<iPreferences->iStartingDirs.Count())
    		{
    			//we have to delete a source
    			//MyAppUi()->MyDoc().iFilesTree->MarkSourceAsDeleted(*iPreferences->iStartingDirs[index]); not needed, because only sources with zero files can be deleted
    			delete iPreferences->iStartingDirs[index];
    			iPreferences->iStartingDirs.Remove(index);
    			iPreferences->iStartingDirsNrFiles.Remove(index);
    			iChangesMade=ETrue;
    			iSourcesContainer->AddListElementsL(ETrue);
    		}
    		else if(index-iPreferences->iStartingDirs.Count()<iPreferences->iBlacklistDirs.Count())
    		{
    			//we have to delete a blacklisted source
    			index-=iPreferences->iStartingDirs.Count();
    			delete iPreferences->iBlacklistDirs[index];
    			iPreferences->iBlacklistDirs.Remove(index);
    			iPreferences->iBlacklistDirsNrFiles.Remove(index);
    			iChangesMade=ETrue;
    			iSourcesContainer->AddListElementsL(ETrue);
    		}
    	}
    };break;
    case ECommandDeleteAllSources:
    {
    	if(MyAppUi()->DisplayQueryOrNoteL(EMsgQueryYesNo,R_QUERY_DELETEALLSOURCES))
    	{
    		iPreferences->iStartingDirs.ResetAndDestroy();
    		iPreferences->iStartingDirsNrFiles.Reset();
    		iPreferences->iBlacklistDirs.ResetAndDestroy();
    		iPreferences->iBlacklistDirsNrFiles.Reset();
    		iPreferences->SavePreferences();
    		iSourcesContainer->AddListElementsL(ETrue);
    		AppUi()->Exit();
    	};
    };break;
    case ECommandAddSource:
    {
    	if(MyAppUi()->DisplayQueryOrNoteL(EMsgQueryYesNo,R_QUERY_ADD_SOURCE_WARNING))
    	{
    		TFileName path;
    		AknCommonDialogs::RunCopyDlgLD(path,R_SOURCE_FOLDER_MEMORY_SELECT_DIALOG,R_SOURCE_FOLDER_FILE_SELECT_DIALOG,this);
    		LOG0("Adding a source: %S",&path);
    		
    		//check the path against sources and blacklisted
    		TInt i;
    		TBool found(EFalse);
    		iPreferences->RemoveEndingBackslash(path); //sources do not have trailing backslashes

    		for(i=0;i<iPreferences->iStartingDirs.Count();i++)
    			if(path== *iPreferences->iStartingDirs[i])
    			{
    				found=ETrue;
    				break;
    			}
    		if(!found)
    			for(i=0;i<iPreferences->iBlacklistDirs.Count();i++)
    				if(path== *iPreferences->iBlacklistDirs[i])
    				{
    					found=ETrue;
    					break;
    				}
    		if(found)MyAppUi()->DisplayQueryOrNoteL(EMsgWarningPersistent,R_ADD_SOURCE_ALREADY_EXISTS);
    		else
    		{
    			LOG0("Adding the source ...");
    			//add the source 
    			HBufC *src=HBufC::NewLC(path.Length());
    			src->Des().Copy(path);
    			iPreferences->iStartingDirs.AppendL(src);
    			iPreferences->iStartingDirsNrFiles.AppendL(0xFFFFFF);
    			CleanupStack::Pop(src);
    			iChangesMade=ETrue;
    			iSourcesContainer->AddListElementsL(ETrue);
    			LOG0("Done adding the source!");
    		}
    	}
    	
    	//AknCommonDialogs::RunSelectDlgLD(path,R_SOURCE_FOLDER_SELECT_DIALOG,this,this);
    	//CAknMemorySelectionDialog dlg=CAknMemorySelectionDialog::NewL(ECFDDialogTypeCopy,R_SOURCE_FOLDER_SELECT_DIALOG,EFalse);
    	/*
    	CAknFileSelectionDialog* dlg=CAknFileSelectionDialog::NewL(ECFDDialogTypeCopy);
    	dlg->SetTitleL(_L("Select folder"));
    	//dlg->AddFilterL(this);
    	dlg->SetObserver(this);
    	path = PathInfo::MemoryCardRootPath();
    	dlg->ExecuteL(path);*/
    	
    	
    	
    };break;
    default:
        Panic( EMLauncherUi );
    };
    LOG(ELogGeneral,-1,"CSourcesView::HandleCommandL--");
}

#ifdef __MSK_ENABLED__
void CSourcesView::SetMSKUseIgnore(TBool aUsed, TBool aHasSelection/*=ETrue*/)
{
	if(!iMskUse)
	{
		iMskUse=StringLoader::LoadL(R_MSK_USE);
		iMskIgnore=StringLoader::LoadL(R_MSK_IGNORE);
	};
	if(aHasSelection)
	{
		if(aUsed)
			Cba()->SetCommandL(EAknSoftkeyForwardKeyEvent,*iMskIgnore);
		else
			Cba()->SetCommandL(EAknSoftkeyForwardKeyEvent,*iMskUse);
	}
	else
		Cba()->SetCommandL(EAknSoftkeyForwardKeyEvent,_L(""));

	Cba()->DrawDeferred();
}
#endif


TBool CSourcesView::OkToExitL(const TDesC &aDriveAndPath, const TEntry &aEntry)
{
	return ETrue; //for the moment
}
	
	
	
