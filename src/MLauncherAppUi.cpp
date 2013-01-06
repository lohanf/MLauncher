/*
============================================================================
 Name        : MLauncherAppUi.cpp
 Author      : Florin Lohan
 Copyright   : Florin Lohan
 Description : CMLauncherAppUi implementation
============================================================================
*/
#include <eikmenub.h>
#include <eikapp.h>
#include <aknnotewrappers.h>
#include <aknmessagequerydialog.h>
#include <stringloader.h>
#include <DocumentHandler.h>
#include <apgcli.h>
#include <utf.h>

#include "MLauncherAppUi.h"
#include "Observers.h"
#include "MLauncherPreferences.h"
#include "FFListView.h"
#include "FFListContainer.h"
#include "PlaylistsView.h"
#include "FiletransferView.h"
#include "FiletransferContainer.h"
#include "StopwatchView.h"
#include "MusicPlayerView.h"
#include "SourcesView.h"
#include "SourcesContainer.h"
#include "MyFilesView.h"
#include "MLauncher.hrh"
#include <MLauncher.rsg>
#include "MLauncher.pan"
#include "XMLparsing.h"
#include "log.h"
#include "defs.h"
#include "common_cpp_defs.h"

const TInt KItemsUpdateView=50;

_LIT(KXSel," [X]");
//const TInt KMinSongs=50; //minimum number of songs that should be in a folder so that we stop counting
// not needed _LIT(KOggFlacPluginName,":\\sys\\bin\\oggplaycontroller.dll")

void CMLauncherAppUi::ConstructL()
{
    // Initialise app UI with standard value.
	LOG(ELogGeneral,1,"CMLauncherAppUi::ConstructL++");
    TInt flags(EAknEnableSkin);
#ifdef _FLL_SDK_VERSION_50_
    flags|=EAknTouchCompatible;
#endif
    
#ifdef __MSK_ENABLED__
    flags|=EAknEnableMSK;
#endif
    BaseConstructL(flags);
    
    iCurrentView=iListView=CFFListView::NewLC(/*iDocument*/);
    
    iNoteId=-1;
    /* This moved to a "job", so we can discover sources at the same time the user reads the message
    if(MyDoc().iPreferences->iCFlags&CMLauncherPreferences::ENoPreferencesFileFound)
    {
#ifdef __TOUCH_ENABLED__
    	if(AknLayoutUtils::PenEnabled())
    		DisplayQueryOrNoteL(EMsgBigNote,R_FFLIST_HELP_TEXT_TOUCH,R_FFLIST_HELP_HEADER);//touch UI  
    	else
#endif
    		DisplayQueryOrNoteL(EMsgBigNote,R_FFLIST_HELP_TEXT_NONTOUCH,R_FFLIST_HELP_HEADER);//non-touch UI
    }
    else if(MyDoc().iPreferences->iStartingDirs.Count()==0)
    {
    	//there are preferences, but there is no source folder. We will try to discover them.
    		
    	//verific daca iPreferences->iSources.Count() e zero, si daca e pun o nota in care sa zic
    	//ca se cauta sursele. Schimb si nota de deasupra, ca sa se intample ceva in background
    	 
    			
    }*/
    LOG0("List vew constructed");
    AddViewL(iListView);//transfers ownership
    CleanupStack::Pop(iListView);

#ifndef __WINS__
    if(MyDoc().iCrashLogFound)
    	iListView->UploadCrashLogFileL();
#endif

    //check if we should open a playlist and play it
    if(MyDoc().iCurrentPlaylist && MyDoc().iPreferences->iPFlags&CMLauncherPreferences::EPreferencesUseInternalMusicPlayer)
    	CreateMusicPlayerViewL(MyDoc().iCurrentPlaylist,EFalse,EFalse);
   
    //no need to initialize the other views at this time
    SetDefaultViewL(*iListView);
    iActiveViews|=EFFListViewActive;
    
#ifndef __WINS__
    CCommReceiver::GetInstanceL()->ListenL(this);
#endif
    
    ScheduleWorkerL(EJobSubfolders | EJobStartDiscoveringSourcesAndParsingMetadata);
    
    LOG(ELogGeneral,-1,"CMLauncherAppUi::ConstructL--");
};

CMLauncherAppUi::CMLauncherAppUi() : iActiveViews(0), iDirection(EDirectionReceiving)
{
};

CMLauncherAppUi::~CMLauncherAppUi()
{
	LOG(ELogGeneral,1,"CMLauncherAppUi::~CMLauncherAppUi++");
	if(iIdle)
	{
		LOG0("Canceling iIdle");
		iIdle->Cancel();
		LOG0("Deleting iIdle");
		delete iIdle;
	};
	LOG0("iIdle is deleted. Deleting iDlg*");
	delete iDlgMsg;
	delete iDlgNote;
	delete iDlgNoteString;
	LOG0("Deleting iDocHandler (should be NULL already)");
	delete iDocHandler;
	LOG(ELogGeneral,-1,"CMLauncherAppUi::~CMLauncherAppUi--");
};

void CMLauncherAppUi::CreateStopwatchViewL(void)
{
    LOG(ELogGeneral,1,"AppUi::CreateStopwatchViewL: start");

    //first, display the form to the user
    //create & initialie the data
    TStopwatchData stopwatchData;
    //TODO: initialize from preferences

    // Launch the dialog to edit data
    CAknForm* dlg = CStopwatchForm::NewL(stopwatchData);
    if(dlg->ExecuteLD(R_STOPWATCH_FORM_DIALOG))
    {
        //user did not dismissed the dialog, create the view
        if(iStopwatchView==NULL)
        {
            //create the view and switch to it
            iStopwatchView=CStopwatchView::NewLC();
            iStopwatchView->iStopwatchData=stopwatchData;
            AddViewL(iStopwatchView);//transfers ownership
            CleanupStack::Pop(iStopwatchView);
        };
        iCurrentView=iStopwatchView;
        iActiveViews|=EStopwatchViewActive;
        //ChangeExitWithNaviL(KStopwatchViewId);
        ActivateLocalViewL(TUid::Uid(KStopwatchViewId));
    };

    LOG(ELogGeneral,-1,"AppUi::CreateStopwatchViewL: end");
};

void CMLauncherAppUi::CreateSourcesViewL()
{
	LOG(ELogGeneral,1,"AppUi::CreateSourcesViewL: start");

	if(iSourcesView==NULL)
	{
		//create the view and switch to it
		iSourcesView=CSourcesView::NewLC();
		AddViewL(iSourcesView);//transfers ownership
		CleanupStack::Pop(iSourcesView);
	};
	iCurrentView=iSourcesView;
	//iActiveViews|=ESourcesViewActive;//no need to set this view as active, because we can not switch away from it anyway
	ActivateLocalViewL(TUid::Uid(KSourcesViewId));
	LOG(ELogGeneral,-1,"AppUi::CreateSourcesViewL: end");
};

void CMLauncherAppUi::CreatePlaylistsViewL()
{
	LOG(ELogGeneral,1,"AppUi::CreatePlaylistsViewL: start");

	if(iPlaylistsView==NULL)
	{
		//create the view and switch to it
		iPlaylistsView=CPlaylistsView::NewLC();
		AddViewL(iPlaylistsView);//transfers ownership
		CleanupStack::Pop(iPlaylistsView);
	};
	iCurrentView=iPlaylistsView;
	iActiveViews|=EPlaylistsViewActive;
	ActivateLocalViewL(TUid::Uid(KPlaylistsViewId));
	LOG(ELogGeneral,-1,"AppUi::CreatePlaylistsViewL: end");
};

void CMLauncherAppUi::CreateMyFilesViewL()
{
	LOG(ELogGeneral,1,"AppUi::CreateMyFilesView: start");

	if(iMyFilesView==NULL)
	{
		//create the view and switch to it
		iMyFilesView=CMyFilesView::NewLC();
		AddViewL(iMyFilesView);//transfers ownership
		CleanupStack::Pop(iMyFilesView);
	};
	iCurrentView=iMyFilesView;
	iActiveViews|=EMyFilesViewActive;
	ActivateLocalViewL(TUid::Uid(KMyFilesViewId));
	LOG(ELogGeneral,-1,"AppUi::CreateMyFilesView: end");
}

void CMLauncherAppUi::UpdateSourcesView()
{
	if(!iSourcesView || !iSourcesView->iSourcesContainer)return;
	
	iSourcesView->iSourcesContainer->StoreSelection();
	iSourcesView->iSourcesContainer->AddListElementsL(ETrue);
}

void CMLauncherAppUi::CreateMusicPlayerViewL(CPlaylist *aPlaylist, TBool aIsPreview, TBool aStartPlaying)
{
	LOG(ELogGeneral,1,"AppUi::CreateMusicPlayerViewL: start");
	
	//check if we need to load this playlist
	TRAPD(err,aPlaylist->LoadPlaylistL(MyDoc()));
	if(err)
	{
		LOG(ELogGeneral,-1,"AppUi::CreateMusicPlayerViewL: end (error loading playlist: %d)",err);
		return;
	};

	if(iMusicPlayerView==NULL)
	{
		//create the view and switch to it
		iMusicPlayerView=CMusicPlayerView::NewLC();
		AddViewL(iMusicPlayerView);//transfers ownership
		CleanupStack::Pop(iMusicPlayerView);
	};
	iCurrentView=iMusicPlayerView;
	iActiveViews|=EMusicPlayerViewActive;
	ActivateLocalViewL(TUid::Uid(KMusicPlayerViewId));
	err=iMusicPlayerView->StartPlaylistL(aPlaylist,aIsPreview,aStartPlaying);
	if(err)
		CloseMusicPlayerViewL();
	LOG(ELogGeneral,-1,"AppUi::CreateMusicPlayerViewL: end");
};

void CMLauncherAppUi::CloseMusicPlayerViewL()
{
	LOG(ELogGeneral,1,"CloseMusicPlayerViewL: start");

	iActiveViews&=~EMusicPlayerViewActive;
	if(iMusicPlayerView)
		iMusicPlayerView->ClearPlaylist();
	if(iCurrentView==iMusicPlayerView)
	{
		//we need to switch to a different view, e.g. list view
		LOG0("Activating list view");
		SwitchViewL(iListView);
		LOG0("List view activated");
	}/*
	else
	{
		//no need to switch view, but we must take care of the navi menu (may need replacing with exit)
		ChangeExitWithNaviL(-1);
	};*/

	LOG(ELogGeneral,-1,"CloseMusicPlayerViewL: end");
};

void CMLauncherAppUi::NewTransferL(const TInt aNrFiles, const TInt aTotalSize, const RArray<TInt> &aFileSizes)
{
	LOG(ELogGeneral,1,"AppUi::NewTransferL: start");
	if(iFiletransferView==NULL)
	{
		//create the view and switch to it
		iFiletransferView=CFiletransferView::NewLC();
		AddViewL(iFiletransferView);//transfers ownership
	    CleanupStack::Pop(iFiletransferView);
	};
	iFiletransferView->NewTransferL(aNrFiles,aTotalSize,aFileSizes);
	iCurrentView=iFiletransferView;
	iActiveViews|=EFiletransferViewActive;
	//ChangeExitWithNaviL(KFiletransferViewId);
	ActivateLocalViewL(TUid::Uid(KFiletransferViewId));
	LOG(ELogGeneral,-1,"AppUi::NewTransferL: end");
};

void CMLauncherAppUi::TransferingNewFileL(const TDirection aDirection, const TDes& aFilename, const TInt aSize)
{
	LOG(ELogGeneral,1,"AppUi::TransferingNewFileL: start");
	iFiletransferView->NewFileL(aDirection,aFilename,aSize);
	//update the view
    if(iDirection==EDirectionReceiving)
    	iListView->UpdateEntriesL();
	LOG(ELogGeneral,-1,"AppUi::TransferingNewFileL: end");
};

void CMLauncherAppUi::MoreDataTransfered(const TInt aBytesSent) //total sent so far
{
	iFiletransferView->UpdateData(aBytesSent);
};

void CMLauncherAppUi::PlaylistTransferCompleteL()
{
	LOG(ELogGeneral,1,"PlaylistTransferCompleteL: start");

	iActiveViews&=~EFiletransferViewActive;
	if(iCurrentView==iFiletransferView)
	{
		//we need to switch to a diferent vierw, e.g. list view
		LOG0("Activating list view");
		SwitchViewL(iListView);
		LOG0("List view activated");
	}/*
	else
	{
		//no need to switch view, but we must take care of the navi menu (may need replacing with exit)
		ChangeExitWithNaviL(-1);
	};*/

	Clean(ETrue);//this function performs asynchronously

    //update the view
    if(iDirection==EDirectionReceiving)
    	UpdateView();

	LOG(ELogGeneral,-1,"PlaylistTransferCompleteL: end");
};

void CMLauncherAppUi::StopwatchDoneL()
{
    LOG(ELogGeneral,1,"StopwatchDone: start");

    iActiveViews&=~EStopwatchViewActive;
    if(iCurrentView==iStopwatchView)
    {
        //we need to switch to a diferent vierw, e.g. list view
        LOG0("Activating list view");
        SwitchViewL(iListView);
        LOG0("List view activated");
    }/*
    else
    {
        //no need to switch view, but we must take care of the navi menu (may need replacing with exit)
        ChangeExitWithNaviL(-1);
    };*/

    //do cleaning here

    LOG(ELogGeneral,-1,"StopwatchDone: end");
};

void CMLauncherAppUi::SourcesViewDoneL()
{
    LOG(ELogGeneral,1,"SourcesViewDone: start");
    //iActiveViews&=~ESourcesViewActive; //no need to do it, because we can not activate any other view from the sources view
    //we need to switch to list view
    iCurrentView=iListView;
    ActivateLocalViewL(iCurrentView->Id());

    LOG(ELogGeneral,-1,"SourcesViewDone: end");
};


void CMLauncherAppUi::UpdateView()
{
	if(iListView)
		iListView->UpdateEntriesL();
};

void CMLauncherAppUi::SetDirection(TDirection aDirection)
{
	iDirection=aDirection;
};

MMLauncherTransferNotifier::TDirection CMLauncherAppUi::Direction()
{
	return iDirection;
};

void CMLauncherAppUi::ShowErrorCleanCommL(TMsgType aErrorType, TInt aErrorStringResource, TInt aErr, TBool aClean, TBool aKeepListening)
{
	HBufC* noteText=StringLoader::LoadL(aErrorStringResource);
	ShowErrorCleanCommL(aErrorType,noteText,aErr,aClean,aKeepListening);//takes ownership of noteText
};

void CMLauncherAppUi::ShowErrorCleanCommL(TMsgType aErrorType, HBufC* aErrorString, TInt aErr, TBool aClean, TBool aKeepListening)
{
	//write the message into the LOG as well
	CleanupStack::PushL(aErrorString);
	LOG(ELogGeneral,1,"CMLauncherAppUi::ShowErrorCleanCommL: error type=%d, error=%d, error text: %S",aErrorType,aErr,aErrorString);
	//LOG(*);

	// if we are going to switch the view in this function, we would need to postpone showing the error
	// message until the new view is activated. Otherwise the activation will delete out error
	TBool showErrorAfterViewSwitch(EFalse);
	//first, we close the stuff!
	__ASSERT_ALWAYS((!(aErrorType==EMsgError || aErrorType==EMsgErrorPersistent) || aClean),Panic(EBTErrorAndNoClean));

	if(aClean)
	{
		if((iDirection==EDirectionReceiving && CCommReceiver::GetInstanceL()->iActiveRequest==CCommCommon::EActiveRequestTransferPlaylist) ||
		   (iDirection==EDirectionSending && CCommSender::GetInstanceL()->iActiveRequest==CCommCommon::EActiveRequestTransferPlaylist))
		{
			if(iCurrentView==iFiletransferView)
				showErrorAfterViewSwitch=ETrue; //the view will be changed!
			PlaylistTransferCompleteL();//this cleans the data as well (delete the socket)
		}
		else
		    Clean(aKeepListening);
	};

	LOG0("CMLauncherAppUi::ShowErrorCleanCommL: closing stuff ended, now we show the error message");
	if(aErrorType==EMsgNotePersistent || aErrorType==EMsgWarningPersistent || aErrorType==EMsgErrorPersistent)
	{
		//this is a different type of note
		CAknMessageQueryDialog* dlg = new (ELeave)CAknMessageQueryDialog;
		CleanupStack::PushL(dlg);

		dlg->SetMessageTextL(*aErrorString);
		HBufC* header(NULL);
		switch(aErrorType)
		{
		case EMsgNotePersistent:header=StringLoader::LoadLC(R_NOTE_HEADER);break;
		case EMsgWarningPersistent:header=StringLoader::LoadLC(R_WARNING_HEADER);break;
		case EMsgErrorPersistent:header=StringLoader::LoadLC(R_ERROR_HEADER);break;
		default:LOG0("CMLauncherAppUi::ShowErrorCleanCommL: switch(aErrorType)/persistent: %d not handled",aErrorType);Panic(ESwitchValueNotHandled);
		};

		dlg->SetHeaderTextL(*header);
		CleanupStack::PopAndDestroy(header);

		if(showErrorAfterViewSwitch && iListView)
		{
			iDlgMsg=dlg;
			iListView->iFlags|=CFFListView::EHasMessage2Display;
			LOG0("Show dialog postponed after view change");
		}
		else
		{
			iDlgMsg=dlg;
			DisplayMessageL();
		};
		CleanupStack::Pop(dlg);
		CleanupStack::PopAndDestroy(aErrorString);
	}
	else
	{
		CAknResourceNoteDialog *note(NULL);
		switch(aErrorType)
		{
		case EMsgError:note=new(ELeave)CAknErrorNote;break;
		case EMsgWarning:note=new(ELeave)CAknWarningNote;break;
		case EMsgNote:note=new(ELeave)CAknInformationNote;break;
		default:LOG0("CMLauncherAppUi::ShowErrorCleanCommL: switch(aErrorType)/note: %d not handled",aErrorType);Panic(ESwitchValueNotHandled);
		};
		if(showErrorAfterViewSwitch && iListView)
		{
			iDlgNote=note;
			iDlgNoteString=aErrorString;
			iListView->iFlags|=CFFListView::EHasMessage2Display;
			CleanupStack::Pop(aErrorString);
			LOG0("Show note postponed after view change");
		}
		else
		{
			iDlgNote=note;
			iDlgNoteString=aErrorString;
			CleanupStack::Pop(aErrorString);
			DisplayMessageL();
		};
	};

	LOG(ELogGeneral,-1,"CMLauncherAppUi::ShowErrorCleanCommL: end");
};

void CMLauncherAppUi::ScheduleWorkerL(TUint aJobs)
{	
	LOG(ELogGeneral,1,"CMLauncherAppUi::ScheduleWorkerL++ (jobs: %x)",aJobs);
	if(aJobs&EJobSubfolders)
	{
		iIi=-1;
		iFlags&=~EListViewNeedsUpdatingProperties;
	};
	iJobs|=aJobs;
	if(!iIdle)iIdle=CIdle::NewL(CActive::EPriorityIdle);
	if(!iIdle->IsActive() && !(iJobs&EDoingJobs))
	{
		TCallBack cb(&IdleWorkerStatic,this);
		iIdle->Start(cb);
	}
	else LOG0("Worker already scheduled.")
	
	LOG(ELogGeneral,-1,"CMLauncherAppUi::ScheduleWorkerL--");
}

void CMLauncherAppUi::DisplayMessageL()
{
	LOG(ELogGeneral,1,"CMLauncherAppUi::DisplayMessageL start");
	__ASSERT_ALWAYS(iDlgMsg || (iDlgNote && iDlgNoteString),Panic(EBTNoError2Display));

	if(!(iJobs&EJobDisplayMessage))
	{
		ScheduleWorkerL(EJobDisplayMessage);
	};
	LOG(ELogGeneral,-1,"CMLauncherAppUi::DisplayMessageL end");
};

TBool CMLauncherAppUi::DisplayQueryOrNoteL(TMsgType aMsgType,TInt aTextResource,TInt aHeaderResource/*=-1*/,TInt aDlgResource/*=-1*/)
{
	switch(aMsgType)
	{
	case EMsgNotePersistent:aMsgType=EMsgBigNote;aHeaderResource=R_NOTE_HEADER;break;
	case EMsgWarningPersistent:aMsgType=EMsgBigNote;aHeaderResource=R_WARNING_HEADER;break;
	case EMsgErrorPersistent:aMsgType=EMsgBigNote;aHeaderResource=R_ERROR_HEADER;break;
	default:break; //to avoid a warning
	};
	
	switch(aMsgType)
	{
	case EMsgQueryYesNo:
	{
		CAknQueryDialog *dlgQuery=CAknQueryDialog::NewL();
		CleanupStack::PushL(dlgQuery);
		HBufC* queryPrompt=StringLoader::LoadLC(aTextResource);
		dlgQuery->SetPromptL(*queryPrompt);
		CleanupStack::PopAndDestroy(queryPrompt);
		CleanupStack::Pop(dlgQuery);
		return dlgQuery->ExecuteLD(R_QUERY_CONFIRMATION);
	};break;
	case EMsgBigNote:
	{
		HBufC* text=StringLoader::LoadLC(aTextResource);
		HBufC* header=StringLoader::LoadLC(aHeaderResource);
		CAknMessageQueryDialog* dlg = new (ELeave)CAknMessageQueryDialog(text,header);//takes ownership of text and header
		CleanupStack::Pop(2,text);
		if(aDlgResource<=0)aDlgResource=R_MSG_BIG_NOTE;
		dlg->ExecuteLD(aDlgResource);
	};break;
	
	default:
		Panic(EUnrecognizedMsgType);
	};//switch
	return EFalse; //so that we do not get a warning
}

void CMLauncherAppUi::Clean(TBool aKeepListening)
{
	LOG(ELogGeneral,1,"CMLauncherAppUi::Clean: start (aKeepListening=%d)",aKeepListening);
	//delete the BT classes
	if(aKeepListening)
		iFlags|=EDeleteBtKeepListening;
	else 
		iFlags&=~EDeleteBtKeepListening;
	if(!(iJobs&EJobClean))
	{
	    ScheduleWorkerL(EJobClean);
	};
    LOG(ELogGeneral,-1,"CMLauncherAppUi::Clean: end");
};

TInt CMLauncherAppUi::IdleWorker()
{
	//LOG(ELogGeneral,1,"CMLauncherAppUi::IdleWorker: start");
	TInt otherJobs(EFalse);
	//TInt err(0);
	
	if(iMusicPlayerView && iCurrentView==iMusicPlayerView && iMusicPlayerView->IsImgProcessingOngoing())
	{
		iMusicPlayerView->iFlags|=CMusicPlayerView::EReactivateCIdleAfterImgProcessingEnds;
		iIdle->Cancel();
		LOG0("CMLauncherAppUi::IdleWorker: end (cancelled because ImgEngine is active!)");
		return 0; //no more work to do
	}
	iJobs|=EDoingJobs; //this is so that we do not re-schedule the CIdle AO during some job
	
	if(iJobs&EJobDisplayMessage)
	{
		LOG0("CMLauncherAppUi::IdleWorker: Displaying msg ...");
		//we have to display a message
		if(iDlgMsg)
		{
			CAknMessageQueryDialog *dlgMsg=iDlgMsg;
			iDlgMsg=NULL;
			dlgMsg->ExecuteLD(R_MESSAGE_DLG_OK_EMPTY);
		};

		if(iDlgNote)
		{
			CAknResourceNoteDialog *dlgNote=iDlgNote;
			iDlgNote=NULL;
			dlgNote->ExecuteLD(*iDlgNoteString);
			delete iDlgNoteString;
			iDlgNoteString=NULL;
		};
		iJobs&=~EJobDisplayMessage;
		LOG0("Displaying msg done!");
	}
	else if(iJobs&EJobClean)
	{
		LOG0("CMLauncherAppUi::IdleWorker: Cleaning ...");
		//we have to clean the Comm stuff
		CCommSender::DeleteInstance();
		if(iFlags&EDeleteBtKeepListening)
			CCommReceiver::GetInstanceL()->Clean(ETrue);
		else
			CCommReceiver::DeleteInstance();
		iJobs&=~EJobClean;
		LOG0("Cleaning done!");
	}
	else if(iMusicPlayerView && iCurrentView==iMusicPlayerView && iMusicPlayerView->iJobs)
		otherJobs|=iMusicPlayerView->IdleWorker();
	else if(iListView && iCurrentView==iListView && iListView->iJobs)
		otherJobs|=iListView->IdleWorker();
	else if(iJobs & EJobSubfolders)
	{
		//LOG0("CMLauncherAppUi::IdleWorker:  subfolders"); 
		__ASSERT_ALWAYS(MyDoc().iFilesTree->iCurrentParent,Panic(ECurrentParentIsNull));
		TBool update(EFalse);
		if(iIi==-1)
			iIi=0; //we are just started
		if(iIi<MyDoc().iFilesTree->iCurrentParent->ChildrenCount())
		{
			if(MyDoc().iFilesTree->MakeChildren(&(*MyDoc().iFilesTree->iCurrentParent->iChildren)[iIi]))
			{
				iFlags|=EListViewNeedsUpdatingProperties;
				//LOG0("Subfolder job: created some children"); 
			}
			iIi++;
			//update the view from time to time
			if((iFlags&EListViewNeedsUpdatingProperties) && !(iIi % KItemsUpdateView))
				update=ETrue;
		}
		else
		{
			//we are finishing
			iIi=-1;
			iJobs-=EJobSubfolders;
			if(iFlags&EListViewNeedsUpdatingProperties)
			{
				iFlags&=~EListViewNeedsUpdatingProperties;
				update=ETrue;
			};
		};
		if(update && iListView)
			iListView->CurrentListChanged(CFFListView::EProperties);
	}
	else if(iJobs & EJobStartDiscoveringSourcesAndParsingMetadata)
	{
		if(MyDoc().iPreferences->iCFlags&CMLauncherPreferences::ENoPreferencesFileFound)
		{
			HBufC* text(NULL);
#ifdef __TOUCH_ENABLED__
			if(AknLayoutUtils::PenEnabled())
				text=StringLoader::LoadLC(R_FFLIST_HELP_TEXT_TOUCH);
			else
#endif
				text=StringLoader::LoadLC(R_FFLIST_HELP_TEXT_NONTOUCH);

			HBufC* header=StringLoader::LoadLC(R_FFLIST_HELP_HEADER);
			CAknMessageQueryDialog* dlg = new (ELeave)CAknMessageQueryDialog(text,header);//takes ownership of text and header
			CleanupStack::Pop(2,text);
			dlg->ExecuteLD(R_MSG_BIG_NOTE_NOWAIT);			
		}
		else if(MyDoc().iPreferences->iStartingDirs.Count()==0)
		{
			//there are preferences, but there is no source folder. We will try to discover them.

			//verific daca iPreferences->iSources.Count() e zero, si daca e pun o nota in care sa zic
			//ca se cauta sursele. Schimb si nota de deasupra, ca sa se intample ceva in background


		}
		//set the empty text to "searching for music", or something like that..
		MyDoc().iPreferences->iCFlags|=CMLauncherPreferences::EMusicSearchInProgress;
		if(iListView && iListView->iListContainer)
			iListView->iListContainer->SetListEmptyTextL();
		iJobs-=EJobStartDiscoveringSourcesAndParsingMetadata;
		iJobs|=EJobDiscoverSourcesAndParseMetadata;
		LOG0("EJobStartDiscoveringSourcesAndParsingMetadata->EJobDiscoverSourcesAndParseMetadata")
	}
	else if(iJobs & EJobDiscoverSourcesAndParseMetadata)
	{
		//LOG0("Current job: discover sources and parse metadata"); 
		TInt ret=MyDoc().DiscoverNewSources();
		if(ret)
		{
			//we are done 
			//set the empty text to "no music files", or something like that..
			MyDoc().iPreferences->iCFlags&=~CMLauncherPreferences::EMusicSearchInProgress;
			if(iListView && iListView->iListContainer)
				iListView->iListContainer->SetListEmptyTextL();		
			if(ret==2)
			{
				LOG0("CMLauncherAppUi::IdleWorker: Discovering new sources done, changes found!");
				//update the sources view
				UpdateSourcesView();
				//update the iListView
				if(iListView)
				{
					iListView->CurrentListChanged(CFFListView::EElements);
					ScheduleWorkerL(EJobSubfolders);
				}
			}
			else
				LOG0("CMLauncherAppUi::IdleWorker: Discovering new sources done, NO changes!");
			iJobs-=EJobDiscoverSourcesAndParseMetadata;
		}
	}
	else if(!otherJobs)
	{
		__ASSERT_ALWAYS(iCurrentView,Panic(ECurrentViewIsNull));

		if(iListView && iListView->iJobs)
			otherJobs|=iListView->IdleWorker();
		else if(iMusicPlayerView && iMusicPlayerView->iJobs)
			otherJobs|=iMusicPlayerView->IdleWorker();
	};

	iJobs&=~EDoingJobs;
	if(iJobs || otherJobs)
	{
		//LOG(ELogGeneral,-1,"CMLauncherAppUi::IdleWorker: end (there are still jobs to do)");
		return 1; //more work to do
	}
	else
	{
		iIdle->Cancel();
		LOG0("CMLauncherAppUi::IdleWorker: end (no more jobs to do)");
		return 0; //no more work to do
	};
};

TInt CMLauncherAppUi::IdleWorkerStatic(TAny *aInstance)
{
	return ((CMLauncherAppUi*)aInstance)->IdleWorker();
};

/*
void CMLauncherAppUi::CancelCurrentTransfer()
{
	LOG("CMLauncherAppUi::CancelCurrentTransfer: start");
	LOG("CMLauncherAppUi::CancelCurrentTransfer: end");
}
*/
void CMLauncherAppUi::Exit()
{
	LOG(ELogGeneral,1,"CMLauncherAppUi::Exit()++");
	//first, switch to background
	const TUid KMyAppUid = {_UID3}; //_UID3 defined in .hrh
	TApaTaskList taskList(iCoeEnv->WsSession());
	TApaTask task = taskList.FindApp(KMyAppUid);
	if (task.Exists())
	{
		task.SendToBackground();
	};
	/*    	
	if(iMusicPlayerView)
		iMusicPlayerView->SaveBeforeExit();*/
	if(iListView)
	{
		if(iListView->iFlags&CFFListView::EShouldUpgrade)
			iListView->CheckAndUpgradeL(EFalse);
	};

	iCurrentView=NULL;
	LOG(ELogGeneral,-2,"Before calling CAknAppUiBase::Exit()");
	CAknViewAppUi::Exit();
	//the app should not reach here
}

void CMLauncherAppUi::HandleCommandL(TInt aCommand)
{
	LOG(ELogGeneral,1,"CMLauncherAppUi::HandleCommandL++");
	AppUiHandleCommandL(aCommand,NULL);
	LOG(ELogGeneral,-1,"CMLauncherAppUi::HandleCommandL--");
}

void CMLauncherAppUi::HandleForegroundEventL(TBool aForeground)
{
	if(aForeground)
	{
		LOG0("MLauncher switched to foreground.");
		iFlags&=~EAppIsInBackground;
	}
	else
	{
		LOG0("MLauncher switched to background.");
		iFlags|=EAppIsInBackground;
	}
	CAknViewAppUi::HandleForegroundEventL(aForeground);
}

TBool CMLauncherAppUi::AppUiHandleCommandL(const TInt aCommand, const CAknView *aView)
{
	LOG(ELogGeneral,1,"AppUIHandleCommandL: start (0x%x)",aCommand);
	TBool handled(EFalse);
	if(aView)
		iCurrentView=aView;

	switch(aCommand)
	{
	case EEikCmdExit:
	{
		LOG0("AppUIHandleCommandL: EEikCmdExit command received");
		/*we can be here in 2 cases:
		-user has pressed the Red/End key
		-phone is rebooting
		We need to save what it is to be saved, then check the red key behaviour: switch to background or exit. 
		If the command was due to the phone rebooting, we will be terminated any way.
		*/
		//nothing to save at the moment
		if(MyDoc().iPreferences->iPFlags & CMLauncherPreferences::EPreferencesEndKeyBackgroundOnly)
		{
			//just switch to background
			const TUid KMyAppUid = {_UID3}; //_UID3 defined in .hrh
			TApaTaskList taskList(iCoeEnv->WsSession());
			TApaTask task = taskList.FindApp(KMyAppUid);
			if (task.Exists())
			{
				if(iFlags&EAppIsInBackground)
				{
					LOG0("MLauncher already in background, calling Exit()");
					Exit();
				}
				else
				{
					task.SendToBackground();
					LOG0("MLauncher sent to background");
				}
			}
			else LOG0("Sending to background failed because task not found!");
		}
		else 
		{
			LOG0("Before calling Exit()");
			Exit();
		};
		handled=ETrue;
	};break;
    case EAknSoftkeyExit:
    	LOG0("AppUIHandleCommandL: EAknSoftkeyExit command received");
    	//now do other stuff
    	Exit();
    	handled=ETrue;
        break;
	case ESoftkeyNavi:
	{
		//show the navi menu
		CEikMenuBar *myMenuBar = iEikonEnv->AppUiFactory()->MenuBar();
		if(myMenuBar)
		{
#ifdef __MSK_ENABLED__
			/*myMenuBar->SetContextMenuTitleResourceId(R_MENUBAR_NAVI);
			myMenuBar->TryDisplayContextMenuBarL();*/
			myMenuBar->TryDisplayMenuBarL();
#else
			myMenuBar->TryDisplayMenuBarL();
#endif
		};
		//done showing the navi menu
		handled=ETrue;
	};break;
	case ECommandSwitchToFFList:
		SwitchViewL(iListView);
		handled=ETrue;
		break;
	case ECommandSwitchToPlaylists:
		if(!iPlaylistsView)
			CreatePlaylistsViewL();
		else
			SwitchViewL(iPlaylistsView);
		handled=ETrue;
		break;
	case ECommandSwitchToFiletransfer:
		SwitchViewL(iFiletransferView);
		handled=ETrue;
		break;
	case ECommandSwitchToStopwatch:
	    SwitchViewL(iStopwatchView);
	    handled=ETrue;
	    break;
	case ECommandSwitchToMusicPlayer:
		SwitchViewL(iMusicPlayerView);
		handled=ETrue;
		break;
	case ECommandSwitchToMyFiles:
		if(!iMyFilesView)
			CreateMyFilesViewL();
		else
			SwitchViewL(iMyFilesView);
		handled=ETrue;
		break;
	};
	LOG(ELogGeneral,-1,"AppUIHandleCommandL: end (%d)",handled);
	return handled;
};

void CMLauncherAppUi::DynInitMenuPaneL(TInt aResourceId, CEikMenuPane* aMenuPane)
{
	if(aResourceId==R_MENU_NAVI)
	{
		LOG0("DynInitMenuPaneL for Navi menu++");
		if(!iFiletransferView || iCurrentView==iFiletransferView || !(iActiveViews&EFiletransferViewActive))
			aMenuPane->SetItemDimmed(ECommandSwitchToFiletransfer,ETrue);
		if(!iStopwatchView || !(iActiveViews&EStopwatchViewActive))
			aMenuPane->SetItemDimmed(ECommandSwitchToStopwatch,ETrue);
		if(!iMusicPlayerView || !(iActiveViews&EMusicPlayerViewActive))
			aMenuPane->SetItemDimmed(ECommandSwitchToMusicPlayer,ETrue);
		//no need to check for sources view, as it does not have a Navi menu
		TBuf<KMaxTexLengthInSelectionText> buf;
		iListView->GetSelectionText(buf);
		if(buf!=_L("myfiles"))
			aMenuPane->SetItemDimmed(ECommandSwitchToMyFiles,ETrue);

		//set the "radio button"
		if(iCurrentView==iListView) 
			aMenuPane->ItemData(ECommandSwitchToFFList).iText.Append(KXSel);
		else if(iCurrentView==iFiletransferView) 
			aMenuPane->ItemData(ECommandSwitchToFiletransfer).iText.Append(KXSel);
		else if(iCurrentView==iStopwatchView) 
			aMenuPane->ItemData(ECommandSwitchToStopwatch).iText.Append(KXSel);
		else if(iCurrentView==iMusicPlayerView) 
			aMenuPane->ItemData(ECommandSwitchToMusicPlayer).iText.Append(KXSel);
		else if(iCurrentView==iPlaylistsView) 
			aMenuPane->ItemData(ECommandSwitchToPlaylists).iText.Append(KXSel);
		else if(iCurrentView==iMyFilesView)
			aMenuPane->ItemData(ECommandSwitchToMyFiles).iText.Append(KXSel);
		LOG0("DynInitMenuPaneL for Navi menu--");
	};
};

void CMLauncherAppUi::SwitchViewL(const CAknView *aCurrentView)
{
	iCurrentView=aCurrentView;
	ActivateLocalViewL(iCurrentView->Id());
	//change to the Navi menu, if necessary
	//ChangeExitWithNaviL(iCurrentView->Id().iUid);
};

/*
CPlaylist* CMLauncherAppUi::GetNewPlaylist()
{
	//select one of the playlists
	
	//files4Playlist=new (ELeave)CPlaylist(KPointerArrayPlaylistGranularity);
	//files4Playlist->SetPool(iFilesTree);
	
	return iCurrentPlaylist;
}*/


void CMLauncherAppUi::LaunchExternalPlayerL()
{
	TFileName filename;
    CreatePlaylistFilename(filename);
    LOG0("LaunchExternalPlayerL++ (%S)",&filename);
    //add the files from the current playlist 
    MyDoc().iCurrentPlaylist->ExportPlaylist(MyDoc().iFs,filename);
	//we launch the player with a playlist or file
#ifdef S60BUG_SHARELAUNCH

    const TUid KAppUidMPX={ 0x102072C3 };
    const TUid KMPXPlaybackViewTypeUid  = { 0x101FFCA0 }; // Now Playing View
    const TUid KMPXCollectionViewTypeUid= { 0x101FFCA1 }; // Collection View


    // Construct MPX parameter string
    TUid viewUid(KMPXPlaybackViewTypeUid);
    TBuf8<16> param;
    param.FillZ(16);
    param[8] = 0x01;
    param[12] = (TUint8) (viewUid.iUid & 0x000000ff);
    param[13] = (TUint8)((viewUid.iUid & 0x0000ff00) >> 8);
    param[14] = (TUint8)((viewUid.iUid & 0x00ff0000) >> 16);
    param[15] = (TUint8)((viewUid.iUid & 0xff000000) >> 24);

    TApaTaskList taskList( CEikonEnv::Static()->WsSession() );
    TApaTask task = taskList.FindApp( KAppUidMPX );

    if ( task.Exists() )
    	{
    	task.SendMessage( KAppUidMPX, param );
    	return;
    	}

    RApaLsSession apaSession;
    TApaAppInfo appInfo;
    User::LeaveIfError( apaSession.Connect() );
    CleanupClosePushL( apaSession );

    User::LeaveIfError( apaSession.GetAppInfo( appInfo, KAppUidMPX ) );

    CApaCommandLine *cmdLine = CApaCommandLine::NewLC();
    cmdLine->SetExecutableNameL( appInfo.iFullName );
    cmdLine->SetCommandL( EApaCommandRun );
    cmdLine->SetDocumentNameL(filename);

    // Collection view is the default view at startup
    if( viewUid != KMPXCollectionViewTypeUid )
    	{
    	cmdLine->SetTailEndL( param );
    	}

    User::LeaveIfError( apaSession.StartApp( *cmdLine ) );

    CleanupStack::PopAndDestroy( 2 ); // cmdLine, apaSession

#else
    if(MyDoc().iPreferences->iPFlags&CMLauncherPreferences::EPreferencesLaunchEmbedded)
    {
        //launch the Music player in embedded mode
        TDataType dataType(_L8("playlist/mpegurl"));
        //TDataType dataType(_L8("audio/mpeg"));
        //TDataType dataType;
        if(iDocHandler==NULL)
        {
            iDocHandler = CDocumentHandler::NewL();
            iDocHandler->SetExitObserver(this);
        };
        /*
        CAiwGenericParamList *gParam=CAiwGenericParamList::NewLC();
        TAiwGenericParam param(0,0x101FFCA0);
        gParam->AppendL(param);
        */
        iDocHandler->OpenFileEmbeddedL( filename, dataType );
        /*
        RFile shar;
        User::LeaveIfError(shar.Open(iEikonEnv->FsSession(),aFilename,EFileRead|EFileShareReadersOnly));
        CleanupClosePushL(shar);
        iDocHandler->OpenFileEmbeddedL( shar, dataType, *gParam );
        shar.Close();
        CleanupStack::PopAndDestroy(gParam);
        */
    }
    else
    {
        //iDocHandler->OpenFileL( playlist, dataType );
        //Do it with AppArc
        RApaLsSession ls;
        TThreadId tid;
        User::LeaveIfError(ls.Connect());
        ls.GetAllApps();
        ls.StartDocument(filename,tid);
        ls.Close();
    };
#endif
    LOG0("LaunchExternalPlayerL--");
}

void CMLauncherAppUi::CreatePlaylistFilename(TFileName &aPlaylist)
{
    TFileName tempPlaylist(aPlaylist);
    if(tempPlaylist.Length()==0)
    {
        aPlaylist.Copy(KDDrive); //we create a temporary playlist in RAM
        aPlaylist.Append(KDirBackslash);
    }
    else 
    {
    	//iSelection->GetFullPath(aPlaylist);
    	aPlaylist.Copy(KPlaylistFolder);
    };
    //assign a temporary name to playlist, if no name is given
    if(tempPlaylist.Length()==0)
    {
        aPlaylist.Append(KTempPlaylist);
        aPlaylist.Append(KPlsExtension);
    }
    else
    {
        aPlaylist.Append(tempPlaylist);
        //check if the playlist contains the right extension
        if(aPlaylist.Right(KPlsExtension().Length())!=KPlsExtension)
            aPlaylist.Append(KPlsExtension);
    };
}


void CMLauncherAppUi::HandleServerAppExit (TInt /*aReason*/)
{
    //delete the temporary playlist
    TFileName plsFilename;
    CreatePlaylistFilename(plsFilename);
    MyDoc().iFs.Delete(plsFilename);
}

void CMLauncherAppUi::StartPlayingPlaylist(TInt aPlaylistIndex)
{
	LOG(ELogGeneral,1,"CMLauncherAppUi::StartPlayingPlaylist: start (%d/%d)",aPlaylistIndex,MyDoc().iPlaylists.Count());
	MyDoc().SetPlaylistAsCurrent(aPlaylistIndex);	
	
	if(MyDoc().iPreferences->iPFlags&CMLauncherPreferences::EPreferencesUseInternalMusicPlayer)
		CreateMusicPlayerViewL(MyDoc().iCurrentPlaylist,EFalse,ETrue); 
	else 
	{
		TRAPD(err,MyDoc().iCurrentPlaylist->LoadPlaylistL(MyDoc()));
		LaunchExternalPlayerL(); //for current playlist
	}
	
	LOG(ELogGeneral,-1,"CMLauncherAppUi::StartPlayingPlaylist: end");
}

void CMLauncherAppUi::DeletePlaylist(TInt aPlaylistIndex)
{
	if(iMusicPlayerView && MyDoc().iCurrentPlaylist==MyDoc().iPlaylists[aPlaylistIndex])
	{
		iMusicPlayerView->ClearPlaylist();
	}
	MyDoc().DeletePlaylist(aPlaylistIndex);
}



