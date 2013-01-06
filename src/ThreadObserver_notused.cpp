/*
 * ThreadObserver_notused.cpp
 *
 *  Created on: Nov 19, 2011
 *      Author: Florin
 */

#ifdef 0

////////////////////////////////////////////////////////////


CThreadObserver::CThreadObserver(CMLauncherAppUi *aAppUi) : CActive(CActive::EPriorityStandard), iShouldExit(EFalse), iAppUi(aAppUi)
{
	CActiveScheduler::Add(this);
}

CThreadObserver::~CThreadObserver()
{
	if(IsActive())
		Cancel();
	iSources.ResetAndDestroy();
	iSourcesCount.Close();
	//we do not own iListView
	delete iCleanupStack;
}

TInt CThreadObserver::LaunchThread()
{
	LOG(ELogGeneral,1,"CThreadObserver::LaunchThread start");
	/*
	if(LOG_ENABLED_FLAG(ELogNewSources))
	{
		GetNewSourcesL();
		AddToExistingSourcesL();
		ParseMetadataL();
	}
	else*/
	
	TBuf<16> thrname(KThreadName);
	TInt err=iThread.Create(thrname,&ThreadFunction,32768,NULL,this);
	if(err)
	{
		iAppUi->ScheduleWorkerL(CMLauncherAppUi::EJobDiscoverSourcesAndParseMetadata);
		LOG(ELogGeneral,-1,"CThreadObserver::LaunchThread end (thread creation error. Will create later)");
		return err;
	};
	//create queue
	LOG(ELogGeneral,0,"Thread created successfully!");
	iMutex.CreateLocal();
	iMutex.Wait(); //will be signaled after sources are added so that metadata parsing may continue
	iQueue.CreateLocal(5,EOwnerProcess);
	iQueue.NotifyDataAvailable(iStatus);
	SetActive();
	iThread.SetPriority(EPriorityLess);
	iThread.Resume();
	
	LOG(ELogGeneral,-1,"CThreadObserver::LaunchThread end");
	return KErrNone;
}

TInt CThreadObserver::ThreadFunction(TAny *aInstance)
{
	CThreadObserver *threadObserver=(CThreadObserver*)aInstance;
	//Add cleanup stack support.
	if(threadObserver->iCleanupStack)delete threadObserver->iCleanupStack;
	threadObserver->iCleanupStack = CTrapCleanup::New();
	TInt err;
	TRAP(err,threadObserver->GetNewSourcesL());
	LOG(ELogNewSources,0,"ThreadFunction: waiting for mutex");
	threadObserver->iMutex.Wait(); //we suspend the activity until the sources are added
	LOG(ELogNewSources,0,"ThreadFunction: done waiting");
	if(threadObserver->iAppUi->iPreferences->iFlags&CMLauncherPreferences::EPreferencesUseMetadata)
		TRAP(err,threadObserver->ParseMetadataL());
	threadObserver->iMutex.Signal(); 
	delete threadObserver->iCleanupStack;
	threadObserver->iCleanupStack=NULL;
	RThread().Kill(err);
	return KErrNone;//value discarded
}

void CThreadObserver::GetNewSourcesL()
{
	//create a list of choices
	TFileDirEntry *source;//not owned
	TFileName path;
	TInt flags(TFileDirEntry::EIsDir|TFileDirEntry::EIsSource);
	TBool fast(EFalse);
	if(iAppUi->iPreferences->iStartingDirs.Count()==0)
		fast=ETrue; //there is nothing to show to the user, so we need to hurry!
	LOG(ELogNewSources,1,"MLauncher_src: GetNewSourcesL() start");

	//for E:
	LOG(ELogNewSources,0,"drive E start");
	path.Copy(KEDrive);
	CFileDirPool *newTree=CFileDirPool::NewLC();
	source=newTree->NewFileDirEntry(path,flags,NULL); //if parent is NULL, it must be a source
	newTree->GetNrMusicFilesL(source,fast,iShouldExit);
	source->GetStartingDirL(iSources,iSourcesCount,path);
	newTree->Recycle();
	LOG(ELogNewSources,0,"drive E done");

	//for F:
	LOG(ELogNewSources,0,"drive F start");
	path.Copy(KFDrive);
	source=newTree->NewFileDirEntry(path,flags,NULL); //if parent is NULL, it must be a source
	newTree->GetNrMusicFilesL(source,fast,iShouldExit);
	source->GetStartingDirL(iSources,iSourcesCount,path);
	newTree->Recycle();
	LOG(ELogNewSources,0,"drive F done");

	//for C:
	LOG(ELogNewSources,0,"drive C start");
	path.Copy(KCDrive);
	source=newTree->NewFileDirEntry(path,flags,NULL); //if parent is NULL, it must be a source
	newTree->GetNrMusicFilesL(source,fast,iShouldExit);
	source->GetStartingDirL(iSources,iSourcesCount,path);
	CleanupStack::PopAndDestroy(newTree);
	LOG(ELogNewSources,0,"drive C done");

	//done finding sources
	LOGn(ELogNewSources,-1,"MLauncher_src: GetNewSourcesL() end (new sources count: %d)",iSources.Count());
	iQueue.Send(EMsgSourcesDone);
}

void CThreadObserver::RunL()
{
	TExitType threadExitType = iThread.ExitType();
	LOGn(ELogGeneral,1,"CThreadObserver::RunL start (thread exit type=%d, iShoudExit=%d)",threadExitType,iShouldExit);

	if(threadExitType==EExitPending) // Thread is still running
	{
		//we may have received a message from the queue
		TInt msg;
		TInt err=iQueue.Receive(msg);
		LOGn(ELogGeneral,0,"Msg received: %d, err=%d",msg, err);
		if(!err)
			switch(msg)
			{
			case EMsgSourcesDone:
				if(!iShouldExit)
					AddToExistingSourcesL();
				//renew the notification request
				iQueue.NotifyDataAvailable(iStatus);
				SetActive();
				//memory info
				TInt nrSegments, segmentLen;
				iAppUi->iFilesTree->GetMemoryInfo(nrSegments,segmentLen);
				LOGn(ELogGeneral,0,"Memory info before metadata parsing: nr segments: %d, bytes per segment: %d",nrSegments,segmentLen);
				LOG(ELogGeneral,0,"Signaling the mutex");
				iMutex.Signal(); //metadata parsing may continue
				
				break;
			case EMsgUpdateView:
			{
				//update EContainerHasProgressBar flag
				CFileDirPool *p=iAppUi->iFilesTree;
				if(p->iTotalNrFiles>0 && p->iCurrentFileNr==0)
				{
					if(iAppUi->iListView)
						iAppUi->iListView->CurrentListChanged(CFFListView::EMetadataParsingStarted);
				}
				else if(p->iTotalNrFiles>0 && p->iCurrentFileNr==p->iTotalNrFiles)
				{
					if(iAppUi->iListView)
						iAppUi->iListView->CurrentListChanged(CFFListView::EMetadataParsingEnded);
				};
				//update view
				if(iAppUi->iListView)
					iAppUi->iListView->CurrentListChanged(CFFListView::EMetadataParsingInProgress);
				
				//renew the notification request
				iQueue.NotifyDataAvailable(iStatus);
				SetActive();
				LOG(ELogGeneral,0,"(update view msg received from the queue)");
			};break;
			case EMsgDone:
			{
				//
				iThread.Logon(iStatus);
				SetActive();
				//memory info
				TInt nrSegments, segmentLen;
				iAppUi->iFilesTree->GetMemoryInfo(nrSegments,segmentLen);
				LOGn(ELogGeneral,0,"Memory info after metadata parsing: nr segments: %d, bytes per segment: %d",nrSegments,segmentLen);
				LOG(ELogGeneral,0,"(done msg received from the queue)");
			};break;
			};//switch
		LOG(ELogGeneral,-1,"CThreadObserver::RunL end (thread continues)");
		return;
	}
	else
	{
		//thread is not running any more
		LOG(ELogGeneral,0,"CThreadObserver::RunL: thread not running any more");
		//the error we get should be KErrUnderflow
		//iThread.Kill(KErrNone);
	};
	iThread.Close(); // Close the thread handle, no need to LogonCancel()
	
	//clean stuff
	LOG(ELogGeneral,-1,"CThreadObserver::RunL: end (thread closed)");
	//TUint jobs(CMLauncherAppUi::EJobCleanThreadObserver);
	//iAppUi->ScheduleWorkerL(jobs);
}

void CThreadObserver::AddToExistingSourcesL()
{
	LOGn(ELogGeneral,1,"AddToExistingSourcesL(): start (%d sources)",iSources.Count());
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
	if(iSources.Count())
	{
		//try to match the new sources against our already existing sources
		TInt j;
		TBool savePreferences(EFalse);

		for(i=0;i<iSources.Count();i++)
		{
			TBool found(EFalse);
			TPtr path(iSources[i]->Des());
			CMLauncherPreferences::RemoveEndingBackslash(path);//sources in preferences do not have trailing slashes
			for(j=0;j<iAppUi->iPreferences->iStartingDirs.Count();j++)
			{
				LOGn(ELogGeneral,0,"Testing potential source %S with source %S",iSources[i],iAppUi->iPreferences->iStartingDirs[j]);
				if(*(iSources[i])== *(iAppUi->iPreferences->iStartingDirs[j]))
				{
					found=ETrue;
					//update files count before removing
					iAppUi->iPreferences->iStartingDirsNrFiles[j]=iSourcesCount[i];
					delete iSources[i];
					iSources.Remove(i);
					iSourcesCount.Remove(i);
					i--;
					break;
				};
			};
			if(!found) for(j=0;j<iAppUi->iPreferences->iBlacklistDirs.Count();j++)
			{
				LOGn(ELogGeneral,0,"Testing potential source %S with blacklisted folder %S",iSources[i],iAppUi->iPreferences->iBlacklistDirs[j]);
				if(*(iSources[i])== *(iAppUi->iPreferences->iBlacklistDirs[j]))
				{
					found=ETrue;
					//update files count before removing
					iAppUi->iPreferences->iBlacklistDirsNrFiles[j]=iSourcesCount[i];
					delete iSources[i];
					iSources.Remove(i);
					iSourcesCount.Remove(i);
					i--;
					break;
				};
			};

			if(!found) for(j=0;j<iAppUi->iPreferences->iStartingDirs.Count();j++)
			{
				TFileName startingDir(*(iAppUi->iPreferences->iStartingDirs[j]));
				startingDir.Append(KDirBackslash);
				if(iSources[i]->Find(startingDir)==0)
				{
					//the new source string contains one of the existing sources substrings.
					//this means that the existing source folder is a parent of the new source
					//ACTION: we replace the existing source with the new source
					LOGn(ELogGeneral,0,"Existing source %S is a parent of %S",iAppUi->iPreferences->iStartingDirs[j],iSources[i]);
					delete iAppUi->iPreferences->iStartingDirs[j];
					iAppUi->iPreferences->iStartingDirs[j]=iSources[i];
					iAppUi->iPreferences->iStartingDirsNrFiles[j]=iSourcesCount[i];
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
					if(iAppUi->iPreferences->iStartingDirs[j]->Find(sourceDir)==0)
					{
						//the new source string is contained in one of the existing source strings
						//this means that the new source is a parent of the existing source
						//ACTION: we replace the existing source with the new source
						LOGn(ELogGeneral,0,"New source %S is a parent of %S",iSources[i],iAppUi->iPreferences->iStartingDirs[j]);
						delete iAppUi->iPreferences->iStartingDirs[j];
						iAppUi->iPreferences->iStartingDirs[j]=iSources[i];
						iAppUi->iPreferences->iStartingDirsNrFiles[j]=iSourcesCount[i];
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
				//we add this source to preference and iSelection
				LOGn(ELogGeneral,0,"Keeping source: %S [%d]",iSources[i],iSourcesCount[i]);
				iAppUi->iPreferences->AddStartingDirL(iSources[i],iSourcesCount[i]);//ownership transfer

				TUint flags(TFileDirEntry::EIsDir|TFileDirEntry::EIsSource);
				TFileDirEntry *source=iAppUi->iFilesTree->NewFileDirEntry(*iSources[i],flags,NULL); //if parent is NULL, it must be a source;
				iAppUi->iFilesTree->MakeChildren(source);

				savePreferences=ETrue;
			};
		};
		//close remaining stuff
		iSources.Reset();//all remaining pointers have been transfered to iPreferences
		iSourcesCount.Reset();
		//update the sources view
		iAppUi->UpdateSourcesView();

		if(savePreferences)
		{
			iAppUi->iPreferences->SavePreferences();
			iAppUi->ConstructStartingFolderListL(ETrue,EFalse);
			//TODO: Here we should announce to the user that new folders have been discovered (and saved)
		};
	};

	//TODO: Here we should check if there are no sources at all, and display something
	LOGn(ELogGeneral,-1,"AddToExistingSourcesL(): end (total starting dirs: %d)",iAppUi->iPreferences->iStartingDirs.Count());
}

void CThreadObserver::ParseMetadataL()
{
	TInt i,nrFiles(0);
	LOG(ELogNewSources,1,"ParseMetadataL start");
	for(i=0;i<iAppUi->iFilesTree->iSources.Count();i++)
	{
		iAppUi->iFilesTree->GetNrMusicFilesL(iAppUi->iFilesTree->iSources[i],EFalse,iShouldExit);
		LOG(ELogNewSources,0,"In ParseMetadataL");
		nrFiles+=iAppUi->iFilesTree->iSources[i]->iNrMusicFiles;
	};
	LOG(ELogNewSources,0,"In ParseMetadataL 2");
	TFileName tmpPath;
	TCallBack *cb=new TCallBack(&UpdateView,this);
	iAppUi->iFilesTree->SetCallback(cb,nrFiles); //transfers ownership
	LOG(ELogNewSources,0,"Before GetMetadata");
	for(i=0;i<iAppUi->iFilesTree->iSources.Count();i++)
		iAppUi->iFilesTree->GetMetadata(iAppUi->iFilesTree->iSources[i],tmpPath,iShouldExit);
	//cb->CallBack();
	//clean
	iAppUi->iFilesTree->CleanMetadataVariables();
	iQueue.Send(EMsgDone);
	LOG(ELogNewSources,-1,"ParseMetadataL end");
}

void CThreadObserver::DoCancel()
{
	// Kill the thread and complete with KErrCancel
	// ONLY if it is still running
	TExitType threadExitType = iThread.ExitType();
	if(threadExitType==EExitPending)
	{
		// Thread is still running
		iThread.LogonCancel(iStatus);
		iThread.Kill(KErrCancel);
		iThread.Close();
		iCleanupStack=NULL; //TODO: this needs changing
	};
}

TInt CThreadObserver::UpdateView(TAny *aInstance)
{
	CThreadObserver *threadObserver=(CThreadObserver*)aInstance;
	threadObserver->iQueue.Send(EMsgUpdateView);
	return 1;
}
#endif
