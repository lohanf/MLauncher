/*
============================================================================
 Name        : FFListView.cpp
 Author      : Florin Lohan
 Copyright   : Florin Lohan
 Description : CFFListView implementation
============================================================================
 */

// INCLUDE FILES
#include <avkon.hrh>
#include <aknnotewrappers.h>
#include <aknwaitdialog.h>
#include <eikclbd.h> //CColumnListBoxData
#include <stringloader.h>
#include <MLauncher.rsg>
#include <f32file.h>
#include <utf.h>
#include <apgcli.h>
#include <aknmessagequerydialog.h>
#include <aknsfld.h> //CAknSearchField
#include <mmf/common/mmfcontrollerpluginresolver.h> //for CleanupResetAndDestroyPushL


#include "MLauncher.pan"
#include "MLauncherAppUi.h"
#include "FFListView.h"
#include "ChildrenArray.inl"
#include "FFListContainer.h"
#include "MLauncherPreferences.h"
#include "HTTPclient.h"
#include "MLauncher.hrh"
//#include "XMLparsing.h"
#include "MLauncherBT.h"
//#include "HTTPclient.h"
#include "log.h"
#include "urls.h"
#include "defs.h"
#include "common_cpp_defs.h"
#ifdef __TOUCH_ENABLED__
#include <akntoolbar.h>
#endif

const TInt KMaxItemsToEraseSubstrings=100;
const TInt KMinLenSubstringToErase=4;

const TInt KStartEraseSubstringMs=2000000;
const TInt KEraseSubstringIntervalMs=150000; //~6 chars per second

// ============================ MEMBER FUNCTIONS ===============================


// -----------------------------------------------------------------------------
// CFFListView::ConstructL()
// Symbian 2nd phase constructor can leave.
// -----------------------------------------------------------------------------
//
void CFFListView::ConstructL()
{
    // Initialise app UI with standard value.
    BaseConstructL(R_LIST_VIEW);
    CHTTPclient::iee=CCommCommon::iee=iEikonEnv;

    CheckAndUpgradeL(ETrue);

    
#ifdef __TOUCH_ENABLED__
    ShowToolbarOnViewActivation(ETrue);
    if(Toolbar())
    {
    	Toolbar()->SetToolbarObserver(this);
    	if(MyDoc()->iPreferences->iPFlags&CMLauncherPreferences::EPreferencesPlayButtonNoShuffle)
    		Toolbar()->HideItem(ECommandFilesShufflePlayMenu,ETrue,EFalse);
    	else 
    		Toolbar()->HideItem(ECommandFilesPlayMenu,ETrue,EFalse);
    };
#endif
}
// -----------------------------------------------------------------------------
// CFFListView::CFFListView()
// C++ default constructor can NOT contain any code, that might leave.
// -----------------------------------------------------------------------------
//
CFFListView::CFFListView()
{
    // No implementation required
}

// -----------------------------------------------------------------------------
// CFFListView::~CFFListView()
// Destructor.
// -----------------------------------------------------------------------------
//
CFFListView::~CFFListView()
{
    LOG(ELogGeneral,1,"CFFListView::~CFFListView++");
	if ( iListContainer )
    {
        DoDeactivate();
    };
    CCommSender::DeleteInstance();
    CCommReceiver::DeleteInstance();

    delete iHTTPclient;
    //delete the moving files (if any)
    iMovingFiles.ResetAndDestroy();
    //iMovingFiles.Close();

    if(iPeriodic)
    {
        iPeriodic->Cancel();
        delete iPeriodic;
    };
#ifdef __MSK_ENABLED__
    delete iMskSelect;
    delete iMskUnselect;
#endif
    LOG(ELogGeneral,-1,"CFFListView::~CFFListView--");
}

CFFListView* CFFListView::NewLC()
{
    CFFListView* self=new(ELeave)CFFListView;
    CleanupStack::PushL(self);
    self->ConstructL();
    return self;
}


#ifdef __MSK_ENABLED__
void CFFListView::SetMSKSelectUnselect(TBool aSelected, TBool aHasSelection/*=ETrue*/)
{
	LOG(ELogGeneral,1,"SetMSKSelectUnselect++ (aSelected=%d, aHasSelection=%d)",aSelected,aHasSelection);
	if(!iMskSelect)
	{
		iMskSelect=StringLoader::LoadL(R_MSK_SELECT);
		iMskUnselect=StringLoader::LoadL(R_MSK_UNSELECT);
	};
	if(aHasSelection)
	{
		if(aSelected)
			Cba()->SetCommandL(EAknSoftkeyForwardKeyEvent,*iMskUnselect);
		else
			Cba()->SetCommandL(EAknSoftkeyForwardKeyEvent,*iMskSelect);
	}
	else
		Cba()->SetCommandL(EAknSoftkeyForwardKeyEvent,_L(""));
	Cba()->DrawDeferred();
	LOG(ELogGeneral,-1,"SetMSKSelectUnselect--");
}
#endif

void CFFListView::LaunchPlayerL(TBool aShuffle)
{
    if(!iListContainer)return;
    LOG(ELogGeneral,1,"CFFListView::LaunchPlayerL++ (shuffle: %d)",aShuffle);
    iListContainer->StoreSelection(MyDoc()->iFilesTree);
    CPlaylist *currentPlaylist=MyDoc()->BuildCurrentPlaylistL();
    TInt nrFilesInThePlaylist=currentPlaylist->Count();
    if(!nrFilesInThePlaylist)
    {
    	//no files in the playlist. Select the current item and try again
    	TInt index=iListContainer->iFileList->CurrentItemIndex();
    	if(index>=0)
    	{
    		(*MyDoc()->iFilesTree->iCurrentParent->iChildren)[index].SetSelection4Subtree(ETrue);
    		//iListContainer->AddListElementsL(*iFilesTree,ETrue);
    		CListBoxView *view=iListContainer->iFileList->View();
    		view->ToggleItemL(index);
    		view->DrawItem(index);
#ifdef __MSK_ENABLED__
    		SetMSKSelectUnselect(ETrue);
#endif
    		//try again
    		currentPlaylist=MyDoc()->BuildCurrentPlaylistL();
    		nrFilesInThePlaylist=currentPlaylist->Count();
    	};
    };
    if(nrFilesInThePlaylist)
    {
    	//there are files in the playlist
    	//shuffle, if needed
    	if(aShuffle && !currentPlaylist->iIsAudiobook)
    		currentPlaylist->Shuffle();
    	//save
    	currentPlaylist->SaveCurrentPlaylist(*MyDoc());
    	//launch
    	if(MyDoc()->iPreferences->iPFlags&CMLauncherPreferences::EPreferencesUseInternalMusicPlayer)
    		MyAppUi()->CreateMusicPlayerViewL(currentPlaylist,EFalse,ETrue); 
    	else 
    		MyAppUi()->LaunchExternalPlayerL(); //for current playlist
    }
    else
    {
    	//playlist is empty
    	//TODO: but up a dialog explaining there are no files in the laylist
    }
    LOG(ELogGeneral,-1,"CFFListView::LaunchPlayerL--");
}

void CFFListView::CurrentListChanged(TWhatChanged aWhatChanged)
{
	LOG(ELogGeneral,1,"CurrentListChanged: start (what changed: %d)",aWhatChanged);
	if(!iListContainer || aWhatChanged==EFilesTree)
	{
		LOG(ELogGeneral,-1,"CurrentListChanged: end (iListContainer=%X, aWhatChanged=%d)",iListContainer,aWhatChanged); 
		return;
	}
	
	if(aWhatChanged==EMetadataParsingStarted)
	{
		iFlags|=EContainerHasProgressBar;		
		iMetadataParseStart.UniversalTime();
		iListContainer->SizeChanged();
	}
	else if(aWhatChanged==EMetadataParsingEnded)
	{
		iFlags&=~EContainerHasProgressBar;		
		iListContainer->SizeChanged();
	}
	else if(aWhatChanged==EMetadataParsingInProgress)
	{
		if(MyDoc()->iFilesTree->iCurrentParent->iFlags&TFileDirEntry::EIsMetadata)
		{
			//update
			iListContainer->StoreSelection(MyDoc()->iFilesTree);
			iListContainer->UpdateProgressBar();
			iListContainer->AddListElementsL(*MyDoc()->iFilesTree,ETrue);
		};
	}
	else if(aWhatChanged==EElements)
	{
		iListContainer->AddListElementsL(*MyDoc()->iFilesTree,ETrue);
		//we need to enable going down into folders
#ifdef __TOUCH_ENABLED__
		if(Toolbar())
		{
			LOG0("Enabling FilesDown button");
			Toolbar()->HideItem(ECommandFilesDown,EFalse,EFalse);
			Toolbar()->SetItemDimmed(ECommandFilesDown,EFalse,ETrue);
			CurrentItemChanged();
		}
#endif
		ScheduleWorkerL(EJobEraseSubstrings);
	}
	else //common for all the rest
	{
		MyDoc()->iFilesTree->iCurrentParent->iSelectedPosition=iListContainer->iFileList->CurrentItemIndex();
		iListContainer->AddListElementsL(*MyDoc()->iFilesTree,ETrue);
	};
	LOG(ELogGeneral,-1,"CurrentListChanged: end"); 
}

void CFFListView::CurrentItemChanged(TInt aNewItemIndex/*=-1*/)
{
#ifdef __TOUCH_ENABLED__
	if(!Toolbar() || !iListContainer)return;
	LOG(ELogGeneral,1,"CurrentItemChanged++");
	if(aNewItemIndex<0)
		aNewItemIndex=iListContainer->iFileList->CurrentItemIndex();
	LOG0("aNewItemIndex=%d",aNewItemIndex);
	
	if(aNewItemIndex<0 )//it may be that this is an empty folder!
	{
		//we show the "FilesDown" icon, but greyed
		Toolbar()->HideItem(ECommandFilesPreview,ETrue,EFalse);
		Toolbar()->HideItem(ECommandFilesDown,EFalse,EFalse);
		Toolbar()->SetItemDimmed(ECommandFilesDown,ETrue,ETrue);
		iFlags&=~EToolbarDisplaysPlaysong;
	}
	else
	{
		TFileDirEntry &child=(*MyDoc()->iFilesTree->iCurrentParent->iChildren)[aNewItemIndex];
		if(child.iFlags&TFileDirEntry::EIsDir)
		{
			if(iFlags&EToolbarDisplaysPlaysong)
			{
				//we need to change this to gointo_folder
				Toolbar()->HideItem(ECommandFilesPreview,ETrue,EFalse);
				Toolbar()->HideItem(ECommandFilesDown,EFalse,ETrue);
				iFlags&=~EToolbarDisplaysPlaysong;
			}
		}
		else
		{
			if(!(iFlags&EToolbarDisplaysPlaysong))
			{
				//we need to change this into preview
				Toolbar()->HideItem(ECommandFilesDown,ETrue,EFalse);
				Toolbar()->HideItem(ECommandFilesPreview,EFalse,ETrue);
				iFlags|=EToolbarDisplaysPlaysong;
			}
		}
	}
	LOG(ELogGeneral,-1,"CurrentItemChanged--");
#endif
}

void CFFListView::MoveRightL(TInt aIndex)
{
	LOG(ELogGeneral,1,"MoveRightL start");
	if(aIndex<0 || aIndex>=MyDoc()->iFilesTree->iCurrentParent->ChildrenCount() || (!iListContainer))return;//nowhere to move

    if((*MyDoc()->iFilesTree->iCurrentParent->iChildren)[aIndex].iFlags & TFileDirEntry::EIsDir)
    {
        //This is a folder
        iListContainer->StoreSelection(MyDoc()->iFilesTree);
#ifdef __TOUCH_ENABLED__
        if(Toolbar())
        {
        	if(!MyDoc()->iFilesTree->iCurrentParent->iParent)//we came from the top, enable the go up icon
        		Toolbar()->SetItemDimmed(ECommandFilesUp,EFalse,ETrue);
        }
#endif
        MyDoc()->iFilesTree->iCurrentParent=& (*MyDoc()->iFilesTree->iCurrentParent->iChildren)[aIndex];
        iListContainer->AddListElementsL(*MyDoc()->iFilesTree,ETrue);
        if(iFlags&EContainerHasProgressBar)
        	iListContainer->SizeChanged();
        CurrentItemChanged();
        RestartEraseSubstrings();
        MyAppUi()->ScheduleWorkerL(CMLauncherAppUi::EJobSubfolders);
    }
    else
    {
        //this is a music file! Play it!
    	
        if(MyDoc()->iPreferences->iPFlags&CMLauncherPreferences::EPreferencesUseInternalMusicPlayer)
        {
        	//launch the internal player
        	CPlaylist *tempPlaylist=MyDoc()->CreateTemporaryPlaylistL(&(*MyDoc()->iFilesTree->iCurrentParent->iChildren)[aIndex]);
        	MyAppUi()->CreateMusicPlayerViewL(tempPlaylist,ETrue,ETrue); 
        }
        else
        {
        	MyDoc()->CreateTemporaryPlaylistL(&(*MyDoc()->iFilesTree->iCurrentParent->iChildren)[aIndex]);
        	MyAppUi()->LaunchExternalPlayerL();
        };
    };
    LOG(ELogGeneral,-1,"MoveRightL end");
}

void CFFListView::MoveLeftL()
{
	if(MyDoc()->iFilesTree->iCurrentParent->iParent && iListContainer)
    {
        iListContainer->StoreSelection(MyDoc()->iFilesTree);
        MyDoc()->iFilesTree->iCurrentParent=MyDoc()->iFilesTree->iCurrentParent->GetProperParent(MyDoc()->iFilesTree);
        iListContainer->AddListElementsL(*MyDoc()->iFilesTree,ETrue);
        if(iFlags&EContainerHasProgressBar)
        	iListContainer->SizeChanged();
#ifdef __TOUCH_ENABLED__
        if(Toolbar())
        {
        	if(!MyDoc()->iFilesTree->iCurrentParent->iParent)//we are at the top, disable the go up icon
        		Toolbar()->SetItemDimmed(ECommandFilesUp,ETrue,ETrue);
        	//perhaps the FilesDown icon was disabled ...
        	if(!(iFlags&EToolbarDisplaysPlaysong))
        		Toolbar()->SetItemDimmed(ECommandFilesDown,EFalse,ETrue);
        }
#endif
        CurrentItemChanged();
        RestartEraseSubstrings();
    };
}

void CFFListView::ResetViewL()
{
	if(!iListContainer || !MyDoc()->iFilesTree->iCurrentParent || (iFlags&EDoActivateInProgress))return;

    iListContainer->StoreSelection(MyDoc()->iFilesTree);
    iListContainer->AddListElementsL(*MyDoc()->iFilesTree,ETrue);
    iEraseSubstringLength=-1;//this will abort any ongoing operation of erasing substrings
    ScheduleWorkerL(EJobEraseSubstrings);
}
// -----------------------------------------------------------------------------
// CFFListView::HandleCommandL()
// Takes care of command handling.
// -----------------------------------------------------------------------------
//
void CFFListView::HandleCommandL( TInt aCommand )
{
    //try first to send it to the AppUi for handling

    if(MyAppUi()->AppUiHandleCommandL(aCommand,this))
        return; //AppUiHandleCommandL returned TRUE, it handled the command
    LOG(ELogGeneral,1,"CMLauncherListView::HandleCommandL: start(command=%x)",aCommand);
    //AknFind::HandleFindPopupProcessCommandL(aCommand,iListContainer->iFileList,iListContainer->iFindBox,iListContainer);

    TInt err(0); //TODO: keep this TRAP?
    //TRAP(err,
            //AppUiHandleCommandL did not handle the command
            switch(aCommand)
            {
            case ECommandPlay:
                LaunchPlayerL(EFalse);
                break;
            case ECommandShufflePlay:
                LaunchPlayerL(ETrue);
                break;
            case ECommandFind:
            	if(MyDoc()->iFilesTree)
            	{
            		iListContainer->StoreSelection(MyDoc()->iFilesTree);
            		iListContainer->ShowFindPopup();
            		//iListContainer->AddListElementsL(*iFilesTree,EFalse);
            	}
            	break;
                /*
            case ECommandPlaylist:
            {
                TFileName playlistFilename;
                CAknTextQueryDialog* playlistFilenameQuery=CAknTextQueryDialog::NewL(playlistFilename);
                CleanupStack::PushL(playlistFilenameQuery);

                HBufC* prompt=StringLoader::LoadLC(R_DATA_QUERY_PLAYLIST_PROMPT);
                playlistFilenameQuery->SetPromptL(*prompt);
                CleanupStack::PopAndDestroy(prompt);
                CleanupStack::Pop(playlistFilenameQuery);

                if(playlistFilenameQuery->ExecuteLD(R_DATAQUERY_INPUT_FILENAME))
                {
                    CPlaylist *tempPlaylist=CEngine::Instance()->CreateTemporaryPlaylistL(iFilesTree->iRoot);
                    TInt err=tempPlaylist->ExportPlaylist(MyAppUi()->iFs,playlistFilename);
                    //CreatePlaylistL(playlistFilename,EFalse);
                    // Load a string from the resource file and display it
                    HBufC* textResource(NULL);
                    if(err)
                    	StringLoader::LoadLC(R_PLAYLIST_CREATED);
                    else
                    	StringLoader::LoadLC(R_PLAYLIST_CREATED);
                    CAknInformationNote* informationNote;

                    informationNote = new ( ELeave ) CAknInformationNote;

                    // Show the information Note with
                    // textResource loaded with StringLoader.
                    informationNote->ExecuteLD( *textResource);

                    // Pop HBuf from CleanUpStack and Destroy it.
                    CleanupStack::PopAndDestroy( textResource );
                };
            };break;
            */
            case ECommandSendPlaylistBTDiscover:
            {
                iListContainer->StoreSelection(MyDoc()->iFilesTree);
                CPlaylist *tempPlaylist=MyDoc()->CreateTemporaryPlaylistL(MyDoc()->iFilesTree->iRoot,ETrue);
                //TODO: this part needs some rewriting
                CCommSender::GetInstanceL()->SendPlaylistL(CCommCommon::EBTConnection,MyAppUi(),tempPlaylist);//ownership is transferred
                
            };break;
            case ECommandSendPlaylistIP:
            {
            	//TODO: this part needs some rewriting
            	/*
            	iListContainer->StoreSelection(iFilesTree);
            	CPlaylist *tempPlaylist=MyAppUi()->CreateTemporaryPlaylistL(iFilesTree->iRoot,ETrue);
                */
                //TInt prefixLength=iSelection->GetFullPathLength();
                //CCommSender::GetInstanceL()->SendPlaylistL(CCommCommon::EIPConnection,MyAppUi(),tempPlaylist,prefixLength,&KIP);//ownership is transferred
            };break;
            case ECommandSelectAll:
            {
            	MyDoc()->iFilesTree->iCurrentParent->iSelectedPosition=iListContainer->iFileList->CurrentItemIndex();
            	MyDoc()->iFilesTree->iCurrentParent->SetSelection4Subtree(ETrue);
                //refresh the list
                iListContainer->AddListElementsL(*MyDoc()->iFilesTree,ETrue);
            };break;
            case ECommandUnselectAll:
            {
            	MyDoc()->iFilesTree->iCurrentParent->iSelectedPosition=iListContainer->iFileList->CurrentItemIndex();
            	MyDoc()->iFilesTree->iCurrentParent->SetSelection4Subtree(EFalse);
                //refresh the list
                iListContainer->AddListElementsL(*MyDoc()->iFilesTree,ETrue);
            };break;
            /* NOT IMPLEMENTED YET
            case ECommandSaveSelection:
            {
                //TODO: NOT IMPLEMENTED YET
               
            };break;
            */
            case ECommandDelete:
            {
                //delete the files from the current folder
                DeleteSelectionL();
            };break;
            case ECommandRename:
            {
                RenameCurrentEntryL();
            };break;
            case ECommandMove:
            {
                MoveSelectionL();
            };break;
            case ECommandMoveHere:
            {
                MoveSelectionHereL();
            };break;
            case ECommandCancelMove:
            {
                //delete the moving files (if any)
                TInt i;
                for(i=0;i<iMovingFiles.Count();i++)
                    delete iMovingFiles[i];
                iMovingFiles.Close();
            };break;
            case ECommandNewFolder:
            {
                TFileName newFolder;
                CAknTextQueryDialog* newFolderQuery=CAknTextQueryDialog::NewL(newFolder);
                CleanupStack::PushL(newFolderQuery);

                HBufC* prompt=StringLoader::LoadLC(R_DATA_QUERY_NEW_FOLDER_PROMPT);
                newFolderQuery->SetPromptL(*prompt);
                CleanupStack::PopAndDestroy(prompt);
                CleanupStack::Pop(newFolderQuery);

                if(newFolderQuery->ExecuteLD(R_DATAQUERY_INPUT_FILENAME))
                {
                    //create the full path
                    TFileName newFolderPath;
                    MyDoc()->iFilesTree->iCurrentParent->GetFullPath(newFolderPath);
                    //make sure startingdir ends with "\\"
                    CMLauncherPreferences::AddEndingBackslash(newFolderPath);
                    newFolderPath.Append(newFolder);
                    //make sure startingdir ends with "\\"
                    CMLauncherPreferences::AddEndingBackslash(newFolderPath);
                    //create the dir
                    TInt err=iEikonEnv->FsSession().MkDir(newFolderPath);

                    if(err==KErrNone)
                    {
                        //refresh the list
                    	MyDoc()->iFilesTree->UpdateEntry(MyDoc()->iFilesTree->iCurrentParent);
                        if(iListContainer)
                            iListContainer->AddListElementsL(*MyDoc()->iFilesTree,ETrue);
                    }
                    else
                    {
                        // Load a string from the resource file and display it
                        HBufC* textResource = StringLoader::LoadLC( R_NEW_FOLDER_ERROR, err );
                        CAknInformationNote* informationNote;

                        informationNote = new ( ELeave ) CAknInformationNote;

                        // Show the information Note with
                        // textResource loaded with StringLoader.
                        informationNote->ExecuteLD( *textResource);

                        // Pop HBuf from CleanUpStack and Destroy it.
                        CleanupStack::PopAndDestroy( textResource );
                    };
                };
            };break;
            case ECommandCountdownTimer:
            {
                MyAppUi()->CreateStopwatchViewL();
            };break;
            case ECommandSendApplicationBTDiscover:
            {
                CCommSender::GetInstanceL()->SendApplicationL(MyAppUi());
            };break;
            case ECommandSendApplicationSMS:
            {
                CCommSender::GetInstanceL()->SendUrlSMSL(MyAppUi());
            };break;
            case ECommandUploadCrashLogFile:
            {
                UploadCrashLogFileL();
            };break;
            case ECommandCheckUpdates:
            {
                if(!iHTTPclient)
                    iHTTPclient=CHTTPclient::NewL(*this);
                if(iHTTPclient) //can be NULL, if initialization failed
                	iHTTPclient->CheckForUpdatesL();
            };break;
            case ECommandPreferencesStartingDirs:
            {
            	MyAppUi()->CreateSourcesViewL();
            };break;
            case ECommandPreferencesUseMetadata:
            {
            	CAknQueryDialog *usemetadataQuery=CAknQueryDialog::NewL();
            	CleanupStack::PushL(usemetadataQuery);
            	HBufC* queryPrompt=StringLoader::LoadLC(R_QUERY_USEMETADATA);
            	usemetadataQuery->SetPromptL(*queryPrompt);
            	CleanupStack::PopAndDestroy(queryPrompt);
            	CleanupStack::Pop(usemetadataQuery);
            	TBool oldPreferenceUseMetadata(MyDoc()->iPreferences->iPFlags&CMLauncherPreferences::EPreferencesUseMetadata);
            	TBool changed(EFalse);
            	if(usemetadataQuery->ExecuteLD(R_QUERY_CONFIRMATION))
            	{
            		if(!oldPreferenceUseMetadata)
            		{
            			MyDoc()->iPreferences->iPFlags|=CMLauncherPreferences::EPreferencesUseMetadata;
            			changed=ETrue;
            		}
            	}
            	else
            	{
            		if(oldPreferenceUseMetadata)
            		{
            			MyDoc()->iPreferences->iPFlags&=~CMLauncherPreferences::EPreferencesUseMetadata;
            			changed=ETrue;
            		}
            	}
            	if(changed)
            	{
            		MyDoc()->iPreferences->SavePreferences();
            		//notify the user that the change will take place next time when MLauncher is restarted
            		MyAppUi()->DisplayQueryOrNoteL(EMsgNotePersistent,R_MESSAGE_USEMETADATA_AFTER_RESTART,-1,R_MESSAGE_DLG_OK_EMPTY);
            		//reuse our query
            		usemetadataQuery=CAknQueryDialog::NewL();
            		CleanupStack::PushL(usemetadataQuery);
            		queryPrompt=StringLoader::LoadLC(R_MESSAGE_USEMETADATA_AFTER_RESTART);
            		usemetadataQuery->SetPromptL(*queryPrompt);
            		CleanupStack::PopAndDestroy(queryPrompt);
            		CleanupStack::Pop(usemetadataQuery);
            		usemetadataQuery->ExecuteLD(R_QUERY_CONFIRMATION);
            	}
            };break;
            case ECommandPreferencesKeepSelection:
            {
                CAknQueryDialog *keepselectionQuery=CAknQueryDialog::NewL();
                CleanupStack::PushL(keepselectionQuery);
                HBufC* queryPrompt=StringLoader::LoadLC(R_QUERY_KEEPSELECTION);
                keepselectionQuery->SetPromptL(*queryPrompt);
                CleanupStack::PopAndDestroy(queryPrompt);
                CleanupStack::Pop(keepselectionQuery);
                if(keepselectionQuery->ExecuteLD(R_QUERY_CONFIRMATION))
                {
                	MyDoc()->iPreferences->iPFlags|=CMLauncherPreferences::EPreferencesKeepSelection;
                    SaveSelection();
                }
                else
                	MyDoc()->iPreferences->iPFlags&=~CMLauncherPreferences::EPreferencesKeepSelection;
                MyDoc()->iPreferences->SavePreferences();
            };break;
            case ECommandPreferencesLaunchEmbedded:
            {
                CAknQueryDialog *launchembeddedQuery=CAknQueryDialog::NewL();
                CleanupStack::PushL(launchembeddedQuery);
                HBufC* queryPrompt=StringLoader::LoadLC(R_QUERY_LAUNCHEMBEDDED);
                launchembeddedQuery->SetPromptL(*queryPrompt);
                CleanupStack::PopAndDestroy(queryPrompt);
                CleanupStack::Pop(launchembeddedQuery);
                if(launchembeddedQuery->ExecuteLD(R_QUERY_CONFIRMATION))
                	MyDoc()->iPreferences->iPFlags|=CMLauncherPreferences::EPreferencesLaunchEmbedded;
                else
                	MyDoc()->iPreferences->iPFlags&=~CMLauncherPreferences::EPreferencesLaunchEmbedded;
                MyDoc()->iPreferences->SavePreferences();
            };break;
            case ECommandPreferencesUsePartialCheck:
            {
                if(iListContainer)
                    iListContainer->StoreSelection(MyDoc()->iFilesTree,EFalse);
                //TODO: This does not work perfectly

                CAknQueryDialog *usePartialCheckQuery=CAknQueryDialog::NewL();
                CleanupStack::PushL(usePartialCheckQuery);
                HBufC* queryPrompt=StringLoader::LoadLC(R_QUERY_USEPARTIALCHECK);
                usePartialCheckQuery->SetPromptL(*queryPrompt);
                CleanupStack::PopAndDestroy(queryPrompt);
                CleanupStack::Pop(usePartialCheckQuery);
                if(usePartialCheckQuery->ExecuteLD(R_QUERY_CONFIRMATION))
                	MyDoc()->iPreferences->iPFlags|=CMLauncherPreferences::EPreferencesUsePartialCheck;
                else
                	MyDoc()->iPreferences->iPFlags&=~CMLauncherPreferences::EPreferencesUsePartialCheck;
                MyDoc()->iPreferences->SavePreferences();
                if(iListContainer)
                {
                    iListContainer->CreateIconsL(ETrue);
                    //refresh the list
                    iListContainer->AddListElementsL(*MyDoc()->iFilesTree,ETrue);
                };
            };break;
            case ECommandPreferencesUseInternalPlayer:
            {
            	if(MyAppUi()->DisplayQueryOrNoteL(EMsgQueryYesNo,R_QUERY_USEINTERNALMUSICPLAYER))
            		MyDoc()->iPreferences->iPFlags|=CMLauncherPreferences::EPreferencesUseInternalMusicPlayer;
            	else
            		MyDoc()->iPreferences->iPFlags&=~CMLauncherPreferences::EPreferencesUseInternalMusicPlayer;
            	MyDoc()->iPreferences->SavePreferences();
            };break;
            case ECommandPreferencesRedKey:
            {
            	if(MyAppUi()->DisplayQueryOrNoteL(EMsgQueryYesNo,R_QUERY_REDKEY))
            		MyDoc()->iPreferences->iPFlags|=CMLauncherPreferences::EPreferencesEndKeyBackgroundOnly;
            	else
            		MyDoc()->iPreferences->iPFlags&=~CMLauncherPreferences::EPreferencesEndKeyBackgroundOnly;
            	MyDoc()->iPreferences->SavePreferences();
            };break;
            case ECommandPreferencesPlayButton:
            {
            	if(MyAppUi()->DisplayQueryOrNoteL(EMsgQueryYesNo,R_QUERY_PLAYBUTTON))
            	{
            		MyDoc()->iPreferences->iPFlags&=~CMLauncherPreferences::EPreferencesPlayButtonNoShuffle;
#ifdef __TOUCH_ENABLED__
            		//set the toolbar accordingly
            		if(Toolbar())
            		{
            			Toolbar()->HideItem(ECommandFilesPlayMenu,ETrue,EFalse);
            			Toolbar()->HideItem(ECommandFilesShufflePlayMenu,EFalse,ETrue);
            		}
#endif
            	}
            	else
            	{
            		MyDoc()->iPreferences->iPFlags|=CMLauncherPreferences::EPreferencesPlayButtonNoShuffle;
#ifdef __TOUCH_ENABLED__
            		//set the toolbar accordingly
            		if(Toolbar())
            		{
            			Toolbar()->HideItem(ECommandFilesPlayMenu,EFalse,EFalse);
            			Toolbar()->HideItem(ECommandFilesShufflePlayMenu,ETrue,ETrue);
            		}
#endif
            	}
            	MyDoc()->iPreferences->SavePreferences();
            };break;
            case ECommandPreferencesDelete:
            {
            	if(MyAppUi()->DisplayQueryOrNoteL(EMsgQueryYesNo,R_QUERY_DELETEPREFERENCES))
            	{
            		MyDoc()->iPreferences->DeletePreferences();
            		AppUi()->Exit();
            		//AppUi()->RunAppShutter();
            	};
            };break;
            case ECommandPreferencesEnableLog:
            {
                //start the log
            	/*
            	iPreferences->iFlags|=CMLauncherPreferences::EPreferencesLogEnabled;
                LOG_ENABLE(iPreferences->iFlags&CMLauncherPreferences::EPreferencesLogEnabled);
                iPreferences->SavePreferences();
                */
            	LOG_ENABLE(ETrue);
            	MyAppUi()->DisplayQueryOrNoteL(EMsgNotePersistent,R_MESSAGE_LOG_ENABLED,-1,R_MESSAGE_DLG_OK_EMPTY);

            	/*
                //display a message to the user
                CAknMessageQueryDialog* dlg = new (ELeave)CAknMessageQueryDialog;
                CleanupStack::PushL(dlg);

                HBufC *message=StringLoader::LoadLC(R_MESSAGE_LOG_ENABLED);
                dlg->SetMessageTextL(*message);
                CleanupStack::PopAndDestroy(message);
                message=StringLoader::LoadLC(R_NOTE_HEADER);
                dlg->SetHeaderTextL(*message);
                CleanupStack::PopAndDestroy(message);
                CleanupStack::Pop(dlg);
                dlg->ExecuteLD(R_MESSAGE_DLG_OK_EMPTY);
                */
            };break;
            case ECommandPreferencesDisableLog:
            {
                //stop the log
            	/*
            	iPreferences->iFlags&=~CMLauncherPreferences::EPreferencesLogEnabled;
            	LOG_ENABLE(iPreferences->iFlags&CMLauncherPreferences::EPreferencesLogEnabled);
                iPreferences->SavePreferences();
                */
            	
            	LOG_ENABLE(EFalse);
            	MyAppUi()->DisplayQueryOrNoteL(EMsgNotePersistent,R_MESSAGE_LOG_DISABLED,-1,R_MESSAGE_DLG_OK_EMPTY);
            };break;
            case ECommandAbout:
            {
                //The about dialog
                CAknMessageQueryDialog* dlg = new (ELeave)CAknMessageQueryDialog;
                CleanupStack::PushL(dlg);

                CArrayFixFlat<TInt> *version=new(ELeave)CArrayFixFlat<TInt>(3);
                CleanupStack::PushL(version);
                TInt ver=Vmajor;
                version->AppendL(ver);
                ver=Vminor;
                version->AppendL(ver);

#ifdef Vdevelopment
                ver=Vbuild;
                version->AppendL(ver);
                HBufC* header=StringLoader::LoadLC(R_ABOUT_HEADER_DEVELOPMENT,*version);
#else
                HBufC* header=StringLoader::LoadLC(R_ABOUT_HEADER,*version);
#endif
                dlg->SetHeaderTextL(*header);
                CleanupStack::PopAndDestroy(header);
                CleanupStack::PopAndDestroy(version);
                HBufC* text=StringLoader::LoadLC(R_ABOUT_TEXT);
                dlg->SetMessageTextL(*text);
                CleanupStack::PopAndDestroy(text);
                CleanupStack::Pop(dlg);
                //dlg->ExecuteLD(R_ABOUT_HEADING_PANE);
                dlg->ExecuteLD(R_MSG_BIG_NOTE);
            };break;
            case ECommandTest:
            {
                //for testing
                Panic(EMLauncherUi);
            };break;
            default:
                Panic( EMLauncherUi );
            };
    //);//TRAPD
    if(err)
    {
        //our command failed
        LOG0("CMLauncherListView::HandleCommandL command (%d) failed. err=%d",aCommand,err);
        //announce the user
        HBufC* noteText=StringLoader::LoadLC(R_ERRMSG_COMMANDFAILED);
        CAknErrorNote *note=new(ELeave)CAknErrorNote;
        note->ExecuteLD(*noteText);
        CleanupStack::PopAndDestroy(noteText);
    };
    LOG(ELogGeneral,-1,"CMLauncherListView::HandleCommandL:end");
}

void CFFListView::GetSelectionText(TDes &aText)
{
	if(iListContainer && iListContainer->iFindBox)
		iListContainer->iFindBox->GetSearchText(aText);
	else aText.SetLength(0);
}

void CFFListView::DoDeactivate()
{
    if(iListContainer)
    {
        if(MyDoc()->iFilesTree)
        	iListContainer->StoreSelection(MyDoc()->iFilesTree);
    	AppUi()->RemoveFromStack(iListContainer);
        delete iListContainer;
        iListContainer = NULL;
    };
}

void CFFListView::DoActivateL(const TVwsViewId& /*aPrevViewId*/,TUid /*aCustomMessageId*/, const TDesC8& /*aCustomMessage*/)
{
    LOG(ELogGeneral,1,"CMLauncherListView::DoActivateL: start");
    iFlags|=(EWaitForFirstDraw|EDoActivateInProgress|EHasBeenActivatedAtLeastOnce);
    CEikStatusPane *sp(StatusPane());
    if(sp)sp->MakeVisible(ETrue);
    CEikButtonGroupContainer *cba(Cba());
    if(cba)cba->MakeVisible(ETrue);
    if(!iListContainer)
    {
        iListContainer = CFFListContainer::NewL( ClientRect(), MyDoc()->iFilesTree, this);
        iListContainer->SetMopParent(this);
    };
    AppUi()->AddToStackL( *this, iListContainer );
    
    //set the current element, if any
    iListContainer->SetCurrentItemAndMSK(*MyDoc()->iFilesTree->iCurrentParent);
#ifdef __TOUCH_ENABLED__
    if(Toolbar())
    {
    	LOG0("We have toolbar");
    	if(MyDoc()->iFilesTree->iCurrentParent->iParent)
    	{
    		LOG0("We are NOT at the top, enable going up");
    		Toolbar()->SetItemDimmed(ECommandFilesUp,EFalse,ETrue);
    	}
    	else
    	{
    		LOG0("We are at the top, disable going up");
    		Toolbar()->SetItemDimmed(ECommandFilesUp,ETrue,ETrue);
    	}

    	CurrentItemChanged();
    }
#endif
    
    if(iFlags&EHasMessage2Display)
    {
        iFlags&=~EHasMessage2Display;
        MyAppUi()->DisplayMessageL();
    };
    ScheduleWorkerL(EJobEraseSubstrings);
    iFlags&=~EDoActivateInProgress;
    LOG(ELogGeneral,-1,"CMLauncherListView::DoActivateL: end");
}

TUid CFFListView::Id() const
{
    return TUid::Uid(KListViewId); //defined in hrh
}

void CFFListView::DynInitMenuPaneL(TInt aResourceId, CEikMenuPane *aMenuPane)
{
    if(aResourceId==R_MENU_LIST)
    {
        if(iMovingFiles.Count())
        {
            //we move files, dime everything except for moving commands
            aMenuPane->SetItemDimmed(ECommandPlay,ETrue);
            aMenuPane->SetItemDimmed(ECommandShufflePlay,ETrue);
            aMenuPane->SetItemDimmed(ECommandFind,ETrue);
            //aMenuPane->SetItemDimmed(ECommandPlaylist,ETrue); this does not exist any more
            aMenuPane->SetItemDimmed(ECommandSendPlaylist,ETrue);
            aMenuPane->SetItemDimmed(ECommandFiles,ETrue);
            aMenuPane->SetItemDimmed(ECommandApplication,ETrue);
            aMenuPane->SetItemDimmed(ECommandPreferences,ETrue);
            aMenuPane->SetItemDimmed(EAknSoftkeyExit,ETrue);
        }
        else
        {
            //Dime the moving commands
            aMenuPane->SetItemDimmed(ECommandMoveHere,ETrue);
            aMenuPane->SetItemDimmed(ECommandCancelMove,ETrue);
            aMenuPane->SetItemDimmed(ECommandNewFolder,ETrue);

        };
    }
    else if(aResourceId==R_MENU_PREFERENCES)
    {
#if RELEASE_LOG
    	//Dime also Enable/Disable log
    	//if(iPreferences->iFlags&CMLauncherPreferences::EPreferencesLogEnabled)
    	if(LOG_ENABLED)
    		aMenuPane->SetItemDimmed(ECommandPreferencesEnableLog,ETrue);
    	else
    		aMenuPane->SetItemDimmed(ECommandPreferencesDisableLog,ETrue);
#else
    	aMenuPane->SetItemDimmed(ECommandPreferencesEnableLog,ETrue);
    	aMenuPane->SetItemDimmed(ECommandPreferencesDisableLog,ETrue);
#endif
    	if(MyDoc()->iPreferences->iPFlags&CMLauncherPreferences::EPreferencesUseInternalMusicPlayer)
    		aMenuPane->SetItemDimmed(ECommandPreferencesLaunchEmbedded,ETrue);
    }
    else if(aResourceId==R_MENU_APPLICATION && !(iFlags&EUploadCrashedLogFile))
            aMenuPane->SetItemDimmed(ECommandUploadCrashLogFile,ETrue);
    else if(aResourceId==R_MENU_FILES)
    {
    	//check if we have valid index
    	TInt index=iListContainer->iFileList->CurrentItemIndex();
    	if(index<0)aMenuPane->SetItemDimmed(ECommandRename,ETrue);
    };
}

#ifdef __TOUCH_ENABLED__
void CFFListView::OfferToolbarEventL(TInt aCommand)
{
	switch(aCommand)
	{
	case ECommandFilesUp:
		MoveLeftL();
		break;
	case ECommandFilesPlayMenu:
		__ASSERT_ALWAYS(MyDoc()->iPreferences->iPFlags&CMLauncherPreferences::EPreferencesPlayButtonNoShuffle,Panic(EToolbarShuffleInconsistency1));
		LaunchPlayerL(EFalse);
		break;
	case ECommandFilesShufflePlayMenu:
		__ASSERT_ALWAYS(!(MyDoc()->iPreferences->iPFlags&CMLauncherPreferences::EPreferencesPlayButtonNoShuffle),Panic(EToolbarShuffleInconsistency2));
		LaunchPlayerL(ETrue);
		break;
	case ECommandFilesDown:
	case ECommandFilesPreview:
		//go one step to the right
		TInt index=iListContainer->iFileList->CurrentItemIndex();
		MoveRightL(index);
		break;
	}
}
#endif


void CFFListView::RenameCurrentEntryL()
{
    //get the filename for the entry to be deleted
	if(!iListContainer)return;
    TInt index=iListContainer->iFileList->CurrentItemIndex();
    if(index<0)return; //nothing to rename
    
    TFileDirEntry &child=(*MyDoc()->iFilesTree->iCurrentParent->iChildren)[index];

    TFileName newName;
    newName.Copy(child.iName,child.iNameLength);
    TFileName oldPath;
    child.GetFullPath(oldPath,EFalse);

    CAknTextQueryDialog* renameQuery=CAknTextQueryDialog::NewL(newName);
    CleanupStack::PushL(renameQuery);
    HBufC* prompt=StringLoader::LoadLC(R_DATA_QUERY_RENAME_PROMPT);
    renameQuery->SetPromptL(*prompt);
    CleanupStack::PopAndDestroy(prompt);
    CleanupStack::Pop(renameQuery);
    if(renameQuery->ExecuteLD(R_DATAQUERY_INPUT_FILENAME))
    {
        //rename the file
    	TBool continueRename(ETrue);
    	//first, get the new path
    	TFileName newPath(oldPath);
    	TInt pos=newPath.LocateReverse('\\');
    	if(pos<1)continueRename=EFalse;
    	//pos should be positive
    	if(continueRename)
    	{
    		newPath.SetLength(pos+1); //we keep the ending "\\"
    		if(newPath.Length()+newName.Length()>KMaxFileName)
    			continueRename=EFalse;
    		else newPath.Append(newName);
    	};
    	
    	//rename the file
    	if(continueRename)
    	{
    		TInt err=iEikonEnv->FsSession().Rename(oldPath,newPath);
    		if(!err)
    		{
    			//renaming succeeded, we change the child as well
    			if(child.iNameLength>=newName.Length())
    			{
    				//the easy alternative
    				TPtr name(child.iName,child.iNameLength);
    				name.Copy(newName); 
    				child.iNameLength=newName.Length();
    			}
    			else
    			{
    				//the more complex alternative
    				TInt nameSize(newName.Size());
    				child.iName=(TUint16*)MyDoc()->iFilesTree->GetNewSpace(nameSize);
    				child.iNameLength=nameSize>>1;
    				TPtr name((TUint16*)child.iName,child.iNameLength);
    				name.Copy(newName);
    			};
    			//refresh the list
    			iListContainer->StoreSelection(MyDoc()->iFilesTree);
    			iListContainer->AddListElementsL(*MyDoc()->iFilesTree,ETrue);
    		}
    		else
    		{
    			//TODO: there was an error renaming, show some error to the user
    		};
    	};
    };
}

void CFFListView::DeleteSelectionL()
{
	if(!iListContainer)return;
    //ask the user if he/she is sure
    CAknQueryDialog *confirmQuery=CAknQueryDialog::NewL();
    CleanupStack::PushL(confirmQuery);
    HBufC* areYouSure=StringLoader::LoadLC(R_QUERY_AREYOUSURE);
    confirmQuery->SetPromptL(*areYouSure);
    CleanupStack::PopAndDestroy(areYouSure);
    CleanupStack::Pop(confirmQuery);
    if(confirmQuery->ExecuteLD(R_QUERY_CONFIRMATION))
    {
        iListContainer->StoreSelection(MyDoc()->iFilesTree);
        //delete the entries
        DeleteSelectedEntriesL(*MyDoc()->iFilesTree->iRoot);

        //refresh the list
        iListContainer->AddListElementsL(*MyDoc()->iFilesTree,ETrue);
    };
}

void CFFListView::MoveSelectionL()
{
	if(!iListContainer)return;
	LOG(ELogGeneral,1,"MoveSelectionL: start");
    iListContainer->StoreSelection(MyDoc()->iFilesTree);
    //create the list of moving items
    TInt i;
    for(i=0;i<MyDoc()->iFilesTree->iCurrentParent->ChildrenCount();i++)
    {
        TFileDirEntry *currentChild=&(*MyDoc()->iFilesTree->iCurrentParent->iChildren)[i];
    	if(currentChild->iFlags & TFileDirEntry::EIsSelected)
        {
            iMovingFiles.AppendL(currentChild);
        };
    };
    iOldParent=MyDoc()->iFilesTree->iCurrentParent;

#ifdef __MSK_ENABLED__
    //create a context menu
    Cba()->AddCommandToStackL(3,R_MSK_CONTEXT);
    Cba()->DrawDeferred();
    //set the context menu
    MenuBar()->SetContextMenuTitleResourceId(R_MENUBAR_LIST);
#endif
    LOG(ELogGeneral,-1,"MoveSelectionL: end");
}

void CFFListView::MoveSelectionHereL()
{
    LOG(ELogGeneral,1,"MoveSelectionHereL: start");
    //move the list of moving items to the current folder
    CFileMan *fm=CFileMan::NewL(iEikonEnv->FsSession());
    TFileName currentPath;
    TFileName currentFile;
    TFileDirEntry *source(NULL);
    TBool moveToRoot(ETrue);
    if(MyDoc()->iFilesTree->iCurrentParent!=MyDoc()->iFilesTree->iRoot)
    {
    	MyDoc()->iFilesTree->iCurrentParent->GetFullPath(currentPath,ETrue);
    	moveToRoot=EFalse;
    };
    TInt i,err,nrFailedMoves(0),nrMoves(iMovingFiles.Count());
    for(i=0;i<nrMoves;i++)
    {
        if(moveToRoot)
        {
        	source=iMovingFiles[i]->GetSource();
        	source->GetFullPath(currentPath,EFalse);
        };
    	iMovingFiles[i]->GetFullPath(currentFile,EFalse);
    	err=fm->Move(currentFile,currentPath, CFileMan::ERecurse );
    	LOG0("Moving %S to %S, error was %d",&currentFile,&currentPath,err);
    	if(err==KErrNone)
    	{
    		//move succeeded
    		if(iOldParent==MyDoc()->iFilesTree->iRoot)
    			iMovingFiles[i]->iParent->iChildren->MarkAsDeleted(iMovingFiles[i]);
    		iOldParent->iChildren->MarkAsDeleted(iMovingFiles[i]);
    		if(moveToRoot)
    			source->AddAndSortChild(MyDoc()->iFilesTree,iMovingFiles[i],ETrue,nrMoves);
    		MyDoc()->iFilesTree->iCurrentParent->AddAndSortChild(MyDoc()->iFilesTree,iMovingFiles[i],ETrue,nrMoves);
    	}
    	else nrFailedMoves++;
    };
    //delete stuff that's not needed any more
    iMovingFiles.Close();
    delete fm;

#ifdef __MSK_ENABLED__
    //put back the old MSK
    Cba()->RemoveCommandFromStack(3,EAknSoftkeyContextOptions);
    Cba()->DrawDeferred();
#endif

    //refresh the list
    //MyDoc()->iFilesTree->SetSelection(iOldParent);
    //clear selection
    RestartEraseSubstrings();
    MyDoc()->iFilesTree->iRoot->SetSelection4Subtree(EFalse);
    if(iListContainer)
    	iListContainer->AddListElementsL(*MyDoc()->iFilesTree,ETrue);
    
    //display a message, either success or failure
    CArrayFixFlat<TInt> *params=new(ELeave)CArrayFixFlat<TInt>(2);
    CleanupStack::PushL(params);
    TInt successfulMoves=nrMoves-nrFailedMoves;
    params->AppendL(successfulMoves);
    params->AppendL(nrFailedMoves);
    HBufC *text;
    HBufC* header;
    if(nrFailedMoves)
    {
    	text=StringLoader::LoadLC(R_MOVING_SOME_FILES_FAILED,*params);//display an error message
    	header=StringLoader::LoadLC(R_ERROR_HEADER);
    }
    else
    {
    	text=StringLoader::LoadLC(R_FILES_MOVED_SUCCESSFULLY,*params);//display an error message
    	header=StringLoader::LoadLC(R_NOTE_HEADER);
    }
    
	CAknMessageQueryDialog* dlg = new (ELeave)CAknMessageQueryDialog(text,header);//takes ownership of text and header
	CleanupStack::Pop(3,params);
	dlg->ExecuteLD(R_MSG_BIG_NOTE);

    LOG(ELogGeneral,-1,"MoveSelectionHereL: end");
}

void CFFListView::RestartEraseSubstrings()
{
	//iEraseSubstringLength=-1;//this will abort any ongoing operation of erasing substrings
	if(iPeriodic)
	{
		iPeriodic->Cancel();
		delete iPeriodic;
		iPeriodic=NULL;
	};
	ScheduleWorkerL(EJobEraseSubstrings);
}

TBool CFFListView::DeleteSelectedEntriesL(TFileDirEntry &aCurrentEntry)
{
    /* There are several posibilities:
     * -this is a selected file, so we delete it
     * -this is a selected dir, so we go into the children
     * -this is a non-selected dir, we do not delete it
     * -this is a selected dir with zero children (because we never went into it). We delete it.
     */
	LOG0("DeleteSelectedEntriesL")
    if(! (aCurrentEntry.iFlags & TFileDirEntry::EIsSelected) && !(aCurrentEntry.iFlags & TFileDirEntry::EIsPartiallySelected))
        return EFalse; //nothing to delete from here

    //so the element is selected
    if(aCurrentEntry.iFlags & TFileDirEntry::EIsDir)
    {
        //this is a dir
        if(aCurrentEntry.ChildrenCount())
        {
            TInt i;
            TBool wasDeleted(ETrue);
            for(i=0;i<aCurrentEntry.ChildrenCount();i++)
            {
                if(DeleteSelectedEntriesL( (*aCurrentEntry.iChildren)[i] ) )
                {
                    //deletion succeeded, remove the child
                	aCurrentEntry.iChildren->MarkAsDeleted(i);
                    i--;
                }
                else wasDeleted=EFalse;
            };
            if(wasDeleted && !(aCurrentEntry.iFlags&TFileDirEntry::EIsSource) && !(aCurrentEntry.iFlags&TFileDirEntry::EIsSpecial)) //we should not delete sources or a special dir
            {
                //remove the dir as well
                TFileName path;
                aCurrentEntry.GetFullPath(path);

                CFileMan *fm=CFileMan::NewL(iEikonEnv->FsSession());
                TInt err=fm->RmDir(path);
                delete fm;
                if(err)wasDeleted=EFalse;//the delete was not successfull
                else if(&aCurrentEntry==MyDoc()->iFilesTree->iCurrentParent)
                	MyDoc()->iFilesTree->iCurrentParent=aCurrentEntry.GetProperParent(MyDoc()->iFilesTree);
            }
            else
            {
                wasDeleted=EFalse; //we did not removed this dir after all
                if(!aCurrentEntry.ChildrenCount())
                {
                	aCurrentEntry.iFlags&=~TFileDirEntry::EHasSubdirs;
                	aCurrentEntry.iFlags&=~TFileDirEntry::EHasMedia;
                };
            };
            return wasDeleted; //this is EFalse if all children were deleted
        }
        else
            if(!(aCurrentEntry.iFlags&TFileDirEntry::EIsSpecial)) //we should not delete special dirs
            {
                //delete this entire dir
                TFileName path;
                aCurrentEntry.GetFullPath(path);

                CFileMan *fm=CFileMan::NewL(iEikonEnv->FsSession());
                TInt err=fm->RmDir(path);
                delete fm;
                if(err)return EFalse;//the delete was not successfull
                else
                {
                    if(&aCurrentEntry==MyDoc()->iFilesTree->iCurrentParent)
                    	MyDoc()->iFilesTree->iCurrentParent=aCurrentEntry.GetProperParent(MyDoc()->iFilesTree);
                    return ETrue;//delete was successfull
                };
            }
            else
                return EFalse;

    }
    else
    {
        //this is a file, delete it
        TFileName path;
        aCurrentEntry.GetFullPath(path);

        CFileMan *fm=CFileMan::NewL(iEikonEnv->FsSession());
        TInt err=fm->Delete(path);
        delete fm;
        if(err)return EFalse;//the delete was not successfull
        else return ETrue;//delete was successfull
    };
}

void CFFListView::CheckForUpgradeEnded(TInt aStatus)
{
    if(aStatus==ESisDownloadedInstall)
    {
        //ask the user if he/she wants to install now or later
    	TFileName sisFileName(MyDoc()->iPreferences->PrivatePathPlus(KSISFileName));
        TBool installed=InstallNowOrLaterL(sisFileName);
        if(!installed)
        {
            //install later
            //copy the new sis file to a new name
            //(we need to _copy_ it, so the user can send the sis by BT meanwhile)
            CFileMan *fm=CFileMan::NewL(iEikonEnv->FsSession());
            TInt err=fm->Copy(sisFileName,MyDoc()->iPreferences->PrivatePathPlus(KSISFileName2Install));
            delete fm;
            if(err==KErrNone)
            {
                //we will upgrade at application exit or application launch
                iFlags|=EShouldUpgrade;
            };
        };
    };
    //delete the iHTPclient in another active object
    ScheduleWorkerL(EDeleteHTTPclient);
}
void CFFListView::CrashLogfileUploadEnded(TInt aStatus)
{
    //the user was notified already in the HTTP client
    //if status was successful, we delete the file. If status was not successful, then we allow the user to upload again
    if(aStatus)
        iFlags|=EUploadCrashedLogFile;
    else
    {
#if RELEASE_LOG
        TFileName logFilename;
        __ASSERT_ALWAYS(FLLog::instance,Panic(ELogInstanceNull));
        FLLog::instance->GetCrashLogFilename(logFilename);
        iEikonEnv->FsSession().Delete(logFilename);
#endif
    };
    //delete the iHTPclient in another active object
    ScheduleWorkerL(EDeleteHTTPclient);
}

CMLauncherPreferences* CFFListView::Preferences()
{
	return MyDoc()->iPreferences;
}

void CFFListView::SaveSelection()
{
    RFile selection;
    TInt err;
    if(iListContainer)
    	iListContainer->StoreSelection(MyDoc()->iFilesTree);
    
    err=selection.Replace(iEikonEnv->FsSession(),MyDoc()->iPreferences->PrivatePathPlus(KSelectionFileName),EFileWrite);
    if(err!=KErrNone)return;//can not save selection, for some reason

    //write header
    selection.Write(_L8("<?xml version=\"1.0\" encoding=\"UTF-8\"?>"));
    selection.Write(KEOL8);
    //we need to wrap everything into one element
    selection.Write(_L8("<selection>"));
    selection.Write(KEOL8);

    //the selection
    TBuf8<KMaxFileName> temp8FileName; //this is needed to avoid a constant memory allocate-deallocate inside the SaveSelection function
    for(TInt i=0;i<MyDoc()->iFilesTree->iSources.Count();i++)
    	MyDoc()->iFilesTree->iSources[i]->SaveSelection(selection,temp8FileName,0);
    //iSelection->SaveSelection(selection,0);
    
    selection.Write(_L8("</selection>"));
    selection.Write(KEOL8);

    //done writing
    selection.Close();
}

void CFFListView::CheckAndUpgradeL(TBool aCheck)
{
    TBool doUpgrade;
    if(aCheck)
    {
        //we need to check if there is an upgrade to do
        TInt err;
        TEntry entry;
        err=iEikonEnv->FsSession().Entry(MyDoc()->iPreferences->PrivatePathPlus(KSISFileName2Install),entry);
        if(err==KErrNone)doUpgrade=ETrue;
        else doUpgrade=EFalse;
        //check if we upgraded already & rename the sis file
        if(!doUpgrade)
        {
            LOG0("CheckAndUpgradeL: no need to upgrade");
            TFileName sisFileNameInstalling(MyDoc()->iPreferences->PrivatePathPlus(KSISFileNameInstalling));
            err=iEikonEnv->FsSession().Entry(sisFileNameInstalling,entry);
            if(err==KErrNone)
            {
                iEikonEnv->FsSession().Rename(sisFileNameInstalling,MyDoc()->iPreferences->PrivatePathPlus(KSISFileName));
                LOG0("CheckAndUpgradeL: sis file renamed");
            };
        };
    }
    else doUpgrade=ETrue;

    //we should upgrade, but first ask the user!
    if(doUpgrade)
    {
        TBool upgraded=InstallNowOrLaterL(MyDoc()->iPreferences->PrivatePathPlus(KSISFileName2Install));
        if(!upgraded)iFlags|=EShouldUpgrade;
    };
}

TBool CFFListView::InstallNowOrLaterL(const TDesC& aSisFilename)
{
    CAknQueryDialog *installQuery=CAknQueryDialog::NewL();
    CleanupStack::PushL(installQuery);
    HBufC* queryPrompt=StringLoader::LoadLC(R_QUERY_INSTALLNOW);
    installQuery->SetPromptL(*queryPrompt);
    CleanupStack::PopAndDestroy(queryPrompt);
    CleanupStack::Pop(installQuery);
    if(installQuery->ExecuteLD(R_QUERY_CONFIRMATION_NOW_LATER)==ESoftkeyNow)
    {
        //install now!
        //rename the sis file, so we can install it
        iEikonEnv->FsSession().Rename(aSisFilename,MyDoc()->iPreferences->PrivatePathPlus(KSISFileNameInstalling));
        //install the new sis file
        RApaLsSession ls;
        TThreadId tid;
        User::LeaveIfError(ls.Connect());
        ls.GetAllApps();
        ls.StartDocument(MyDoc()->iPreferences->PrivatePathPlus(KSISFileNameInstalling),tid);
        ls.Close();
        return ETrue;
    };
    return EFalse;
}

void CFFListView::UpdateEntriesL()
{
    LOG(ELogGeneral,1,"CMLauncherListView::UpdateEntriesL() start");
    if(iListContainer)
        iListContainer->StoreSelection(MyDoc()->iFilesTree);

    TBool currentFolderNeedsUpdate;
    MyDoc()->iFilesTree->UpdateTree(ETrue,currentFolderNeedsUpdate);

    if(currentFolderNeedsUpdate && iListContainer)
        iListContainer->AddListElementsL(*MyDoc()->iFilesTree,ETrue);
    LOG(ELogGeneral,-1,"CMLauncherListView::UpdateEntriesL() start");
}

void CFFListView::UploadCrashLogFileL()
{
    LOG(ELogGeneral,1,"CMLauncherListView::UploadCrashLogFileL start");
#if RELEASE_LOG
    TFileName logFilename;
    __ASSERT_ALWAYS(FLLog::instance,Panic(ELogInstanceNull));
    FLLog::instance->GetCrashLogFilename(logFilename);
    iFlags&=~EUploadCrashedLogFile;

    //get the size of the crash file
    TEntry entry;
    TInt err;
    err=iEikonEnv->FsSession().Entry(logFilename,entry);
    if(err || entry.iSize==0)
    {
        LOG0("Error (err=%d) getting crash log filesize, or filesize is zero (filesize=%d)",err,entry.iSize);
        return;
    };

    //ask the user to allow file upload.
    CAknQueryDialog *uploadQuery=CAknQueryDialog::NewL();
    CleanupStack::PushL(uploadQuery);
    HBufC* queryPrompt=StringLoader::LoadLC(R_QUERY_UPLOADLOG,entry.iSize/1024+1);
    uploadQuery->SetPromptL(*queryPrompt);
    CleanupStack::PopAndDestroy(queryPrompt);
    CleanupStack::Pop(uploadQuery);
    if(uploadQuery->ExecuteLD(R_QUERY_CONFIRMATION_NOW_LATER)==ESoftkeyNow)
    {
        //upload now!
        if(!iHTTPclient)
        {
            iHTTPclient=CHTTPclient::NewL(*this);
            if(iHTTPclient) //can be NULL, if initialization failed
            	iHTTPclient->UploadCrashedLogFileL(logFilename,entry.iSize);
            else
            {
            	//upload later. This should enable a menu entry in the "Application" menu
            	LOG0("Will upload next time.");
            	iFlags&=~EUploadCrashedLogFile;
            };
        };
    }
    else
    {
        //upload later. This should enable a menu entry in the "Application" menu
        LOG0("Will upload next time.");
        iFlags&=~EUploadCrashedLogFile;
    };
#endif
    LOG(ELogGeneral,-1,"CMLauncherListView::UploadCrashLogFileL end");
}

void CFFListView::FirstDrawDone()
{
	iFlags&=~EWaitForFirstDraw;
	LOG0("First draw done!");
	//check if we have jobs to do
	if(iJobs)
		ScheduleWorkerL(0,EFalse); // 0=no extra jobs
}

void CFFListView::ScheduleWorkerL(TInt aJobs, TBool aWaitForFirstDraw/*=ETrue*/)
{
	iJobs|=aJobs;
	if(aWaitForFirstDraw && (iFlags&EWaitForFirstDraw))
	{
		LOG0("Schedule worker: jobs defered after the first draw");
		return; //will do the jobs after first draw
	};
	MyAppUi()->ScheduleWorkerL(0);
}

TInt CFFListView::IdleWorker()
{
    LOG(ELogGeneral,1,"CMLauncherListView::IdleWorker: start (jobs=%d)",iJobs);
    //TInt err;
    
    //do stuff
   if(iJobs == EJobEraseSubstrings) //we do this only when it is the only job left
    {
	   LOG0("Current job: erase substrings"); 
	   TBool shouldProceed(ETrue); //we make this False if at some point we decide we should not proceed with erase substrings job
        TInt i;

        //check if we have too little or too many children in the current view
        if(MyDoc()->iFilesTree->iCurrentParent->ChildrenCount()>KMaxItemsToEraseSubstrings || MyDoc()->iFilesTree->iCurrentParent->ChildrenCount()<=1)
            shouldProceed=EFalse;
        //check if the view is active or not
        if(!iListContainer)
        	shouldProceed=EFalse;

        if(shouldProceed)
        {
            //check if there is at least one child that is too big (does not fit into the screen)
            //and also check if the iDisplayName is used (if it is used, we abort this operation)
            TBool isTooBig(EFalse);

            CColumnListBoxData *fcld=iListContainer->iFileList->ItemDrawer()->ColumnData();
            TInt columnWidth=fcld->ColumnWidthPixel(1)-fcld->ColumnWidthPixel(2);
            TListItemProperties tlp;
            TInt width(0);
            CFont *font=fcld->Font(tlp,1); //font is not owned
            if(!font)
                shouldProceed=EFalse;

            if(shouldProceed)
            {
                LOG0("IdleWorker: proceeding with erase substrings");
                for(i=0;i<MyDoc()->iFilesTree->iCurrentParent->ChildrenCount();i++)
                {
                    TFileDirEntry &child=(*MyDoc()->iFilesTree->iCurrentParent->iChildren)[i];
                	if(child.iDisplayNameRight)
                    {
                        shouldProceed=EFalse;
                        LOG0("IdleWorker: iDisplayName busy");
                        break;
                    };
                    //TInt width=cdr->ItemWidthInPixels(i);
                    TPtr name(child.iName,child.iNameLength,child.iNameLength);
                    width=font->TextWidthInPixels(name);
                    if(width>columnWidth)
                        isTooBig=ETrue;
                    //width=iListContainer->iFileList->CalcWidthBasedOnNumOfChars(iCurrentParent->iChildren[i]->iName->Length());
                    //sz1=fcld->SubCellSize(i);
                };

                if(!isTooBig)
                {
                    shouldProceed=EFalse;
                    LOG0("IdleWorker: names too small");
                };
            };
        };

        TInt j(0), commonStart(-1),commonLength(0);
        TBool same,end(EFalse);

        if(shouldProceed)
        {
            TInt firstNameLength((*MyDoc()->iFilesTree->iCurrentParent->iChildren)[0].iNameLength);
        	TPtr firstName((*MyDoc()->iFilesTree->iCurrentParent->iChildren)[0].iName,firstNameLength,firstNameLength);
            while(1)
            {
                if(firstName.Length()<=j)break;
                same=ETrue;

                for(i=1;i<MyDoc()->iFilesTree->iCurrentParent->ChildrenCount();i++)
                {
                    TInt nameLength((*MyDoc()->iFilesTree->iCurrentParent->iChildren)[i].iNameLength);
                	TPtr name((*MyDoc()->iFilesTree->iCurrentParent->iChildren)[i].iName,nameLength,nameLength);
                    if(nameLength<=j)
                    {
                        same=EFalse;
                        end=ETrue;
                        break;
                    };
                    if(name[j]!=firstName[j])
                    {
                        same=EFalse;
                        break;
                    };
                };

                //
                if(end)break;
                if(same)
                {
                    //this character was the same for all children
                    if(commonStart<0)
                    {
                        commonStart=j;
                        commonLength=1;
                    }
                    else commonLength++;
                }
                else
                {
                    //not the same character
                    if(commonLength>KMinLenSubstringToErase)
                        break;
                    else
                    {
                        commonStart=-1;
                        commonLength=0;
                    };
                };
                j++;
            };//while

            LOG0("IdleWorker: commonLength=%d commonStart=%d",commonLength,commonStart);
            if(iPeriodic)
                LOG0("IdleWorker: periodic job already exists")
            else
            {
            	//check if it is worth to schedule a substring erase session
            	if(commonLength>KMinLenSubstringToErase)
            	{
            		iEraseSubstringStart=commonStart;
            		iEraseSubstringLength=commonLength;
            		iErasedLength=0;
            		//start the alarm
            		iPeriodic = CPeriodic::NewL(EPriorityLow);
            		TCallBack cb(&PeriodicWorker,this);
            		iPeriodic->Start(KStartEraseSubstringMs,KEraseSubstringIntervalMs,cb);
            		LOG0("IdleWorker: periodic job started");
            	};
            }
        };

        iJobs-=EJobEraseSubstrings;
    }
    else if(iJobs & EDeleteHTTPclient)
    {
    	LOG0("Current job: delete HTTP client"); 
    	//we just need to delete the iHTTPinstance
    	if(!iHTTPclient->InUse())
    	{
    		delete iHTTPclient;
    		iHTTPclient=NULL;
    	};
    	iJobs-=EDeleteHTTPclient;
    }
    

    //done
    if(iJobs)
    {
        LOG(ELogGeneral,-1,"CMLauncherListView::IdleWorker: function end, still stuff to do");
        return 1; //more stuff to do
    }
    else
    {
        LOG(ELogGeneral,-1,"CMLauncherListView::IdleWorker: end, nothing to do");
        return 0; //no more work to do
    };
}


TInt CFFListView::PeriodicWorker(TAny *aInstance)
{
    LOG(ELogGeneral,1,"CMLauncherListView::PeriodicWorker: start");
    CFFListView *listView=(CFFListView*)aInstance;

    if(listView->iEraseSubstringLength<0)
    {
        //we abort this, the view is changed
        delete listView->iPeriodic;
        listView->iPeriodic=NULL;
        LOG(ELogGeneral,-1,"CMLauncherListView::PeriodicWorker: end (aborted), nothing to do");
        return 0; //no more work to do
    };

    //erase one character
    TInt i;
    listView->iErasedLength++;
    for(i=0;i<listView->MyDoc()->iFilesTree->iCurrentParent->ChildrenCount();i++)
    {
        TFileDirEntry &currentChild=(*listView->MyDoc()->iFilesTree->iCurrentParent->iChildren)[i];
        currentChild.iDisplayNameLeft=listView->iEraseSubstringStart;
        currentChild.iDisplayNameRight=currentChild.iNameLength-listView->iEraseSubstringStart-listView->iErasedLength;
    };
    //update
    //get the position of the current selection
    if(listView->iListContainer)
    {
    	//listView->iCurrentParent->iSelectedPosition=listView->iListContainer->iFileList->CurrentItemIndex();
    	listView->iListContainer->StoreSelection(listView->MyDoc()->iFilesTree,EFalse);
    	listView->iListContainer->AddListElementsL(*listView->MyDoc()->iFilesTree,ETrue);
    };

    //done
    if(listView->iErasedLength<listView->iEraseSubstringLength)
    {
        LOG(ELogGeneral,-1,"CMLauncherListView::PeriodicWorker: function end, still stuff to do");
        return 1; //more stuff to do
    }
    else
    {
        delete listView->iPeriodic;
        listView->iPeriodic=NULL;
        LOG(ELogGeneral,-1,"CMLauncherListView::PeriodicWorker: end, nothing to do");
        return 0; //no more work to do
    };
}

// End of File
