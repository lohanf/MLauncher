/*
============================================================================
 Name        : MLauncherDocument.cpp
 Author      : Florin Lohan
 Copyright   : Florin Lohan
 Description : CMLauncherDocument implementation
============================================================================
 */


// INCLUDE FILES
#include <stringloader.h>
#include <swinstapi.h>
#include <MLauncher.rsg>
#include "MLauncherAppUi.h"
#include "MLauncherDocument.h"
#include "XMLparsing.h"
#include "TrackInfo.h"
#include "MLauncherPreferences.h"
#include "log.h"
#include "MLauncher.pan"

_LIT(KUnwantedSource1,":\\cities\\diskcache\\voices");
_LIT(KUnwantedSource2,"c:\\resource");

// ============================ MEMBER FUNCTIONS ===============================

// -----------------------------------------------------------------------------
// CMLauncherDocument::NewL()
// Two-phased constructor.
// -----------------------------------------------------------------------------
//
CMLauncherDocument* CMLauncherDocument::NewL( CEikApplication& aApp )
{
	CMLauncherDocument* self = NewLC( aApp );
	CleanupStack::Pop( self );
	return self;
}

// -----------------------------------------------------------------------------
// CMLauncherDocument::NewLC()
// Two-phased constructor.
// -----------------------------------------------------------------------------
//
CMLauncherDocument* CMLauncherDocument::NewLC( CEikApplication& aApp )
{
	CMLauncherDocument* self = new ( ELeave ) CMLauncherDocument( aApp );
	CleanupStack::PushL( self );
	self->ConstructL();
	return self;
}

// -----------------------------------------------------------------------------
// CMLauncherDocument::ConstructL()
// Symbian 2nd phase constructor can leave.
// -----------------------------------------------------------------------------
//
void CMLauncherDocument::ConstructL()
{
	iFs.Connect(10);
	CFileDirPool::iFs=Xml::CPlsSelectionCH::iFs=Xml::CPreferencesCH::iFs=&iFs;

	//start logging
	iCrashLogFound=LOG_START(&iFs,0xFFFFFFFF-ELogNewSources/*-ELogBT*/);

	LOG(ELogGeneral,1,"CMLauncherDocument::ConstructL++");
	//load preferences
	iPreferences=CMLauncherPreferences::NewL(iFs,&iPlaylists); 
	iPreferences->SetPrivatePath(Application()->AppFullName()[0]);
#ifndef __WINS__
	iPreferences->SetPrivatePath(Application()->AppFullName()[0]);
#else
	iPreferences->SetPrivatePath('c');
#endif
	iPreferences->LoadPreferences();
	//we know that we will start searching for music, so we put the flag up right now, so if there is no music
	//when MLauncher starts, for the first 1-2 seconds the user sees "searching for music" instead of "no music found"
	iPreferences->iCFlags|=CMLauncherPreferences::EMusicSearchInProgress;

	//set the current playlist, if any
	TInt i;
	for(i=0;i<iPlaylists.Count();i++)
		if(iPlaylists[i]->IsCurrent())
		{
			iCurrentPlaylist=iPlaylists[i];
			break;
		};
	
	//construct the starting folder list
	TUint flags(TFileDirEntry::EIsDir|TFileDirEntry::EIsSource);
	TFileDirEntry *source;
	iFilesTree=CFileDirPool::NewL();
	for(i=0;i<iPreferences->iStartingDirs.Count();i++)
	{
		LOG0("Source : %S",iPreferences->iStartingDirs[i]);
		if(iPreferences->iStartingDirsNrFiles[i]&CMLauncherPreferences::EFlagSourceIsAudiobook)
			source=iFilesTree->NewFileDirEntry(*iPreferences->iStartingDirs[i],flags|TFileDirEntry::EAudiobookSource,NULL); //if parent is NULL, it must be a source
		else
			source=iFilesTree->NewFileDirEntry(*iPreferences->iStartingDirs[i],flags,NULL); //if parent is NULL, it must be a source
		LOG0("Before MakeChildren");
		iFilesTree->MakeChildren(source);
		LOG0("After MakeChildren");
	};

	if(iPreferences->iPFlags&CMLauncherPreferences::EPreferencesKeepSelection)
	{
		//parse the selection file
		Xml::CPlsSelectionCH *plsSel=Xml::CPlsSelectionCH::NewLC();
		plsSel->ParseSelectionL(iPreferences->PrivatePathPlus(KSelectionFileName),iFilesTree);
		CleanupStack::PopAndDestroy(plsSel);
	};

	AddVirtualFoldersL();
	LOG(ELogGeneral,-1,"CMLauncherDocument::ConstructL--");
}

// -----------------------------------------------------------------------------
// CMLauncherDocument::CMLauncherDocument()
// C++ default constructor can NOT contain any code, that might leave.
// -----------------------------------------------------------------------------
//
CMLauncherDocument::CMLauncherDocument( CEikApplication& aApp ) : CAknDocument( aApp )
  {
	// No implementation required
  }

// ---------------------------------------------------------------------------
// CMLauncherDocument::~CMLauncherDocument()
// Destructor.
// ---------------------------------------------------------------------------
//
CMLauncherDocument::~CMLauncherDocument()
{
	LOG(ELogGeneral,1,"CMLauncherDocument::~CMLauncherDocument++");
	LOG0("Deleting iPreferences");
	delete iPreferences;iPreferences=NULL;
	LOG0("Deleting iPlaylists");
	iPlaylists.ResetAndDestroy();
	LOG0("Deleting iTempPlaylist");
	delete iTempPlaylist;
	iTempPlaylist=iCurrentPlaylist=NULL; //not really necessary
	LOG0("Deleting iFilesTree");
	delete iFilesTree;iFilesTree=NULL;
	LOG0("Deleting iNewTree (%x, should be NULL already, if new source discovery is complete)",iNewTree);
	delete iNewTree;iNewTree=NULL;
	LOG0("Deleting discovered sources and source counts");
	iSources.ResetAndDestroy();
	iSourcesCount.Close();
	iGlobalDisplayNames.ResetAndDestroy();
	LOG(ELogGeneral,-1,"CMLauncherDocument::~CMLauncherDocument--");
}

// ---------------------------------------------------------------------------
// CMLauncherDocument::CreateAppUiL()
// Constructs CreateAppUi.
// ---------------------------------------------------------------------------
//
CEikAppUi* CMLauncherDocument::CreateAppUiL()
{
	// Create the application user interface, and return a pointer to it;
	// the framework takes ownership of this object
	return ( static_cast <CEikAppUi*> ( new (ELeave)CMLauncherAppUi ) );
}


///////////////////// Own functions
void CMLauncherDocument::AddVirtualFoldersL()
{
    //Add virtual folders (in the beginning of the list)
    //Received files
	TFileName path;
	TFileName displayPath;
	TInt i,err,flags;
	HBufC *displayName;
	TFileDirEntry* vf;

	__ASSERT_ALWAYS(iPreferences->iStartingDirs.Count()<=iFilesTree->iSources.Count(),Panic(EMoreStartingDirsThanSources));
	
    for(i=0;i<iPreferences->iStartingDirs.Count();i++)
    {
    	//get the potential path for the Received Files folder
    	iPreferences->GetRecvFilesDirFullPath(path,i);
    	//check if it is empty or not
    	CDir *dir=NULL;
    	err=iFs.GetDir(path,KEntryAttDir,ESortByName|EDirsFirst,dir);
    	if(err==KErrNone)
    	{
    		//folder exists and operation was successful
    		if(dir->Count())
    		{
    			//we have stuff in the folder, we need to add it.
    			flags=TFileDirEntry::EIsDir|TFileDirEntry::EIsSpecial;
    			iFilesTree->iSources[i]->GetFullPath(displayPath,EFalse);
    			displayName=StringLoader::LoadLC(R_RECEIVED_FILES_DISPLAY_NAME,displayPath);
    			iGlobalDisplayNames.Append(displayName);
    			vf=iFilesTree->NewFileDirEntry(iPreferences->GetRecvFilesDir(),flags,iFilesTree->iSources[i],0);
    			vf->iGlobalDisplayNameIndex=iGlobalDisplayNames.Count()-1;
    			CleanupStack::Pop(displayName);
    		}
    		else
    		{
    			//folder does not contain anything, we should delete it.
    			CMLauncherPreferences::RemoveEndingBackslash(path);
    			err=iFs.Delete(path);
    		};
    	};
    	delete dir;

    }
    /* old code for creating the folder
    //create the dir, if not created already
    TFileName rcvFilesDir;
    iPreferences->GetRecvFilesDirFullPath(rcvFilesDir);
    iEikonEnv->FsSession().MkDirAll(rcvFilesDir);
    */
    
    //artists and albums
    if(iPreferences->iPFlags&CMLauncherPreferences::EPreferencesUseMetadata)
    {
    	if(iFilesTree->iArtistsHolder)
    	{
    		//we already have the iArtistsHolder allocated. Add it to root for display
    		__ASSERT_ALWAYS(iFilesTree->iArtistsHolder->iParent==iFilesTree->iRoot,Panic(EArtistsHolderParentNotRoot));
    	}
    	else
    	{
    		flags=TFileDirEntry::EIsDir|TFileDirEntry::EIsSpecial|TFileDirEntry::EIsMetadata|TFileDirEntry::EWasExpanded;
    		displayName=StringLoader::LoadLC(R_ARTISTS_ALBUMS_DISPLAY_NAME);
    		iFilesTree->iArtistsHolder=iFilesTree->NewFileDirEntry(*displayName,flags,iFilesTree->iRoot,0);
    		CleanupStack::PopAndDestroy(displayName);
    	};
    }
}

CPlaylist* CMLauncherDocument::CreateTemporaryPlaylistL(TFileDirEntry *aEntry, TBool aTransferOwnership/*=EFalse*/)
{
	if(iTempPlaylist)
		iTempPlaylist->Recycle();
	else
		iTempPlaylist=new (ELeave)CPlaylist(KPointerArrayPlaylistGranularity);
	iTempPlaylist->Initialize(iFilesTree,-1);
	
	TBool hasOggOrFlacs(ETrue);
	if(aEntry)
	{
		if(!(aEntry->iFlags&TFileDirEntry::EIsDir) && !(aEntry->iFlags&TFileDirEntry::EIsSelected))
		{
			//this is a file and it is not selected, it must be "preview this song". 
			//We temporarily select the file, otherwise it will not be added to the playlist
			aEntry->iFlags|=TFileDirEntry::EIsSelected;
			iTempPlaylist->AddToPlaylist(aEntry,hasOggOrFlacs);
			aEntry->iFlags&=~TFileDirEntry::EIsSelected;
		}
		else
			iTempPlaylist->AddToPlaylist(aEntry,hasOggOrFlacs);
	}
		
	if(aTransferOwnership)
	{
		CPlaylist *tempPlaylist=iTempPlaylist;
		iTempPlaylist=NULL;
		return tempPlaylist;
	}
	return iTempPlaylist;
}

CPlaylist* CMLauncherDocument::BuildCurrentPlaylistL()
{
	//first, we need to select a playlist
	TInt i;
	LOG(ELogGeneral,1,"CMLauncherAppUi::BuildCurrentPlaylistL++");
	if(iCurrentPlaylist)
	{
		iCurrentPlaylist->SetCurrent(EFalse);
		iCurrentPlaylist=NULL;
	};
	//first, try to select an existing playlist with zero elements
	for(i=0;i<iPlaylists.Count();i++)
	{
		__ASSERT_ALWAYS(iPlaylists[i],Panic(EPlaylistIsNull));
		if(!iPlaylists[i]->NrElementsWhenLoaded())
		{
			iCurrentPlaylist=iPlaylists[i];
			iCurrentPlaylist->Recycle();
			break;
		};
	};
	if(!iCurrentPlaylist && iPlaylists.Count()<KMaxNrPlaylists)
	{
		//add a new playlist
		iCurrentPlaylist=new (ELeave)CPlaylist(KPointerArrayPlaylistGranularity);
		iCurrentPlaylist->Initialize(iFilesTree,iPlaylists.Count());
		iPlaylists.AppendL(iCurrentPlaylist);
	};
	if(!iCurrentPlaylist)
		for(i=0;i<iPlaylists.Count();i++) //select the first unnamed playlist
		{
			__ASSERT_ALWAYS(iPlaylists[i],Panic(EPlaylistIsNull));
			if(!iPlaylists[i]->iName)
			{
				iCurrentPlaylist=iPlaylists[i];
				iCurrentPlaylist->Recycle();
				break;
			};
		};
	if(!iCurrentPlaylist)
	{
		//create a temporary playlist
		iCurrentPlaylist=CreateTemporaryPlaylistL(NULL);
		iCurrentPlaylist->Initialize(iFilesTree,-1);
	}
	else
		iCurrentPlaylist->Initialize(iFilesTree,i);
	__ASSERT_ALWAYS(iCurrentPlaylist,Panic(ECurrentPlaylistIsNull));
	iCurrentPlaylist->SetCurrent(ETrue);
	
	//we suppose the current playlist will be audiobook. If not, the AddToPlaylist will change the type.
	iCurrentPlaylist->iIsAudiobook=ETrue;
	//we have a current playlist, now add elements to it
	TBool hasOggsOrFlacs(EFalse);
	iCurrentPlaylist->AddToPlaylist(iFilesTree->iRoot,hasOggsOrFlacs);
	if(hasOggsOrFlacs && !(TFileDirEntry::iStaticFlags&TFileDirEntry::ESupportsOggAndFlac))
	{
		//we have Ogg or FLAC files, but we do not have the plugin installed
		TBool removeOggsAndFlacs(EFalse);
		//lets check if the plugin exists
		TEntry entry;
		TInt err=iFs.Entry(iPreferences->PrivatePathPlus(KOggPlayControllerSisx),entry);
		if(!err)
		{
			//we have the Plugin in our private path
			if( ((CMLauncherAppUi*)iAppUi)->DisplayQueryOrNoteL(EMsgQueryYesNo,R_QUERY_INSTALL_OGG_FLAC_PLUGIN) )
			{
				//install the plugin
				SwiUI::RSWInstLauncher swi;
				swi.Connect();
				err=swi.Install(iPreferences->PrivatePathPlus(KOggPlayControllerSisx)); //this waits until the SW installation is over
				swi.Close();
				if(err)removeOggsAndFlacs=ETrue;//installation was NOT successful
				else TFileDirEntry::iStaticFlags|=TFileDirEntry::ESupportsOggAndFlac;
			}
			else removeOggsAndFlacs=ETrue;
		}
		else 
		{
			((CMLauncherAppUi*)iAppUi)->DisplayQueryOrNoteL(EMsgWarningPersistent,R_QUERY_OGG_FLAC_PLUGIN_NOT_FOUND);
			removeOggsAndFlacs=ETrue;
		}
		if(removeOggsAndFlacs)
			iCurrentPlaylist->RemoveOggAndFlacEntries();
	}
	//check if the autoname is valid
	if(iCurrentPlaylist->iName && iCurrentPlaylist->iName->Length()==0)
	{
		//the name is not valid, lets remove it
		delete iCurrentPlaylist->iName;
		iCurrentPlaylist->iName=NULL;
	}
	LOG(ELogGeneral,-1,"CMLauncherAppUi::BuildCurrentPlaylistL--");
	return iCurrentPlaylist;
}

CPlaylist* CMLauncherDocument::SetPlaylistAsCurrent(TInt aPlaylistIndex)
{
	TInt i;
	for(i=0;i<iPlaylists.Count();i++)
		iPlaylists[i]->SetCurrent(EFalse);
	iPlaylists[aPlaylistIndex]->SetCurrent(ETrue);
	iCurrentPlaylist=iPlaylists[aPlaylistIndex];
	return iCurrentPlaylist;
}

void CMLauncherDocument::DeletePlaylist(TInt aPlaylistIndex)
{
	if(iCurrentPlaylist==iPlaylists[aPlaylistIndex])
	{
		iCurrentPlaylist=NULL;
	}
	iPlaylists[aPlaylistIndex]->RemoveFromDisk(iFs);
	delete iPlaylists[aPlaylistIndex];
	iPlaylists.Remove(aPlaylistIndex);
	//for all playlist after this one, we need to decrease the index and also rename their file
	TInt i;
	for(i=aPlaylistIndex;i<iPlaylists.Count();i++)
		iPlaylists[i]->DecrementIndex(i,iFs);
}

void CMLauncherDocument::RenamePlaylist(TInt aPlaylistIndex, TDesC& aName)
{
	if(!iPlaylists[aPlaylistIndex]->iName || iPlaylists[aPlaylistIndex]->iName->Length()<aName.Length())
	{
		delete iPlaylists[aPlaylistIndex]->iName;
		iPlaylists[aPlaylistIndex]->iName=HBufC::NewL(aName.Length());
	}
	iPlaylists[aPlaylistIndex]->iName->Des().Copy(aName);
}

TInt CMLauncherDocument::DiscoverNewSources()
{
	LOG(ELogNewSources,1,"DiscoverNewSources++");
	if(!iNewTree)
	{
		iNewTree=CFileDirPool::NewL();
		if(iPreferences->iStartingDirs.Count()==0)
			iNewTree->iFlags|=CFileDirPool::EGetNrMusicFilesFast;
		TInt flags(TFileDirEntry::EIsDir|TFileDirEntry::EIsSource);
		TFileName path;
		path.Copy(KEDrive);
		iCurrentDrive='e';
		iNewTree->iCurrentParent=iNewTree->NewFileDirEntry(path,flags,NULL); //if parent is NULL, it must be a source
	}
	iNewTree->GetNrMusicFiles_ActiveIdleModeL();
	if(!iNewTree->iCurrentParent)
	{
		LOG0("We finished parsing drive %c",(TInt)iCurrentDrive);
		//we finished parsing this drive
		TFileName path;
		switch(iCurrentDrive)
		{
		case 'e':path.Copy(KEDrive);iCurrentDrive='f';break;
		case 'f':path.Copy(KFDrive);iCurrentDrive='c';break;
		case 'c':path.Copy(KCDrive);iCurrentDrive='0';break;
		default:Panic(EUnknownDriveLetter);
		}
		iNewTree->iSources[0]->GetStartingDirL(iSources,iSourcesCount,path);
		iNewTree->Recycle();

		LOG0("Current drive here: %c",(TInt)iCurrentDrive);
		if(iCurrentDrive!='0')
		{
			//we start a new count
			TInt flags(TFileDirEntry::EIsDir|TFileDirEntry::EIsSource);
			switch(iCurrentDrive)
			{
			case 'f':path.Copy(KFDrive);break;
			case 'c':path.Copy(KCDrive);break;
			default:Panic(EUnknownDriveLetter);
			}
			iNewTree->iCurrentParent=iNewTree->NewFileDirEntry(path,flags,NULL); //if parent is NULL, it must be a source
		}
		else
		{
			//we are done with getting sources
			delete iNewTree;iNewTree=NULL;
			LOG(ELogNewSources,-1,"DiscoverNewSources-- (returning 1 or 2)");
			return 1+(AddToExistingSourcesL()?1:0);
		}
	}
	LOG(ELogNewSources,-1,"DiscoverNewSources-- (returning 0)");
	return 0;
}

TBool CMLauncherDocument::AddToExistingSourcesL()
{
	LOG(ELogGeneral,1,"AddToExistingSourcesL(): start (%d sources)",iSources.Count());
	__ASSERT_ALWAYS(iSources.Count()==iSourcesCount.Count(),Panic(ESourcesAndSourcesCountDifferent));
	TInt i;
	//filter our sources for "unwanted" folders
	for(i=0;i<iSources.Count();i++)
	{
		if(iSources[i]->Length()>=KUnwantedSource1().Length()+1)
		{
			TFileName src(iSources[i]->Des().Mid(1,KUnwantedSource1().Length()));
			src.LowerCase();
			if(src==KUnwantedSource1)
			{
				//remove this source
				delete iSources[i];
				iSources.Remove(i);
				iSourcesCount.Remove(i);
				i--;
				continue;
			}
		}
		if(iSources[i]->Length()>=KUnwantedSource2().Length())
		{
			TFileName src(iSources[i]->Des().Left(KUnwantedSource2().Length()));
			src.LowerCase();
			if(src==KUnwantedSource2)
			{
				//remove this source
				delete iSources[i];
				iSources.Remove(i);
				iSourcesCount.Remove(i);
				i--;
				continue;
			}
		}
	};
	//done removing unwanted sources
	TBool savePreferences(EFalse);
	if(iSources.Count())
	{
		//try to match the new sources against our already existing sources
		TInt j;
		for(i=0;i<iSources.Count();i++)
		{
			TBool found(EFalse);
			TPtr path(iSources[i]->Des());
			CMLauncherPreferences::RemoveEndingBackslash(path);//sources in preferences do not have trailing slashes
			for(j=0;j<iPreferences->iStartingDirs.Count();j++)
			{
				LOG0("Testing potential source %S with source %S",iSources[i],iPreferences->iStartingDirs[j]);
				if(*(iSources[i])== *(iPreferences->iStartingDirs[j]))
				{
					found=ETrue;
					//update files count before removing
					//__ASSERT_ALWAYS(iSourcesCount[i]<0xFFFFFF,); //24 bits only for the sources count, the rest are for flags	
					iPreferences->iStartingDirsNrFiles[j]=(iPreferences->iStartingDirsNrFiles[j]&0xFF000000)+iSourcesCount[i];
					delete iSources[i];
					iSources.Remove(i);
					iSourcesCount.Remove(i);
					i--;
					break;
				};
			};
			if(!found) for(j=0;j<iPreferences->iBlacklistDirs.Count();j++)
			{
				LOG0("Testing potential source %S with blacklisted folder %S",iSources[i],iPreferences->iBlacklistDirs[j]);
				if(*(iSources[i])== *(iPreferences->iBlacklistDirs[j]))
				{
					found=ETrue;
					//update files count before removing
					iPreferences->iBlacklistDirsNrFiles[j]=iSourcesCount[i];
					delete iSources[i];
					iSources.Remove(i);
					iSourcesCount.Remove(i);
					i--;
					break;
				};
			};

			if(!found) for(j=0;j<iPreferences->iStartingDirs.Count();j++)
			{
				TFileName startingDir(*(iPreferences->iStartingDirs[j]));
				startingDir.Append(KDirBackslash);
				if(iSources[i]->Find(startingDir)==0)
				{
					//the new source string contains one of the existing sources substrings.
					//this means that the existing source folder is a parent of the new source
					//ACTION: we replace the existing source with the new source
					LOG0("Existing source %S is a parent of %S",iPreferences->iStartingDirs[j],iSources[i]);
					iFilesTree->MarkSourceAsDeleted(*iPreferences->iStartingDirs[j]);
					delete iPreferences->iStartingDirs[j];
					iPreferences->iStartingDirs[j]=iSources[i];
					//__ASSERT_ALWAYS(iSourcesCount[i]<0xFFFFFF,); //24 bits only for the sources count, the rest are for flags	
					iPreferences->iStartingDirsNrFiles[j]=(iPreferences->iStartingDirsNrFiles[j]&0xFF000000)+iSourcesCount[i];
					AddANewSource(j);
					found=ETrue;
					savePreferences=ETrue;
					iSources.Remove(i);
					iSourcesCount.Remove(i);
					i--;
					break;
				}
				else
				{
					TFileName sourceDir(*(iSources[i]));
					sourceDir.Append(KDirBackslash);
					if(iPreferences->iStartingDirs[j]->Find(sourceDir)==0)
					{
						//the new source string is contained in one of the existing source strings
						//this means that the new source is a parent of the existing source
						//ACTION: we replace the existing source with the new source
						LOG0("New source %S is a parent of %S",iSources[i],iPreferences->iStartingDirs[j]);
						iFilesTree->MarkSourceAsDeleted(*iPreferences->iStartingDirs[j]);
						delete iPreferences->iStartingDirs[j];
						iPreferences->iStartingDirs[j]=iSources[i];
						//__ASSERT_ALWAYS(iSourcesCount[i]<0xFFFFFF,); //24 bits only for the sources count, the rest are for flags	
						iPreferences->iStartingDirsNrFiles[j]=(iPreferences->iStartingDirsNrFiles[j]&0xFF000000)+iSourcesCount[i];
						AddANewSource(j);
						found=ETrue;
						savePreferences=ETrue;
						iSources.Remove(i);
						iSourcesCount.Remove(i);
						i--;
						break;
					};
				};
			};

			if(!found)
			{
				//we add this source 
				TChar drive=iSources[i]->Des()[0];
				if(drive=='c' || drive=='C')
				{
					LOG0("Keeping source: %S [%d] but because it is on C drive, we add it to blacklisted sources.",iSources[i],iSourcesCount[i]);
					iPreferences->AddBlacklistedDirL(iSources[i],iSourcesCount[i]|CMLauncherPreferences::EFlagSourceAddedNow);//ownership transfer
				}
				else
				{
					LOG0("Keeping source: %S [%d]",iSources[i],iSourcesCount[i]);
					iPreferences->AddStartingDirL(iSources[i],iSourcesCount[i]);//ownership transfer
					AddANewSource(iPreferences->iStartingDirs.Count()-1);
				}
				savePreferences=ETrue;
			};
		};//for
		//close remaining stuff
		iSources.Reset();//all remaining pointers have been transfered to iPreferences
		iSourcesCount.Reset();
		//check if we only have newly added blacklisted C: paths
		if(!iPreferences->iStartingDirs.Count())
		{
			//we have no starting dirs, so we will convert to starting dirs the newly added C: blacklisted dirs
			for(i=0;i<iPreferences->iBlacklistDirsNrFiles.Count();i++)
				if(iPreferences->iBlacklistDirsNrFiles[i]&CMLauncherPreferences::EFlagSourceAddedNow)
				{
					iPreferences->AddStartingDirL(iPreferences->iBlacklistDirs[i],iPreferences->iBlacklistDirsNrFiles[i]&~CMLauncherPreferences::EFlagSourceAddedNow);
					iPreferences->iBlacklistDirs.Remove(i);
					iPreferences->iBlacklistDirsNrFiles.Remove(i);
					i--;
					savePreferences=ETrue;
					AddANewSource(iPreferences->iStartingDirs.Count()-1);
				};
		}
		else //lets just remove the EFlagSourceAddedNow flag
			for(i=0;i<iPreferences->iBlacklistDirsNrFiles.Count();i++)
				iPreferences->iBlacklistDirsNrFiles[i]&=~CMLauncherPreferences::EFlagSourceAddedNow;
	};
	if(savePreferences)
		iPreferences->SavePreferences();

	LOG(ELogGeneral,-1,"AddToExistingSourcesL(): end (total starting dirs: %d)",iPreferences->iStartingDirs.Count());
	return savePreferences;
}

void CMLauncherDocument::AddANewSource(TInt aSourceIndex, TUint extraFlags/*=0*/)
{
	LOG(ELogGeneral,1,"AddANewSource++");
	TUint flags(TFileDirEntry::EIsDir|TFileDirEntry::EIsSource|extraFlags);
	if(iPreferences->iStartingDirsNrFiles[aSourceIndex]&CMLauncherPreferences::EFlagSourceIsAudiobook)
		flags|=TFileDirEntry::EAudiobookSource;
	TFileDirEntry *source=iFilesTree->NewFileDirEntry(*iPreferences->iStartingDirs[aSourceIndex],flags,NULL); //if parent is NULL, it must be a source
	LOG0("Before MakeChildren");
	iFilesTree->MakeChildren(source);
	LOG(ELogGeneral,-1,"AddANewSource-- (iRoot nrChildren: %d)",iFilesTree->iRoot->ChildrenCount());
}

void CMLauncherDocument::SourcesChanged()
{
	//this function is called when sources in preferences have been changed. What we need to do: 
	//-for each starting dir in iPreferences, check if there is a corresponding source in iFilesTree. 
	//-If it does, mark it. If it does not, add it.
	//-unmarked sources from iFilesTree are to be deleted
	//Then:
	//-for all playlists, check for deleted elements from each playlist.
	LOG(ELogGeneral,1,"SourcesChanged++");
	TInt i,j;
	TBool checkSubfolders(EFalse),sourcesDeletedOrUndeleted(EFalse);
	for(i=0;i<iPreferences->iStartingDirs.Count();i++)
	{
		LOG0("Checking %S",iPreferences->iStartingDirs[i]);
		for(j=0;j<iFilesTree->iSources.Count();j++)
		{
			TPtrC srcName(iFilesTree->iSources[j]->iName,iFilesTree->iSources[j]->iNameLength);
			LOG0("Comparing with %S",&srcName);
			if(!iPreferences->iStartingDirs[i]->Compare(srcName))
			{
				//we found it!
				LOG0("Found!");
				iFilesTree->iSources[j]->iFlags|=TFileDirEntry::EMarked;
				iFilesTree->iSources[j]->UnmarkAsDeleted();
				sourcesDeletedOrUndeleted=ETrue;
				if(!(iFilesTree->iSources[j]->iFlags&TFileDirEntry::EWasExpanded))
				{
					iFilesTree->MakeChildren(iFilesTree->iSources[j]);
					checkSubfolders=ETrue;
				}
				break;
			}
		}
		//check if we found it
		if(j>=iFilesTree->iSources.Count())//we did not found the current starting dir, we need to add it
		{
			AddANewSource(i,TFileDirEntry::EMarked);
			checkSubfolders=ETrue;
		}
	}//for(i=0;...
	
	//delete all unmarked sources
	for(j=0;j<iFilesTree->iSources.Count();j++)
		if(!(iFilesTree->iSources[j]->iFlags&TFileDirEntry::EMarked))
		{
			iFilesTree->iSources[j]->MarkAsDeleted();
			sourcesDeletedOrUndeleted=ETrue;
		}
		else //remove the mark 
			iFilesTree->iSources[j]->iFlags&=~TFileDirEntry::EMarked;
	//if we have sources unmarked, review the iRoot element
	if(sourcesDeletedOrUndeleted)
		iFilesTree->iRoot->ReviewChildren();

	if(checkSubfolders)
		TRAP_IGNORE(((CMLauncherAppUi*)iAppUi)->ScheduleWorkerL(CMLauncherAppUi::EJobSubfolders));
	//check for deleted elements from each playlist
	for(i=0;i<iPlaylists.Count();i++)
	{
		iPlaylists[i]->CheckForDeletedElements();
		if(iCurrentPlaylist==iPlaylists[i] && !iCurrentPlaylist->Count())
			((CMLauncherAppUi*)iAppUi)->CloseMusicPlayerViewL();
	}
			
	LOG(ELogGeneral,-1,"SourcesChanged--");
}

// End of File
