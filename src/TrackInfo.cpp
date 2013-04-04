/*
 * TrackInfo.cpp
 *
 *  Created on: 27.12.2008
 *      Author: Florin Lohan
 */

#include <eikenv.h>
#include <utf.h>
#include <random.h>
#include <MetaDataFieldContainer.h>
#include <MetaDataUtility.h>
#include <stringloader.h>
#include <MLauncher.rsg>
#include "TrackInfo.h"
#include "ChildrenArray.inl"
#include "MLauncherPreferences.h"
#include "MLauncherDocument.h"
#include "defs.h"
#include "common_cpp_defs.h"
#include "MLauncher.pan"
#include "log.h"

const TInt KMinSongs=50; //minimum number of songs that should be in a folder so that we stop counting
const TInt KExtraAfterBefore=20;
const TInt64 KSecondsTo2000=(1500*365+500*366+18)*24*3600; //probably an approx date, but it does not matter
_LIT(KUnknown,"(Unknown)");

_LIT(KTrackNrFormat,"[%2d] ");
_LIT(KYearFormat," [%4d]");
_LIT(KNrAlbumsFormat," [%d albums]");
_LIT(KOneAlbumFormat," [1 album]");

//const TUint KTrackNrExtraLength=5;
//const TUint KYearExtraLength=7;
//const TUint KNrAlbumsExtraLength=13;
//const TUint KOneAlbumExtraLength=KOneAlbumFormat().Length();

const TInt KMetadataFilesStep=100;
const TInt KChildrenInitForMetadata=8;

RFs* CFileDirPool::iFs=NULL;
TUint TFileDirEntry::iStaticFlags=0;

CFileDirPool* CFileDirPool::NewL(TInt aPoolSize)
{
	CFileDirPool *self=new (ELeave)CFileDirPool();
	CleanupStack::PushL(self);
	self->ConstructL(aPoolSize);
	CleanupStack::Pop(self);
	return self;
}

CFileDirPool* CFileDirPool::NewLC(TInt aPoolSize)
{
	CFileDirPool *self=new (ELeave)CFileDirPool();
	CleanupStack::PushL(self);
	self->ConstructL(aPoolSize);
	return self;
}

CFileDirPool::CFileDirPool() : iSources(5), iFlags(0)
{
}

CFileDirPool::~CFileDirPool()
{
	LOG0("CFileDirPool::~CFileDirPool++ (this=%x)",this);
	delete iFileDirBuf;
	LOG0("iFileDirBuf deleted successfully")
	iSources.Close();
	LOG0("iSources closed");
	//CleanMetadataVariables(); -> not using metadata
	LOG0("CFileDirPool::~CFileDirPool--");
}

void CFileDirPool::Recycle()
{
	//__ASSERT_ALWAYS(iFileDirBuf->Size()==iCurrentPosition);
	//RDebug::Printf("Recycle ++");
	iFileDirBuf->Reset();
	//iCurrentPosition=0;
	iSources.Reset();
	//construct the root entry
	iRoot=(TFileDirEntry*)iFileDirBuf->ExpandL(sizeof(TFileDirEntry));
	//iCurrentPosition=sizeof(TFileDirEntry);
	//iRoot=(TFileDirEntry*)iFileDirBuf->Ptr(0).Ptr();
	iRoot->Initialize(NULL,0,TFileDirEntry::EIsDir|TFileDirEntry::EWasExpanded,NULL);
	iCurrentParent=iRoot;
	iFlags=0;
	//RDebug::Printf("Recycle --");
}

void CFileDirPool::ConstructL(TInt aPoolSize)
{
	iFileDirBuf=new(ELeave)CSegmentedBuffer(aPoolSize);
	//construct the root entry
	iRoot=(TFileDirEntry*)iFileDirBuf->ExpandL(sizeof(TFileDirEntry));
	iRoot->Initialize(NULL,0,TFileDirEntry::EIsDir|TFileDirEntry::EWasExpanded,NULL);
	iCurrentParent=iRoot;
}

TFileDirEntry* CFileDirPool::NewFileDirEntry(const TDesC &aName, TUint aFlags, TFileDirEntry *aParent, TInt aPosition/*=-1*/) //returned entry still owned
{
	/* if aParent is NULL, this must be a source
	 * if aParent is a source, then we add an entry (child) from root to this element
	 */
	
	//first, we get an entry into the buffer
	LOG(ELogNewSources,1,"NewFileDirEntry++ (%S %x %x)",&aName,aParent,this);
	__ASSERT_ALWAYS(aName.Length()>0,Panic(ENameHasZeroLength));
	TFileDirEntry *newEntry=(TFileDirEntry*)GetNewSpace(sizeof(TFileDirEntry));
	//copy the name to the buffer
	TInt nameLength(aName.Length());
	if(aName.LocateReverse('\\')==nameLength-1)
		nameLength--; 
	TUint8 *name=GetNewSpace(aName.Size());
	newEntry->InitializeAndCopy(name,aName,aFlags,aParent);
	//add a child entry to the parent
	if(aParent)
	{
		if(!aParent->iChildren)
			__ASSERT_ALWAYS(0,Panic(EBadCode));
		if(aPosition!=-1)
			aParent->iChildren->InsertIntoPosition(newEntry,this,aPosition);
		else
			aParent->iChildren->Append(newEntry,this,0);
	};
	if(aParent && (aParent->iFlags&TFileDirEntry::EIsSource))
	{
		//add it as a child entry to root as well
		LOG(ELogNewSources,0,"Adding to root ...");
		if(!iRoot->iChildren)
		{
			__ASSERT_ALWAYS(0,Panic(EBadCode));
		};
		if(aPosition!=-1)
			iRoot->iChildren->InsertIntoPosition(newEntry,this,aPosition);
		else
			iRoot->iChildren->Append(newEntry,this,0);
	};
	if(aFlags&TFileDirEntry::EIsSource)//add it to sources array
		iSources.Append(newEntry); //TODO: check returned value
	//__ASSERT_ALWAYS((aFlags&TFileDirEntry::EIsSpecial) || /*(aParent!=NULL)||*/(aFlags&TFileDirEntry::EIsSource), Panic(ESourceInconsistency2));
	LOG(ELogNewSources,-1,"NewFileDirEntry--");
	return newEntry;
}

TFileDirEntry* CFileDirPool::CreateEntriesFromSourceAndPath(const TDesC &aSource, const TDesC &aPath)
{
	//first, find the source. It should already exist
	TInt i,len(aSource.Length());
	TInt pos(0);
	TPtr elmName(NULL,0);
	TFileDirEntry *entry(NULL);
	if(aSource.Length()>0)
	{
		for(i=0;i<iSources.Count();i++)
			if(iSources[i]->iNameLength==len)
			{
				//this might be our source
				elmName.Set(iSources[i]->iName,iSources[i]->iNameLength,iSources[i]->iNameLength);
				if(elmName==aSource)
				{
					entry=iSources[i];
					//LOG("Source located");
					break;
				};
					
			};
	}
	else
	{
		//we need to discover the source by ourselves!!
		TInt pathLen(aPath.Length());
		for(i=0;i<iSources.Count();i++)
			if(pathLen>iSources[i]->iNameLength)
			{
				elmName.Set(iSources[i]->iName,iSources[i]->iNameLength,iSources[i]->iNameLength);
				if(aPath.Left(iSources[i]->iNameLength)==elmName)
				{
					//we found our source
					entry=iSources[i];
					pos=iSources[i]->iNameLength;
					//LOG("Source located");
					break;
				};
			};
	};
	if(!entry)return NULL; //we did not find this source, we ignore this entry
	
	//parse the path and add it, element by element
	TInt loc;
	TBool found;
	TInt flags(TFileDirEntry::EIsDir);
	TPtr elmPath(NULL,0);
	len=aPath.Length();
	while(1)
	{
		loc=aPath.Right(len-pos).Locate('\\');
		if(loc==0)
		{
			pos++;
			continue;
		};
		if(loc<0)
			break;
		//if we are here, we have an element in the path. Identify it against the current element's children
		//LOG1("Located an element of length %d",loc);
		found=EFalse;
		if(!(entry->iFlags&TFileDirEntry::EIsDir))
			return NULL; //current entry not a folder
		if(!(entry->iFlags&TFileDirEntry::EWasExpanded))
			MakeChildren(entry);
		for(i=0;i<entry->ChildrenCount();i++)
		{
			TFileDirEntry& currentChild=(*entry->iChildren)[i];
			if(currentChild.iNameLength==loc)
			{
				//this might be our child
				elmName.Set(currentChild.iName,currentChild.iNameLength,currentChild.iNameLength);
				if(elmName==aPath.Mid(pos,loc))
				{
					//add some flags to the old entry
					//LOG1("We found our entry: %S",&elmName);
					entry->iFlags|=TFileDirEntry::EHasSubdirs;
					entry=&currentChild;
					found=ETrue;
					//MakeChildren(entry);
					break;
				};
			};
		};
		if(!found)
			return NULL;
		
		//we are done with adding/identifying this child
		pos+=loc+1;		
	};
	//LOG1("Out of the loop. Remaining has length %d",len-pos);
	
	//identify the last element
	loc=len-pos;
	found=EFalse;
	if(!(entry->iFlags&TFileDirEntry::EIsDir))
		return NULL; //current entry not a folder
	if(!(entry->iFlags&TFileDirEntry::EWasExpanded))
		MakeChildren(entry);
	flags=0;
	for(i=0;i<entry->ChildrenCount();i++)
	{
		TFileDirEntry& currentChild=(*entry->iChildren)[i];
		if(currentChild.iNameLength==loc)
		{
			//this might be our child
			elmName.Set(currentChild.iName,currentChild.iNameLength,currentChild.iNameLength);
			if(elmName==aPath.Mid(pos,loc))
			{
				//add some flags to the old entry
				//LOG1("We found our entry: %S",&elmName);
				entry->iFlags|=TFileDirEntry::EHasMedia;
				entry=&currentChild;
				found=ETrue;
				break;
			};
		};
	};
	if(!found)
		return NULL;
	
	return entry;
}

TUint8* CFileDirPool::GetNewSpace(TInt size)
{
	__ASSERT_ALWAYS(size>0,Panic(ERequestedSizeIsZero));
	__ASSERT_ALWAYS(iFileDirBuf,Panic(EiFileDirBufIsNull));
	return iFileDirBuf->ExpandL(size);
}

TBool CFileDirPool::MakeChildren(TFileDirEntry *aParent, TInt aLevels/*=1*/)
{
	__ASSERT_DEBUG(aParent!=iRoot,Panic(EElementShouldNotBeRoot));
	//if this was expanded or it is NOT a dir, return
	if(aParent->iFlags&TFileDirEntry::EWasExpanded || !(aParent->iFlags&TFileDirEntry::EIsDir))
		return EFalse;
	
	LOG(ELogNewSources,1,"MakeChildren++ (%x %x)",aParent,this);
	__ASSERT_ALWAYS(!aParent->iChildren,Panic(EBadCode)); //if this was NOT expanded, it should have no children
	aParent->iFlags|=TFileDirEntry::EWasExpanded; //in case we have an error we should not try again
	CDir *dir=NULL;
    TInt err,i;
    TInt flags(0);
    TBool isDir,isMusicFile,hasDirs(EFalse),hasMedia(EFalse),isOggOrFlac;
    TBool isSelected=aParent->iFlags&TFileDirEntry::EIsSelected;
    TFileName path;
    TFileDirEntry *child(NULL);
    TBool childrenChanged(EFalse);
    aParent->GetFullPath(path);
    err=iFs->GetDir(path,KEntryAttNormal|KEntryAttDir,ESortByName|EDirsFirst,dir);
    if(err!=KErrNone)
    	return EFalse; //the element may not be a dir after all
    CleanupDeletePushL(dir); //this is needed because the thread where this function is called may be canceled

    LOG(ELogNewSources,0,"Counting children...");
    TInt nrChildren=0;
    for(i=0;i<dir->Count();i++)
    {
    	LOG(ELogNewSources,0,"Looking into child %d/%d",i+1,dir->Count());
    	if((*dir)[i].IsDir())
    		nrChildren++;
    	else if(TFileDirEntry::IsMusicFile((*dir)[i].iName,isOggOrFlac))
    		nrChildren++;
    }
    aParent->iChildren=(TChildrenArray<TFileDirEntry>*)GetNewSpace(sizeof(TChildrenArray<TFileDirEntry>));
    aParent->iChildren->Initialize(nrChildren,this);
    if(aParent->iFlags&TFileDirEntry::EIsSource)
    {
    	if(iRoot->iChildren)
    		iRoot->iChildren->AddMoreChildren(nrChildren,this);
    	else
    	{
    		iRoot->iChildren=(TChildrenArray<TFileDirEntry>*)GetNewSpace(sizeof(TChildrenArray<TFileDirEntry>));
    		iRoot->iChildren->Initialize(nrChildren,this);
    	}
    }
    	
    
	LOG(ELogNewSources,0,"Looking into children... (%d)",nrChildren);
    //for each folder, create an entry
    for(i=0;i<dir->Count();i++)
    {
    	LOG(ELogNewSources,0,"Looking into child %d/%d",i+1,dir->Count());
    	if((*dir)[i].IsDir())
    	{
    		isDir=hasDirs=ETrue;
    		isMusicFile=EFalse;
    	}
    	else
        {
            isDir=EFalse;
            isMusicFile=TFileDirEntry::IsMusicFile((*dir)[i].iName,isOggOrFlac);
            if(isMusicFile)hasMedia=ETrue;
        };
        if(! (isDir || isMusicFile) )continue;
        
        LOG(ELogNewSources,0,"This child is either folder or music file.");
        
        //add entry in iFiles        
        LOG(ELogNewSources,0,"Child will be added");
        flags=0;
        if(isDir)flags|=TFileDirEntry::EIsDir;
        if(isSelected)flags|=TFileDirEntry::EIsSelected;
        if(isOggOrFlac)flags|=TFileDirEntry::EIsOggOrFlac;
        child=NewFileDirEntry((*dir)[i].iName,flags,aParent);
        child->SetModificationTime((*dir)[i].iModified);
        childrenChanged=ETrue;
        LOG(ELogNewSources,0,"Child added successfully");
        //check if we need to create children for this child
        if(aLevels>1 && child)
        	MakeChildren(child,aLevels-1);
        
    };
    
    //add info about subdirs & media
    if(hasDirs)aParent->iFlags|=TFileDirEntry::EHasSubdirs;
    if(hasMedia)aParent->iFlags|=TFileDirEntry::EHasMedia;
    aParent->iFlags|=TFileDirEntry::EWasExpanded;
    //clean: delete CDir
    //delete dir;
    CleanupStack::PopAndDestroy(dir);
    LOG(ELogNewSources,-1,"MakeChildren--");
    return childrenChanged;
};


void CFileDirPool::GetNrMusicFilesL(TFileDirEntry *aSource, TBool aFast, TBool &aShouldExit)
{
	if(aShouldExit)return;
	/*to use fast find, we need that:
	 * -we have found at least KMinSongs (50?) songs in this folder
	 * -there are at least 5 folders with more than 5 songs in this folder
	 */
	// log is not very useful for this function 
	LOG(ELogNewSources,1,"GetNrMusicFilesL start (fast=%d)",aFast);
	__ASSERT_DEBUG(aSource!=iRoot,Panic(EElementShouldNotBeRoot));
    if(aSource->iFlags&TFileDirEntry::EIsDir)
    {
        //this is a folder
    	LOG(ELogNewSources,0,"Current entry is a folder");
        if(!(aSource->iFlags&TFileDirEntry::EWasExpanded))
            MakeChildren(aSource);
        TInt nrFolders(0);
        aSource->iNrMusicFiles=0;
        if(aSource->iChildren)
        for(TInt i=0;i<aSource->ChildrenCount();i++)
        {
        	LOG(ELogNewSources,0,"Child nr: %d/%d",i,aSource->ChildrenCount());
        	TFileDirEntry &child=(*aSource->iChildren)[i];
        	LOG(ELogNewSources,0,"Child grabbed");
        	if(LOG_ENABLED_FLAG(ELogNewSources))
        	{
        		TFileName tempPath;
        		child.GetFullPath(tempPath);
        		LOG(ELogNewSources,0,"Child path: %S",&tempPath);
        	};
        	GetNrMusicFilesL(&child,aFast,aShouldExit);
        	LOG(ELogNewSources,0,"Out of child's GetNrMusicFilesLx");
        	aSource->iNrMusicFiles+=child.iNrMusicFiles;
            
            if(aFast)
            {
            	if(child.iFlags&TFileDirEntry::EIsDir && child.iNrMusicFiles>5)
            		nrFolders++;
            	if(aSource->iNrMusicFiles>KMinSongs && nrFolders>5)
            	{
            		LOG(ELogNewSources,-1,"GetNrMusicFilesL end (fast, nr music files: %d)",aSource->iNrMusicFiles);
            		return;
            	};
            };
        };
    }
    else
    	aSource->iNrMusicFiles=1; //this is a file
    LOG(ELogNewSources,-1,"GetNrMusicFilesL end (fast=%d, nr music files: %d)",aFast,aSource->iNrMusicFiles);
}

int CFileDirPool::GetNrMusicFiles_ActiveIdleModeL()
{
	/*to use fast find, we need that:
	 * -we have found at least KMinSongs (50?) songs in this folder
	 * -there are at least 5 folders with more than 5 songs in this folder
	 */
	// log is not very useful for this function 
	LOG(ELogNewSources,1,"GetNrMusicFiles_ActiveIdleModeL++ (fast=%d, currentParent=%08x)",iFlags&EGetNrMusicFilesFast,iCurrentParent);
    if(iCurrentParent->iFlags&TFileDirEntry::EIsDir)
    {
        //this is a folder
    	LOG(ELogNewSources,0,"Current entry is a folder");
        if(!(iCurrentParent->iFlags&TFileDirEntry::EWasExpanded))
        {
            MakeChildren(iCurrentParent);
            LOG(ELogNewSources,-1,"GetNrMusicFiles_ActiveIdleModeL-- (MadeChildren)")
            return 1; //1 means we need to continue
        }
        if(iCurrentParent->iChildren)
        {
        	TFileDirEntry *initialParent=iCurrentParent;
        	for(;iCurrentParent->iSelectedPosition<iCurrentParent->ChildrenCount();iCurrentParent->iSelectedPosition++)
        	{
        		LOG(ELogNewSources,0,"Child nr: %d/%d",iCurrentParent->iSelectedPosition,iCurrentParent->ChildrenCount());
        		TFileDirEntry *currentChild=iCurrentParent=&(*iCurrentParent->iChildren)[iCurrentParent->iSelectedPosition];
        		LOG(ELogNewSources,0,"Child grabbed");
        		if(LOG_ENABLED_FLAG(ELogNewSources))
        		{
        			TFileName tempPath;
        			iCurrentParent->GetFullPath(tempPath);
        			LOG(ELogNewSources,0,"Child path: %S",&tempPath);
        		};
        		if(GetNrMusicFiles_ActiveIdleModeL())
        		{
        			//this subtree is not ready. Update data and return
        			LOG(ELogNewSources,-1,"GetNrMusicFiles_ActiveIdleModeL-- (subtree not ready)");
        			return 1;
        		}
        		//if we are here, this child (and its subtree) is ready
        		LOG(ELogNewSources,0,"Child & subtree ready");
        		__ASSERT_ALWAYS(initialParent==iCurrentParent,Panic(ECurrentParentNotTheSame));
        		iCurrentParent->iNrMusicFiles+=currentChild->iNrMusicFiles;
        		if(iFlags&EGetNrMusicFilesFast)
        		{
        			if(currentChild->iFlags&TFileDirEntry::EIsDir && currentChild->iNrMusicFiles>5)
        				iCurrentParent->iDisplayNameLeft++;
        			if(iCurrentParent->iNrMusicFiles>KMinSongs && iCurrentParent->iDisplayNameLeft>5)
        			{
        				iCurrentParent->iSelectedPosition=iCurrentParent->ChildrenCount();
        				LOG(ELogNewSources,-1,"GetNrMusicFiles_ActiveIdleModeL-- (fast, nr music files: %d, currentParent=%08x)",iCurrentParent->iParent->iDisplayNameLeft,iCurrentParent);
        				iCurrentParent=iCurrentParent->iParent;
        				return 0;
        			};
        		};
        	};//for
        	LOG(ELogNewSources,0,"We are done with this (%08x) entry's children",iCurrentParent);
        }//if iCurrentParent has children
    }
    else
    	iCurrentParent->iNrMusicFiles=1; //this is a file

    LOG(ELogNewSources,-1,"GetNrMusicFiles_ActiveIdleModeL-- (fast=%d, nr music files: %d, currentParent=%08x)",iFlags&EGetNrMusicFilesFast,iCurrentParent->iNrMusicFiles,iCurrentParent);
    iCurrentParent=iCurrentParent->iParent;
    return 0;//we are done for this subtree
}

/* Metadata functions, not used at the moment
void CFileDirPool::CleanMetadataVariables()
{
	if(iFields)
	{
		iFields->Close();
		delete iFields;
		iFields=NULL;
	};
	delete iMDUtil;
	iMDUtil=NULL;
	//call the callback one last time
	iCurrentFileNr=iTotalNrFiles;
	if(iMetadataCallback)
	{
		iMetadataCallback->CallBack();
		delete iMetadataCallback;
		iMetadataCallback=NULL;
	};
	//iCurrentFileNr=iTotalNrFiles=0;
}

void CFileDirPool::GetMetadata(TFileDirEntry *aEntry, TFileName &aTmpPath, TBool &aShouldExit)
{
	LOG(ELogNewSources,1,"GetMetadata start");
	if(aShouldExit)return;
	__ASSERT_DEBUG(aEntry!=iRoot,Panic(EElementShouldNotBeRoot));
	if(aEntry->iFlags&TFileDirEntry::EIsDir)
	{
		//this is a folder
		LOG(ELogNewSources,0,"Current entry is a folder");
		if(!(aEntry->iFlags&TFileDirEntry::EWasExpanded))
			MakeChildren(aEntry);
		if(aEntry->iChildren)
			for(TInt i=0;i<aEntry->ChildrenCount();i++)
			{
				TFileDirEntry &child=(*aEntry->iChildren)[i];
				GetMetadata(&child,aTmpPath,aShouldExit);
			};
	}
	else
	{
		//this is a file
#ifdef METADATA_FILES_ONLY
		RFile f;
		f.Open();
		
#else
		if(!iMDUtil)
		{
			iMDUtil=CMetaDataUtility::NewL();
			iFields=new RArray<TMetaDataFieldId>;
			iFields->Append(EMetaDataSongTitle);
			iFields->Append(EMetaDataArtist);
			iFields->Append(EMetaDataAlbum);
			iFields->Append(EMetaDataYear);
			iFields->Append(EMetaDataAlbumTrack);
		};
		aEntry->GetFullPath(aTmpPath);
		iMDUtil->OpenFileL(aTmpPath,*iFields);		
		const CMetaDataFieldContainer& fields = iMDUtil->MetaDataFieldsL();
		AddMetadataEntry(fields.Field(EMetaDataArtist), fields.Field(EMetaDataAlbum), fields.Field(EMetaDataYear), fields.Field(EMetaDataSongTitle), fields.Field(EMetaDataAlbumTrack), aEntry,aTmpPath);
		iMDUtil->ResetL();
#endif
		iCurrentFileNr++;
		if(!(iCurrentFileNr%KMetadataFilesStep) && iMetadataCallback)
		{
			iMetadataCallback->CallBack();
		}
	};
	LOG(ELogNewSources,-1,"GetMetadata end");
}

void CFileDirPool::AddMetadataEntry(TPtrC aArtist, TPtrC aAlbum, TPtrC aYear, TPtrC aSong, TPtrC aTrackNr, TFileDirEntry *aEntry, TFileName &aTmpPath)
{
	if(aArtist==KNullDesC && aAlbum==KNullDesC && aSong==KNullDesC)return; //no point in adding this song
	
	//do we have this artist in our list?
	TInt c;
	TUint flags(TFileDirEntry::EIsDir|TFileDirEntry::EIsSpecial|TFileDirEntry::EIsMetadata|TFileDirEntry::EWasExpanded);
	TPtr p(NULL,0,0);
	//we use a binary search
	if(!iArtistsHolder)
	{
		HBufC *displayName=StringLoader::LoadLC(R_ARTISTS_ALBUMS_DISPLAY_NAME); 		
		iArtistsHolder=NewFileDirEntry(*displayName,flags|TFileDirEntry::EHasSubdirs,iRoot,0);
		CleanupStack::PopAndDestroy(displayName);
	}
	else iArtistsHolder->iFlags|=TFileDirEntry::EHasSubdirs;
	TInt low(0),high(iArtistsHolder->ChildrenCount()),mid;
	TFileDirEntry *ourArtist(NULL);
	
	if(aArtist!=KNullDesC)
	{
		//we have an artist string. Trim it and see if we still have something.
		aTmpPath.Copy(aArtist.Left(KMaxFileName));
		aTmpPath.Trim();
		if(!aTmpPath.Compare(KUnknown))
			aTmpPath.SetLength(0); //force to use KUnknown
	};
	if(aArtist==KNullDesC || !aTmpPath.Length())
	{
		//we have a NULL artist, replace it with KUnknown
		if(iArtistsHolder->ChildrenCount()>0)
		{
			ourArtist=&(*iArtistsHolder->iChildren)[iArtistsHolder->ChildrenCount()-1];
			//p.Set(ourArtist->iName,ourArtist->iNameLength,ourArtist->iNameLength);
			//c=p.Compare(KUnknown);
			if(ourArtist->iFlags&TFileDirEntry::EMetadataUnknown)
				c=0;
			else c=1; //different
		}
		else c=1;
		if(c)
		{
			//we do not have KUnknown as an artist, add it
			//flags=TFileDirEntry::EIsDir|TFileDirEntry::EIsSpecial|TFileDirEntry::EWasProperlyExpanded|;
			ourArtist=NewFileDirEntry(KUnknown,flags|TFileDirEntry::EHasSubdirs|TFileDirEntry::EIsArtist|TFileDirEntry::EMetadataUnknown,iArtistsHolder);
			//ourArtist->SetModificationTime();
		}
		//else: we already have set ourArtist to KUnknown artist
	}
	else
	{
		//we have a proper artist, do a binary search
		if(high-1>=0 && ((*iArtistsHolder->iChildren)[high-1].iFlags&TFileDirEntry::EMetadataUnknown))
			high--; //we do not compare with the last element
		while(low<high) 
		{
			mid=low+((high-low)>>1);
			ourArtist=&(*iArtistsHolder->iChildren)[mid];
			p.Set(ourArtist->iName,ourArtist->iNameLength,ourArtist->iNameLength);
			c=p.CompareC(aTmpPath);
			if(c<0)low=mid+1; 
			else high=mid; 
		};
		// high == low, using high or low depends on taste 
		if(low<iArtistsHolder->ChildrenCount())
		{
			ourArtist=&(*iArtistsHolder->iChildren)[low];
			p.Set(ourArtist->iName,ourArtist->iNameLength,ourArtist->iNameLength);
			c=p.CompareC(aTmpPath);
		}
		else c=1;
		if((low<iArtistsHolder->ChildrenCount()) && (c==0))
		{
			//found at position low. We already have it in ourArtist
		}
		else
		{
			//we need to create the artist and insert it at position low
			//flags=TFileDirEntry::EIsDir|TFileDirEntry::EIsSpecial|TFileDirEntry::EWasProperlyExpanded|TFileDirEntry::EIsArtist;
			ourArtist=NewFileDirEntry(aTmpPath,flags|TFileDirEntry::EHasSubdirs|TFileDirEntry::EIsArtist,iArtistsHolder,low);
			//ourArtist->SetModificationTime();
		};
	};//artist done
	__ASSERT_ALWAYS(ourArtist,Panic(EOurArtistIsNull));
	if(!ourArtist->iChildren)
	{
		ourArtist->iChildren=(TChildrenArray<TFileDirEntry>*)GetNewSpace(sizeof(TChildrenArray<TFileDirEntry>));
		ourArtist->iChildren->Initialize(KChildrenInitForMetadata,this);
	};
	//__ASSERT_ALWAYS(ourArtist->iChildren,Panic(EOurArtistAlbumsIsNull));
	
	//we now got the artist, lets look into the Album
	TFileDirEntry *ourAlbum(NULL);
	TUint year(0);
	if(aYear!=KNullDesC)
	{
		//parsing album year
		TLex lex(aYear);
		lex.Val(year,EDecimal);
		if(year<KMetadataMinYear || year>KMetadataMaxYear)year=0;
	};
	if(aAlbum!=KNullDesC)
	{
		//we have an album string. Trim it and see if we still have something.
		aTmpPath.Copy(aAlbum.Left(KMaxFileName));
		aTmpPath.Trim();
		if(!aTmpPath.Compare(KUnknown))
			aTmpPath.SetLength(0); //force to use KUnknown
	};
	if(aAlbum==KNullDesC || !aTmpPath.Length())
	{
		//we have a NULL album, replace it with KUnknown
		if(ourArtist->iChildren->ChildrenCount()>0)
		{
			ourAlbum=&(*ourArtist->iChildren)[ourArtist->iChildren->ChildrenCount()-1];
			//p.Set(ourAlbum->iName,ourAlbum->iNameLength,ourAlbum->iNameLength);
			//c=p.Compare(KUnknown);
			if(ourAlbum->iFlags&TFileDirEntry::EMetadataUnknown)
				c=0;
			else c=1; //different
		}
		else c=1;
		if(c)
		{
			//we do not have KUnknown as an album, add it
			//flags=TFileDirEntry::EIsSpecial|TFileDirEntry::EWasProperlyExpanded|TFileDirEntry::EIsAlbum;
			ourAlbum=NewFileDirEntry(KUnknown,flags|TFileDirEntry::EHasMedia|TFileDirEntry::EIsAlbum|TFileDirEntry::EMetadataUnknown,ourArtist);
			ourAlbum->iModificationTime=year;
		}
		//else: we already have set ourAlbum to KUnknown album
	}
	else
	{
		//we have a proper album, do a binary search
		low=0;
		high=ourArtist->iChildren->ChildrenCount();
		if(high-1>=0 && ((*ourArtist->iChildren)[high-1].iFlags&TFileDirEntry::EMetadataUnknown))
			high--; //we do not compare with the last element
		while(low<high) 
		{
			mid=low+((high-low)>>1);
			ourAlbum=&(*ourArtist->iChildren)[mid];
			if(year && ourAlbum->iModificationTime)
			{
				//compare years
				if(ourAlbum->iModificationTime==year)
					c=0;
				else
					c=ourAlbum->iModificationTime<year?-1:1;
			}
			else
			{
				p.Set(ourAlbum->iName,ourAlbum->iNameLength,ourAlbum->iNameLength);
				c=p.CompareC(aTmpPath);
			};
			if(c<0)low=mid+1; 
			else high=mid; 
		};
		// high == low, using high or low depends on taste 
		if(low<ourArtist->iChildren->ChildrenCount())
		{
			ourAlbum=&(*ourArtist->iChildren)[low];
			if(year && ourAlbum->iModificationTime)
			{
				//compare years
				if(ourAlbum->iModificationTime==year)
					c=0;
				else
					c=ourAlbum->iModificationTime<year?-1:1;
			}
			else
			{
				p.Set(ourAlbum->iName,ourAlbum->iNameLength,ourAlbum->iNameLength);
				c=p.CompareC(aTmpPath);
			};
		}
		else c=1;
		if((low<ourArtist->iChildren->ChildrenCount()) && (c==0))
		{
			//found at position low. We already have it in ourAlbum
			if(!ourAlbum->iModificationTime)
				ourAlbum->iModificationTime=year;
		}
		else
		{
			//we need to create the album and insert it at position low
			//flags=TFileDirEntry::EIsSpecial|TFileDirEntry::EWasProperlyExpanded|TFileDirEntry::EIsAlbum;
			ourAlbum=NewFileDirEntry(aTmpPath,flags|TFileDirEntry::EHasMedia|TFileDirEntry::EIsAlbum,ourArtist,low);
			ourAlbum->iModificationTime=year;
		};
	};//album done
	__ASSERT_ALWAYS(ourAlbum,Panic(EOurAlbumIsNull));
	if(!ourAlbum->iChildren)
	{
		ourAlbum->iChildren=(TChildrenArray<TFileDirEntry>*)GetNewSpace(sizeof(TChildrenArray<TFileDirEntry>));
		ourAlbum->iChildren->Initialize(KChildrenInitForMetadata,this);
	};
	//__ASSERT_ALWAYS(ourAlbum->iSongs,Panic(EOurAlbumSongsIsNull));
	
	//we now got the artist & the album, lets add the song
	TFileDirEntry *ourSong(NULL);
	TUint trackNr(0);
	if(aTrackNr!=KNullDesC)
	{
		//parsing track number
		TLex lex(aTrackNr);
		lex.Val(trackNr,EDecimal);
		if(trackNr<KMetadataMinTrack || trackNr>KMetadataMaxTrack)trackNr=0;
	};
	if(aSong!=KNullDesC)
	{
		//we have an album string. Trim it and see if we still have something.
		aTmpPath.Copy(aSong.Left(KMaxFileName));
		aTmpPath.Trim();
	};
	if(aSong==KNullDesC || !aTmpPath.Length())
	{
		//we have a NULL song, replace it with the filename
		aTmpPath.Copy(aEntry->iName,aEntry->iNameLength);
	};
	//now insert the song
	low=0;
	high=ourAlbum->iChildren->ChildrenCount();
	while(low<high) 
	{
		mid=low+((high-low)>>1);
		ourSong=&(*ourAlbum->iChildren)[mid];
		if(trackNr && ourSong->iModificationTime)
		{
			//compare track number (unlikely to be the same)
			if(ourSong->iModificationTime==trackNr)
				c=0;
			else
				c=ourSong->iModificationTime<trackNr?-1:1;
		}
		else
		{
			p.Set(ourSong->iName,ourSong->iNameLength,ourSong->iNameLength);
			c=p.CompareC(aTmpPath);
		};
		if(c<0)low=mid+1; 
		else high=mid; 
	};
	// high == low, using high or low depends on taste 
	if(low<ourAlbum->iChildren->ChildrenCount())
	{
		ourSong=&(*ourAlbum->iChildren)[low];
		if(trackNr && ourSong->iModificationTime)
		{
			//compare track number (unlikely to be the same)
			if(ourSong->iModificationTime==trackNr)
				c=0;
			else
				c=ourSong->iModificationTime<trackNr?-1:1;
		}
		else
		{
			p.Set(ourSong->iName,ourSong->iNameLength,ourSong->iNameLength);
			c=p.CompareC(aTmpPath);
		};
	}
	else c=1;
	if((low<ourAlbum->iChildren->ChildrenCount()) && (c==0))
	{
		//found at position low. We already have it in ourSong
	}
	else
	{
		//we need to create the song and insert it at position low
		flags=TFileDirEntry::EIsSpecial|TFileDirEntry::EIsMetadata|TFileDirEntry::EWasExpanded|TFileDirEntry::EIsSong; //it is not dir
		ourSong=NewFileDirEntry(aTmpPath,flags,ourAlbum,low);
		ourSong->iModificationTime=trackNr;
		ourSong->iSelectedPosition=(TInt)aEntry;
		aEntry->iSelectedPosition=(TInt)ourSong;
	};
	//song done
	__ASSERT_ALWAYS(ourSong,Panic(EOurSongIsNull));
}

void CFileDirPool::SetCallback(TCallBack *aCallback, TInt aTotalNrFiles)
{
	iMetadataCallback=aCallback;
	iCurrentFileNr=0;
	iTotalNrFiles=aTotalNrFiles;
	iMetadataCallback->CallBack();
}*/

void CFileDirPool::ApplySelection(CFileDirPool *aSelection)
{
	TInt i,j;
	
	//check lenghts first
	if(iCurrentParent->iNameLength!=aSelection->iCurrentParent->iNameLength)
		return; //the paths cannot be the same
	
	TPtr name(iCurrentParent->iName,iCurrentParent->iNameLength,iCurrentParent->iNameLength);
	TPtr selectionName(aSelection->iCurrentParent->iName,aSelection->iCurrentParent->iNameLength,aSelection->iCurrentParent->iNameLength);
	
	if(name != selectionName)//check if the paths are the same
		return; 
	
	//check the children
	if(aSelection->iCurrentParent->ChildrenCount())
	{
		MakeChildren(iCurrentParent);
	
		for(i=0;i<iCurrentParent->ChildrenCount();i++)
		{
			name.Set((*iCurrentParent->iChildren)[i].iName,(*iCurrentParent->iChildren)[i].iNameLength,(*iCurrentParent->iChildren)[i].iNameLength);
			for(j=0;j<aSelection->iCurrentParent->ChildrenCount();j++)
			{
				if(name.Length()!=(*aSelection->iCurrentParent->iChildren)[j].iNameLength)
					continue; //this is not what we are looking for
				selectionName.Set((*aSelection->iCurrentParent->iChildren)[j].iName,(*aSelection->iCurrentParent->iChildren)[j].iNameLength,(*aSelection->iCurrentParent->iChildren)[j].iNameLength);
				if(name == selectionName)
				{
					iCurrentParent=&(*iCurrentParent->iChildren)[i];
					aSelection->iCurrentParent=&(*aSelection->iCurrentParent->iChildren)[j];
					ApplySelection(aSelection);
					iCurrentParent=iCurrentParent->GetProperParent(this);
					aSelection->iCurrentParent=aSelection->iCurrentParent->GetProperParent(aSelection);
					//remove the element from children list
					aSelection->iCurrentParent->iChildren->MarkAsDeleted(j);
					break;
				};
			};
		};
	}
	else
	{
		//aSelection has no children, so we select everything
		iCurrentParent->SetSelection4Subtree(ETrue);
	}
};

void CFileDirPool::UpdateEntry(TFileDirEntry *aEntry)
{
    //this function updates aEntry and all its children. 
    //if we are here, the current path exists
	__ASSERT_DEBUG(aEntry!=iRoot,Panic(EElementShouldNotBeRoot));
    __ASSERT_ALWAYS(aEntry->iFlags&TFileDirEntry::EIsDir,Panic(EEntryNotPath));
    if(aEntry->ChildrenCount()==0)return; //this node is either a file or it is a non-expanded dir

    TInt err,i,j;
    CDir *dir=NULL;
    TBool found;
    TBool isDir,isMusicFile,hasDirs(EFalse),hasMedia(EFalse),isOggOrFlac;

    TFileName path;
    aEntry->GetFullPath(path);
    err=iFs->GetDir(path,KEntryAttNormal|KEntryAttDir,ESortByName|EDirsFirst,dir);
    if(err)return;

    RArray<TInt> entryFound(dir->Count());
    for(i=0;i<dir->Count();i++)
        if(entryFound.Append(0)!=KErrNone)//0=not found
        {
        	delete dir;
        	entryFound.Close();
        	return; //probably a "no memory" error
        }

    for(i=0;i<aEntry->ChildrenCount();i++)
    {
        found=EFalse;
        TFileDirEntry &child=(*aEntry->iChildren)[i];
        TPtr childName(child.iName,child.iNameLength,child.iNameLength);
        for(j=0;j<dir->Count();j++)
            if(childName==(*dir)[j].iName &&
                    ((child.iFlags&TFileDirEntry::EIsDir && (*dir)[j].IsDir()) || (!(child.iFlags&TFileDirEntry::EIsDir) && !(*dir)[j].IsDir())))
            {
                found=ETrue;
                entryFound[j]=1;
                if(child.iFlags&TFileDirEntry::EIsDir)
                    hasDirs=ETrue;

                break;
            };
        if(!found)
        {
        	aEntry->iChildren->MarkAsDeleted(i);
        	i--;
        };
    };

    //we add the elements from dir that were not found
    for(i=0;i<dir->Count();i++)
        if(!entryFound[i])
        {
        	if((*dir)[i].IsDir())
            {
                isDir=hasDirs=ETrue;
                isMusicFile=EFalse;
            }
            else
            {
                isDir=EFalse;
                isMusicFile=TFileDirEntry::IsMusicFile((*dir)[i].iName,isOggOrFlac);
                if(isMusicFile)hasMedia=ETrue;
            };
            if(! (isDir || isMusicFile) )continue;

            //add entry in iFiles
            TUint flags(isDir?TFileDirEntry::EIsDir:0);
            if(isOggOrFlac)flags|=TFileDirEntry::EIsOggOrFlac;
            NewFileDirEntry((*dir)[i].iName,flags,aEntry);
        };
    //close stuff
    delete dir;
    entryFound.Close();
    
    if(hasDirs)aEntry->iFlags|=TFileDirEntry::EHasSubdirs;
    else aEntry->iFlags&=~TFileDirEntry::EHasSubdirs;
};

void CFileDirPool::UpdateEntry2(TFileDirEntry *aEntry, TBool aRecursive, TBool &aCurrentParentWasUpdated)
{
	//this function updates aEntry and all its children. 
	//if we are here, the current path exists
	__ASSERT_DEBUG(aEntry!=iRoot,Panic(EElementShouldNotBeRoot));
	__ASSERT_ALWAYS(aEntry->iFlags&TFileDirEntry::EIsDir,Panic(EEntryNotPath));
	if(aEntry->ChildrenCount()==0)return; //this node is either a file or it is a non-expanded dir

	TInt err,i,j;
	TEntry fileEntry;
	TFileName path;
	aEntry->GetFullPath(path);
	err=iFs->Entry(path,fileEntry);
	
	if(aEntry->HasSameModificationTime(fileEntry.iModified))
	{
		LOG0("UpdateEntry2: Nothing is modified inside path %S",&path);
		return; //nothing is modified in the subtree
	};
	
	//if we are here, there are modifications with the children
	CDir *dir=NULL;
	TBool found;
	TBool isDir,isMusicFile,hasDirs(EFalse),hasMedia(EFalse),isOggOrFlac;
	aEntry->GetFullPath(path);
	err=iFs->GetDir(path,KEntryAttNormal|KEntryAttDir,ESortByName|EDirsFirst,dir);
	if(err)return;

	RArray<TInt> entryFound(dir->Count());
	for(i=0;i<dir->Count();i++)
		if(entryFound.Append(0)!=KErrNone)
		{
			delete dir;
			entryFound.Close();
			return;
		}

	for(i=0;i<aEntry->ChildrenCount();i++)
	{
		found=EFalse;
		TFileDirEntry &child=(*aEntry->iChildren)[i];
		TPtr childName(child.iName,child.iNameLength,child.iNameLength);
		for(j=0;j<dir->Count();j++)
			if(childName==(*dir)[j].iName &&
					((child.iFlags&TFileDirEntry::EIsDir && (*dir)[j].IsDir()) || (!(child.iFlags&TFileDirEntry::EIsDir) && !(*dir)[j].IsDir())))
			{
				found=ETrue;
				entryFound[j]=1;
				if(child.iFlags&TFileDirEntry::EIsDir)
				{
					if(aRecursive)
						UpdateEntry2(&child,aRecursive,aCurrentParentWasUpdated);
					hasDirs=ETrue;
				};
				break;
			};
		if(!found)
		{
			aEntry->iChildren->MarkAsDeleted(i);
			i--;
			if(iCurrentParent==aEntry)
				aCurrentParentWasUpdated=ETrue;
		};
	};

	//we add the elements from dir that were not found
	for(i=0;i<dir->Count();i++)
		if(!entryFound[i])
		{
			//add this dir as a child
			if((*dir)[i].IsDir())
			{
				isDir=hasDirs=ETrue;
				isMusicFile=EFalse;
			}
			else
			{
				isDir=EFalse;
				isMusicFile=TFileDirEntry::IsMusicFile((*dir)[i].iName,isOggOrFlac);
				if(isMusicFile)hasMedia=ETrue;
			};
			if(! (isDir || isMusicFile) )continue;

			//add entry in iFiles
			TUint flags(isDir?TFileDirEntry::EIsDir:0);
			if(isOggOrFlac)flags|=TFileDirEntry::EIsOggOrFlac;
			TFileDirEntry *entry=NewFileDirEntry((*dir)[i].iName,flags,aEntry);
			entry->SetModificationTime((*dir)[i].iModified);
			if(iCurrentParent==aEntry)
				aCurrentParentWasUpdated=ETrue;
		};

	//clean
	delete dir;
	entryFound.Close();

	if(hasDirs)aEntry->iFlags|=TFileDirEntry::EHasSubdirs;
	else aEntry->iFlags&=~TFileDirEntry::EHasSubdirs;
}

void CFileDirPool::UpdateTree(TBool aFilesReceivedInSpecialFolder, TBool &aCurrentFolderNeedsUpdate)
{
	if(aFilesReceivedInSpecialFolder)
	{
		//take each special folder and update it
		TInt i,j;
		aCurrentFolderNeedsUpdate=EFalse;
		for(i=0;i<iSources.Count();i++)
			for(j=0;j<iSources[i]->ChildrenCount();j++)
				if((*iSources[i]->iChildren)[j].iFlags&TFileDirEntry::EIsSpecial && !((*iSources[i]->iChildren)[j].iFlags&TFileDirEntry::EIsMetadata))
				{
					//update this child
					UpdateEntry2(&((*iSources[i]->iChildren)[j]),ETrue,aCurrentFolderNeedsUpdate);
				};
	}
	else Panic(ENotImplemented);	
}

void CFileDirPool::SetSelection(TFileDirEntry *aEntry, TInt aNrSelectedChildren)
{
	TInt i;
	TInt nr_children(aEntry->ChildrenCount());
	TUint flags;
	if(aNrSelectedChildren<0)
	{
		//we need to compute it
		aNrSelectedChildren=0;
		
		for(i=0;i<nr_children;i++)
		{
			flags=(*aEntry->iChildren)[i].iFlags;
			if(flags&TFileDirEntry::EIsSelected || flags&TFileDirEntry::EIsPartiallySelected)
				aNrSelectedChildren++;
		};
	};
	
	
	if(aNrSelectedChildren)
	{
		//some or all of the children are selected
		//assign the flags
		TFileDirEntry *tempParent=aEntry;
		while(tempParent)
		{
			if(tempParent->AllChildrenAndSubchildrenSelected())
			{
				tempParent->iFlags |= TFileDirEntry::EIsSelected;
				tempParent->iFlags &= ~TFileDirEntry::EIsPartiallySelected;
			}
			else
			{
				tempParent->iFlags |= TFileDirEntry::EIsPartiallySelected;
				tempParent->iFlags &= ~TFileDirEntry::EIsSelected;
			};

			if(tempParent->iFlags&TFileDirEntry::EIsSource)
			{
				//we also have to mark the root
				if(iRoot->AllChildrenAndSubchildrenSelected())
				{
					iRoot->iFlags |= TFileDirEntry::EIsSelected;
					iRoot->iFlags &= ~TFileDirEntry::EIsPartiallySelected;
				}
				else
				{
					iRoot->iFlags |= TFileDirEntry::EIsPartiallySelected;
					iRoot->iFlags &= ~TFileDirEntry::EIsSelected;
				};
			};

			tempParent=tempParent->iParent;
		};
	}
	else
	{
		//some or none of the children are selected (some may be partially selected)
		TBool partiallySelected(EFalse);
		for(i=0;i<nr_children;i++)
		{
			flags=(*aEntry->iChildren)[i].iFlags;
			if((flags & TFileDirEntry::EIsPartiallySelected) || (flags & TFileDirEntry::EIsSelected))
			{
				partiallySelected=ETrue;
				break;
			};
		}

		if(partiallySelected)
		{
			//check the parents grandparents (and ancestors): partially sellect them all
			TFileDirEntry *tempParent=aEntry;
			while(tempParent && !(tempParent->iFlags & TFileDirEntry::EIsPartiallySelected))
			{
				tempParent->iFlags &= ~TFileDirEntry::EIsSelected;
				tempParent->iFlags |= TFileDirEntry::EIsPartiallySelected;

				if(tempParent->iFlags&TFileDirEntry::EIsSource)
				{
					//we also have to mark the root
					iRoot->iFlags |= TFileDirEntry::EIsPartiallySelected;
					iRoot->iFlags &= ~TFileDirEntry::EIsSelected;
				};

				tempParent=tempParent->iParent;
			};
		}
		else
		{
			//mark the parent that none of the children are selected
			aEntry->iFlags &= ~TFileDirEntry::EIsSelected;
			aEntry->iFlags &= ~TFileDirEntry::EIsPartiallySelected;

			//check the grandparents (and ancestors): unselect them if none of ther children are selected
			//if some children are selected, then partially select them all
			TBool hasSelectedChildren=EFalse;
			TFileDirEntry *tempParent=aEntry->iParent;
			while(tempParent && ((tempParent->iFlags & TFileDirEntry::EIsSelected) || (tempParent->iFlags & TFileDirEntry::EIsPartiallySelected)))
			{
				hasSelectedChildren=EFalse;
				for(i=0;i<tempParent->ChildrenCount();i++)
				{
					flags=(*tempParent->iChildren)[i].iFlags;
					if((flags & TFileDirEntry::EIsSelected) || (flags & TFileDirEntry::EIsPartiallySelected))
					{
						hasSelectedChildren=ETrue;
						break;
					};
				}
				if(hasSelectedChildren)break;
				//if we are here, this parent does not have any selected children and it is selected or partially selected
				//we need to unselect it
				tempParent->iFlags &= ~TFileDirEntry::EIsSelected;
				tempParent->iFlags &= ~TFileDirEntry::EIsPartiallySelected;

				if(tempParent->iFlags&TFileDirEntry::EIsSource)
				{
					//we also have to mark the root
					TBool rootHasSelectedChildren(EFalse);
					for(i=0;i<iRoot->ChildrenCount();i++)
					{
						flags=(*iRoot->iChildren)[i].iFlags;
						if((flags & TFileDirEntry::EIsSelected) || (flags & TFileDirEntry::EIsPartiallySelected))
						{
							rootHasSelectedChildren=ETrue;
							break;
						};
					};
					//
					if(rootHasSelectedChildren)
					{
						iRoot->iFlags &= ~TFileDirEntry::EIsSelected;
						iRoot->iFlags |= TFileDirEntry::EIsPartiallySelected;
					}
					else
					{
						//nothing is selected
						iRoot->iFlags &= ~TFileDirEntry::EIsSelected;
						iRoot->iFlags &= ~TFileDirEntry::EIsPartiallySelected;
					};
				};

				//go check for its parent as well
				tempParent=tempParent->iParent;
			};
			//if we are here (and tempParent is NOT NULL, then we need to mark all ancestors as partially selected
			if(hasSelectedChildren)
				while(tempParent && !(tempParent->iFlags & TFileDirEntry::EIsPartiallySelected))
				{
					tempParent->iFlags &= ~TFileDirEntry::EIsSelected;
					tempParent->iFlags |= TFileDirEntry::EIsPartiallySelected;

					if(tempParent->iFlags&TFileDirEntry::EIsSource)
					{
						//we also have to mark the root as partially selected
						iRoot->iFlags &= ~TFileDirEntry::EIsSelected;
						iRoot->iFlags |= TFileDirEntry::EIsPartiallySelected;
					};
					tempParent=tempParent->iParent;
				};
		};
	};
}

void CFileDirPool::MarkSourceAsDeleted(TDesC& aSource)
{
	//first, we need to identify the right source
	LOG(ELogGeneral,1,"MarkSourceAsDeleted++ (source: %S)",&aSource);
	TInt i;
	for(i=0;i<iSources.Count();i++)
	{
		TPtrC srcName(iSources[i]->iName,iSources[i]->iNameLength);
		if(!aSource.Compare(srcName))
		{
			LOG0("Source %S (%d/%d) found to be the same!",&srcName,i+1,iSources.Count());
			iSources[i]->MarkAsDeleted();
			break;
		}
		else
			LOG0("Source %S (%d/%d) found to be different!",&srcName,i+1,iSources.Count());
	}
	LOG(ELogGeneral,-1,"MarkSourceAsDeleted--");
}

void CFileDirPool::GetMemoryInfo(TInt &aNrSegments, TInt &aSegmentLength)
{
	iFileDirBuf->GetMemoryInfo(aNrSegments,aSegmentLength);
}

/*
TInt CFileDirPool::SaveToFile(TFileName &aFilename)
{
	//
}

TInt CFileDirPool::LoadFromFile(TFileName &aFilename)
{
	//
}
*/
///////////////////////////////////////////////////////////////////////////////////////


TFileDirEntry::TFileDirEntry() : iName(NULL), iNameLength(0), iDisplayNameLeft(0), iDisplayNameRight(0), iGlobalDisplayNameIndex(255),
                                 iFlags(0), iSelectedPosition(0), iNrMusicFiles(0), iParent(NULL), iChildren(NULL)
{
}

void TFileDirEntry::Initialize(TUint8 *aName, TInt aNameLength, TUint aFlags, TFileDirEntry *aParent)
{
	iName=(TUint16*)aName;
	iNameLength=aNameLength;
	iFlags=aFlags;
	iParent=aParent;
	
	//other stuff
	iDisplayNameLeft=0;
	iDisplayNameRight=0;
	iGlobalDisplayNameIndex=255;
	iSelectedPosition=0;
	iNrMusicFiles=0;
	iChildren=NULL;
}

void TFileDirEntry::InitializeAndCopy(TUint8 *aName, const TDesC& aNameDes, TUint aFlags, TFileDirEntry *aParent)
{
	iName=(TUint16*)aName;
	iNameLength=aNameDes.Length();
	TPtr name(iName,0,iNameLength);
	name.Copy(aNameDes);
	iFlags=aFlags;
	iParent=aParent;
	
	//other stuff
	iDisplayNameLeft=0;
	iDisplayNameRight=0;
	iGlobalDisplayNameIndex=255;
	iSelectedPosition=0;
	iNrMusicFiles=0;
	iChildren=NULL;
}

void TFileDirEntry::GetFullPath(TDes &aPath,TBool aAppendSlash) const
{
	LOG(ELogNewSources,0,"GetFullPath++ (this=%x)",this)
	if(iFlags&EIsSong)
		return ((TFileDirEntry*)iSelectedPosition)->GetFullPath(aPath,aAppendSlash);
	
	if(iFlags&EIsSource)
	{
		aPath.Copy(iName,iNameLength);
		LOG(ELogNewSources,0,"GetFullPath, %x (is source): %S",this,&aPath);
	}
    else /*if(iParent)*/
    {
    	__ASSERT_ALWAYS(iParent,Panic(EParentIsNull));
    	iParent->GetFullPath(aPath,ETrue);
        aPath.Append(iName,iNameLength);
        LOG(ELogNewSources,0,"GetFullPath, %x: %S",this,&aPath);
    };
    
    if(iFlags&EIsDir && aAppendSlash && aPath.Length())
        aPath.Append(KDirBackslash);
}

void TFileDirEntry::GetPathWithoutSource(TDes &aPath) const
{
	if(iFlags&EIsSource)
	{
		aPath.SetLength(0);
	}
	else /*if(iParent)*/
	{
		__ASSERT_ALWAYS(iParent,Panic(EParentIsNull2));
		iParent->GetPathWithoutSource(aPath);
		aPath.Append(iName,iNameLength);
	};
	
	if(iFlags&EIsDir && aPath.Length())
		aPath.Append(KDirBackslash);
}

TBool TFileDirEntry::IsMusicFile(const TDesC16& aFilename, TBool &aIsOggOrFlac)
{
    //music files are: .mp3, .aac, .wma, .m4a, .ogg, .wav, 
    TBuf<4> ext(aFilename.Right(4));
    aIsOggOrFlac=EFalse;
    ext.LowerCase();
    if(ext==_L(".mp3") ||
            ext==_L(".aac") ||
            ext==_L(".wma") ||
            ext==_L(".m4a") ||
            ext==_L(".wav") ||
            ext==_L(".ram"))return ETrue;
    if(ext.Right(3)==_L(".rm"))return ETrue;
    //if((iStaticFlags&ESupportsOggAndFlac) && (ext==_L("flac") || ext==_L(".ogg")))return ETrue;
    if(ext==_L("flac") || ext==_L(".ogg"))
    {
    	aIsOggOrFlac=ETrue;
    	return ETrue;
    }
    return EFalse;
}

void TFileDirEntry::SetSelection4Subtree(TBool aSelected)
{
    //select/unselect this element and all its children (if any)
    if(aSelected)
        iFlags |= EIsSelected;
    else
    	iFlags &= ~EIsSelected;
    iFlags &= ~EIsPartiallySelected;
    
    //do the same for the children
    TInt i;
    for(i=0;i<ChildrenCount();i++)
    	(*iChildren)[i].SetSelection4Subtree(aSelected);
}

TInt TFileDirEntry::AddAndSortChild(CFileDirPool *aPool, TFileDirEntry *aChild, TBool aMakeCopy, TInt aChildrenToAllocate) //add aChild to the list of children
{
	//__ASSERT_DEBUG(this!=aPool->iRoot,Panic(EElementShouldNotBeRoot));
	if(!iChildren)
	{
		iChildren=(TChildrenArray<TFileDirEntry>*)aPool->GetNewSpace(sizeof(TChildrenArray<TFileDirEntry>));
		iChildren->Initialize(aChildrenToAllocate,aPool);
	};
	//find the position where to insert the child
	TInt i,j;
	if(aMakeCopy)
	{
		//we make a copy of the child
		TPtr name(aChild->iName,aChild->iNameLength,aChild->iNameLength);
		TFileDirEntry *child=aPool->NewFileDirEntry(name,aChild->iFlags&~EDeleted,NULL);
		child->iSelectedPosition=aChild->iSelectedPosition;
		child->iNrMusicFiles=aChild->iNrMusicFiles;
		child->iModificationTime=aChild->iModificationTime;
		aChild=child;
	}
	TBool isDir(aChild->iFlags&EIsDir);
	TInt diff(0);
	for(i=0;i<ChildrenCount();i++)
	{
		TFileDirEntry &currentChild=(*iChildren)[i];
		if((isDir && currentChild.iFlags&EIsDir) || (!isDir && !(currentChild.iFlags&EIsDir)))
		{
			//compare aChild->iName with currentChild->iName, ignoring the case
			TInt commonLength(aChild->iNameLength<currentChild.iNameLength?aChild->iNameLength:currentChild.iNameLength);
			for(j=0;j<commonLength;j++)
			{
				TChar cCh(aChild->iName[j]);
				TChar cChi(currentChild.iName[j]);
				diff=cCh.GetLowerCase()-cChi.GetLowerCase();
				if(diff>0)break;
				if(diff<0)
				{
					iChildren->InsertIntoPosition(aChild,aPool,i);
					break;
				};
			};
			if(diff<0)break; //child was inserted
			if(j>=commonLength && aChild->iNameLength<currentChild.iNameLength)//means that the child was not inserted
			{
				iChildren->InsertIntoPosition(aChild,aPool,i);
				break;
			};
		}
		else if(isDir && !(currentChild.iFlags&EIsDir))
		{
			//we have a folder, and the list of folder ended, insert the folder here
			iChildren->InsertIntoPosition(aChild,aPool,i);
			break;
		};
	};
	if(i>=ChildrenCount())//child was not inserted yet
		iChildren->Append(aChild,aPool);
	
	if(this!=aPool->iRoot)
		aChild->iParent=this;
	return KErrNone;
}
	
/*
TInt TFileDirEntry::AddDisplayName(CFileDirPool *aPool, TDesC& aDisplayName)
{
	__ASSERT_DEBUG(this!=aPool->iRoot,Panic(EElementShouldNotBeRoot));
	iDisplayNameMaxLength=iDisplayNameLength=aDisplayName.Length();
	//iDisplayName=(TUint16*)aPool->GetNewSpace(aDisplayName.Size());
	iDisplayName=(TUint16*)aPool->GetNewSpace(iDisplayNameMaxLength<<1);
	TPtr displayName(iDisplayName,iDisplayNameMaxLength);
	displayName.Copy(aDisplayName);
	iFlags|=EEnhancedName;
	
	return KErrNone;
}
*/

TInt TFileDirEntry::AppendMetadataDisplayName(TFileName &aFileName) const
{
	TUint nr2Append(iModificationTime);
	TBool appendAtTheEnd(ETrue);
	//TInt extraLength(0);
	const TDesC *format(NULL);
	
	if(iFlags&EIsArtist)
	{
		nr2Append=ChildrenCount();
		if(nr2Append<=0)return KErrNone; //nothing to append
		if(nr2Append==1)
		{
			format=&KOneAlbumFormat;
			//extraLength=KOneAlbumExtraLength;
			nr2Append=0; //so that we do not use it
		}
		else
		{
			format=&KNrAlbumsFormat;
			//extraLength=KNrAlbumsExtraLength;
		}
	}
	else if(iFlags&EIsAlbum)
	{
		if(nr2Append<KMetadataMinYear || nr2Append>KMetadataMaxYear)return KErrNone; //nothing to append
		format=&KYearFormat;
		//extraLength=KYearExtraLength;
	}
	else if(iFlags&EIsSong)
	{
		if(nr2Append<KMetadataMinTrack || nr2Append>KMetadataMaxTrack)return KErrNone; //nothing to append
		format=&KTrackNrFormat;
		//extraLength=KTrackNrExtraLength;
		appendAtTheEnd=EFalse;
	};
	//else ASSERT something
		
	TPtr name(iName,iNameLength,iNameLength);

	if(appendAtTheEnd)
	{
		aFileName.Append(name);
		if(nr2Append)
			aFileName.AppendFormat(*format,nr2Append);
		else
			aFileName.Append(*format);
	}
	else
	{
		if(nr2Append)
			aFileName.AppendFormat(*format,nr2Append);
		else
			aFileName.Append(*format);
		aFileName.Append(name);
	};
	return KErrNone;
}

void TFileDirEntry::GetStartingDirL(RPointerArray<HBufC>& aList, RArray<TUint>& aListCount, TFileName &aTmpPath)
{
    /*we check if we have something to add to the list
     * -if a child has the same amount of files as its parent, we continue with that child.
     * -if a child has at least 90% of the files as its parent and those 10% are less than 10, we continue with that child.
     * -if 2 children share all the music within them but there are at least 5 children, we continue with those 2 children.
     * -if none of the above is true and we have music in the current folder, we add the current folder.
     */
	if(LOG_ENABLED_FLAG(ELogNewSources))
	{
		GetFullPath(aTmpPath);
		LOG(ELogNewSources,1,"GetStartingDirL start (%d music files in %S, number of children: %d)",iNrMusicFiles,&aTmpPath,ChildrenCount());
	};
	
    TInt i,max1(-1),posMax1(-1),max2(-1),posMax2(-1);
    for(i=0;i<ChildrenCount();i++)
    {
        TFileDirEntry &child=(*iChildren)[i];
    	if( child.iNrMusicFiles>max1 && child.iFlags&EIsDir)
        {
        	max2=max1;
        	posMax2=posMax1;
            max1=child.iNrMusicFiles;
            posMax1=i;
        }
        else if(child.iNrMusicFiles>max2 && child.iFlags&EIsDir)
        {
            max2=child.iNrMusicFiles;
            posMax2=i;
        };
    };

    if(LOG_ENABLED_FLAG(ELogNewSources))
    {
    	if(max1>=0)
    	{
    		(*iChildren)[posMax1].GetFullPath(aTmpPath);
    		LOG(ELogNewSources,0,"max1: %d music files in %S",max1,&aTmpPath);
    	}
    	else LOG(ELogNewSources,0,"max1: %d",max1);
    	if(max2>=0)
    	{
    	(*iChildren)[posMax2].GetFullPath(aTmpPath);
    	LOG(ELogNewSources,0,"max2: %d music files in %S",max2,&aTmpPath);
    	}
    	else LOG(ELogNewSources,0,"max2: %d",max2);
    };
    
    TBool addThisFolder(EFalse);
    if(iNrMusicFiles>0 && max1==iNrMusicFiles)
    {
    	LOG(ELogNewSources,0,"All music files in max1. Check max1.");
    	//if all children in max1 are files and this is not a drive, then add this to sources
    	if(!(iNameLength==2 && iParent==NULL))
    	{
    		//if we are here, this is not a drive. Check children
    		TBool allChildrenAreFiles=ETrue;
    		for(i=0;i<(*iChildren)[posMax1].ChildrenCount();i++)
    			if((*(*iChildren)[posMax1].iChildren)[i].iFlags&EIsDir)
    			{
    				//we found a folder ...
    				allChildrenAreFiles=EFalse;
    				break;
    			}
    		if(allChildrenAreFiles)
    		{
    			addThisFolder=ETrue;
    			LOG(ELogNewSources,0,"max1 has only files and this folder is not a drive. Will add this folder.");
    		}
    	}
    	(*iChildren)[posMax1].GetStartingDirL(aList,aListCount,aTmpPath);
    }
    else if(iNrMusicFiles>0 && max1>iNrMusicFiles*0.9 && (iNrMusicFiles-max1)<10)
    {
    	LOG(ELogNewSources,0,"More than 90%% of music files in max1 and remaining music files are less than 10. Check max1.");
    	(*iChildren)[posMax1].GetStartingDirL(aList,aListCount,aTmpPath);
    }
    else if((max1+max2)==iNrMusicFiles && ChildrenCount()>5)
    {
    	LOG(ELogNewSources,0,"All music files in max1 and max2. This folder has more than 5 children (%d). Check max1 and max2.",ChildrenCount());
    	(*iChildren)[posMax1].GetStartingDirL(aList,aListCount,aTmpPath);
    	(*iChildren)[posMax2].GetStartingDirL(aList,aListCount,aTmpPath);
    }
    else if(iNrMusicFiles>0)
    {
    	addThisFolder=ETrue;
    	if(iNameLength==2 && iParent==NULL) //this should be a drive, e.g. e:
    	{
    		//this is a root folder. Do not add it to sources, but add all the children
    		LOG(ELogNewSources,0,"This is a root/drive folder and music files are distributed among children (nr children=%d). Checking all children with music",ChildrenCount());
    		addThisFolder=EFalse;
    		for(i=0;i<ChildrenCount();i++)
    		{
    			TFileDirEntry &child=(*iChildren)[i];
    			if(child.iNrMusicFiles>0 && !(child.iFlags&EIsDir))
    			{
    				addThisFolder=ETrue; //this means there are music files in the root folder. We should add it!
    				LOG(ELogNewSources,0,"There are music files in the root folder. We are forced to add it!");
    				break;
    			};
    		};
    		if(!addThisFolder)
    			for(i=0;i<ChildrenCount();i++)
    			{
    				TFileDirEntry &child=(*iChildren)[i];
    				if(child.iNrMusicFiles>0 && child.iFlags&EIsDir)
    					child.GetStartingDirL(aList,aListCount,aTmpPath);
    			};
    	};
    	if(addThisFolder)
    	{
    		LOG(ELogNewSources,0,"Music files are distributed among children (nr children=%d). Will add this folder to sources",ChildrenCount());
    	}
    };
    if(addThisFolder)
    {
    	LOG(ELogNewSources,0,"Adding current folder to sources",ChildrenCount());
    	GetFullPath(aTmpPath);
    	HBufC *path2insert=HBufC::NewLC(aTmpPath.Length());
    	path2insert->Des().Copy(aTmpPath);
    	aList.AppendL(path2insert);
    	aListCount.AppendL(iNrMusicFiles);
    	CleanupStack::Pop(path2insert);
    };
    if(LOG_ENABLED_FLAG(ELogNewSources))
    {
    	GetFullPath(aTmpPath);
    	LOG(ELogNewSources,-1,"GetStartingDirL end (%S)",&aTmpPath);
    };
}

void TFileDirEntry::SaveSelection(RFile& aSelection, TBuf8<KMaxFileName> &aTemp8FileName, TInt aDepth)
{
    if(!(iFlags & EIsSelected) && !(iFlags & EIsPartiallySelected) && !(iFlags & EIsSource))return; //we only add those elements that are selected

    TInt i;
    //put spaces for the depth
    for(i=0;i<aDepth;i++)
        aSelection.Write(KSpaces8);
    //write current element to the file
    if(aDepth==0)
    	aSelection.Write(_L8("<source path=\""));
    else
    	if(iFlags & EIsDir) aSelection.Write(_L8("<folder path=\""));
    	else aSelection.Write(_L8("<song path=\""));
    
    //create the song name as UTF8
    TPtr name(iName,iNameLength,iNameLength);
    CnvUtfConverter::ConvertFromUnicodeToUtf8(aTemp8FileName,name);
    //write the name
    aSelection.Write(aTemp8FileName);
    //write selection
    aSelection.Write(_L8("\" selected=\""));
    if(iFlags&EIsSelected) aSelection.Write(_L8("yes"));
    else aSelection.Write(_L8("no")); //this will not happen, due to the if entry at the begining of this function
    //write the end of the element
    aSelection.Write(_L8("\">"));
    aSelection.Write(KEOL8);

    TBool allSelected=AllChildrenAndSubchildrenSelected();
    if(((iFlags & EIsDir) && !allSelected) || (iFlags & EIsSource))
    {
        //if we are here, this is a DIR, and NOT all children and subchildren are selected
        //if all children are selected, we do not need to add them, as adding the dir would add all children as well
        for(i=0;i<ChildrenCount();i++)
            (*iChildren)[i].SaveSelection(aSelection,aTemp8FileName,aDepth+1);
    };

    //end the element
    //put spaces for the depth
    for(i=0;i<aDepth;i++)
        aSelection.Write(KSpaces8);
    //write the end tag & EOL
    if(aDepth==0)
    	aSelection.Write(_L8("</source>"));
    else
    	if(iFlags & EIsDir) aSelection.Write(_L8("</folder>"));
    	else aSelection.Write(_L8("</song>"));
    aSelection.Write(KEOL8);
}

TBool TFileDirEntry::AllChildrenAndSubchildrenSelected() //returns ETrue if all children and subchildren are selected
{
    TInt i;
    for(i=0;i<ChildrenCount();i++)
    {
        if(!( (*iChildren)[i].iFlags & EIsSelected))return EFalse; //one of the children is not selected
        if(! (*iChildren)[i].AllChildrenAndSubchildrenSelected())return EFalse; //one of the grandchildren is not selected
    };
    return ETrue; //if we reached this point, all children and grandchildren are selected
}

TFileDirEntry* TFileDirEntry::GetProperParent(CFileDirPool *aPool)
{
	if(iParent && (iParent->iFlags&EIsSource))
		return aPool->iRoot;
	else return iParent;
}

TFileDirEntry *TFileDirEntry::GetSource()
{
	if(iFlags&EIsSource)
	{
		__ASSERT_ALWAYS(!iParent,Panic(ESourceInconsistency));
		return this;
	}
	else return iParent->GetSource();
}

TInt TFileDirEntry::GetSourceLength()
{
	if(iFlags&EIsSong)
		return ((TFileDirEntry*)iSelectedPosition)->GetSourceLength();
	if(iFlags&EIsSource)
	{
		__ASSERT_ALWAYS(!iParent,Panic(ESourceInconsistency));
		return iNameLength;
	}
	else return iParent->GetSourceLength();
}

TInt TFileDirEntry::GetLength()
{
	if(iFlags&EIsSong)
		return ((TFileDirEntry*)iSelectedPosition)->GetLength();
	if(iFlags&EIsSource)
	{
		__ASSERT_ALWAYS(!iParent,Panic(ESourceInconsistency));
		return iNameLength;
	}
	else return iParent->GetLength()+1+iNameLength; //1 is for the \ delimitator
}

void TFileDirEntry::SetModificationTime(const TTime &aModificationTime)
{
	TInt64 t(aModificationTime.Int64());
	t>>=6; //this is almost seconds in value
	iModificationTime=(TUint)(t-KSecondsTo2000);
}

TBool TFileDirEntry::HasSameModificationTime(const TTime &aModificationTime)
{
	if(iModificationTime==0)return EFalse;
	TInt64 t(aModificationTime.Int64());
	t>>=6; //this is almost seconds in value
	TUint mt=(TUint)(t-KSecondsTo2000);
	if(mt==iModificationTime)return ETrue;
	else return EFalse;
}

void TFileDirEntry::MarkAsDeleted() //including children/subtree
{
	iFlags|=EDeleted;
	if(iChildren)
		iChildren->MarkAllAsDeleted();
}

void TFileDirEntry::UnmarkAsDeleted()
{
	if(iFlags&EDeleted)
	{
		iFlags&=~EDeleted;
		if(iChildren)
			iChildren->UnmarkAllAsDeleted();
	}
}

void TFileDirEntry::ReviewChildren()
{
	__ASSERT_ALWAYS(iChildren,Panic(EBadCode5));
	iChildren->ReviewChildren();
}

///////////////////////////////////////////////////////////////////////////////////////
const TInt KArrayGranularity=16;

CSegmentedBuffer::CSegmentedBuffer(TInt aSegmentLength) : iSegments(KArrayGranularity), iSegmentLengths(KArrayGranularity), 
                                                          iSegLen(aSegmentLength), iLastSegmentLength(0), iLastSegment(NULL)
{};
                                                          
CSegmentedBuffer::~CSegmentedBuffer()
{
	iSegments.ResetAndDestroy();
	iSegmentLengths.Close();
}

void CSegmentedBuffer::Reset()
{
	iSegments.ResetAndDestroy();
	iSegmentLengths.Reset();
	//iSize=0;
	iLastSegmentLength=0;
	iLastSegment=NULL;
}

TUint8* CSegmentedBuffer::ExpandL(TInt aSize)
{
	//LOG(ELogNewSources,1,"CSegmentedBuffer::ExpandL++ (size=%d)",aSize);
	//if(aSize&3)aSize=((aSize>>2)+1)<<2;//if aSize is not a multiple of 4, it is rounded up to the next multiple of 4
	aSize=((aSize+3)>>2)<<2; //if aSize is not a multiple of 4, it is rounded up to the next multiple of 4
	__ASSERT_DEBUG(iSegments.Count()==iSegmentLengths.Count(),Panic(ECountsNotSame));
	if(!iSegments.Count() || (aSize+iLastSegmentLength>iSegLen))
	{
		if(iSegments.Count()>0)//update the length of last segment
			iSegmentLengths[iSegments.Count()-1]=iLastSegmentLength;
		//we need to allocate a new segment & size
		iLastSegment=(TUint8*)User::AllocL(iSegLen);
		iSegments.AppendL(iLastSegment);
		iSegmentLengths.AppendL(0);
		iLastSegmentLength=0;
	};
	//we should have enough memory in the last segment
	__ASSERT_DEBUG(aSize+iLastSegmentLength<=iSegLen,Panic(ENotEnoughMemory));
	TUint8 *ptr=iLastSegment+iLastSegmentLength;
	iLastSegmentLength+=aSize;
	//LOG(ELogNewSources,-1,"CSegmentedBuffer::ExpandL-- (ptr=%x)",ptr);
	return ptr;
}

void CSegmentedBuffer::GetMemoryInfo(TInt &aNrSegments, TInt &aSegmentLength)
{
	aNrSegments=iSegments.Count();
	aSegmentLength=iSegLen;
}

///////////////////////////////////////////////////////////////////////////////////////

CPlaylist::CPlaylist() 
{};

CPlaylist::CPlaylist(TInt aGranularity) : iPlaylist(aGranularity)
{};

CPlaylist::~CPlaylist()
{
	Recycle();
}

void CPlaylist::Recycle()
{
	iPool=NULL;
	iPlaylist.Reset();
	iTotalDurationInMiliseconds=0;
	iCurrentElement=0;
	delete iName;
	iName=NULL;
	iNrElementsWhenLoaded=0;
	iCurrentPlaybackPosition=0;
}

TInt CPlaylist::Shuffle(TBool aFromCurrent/*=EFalse*/)
{
	//shufle by exchanging some pairs
	LOG(ELogGeneral,0,"CPlaylist::Shuffle++ (from current: %d)",aFromCurrent);
	//first, generate a random string
	HBufC8 *rnd=HBufC8::NewL(iPlaylist.Count()*2);
	TPtr8 ptr=rnd->Des();
	ptr.SetLength(iPlaylist.Count()*2);
	TRandom::Random(ptr);
	//
	TInt i,index1,index2,start(0);
	TFileDirEntry *temp;
	if(aFromCurrent && iCurrentElement>=0 && iCurrentElement+1<iPlaylist.Count())
		start=iCurrentElement+1;
	
	for(i=iPlaylist.Count()-1;i>start;i--) //(i>=1)
	{
		index1=i;
		index2=(ptr[i<<1]<<8)+ptr[(i<<1)+1];
		index2%=(i+1-start);
		index2+=+start;
		//LOG0("Rndval: %d",index2);
		//make the switch
		temp=iPlaylist[index1];
		iPlaylist[index1]=iPlaylist[index2];
		iPlaylist[index2]=temp;
	};      
	delete rnd;
	if(!aFromCurrent)
    	iCurrentPlaybackPosition=iCurrentElement=0;

	LOG(ELogGeneral,0,"CPlaylist::Shuffle--");
	return KErrNone;
}

TInt CPlaylist::Append(TFileDirEntry *aEntry)
{
	//check if the entry is from an audiobook source
	if(iIsAudiobook && !(aEntry->GetSource()->iFlags&TFileDirEntry::EAudiobookSource))
		iIsAudiobook=EFalse;
	TInt err=iPlaylist.Append(aEntry);
	if(!err)
		iTotalDurationInMiliseconds+=aEntry->iNrMusicFiles;
	return err;
}

TInt CPlaylist::ExportPlaylist(RFs& aFs, const TDesC& aFilename)
{
	RFile playlistFile;
	playlistFile.Replace(aFs,aFilename,EFileWrite);
	TUint8 *fileBuf=(TUint8*)User::AllocL(KCacheFileBufferSize);
	TPtr8 fileBufPtr(fileBuf,KCacheFileBufferSize);
	
	fileBufPtr.Copy(_L8("#EXTM3U"));
	fileBufPtr.Append(KEOL8);

	//add the files
	TInt i;
	TInt size;
	TFileName currentFilename;
	//TBuf8<KMaxFileName+2> song8; //+2 from KEOL8().Length()
	for(i=0;i<iPlaylist.Count();i++)
	{
		iPlaylist[i]->GetFullPath(currentFilename);
		size=fileBufPtr.Size();
		if(currentFilename.Size()+KEOL8().Size()+size>KCacheFileBufferSize)
		{
			//we need to write data to file
			playlistFile.Write(fileBufPtr);
			fileBufPtr.SetLength(0);
			size=0;
		};

		TPtr8 song8(fileBuf+size,KCacheFileBufferSize-size);
		CnvUtfConverter::ConvertFromUnicodeToUtf8(song8,currentFilename);
		song8.Append(KEOL8);
		fileBufPtr.SetLength(size+song8.Length());
	};
	//write the remaining buffer to playlist
	if(fileBufPtr.Size()>0)
		playlistFile.Write(fileBufPtr);

	
	//close playlist file
	playlistFile.Close();
	User::Free(fileBuf);
	return KErrNone;
}
/*
TInt CPlaylist::SaveCurrentPlaylistOld(CMLauncherPreferences *aPreferences, RFs& aFs, TInt aPlaybackPosition)
{
	LOG(ELogGeneral,1,"SaveCurrentPlaylist: start (count=%d, current=%d, playback position=%d)",iNrElements,iCurrentElement,aPlaybackPosition);
	RFile playlistFile;
	TInt err=playlistFile.Replace(aFs,aPreferences->PrivatePathPlus(KPlaylistFileNameBin),EFileWrite);
	if(err)
	{
		LOG(ELogGeneral,-1,"SaveCurrentPlaylist: end (error opening file: %d)",err);
		return err;
	};
	TUint8 *fileBuf=(TUint8*)User::AllocL(KCacheFileBufferSize);
	TPtr8 fileBufPtr(fileBuf,KCacheFileBufferSize);
	TInt fileBufLen;
	TInt i;
	
	__ASSERT_ALWAYS(iNrElements==iPlaylist.Count(),Panic(ENrOfElementsMismatch));
	//check if we have all the elements
	if(iMark1!=-1 || iMark2!=-1 || iMark3!=-1)GetProperIndex(iNrElements-1);
	//check the playlist for invalid entries that should not be saved
	for(i=0;i<iNrElements;i++)
	{
		if(!iPlaylist[i] || iPlaylist[i]->iFlags&TFileDirEntry::EWasNotFound)
		{
			//we need to remove this element
			iPlaylist.Remove(i);
			iNrElements--;
			if(iCurrentElement>i)
				iCurrentElement--;
			i--;
		};
	};

	//write the playlist length, current position, nr of files after and before, current playback position
	//4 TInts
	TInt extraAfterBefore(KExtraAfterBefore);
	//fileBufPtr.Copy()
	fileBufPtr.Append((TUint8*)&iNrElements,sizeof(TInt));
	fileBufPtr.Append((TUint8*)&iCurrentElement,sizeof(TInt));
	fileBufPtr.Append((TUint8*)&extraAfterBefore,sizeof(TInt));
	fileBufPtr.Append((TUint8*)&aPlaybackPosition,sizeof(TInt));
	
	//append 2*extraAfterBefore+1 songs
	TInt start(iCurrentElement-extraAfterBefore);
	if(start<0)start=0;
	TInt end(iCurrentElement+extraAfterBefore);
	if(end>=iNrElements)end=iNrElements-1;
	TInt srcLen;
	TPtr filename(NULL,0);
	fileBufLen=fileBufPtr.Length();

	for(i=start;i<=end;i++)
	{
		if(KCacheFileBufferSize-fileBufLen<(KMaxFileName<<1)+sizeof(TUint16))
		{
			//dump the buffer into the file
			fileBufPtr.Set(fileBuf,fileBufLen,KCacheFileBufferSize);
			playlistFile.Write(fileBufPtr);
			fileBufLen=0;
		};
		
		filename.Set((TUint16*)(fileBuf+fileBufLen+sizeof(TUint16)),0,KCacheFileBufferSize-fileBufLen-sizeof(TUint16));
		iPlaylist[i]->GetFullPath(filename);
		*(TUint8*)(fileBuf+fileBufLen)=srcLen=iPlaylist[i]->GetSourceLength(); //source length
		*(TUint8*)(fileBuf+fileBufLen+1)=filename.Length()-srcLen; //rest of the file
		//update fileBufLen
		fileBufLen+=(sizeof(TUint16)+filename.Size());
	};

	//the beginning of the playlist
	for(i=0;i<start;i++)
	{
		if(KCacheFileBufferSize-fileBufLen<(KMaxFileName<<1)+sizeof(TUint16))
		{
			//dump the buffer into the file
			fileBufPtr.Set(fileBuf,fileBufLen,KCacheFileBufferSize);
			playlistFile.Write(fileBufPtr);
			fileBufLen=0;
		};

		filename.Set((TUint16*)(fileBuf+fileBufLen+sizeof(TUint16)),0,KCacheFileBufferSize-fileBufLen-sizeof(TUint16));
		iPlaylist[i]->GetFullPath(filename);
		*(TUint8*)(fileBuf+fileBufLen)=srcLen=iPlaylist[i]->GetSourceLength(); //source length
		*(TUint8*)(fileBuf+fileBufLen+1)=filename.Length()-srcLen; //rest of the file
		//update fileBufLen
		fileBufLen+=(sizeof(TUint16)+filename.Size());
	};

	//the end of the playlist
	for(i=end+1;i<iNrElements;i++)
	{
		if(KCacheFileBufferSize-fileBufLen<(KMaxFileName<<1)+sizeof(TUint16))
		{
			//dump the buffer into the file
			fileBufPtr.Set(fileBuf,fileBufLen,KCacheFileBufferSize);
			playlistFile.Write(fileBufPtr);
			fileBufLen=0;
		};

		filename.Set((TUint16*)(fileBuf+fileBufLen+sizeof(TUint16)),0,KCacheFileBufferSize-fileBufLen-sizeof(TUint16));
		iPlaylist[i]->GetFullPath(filename);
		*(TUint8*)(fileBuf+fileBufLen)=srcLen=iPlaylist[i]->GetSourceLength(); //source length
		*(TUint8*)(fileBuf+fileBufLen+1)=filename.Length()-srcLen; //rest of the file
		//update fileBufLen
		fileBufLen+=(sizeof(TUint16)+filename.Size());
	};

	//dump the remaining of the buffer
	fileBufPtr.Set(fileBuf,fileBufLen,KCacheFileBufferSize);
	playlistFile.Write(fileBufPtr);
	//close playlist file
	playlistFile.Close();
	User::Free(fileBuf);
	//save the index & position
	//aPreferences->iPlaylistIndex=iCurrentElement;			
	//aPreferences->iPosition=aPlaybackPosition;
	aPreferences->iFlags|=CMLauncherPreferences::ESavePreferences;
			
	LOG(ELogGeneral,-1,"SaveCurrentPlaylist: end");
	return KErrNone;
}*/

TInt CPlaylist::SaveCurrentPlaylist(CMLauncherDocument &aDoc)
{
	LOG(ELogGeneral,1,"SaveCurrentPlaylist: start (count=%d)",iPlaylist.Count());
	RFile playlistFile;
	TBuf<KPlaylistFileNameFormatLength> playlistFilename;
	playlistFilename.Format(KPlaylistFileNameFormat,iMyPlaylistIndex);
	
	TInt err=playlistFile.Replace(aDoc.iFs,aDoc.iPreferences->PrivatePathPlus(playlistFilename),EFileWrite);
	if(err)
	{
		LOG(ELogGeneral,-1,"SaveCurrentPlaylist: end (error opening file: %d)",err);
		return err;
	};
	TUint8 *fileBuf=(TUint8*)User::AllocL(KCacheFileBufferSize);
	TPtr8 fileBufPtr8(fileBuf,KCacheFileBufferSize);
	TPtr fileBufPtr(NULL,0);
	TInt fileBufLen;
	TInt i;
	
	//check if we have all the elements
	for(i=0;i<iPlaylist.Count();i++)
	{
		if(!iPlaylist[i] || iPlaylist[i]->iFlags&TFileDirEntry::EWasNotFound)
		{
			//we need to remove this element
			iPlaylist.Remove(i);
			if(iCurrentElement<i)
				iCurrentElement--;
			i--;
		};
	};

	//write the playlist length
	//one TInt
	i=iPlaylist.Count();
	fileBufPtr8.Append((TUint8*)&i,sizeof(TInt));
	fileBufLen=fileBufPtr8.Length();
	
	TUint16 offset(0);
	//dump offsets
	for(i=0;i<iPlaylist.Count();i++)
	{
		if(KCacheFileBufferSize-fileBufLen<(TInt)sizeof(TUint))
		{
			//dump the buffer into the file
			fileBufPtr8.Set(fileBuf,fileBufLen,KCacheFileBufferSize);
			playlistFile.Write(fileBufPtr8);
			fileBufLen=0;
		};
		offset=2+(iPlaylist[i]->GetLength()<<1); //the extra 2 is for the source length
		*(TUint16*)(fileBuf+fileBufLen)=offset;
		fileBufLen+=2;
	};
	
	//dump source lengths + filenames
	for(i=0;i<iPlaylist.Count();i++)
	{
		if(KCacheFileBufferSize-fileBufLen<(TInt)((KMaxFileName<<1)+sizeof(TUint8)))
		{
			//dump the buffer into the file
			fileBufPtr8.Set(fileBuf,fileBufLen,KCacheFileBufferSize);
			playlistFile.Write(fileBufPtr8);
			fileBufLen=0;
		};

		fileBufPtr.Set((TUint16*)(fileBuf+fileBufLen+sizeof(TUint16)),0,KCacheFileBufferSize-fileBufLen-sizeof(TUint16));
		iPlaylist[i]->GetFullPath(fileBufPtr);
		*(TUint16*)(fileBuf+fileBufLen)=iPlaylist[i]->GetSourceLength()<<1; //source length
		//update fileBufLen
		fileBufLen+=sizeof(TUint16)+fileBufPtr.Size();
	};

	//dump the remaining of the buffer
	fileBufPtr8.Set(fileBuf,fileBufLen,KCacheFileBufferSize);
	playlistFile.Write(fileBufPtr8);
	//close playlist file
	playlistFile.Close();
	User::Free(fileBuf);
	//save the index & position
	aDoc.iPreferences->iCFlags|=CMLauncherPreferences::ESavePreferences;
			
	LOG(ELogGeneral,-1,"SaveCurrentPlaylist: end");
	return KErrNone;
}

void CPlaylist::LoadPlaylistL(CMLauncherDocument &aDoc)
{
	//build the playlist;
	LOG(ELogGeneral,1,"LoadPlaylistL++");
	//check if the playlist has been loaded already
	if(iPool==aDoc.iFilesTree)
	{
		LOG(ELogGeneral,-1,"LoadPlaylistL-- (playlist already loaded)");
		return;
	}
		
	//if we are here, the playlist was not loaded
	iPool=aDoc.iFilesTree;
	TBuf<KPlaylistFileNameFormatLength> playlistFilename;
	playlistFilename.Format(KPlaylistFileNameFormat,iMyPlaylistIndex);
		
	RFile pls;
	TInt err=pls.Open(aDoc.iFs,aDoc.iPreferences->PrivatePathPlus(playlistFilename),EFileRead|EFileShareReadersOnly);
	if(err)
	{
		//try loading the legacy, text playlist
		User::LeaveIfError(LoadLegacyPlaylistV1(aDoc,GetCurrentIndex(),iCurrentPlaybackPosition));
	};
	
	TInt plsSize, pos=0;
	pls.Size(plsSize);
	if(plsSize<4)
	{
		pls.Close();
		LOGleave(KErrCorrupt,"LoadPlaylistL-- (corrupted, length(%d)<4)",plsSize);
	}
		
	TUint8 *buf=(TUint8*)User::Alloc(plsSize);
	if(!buf)
	{
		pls.Close();
		LOGleave(KErrNoMemory,"LoadPlaylistL-- (failed, no memory)");
	}
	TPtr8 fileBufPtr8(buf,plsSize);
	pls.Read(fileBufPtr8);
	pls.Close(); //done with the file
	
	//read the number of elements
	TInt nrElements=*(TInt*)buf;
	pos=sizeof(TInt)+(nrElements<<1);
	LOG0("Total elements in playlist: %d",nrElements);
		
	//read the elements
	TUint16 pathSize,sourceSize;
	TPtr source(NULL,0),path(NULL,0);
	TInt i;
	TFileDirEntry *elm;
	for(i=0;i<nrElements;i++)
	{
		pathSize=*(TUint16*)(buf+sizeof(TInt)+(i<<1));
		sourceSize=*(TUint16*)(buf+pos);
		source.Set((TUint16*)(buf+pos+2),sourceSize>>1,sourceSize>>1);
		path.Set((TUint16*)(buf+pos+2+sourceSize),(pathSize-sourceSize-2)>>1,(pathSize-sourceSize-2)>>1);
		//LOG0("File %d: %S %S",i,&source,&path);
		elm=iPool->CreateEntriesFromSourceAndPath(source,path);
		if(elm) //it may be NULL
			iPlaylist.Append(elm);
		pos+=pathSize;
	}
	//close stuff
	User::Free(buf);
	LOG(ELogGeneral,-1,"LoadPlaylistL--");
}

/*
TInt CPlaylist::LoadLegacyPlaylistV2(CMLauncherPreferences *aPreferences, RFs& aFs, CFileDirPool *aPool, TInt aLegacyCurrentElement, TInt aLegacyPosition)
{
	//build the playlist;
	TBool aForceLoadAll(ETrue);
	LOG0("LoadLegacyPlaylistV2: start");
	iPool=aPool;
	iMark1=iMark2=iMark3=-1;
	TInt err=iFPlaylist.Open(aFs,aPreferences->PrivatePathPlus(KPlaylistFileNameBin),EFileRead|EFileShareReadersOnly);
	if(err)
	{
		//try loading the legacy, text playlist
		return LoadLegacyPlaylistV1(aPreferences,aFs,aLegacyCurrentElement,aLegacyPosition);
	};
	
	if(!iFileBuf)
		iFileBuf=(TUint8*)User::AllocL(KCacheFileBufferSize);
	TPtr8 fileBufPtr(iFileBuf,KCacheFileBufferSize);
	iFPlaylist.Read(fileBufPtr);
	iFileBufLen=fileBufPtr.Length();
	iPos=0;
	if(iFileBufLen<16)
	{
		LOG0("LoadLegacyPlaylistV2: end (playlist corrupted)");
		iFPlaylist.Close();
		return KErrCorrupt;
	};
	
	TInt extraAfterBefore,playbackPosition;
	iNrElements=*(TInt*)iFileBuf;
	iPos+=sizeof(TInt);
	iCurrentElement=*(TInt*)(iFileBuf+iPos);
	iPos+=sizeof(TInt);
	extraAfterBefore=*(TInt*)(iFileBuf+iPos);
	iPos+=sizeof(TInt);
	playbackPosition=*(TInt*)(iFileBuf+iPos);
	iPos+=sizeof(TInt);
	
	//compute start an end
	iMark2=iCurrentElement-extraAfterBefore;
	if(iMark2<0)iMark2=0;
	iMark3=iCurrentElement+extraAfterBefore;
	if(iMark3>=iNrElements)iMark3=iNrElements-1;
	
	LOG0("Total elements: %d, current element: %d",iNrElements,iCurrentElement);
	LOG0("Start=%d, End=%d",iMark2,iMark3);
	ReadLegacyInterval(iMark3-iMark2+1,-1);
	
	if(aForceLoadAll)
	{
		if(iMark2>0)
			ReadLegacyInterval(iMark2,0);
		if(iMark3<iNrElements)
			ReadLegacyInterval(iNrElements-iMark3-1,-1);
		iMark1=iMark2=iMark3=-1;
		//close stuff
		iFPlaylist.Close();
		User::Free(iFileBuf);
		iFileBuf=NULL;
	}
	else
	{
		iMark1=0;
		if(iMark1==iMark2)
			iMark1=iMark2=-1;
		iMark3++; //iMark3 is already read
		if(iMark3>=iNrElements)
			iMark3=-1;
			
		if(iMark1==-1 && iMark2==-1 && iMark3==-1)
		{
			//close stuf
			iFPlaylist.Close();
			User::Free(iFileBuf);
			iFileBuf=NULL;
		}
		else
		{
			iJobs|=EJobReadPlaylistFromFile;
		};
	};
	
	//we delete the legacy playlist
	aFs.Delete(aPreferences->PrivatePathPlus(KPlaylistFileNameBin));
	LOG0("LoadLegacyPlaylistV2: end (mark1=%d, mark2=%d, mark3=%d)",iMark1,iMark2,iMark3);
	
	return KErrNone;
}

void CPlaylist::ReadLegacyInterval(TInt aInterval,TInt aInsertPosition)
{
	//read first elements
	LOG(ELogGeneral,1,"ReadLegacyInterval: start (interval=%d, position=%d)",aInterval,aInsertPosition);
	TInt i;
	TUint8 sourceLen,pathLen;
	TPtr source(NULL,0),path(NULL,0);
	TPtr8 fileBufPtr(NULL,0);

	for(i=0;i<aInterval;i++)
	{
		//first, we read 2 bytes, the length of the source and remaining path
		if(iPos+2>iFileBufLen)
		{
			//we need to read some more data
			//but first, we need to copy our data at the beginning of the buffer
			Mem::Copy(iFileBuf,iFileBuf+iPos,iFileBufLen-iPos);
			iFileBufLen-=iPos;
			iPos=0;
			fileBufPtr.Set(iFileBuf+iFileBufLen,0,KCacheFileBufferSize-iFileBufLen);
			iFPlaylist.Read(fileBufPtr);
			iFileBufLen+=fileBufPtr.Length();
		};

		__ASSERT_DEBUG(iFileBufLen>=2,Panic(EFileBufLenLessThan2));
		sourceLen=*(iFileBuf+iPos);
		iPos++;
		pathLen=*(iFileBuf+iPos);
		iPos++;
		if(iPos+(sourceLen<<1)+(pathLen<<1)>iFileBufLen)
		{
			//we need to read some more data
			//but first, we need to copy our data at the beginning of the buffer
			Mem::Copy(iFileBuf,iFileBuf+iPos,iFileBufLen-iPos);
			iFileBufLen-=iPos;
			iPos=0;
			fileBufPtr.Set(iFileBuf+iFileBufLen,0,KCacheFileBufferSize-iFileBufLen);
			iFPlaylist.Read(fileBufPtr);
			iFileBufLen+=fileBufPtr.Length();
		};

		//read the source and the path
		source.Set((TUint16*)(iFileBuf+iPos),sourceLen,sourceLen);
		iPos+=(sourceLen<<1);
		path.Set((TUint16*)(iFileBuf+iPos),pathLen,pathLen);
		iPos+=(pathLen<<1);

		//create an entry into the CFileDirPool
		//LOG2("Read source: %S (length: %u)",&source,sourceLen);
		//LOG2("Read path: %S (length: %u)",&path,pathLen);
		if(aInsertPosition<0)
			iPlaylist.Append(iPool->CreateEntriesFromSourceAndPath(source,path));
		else
			iPlaylist.Insert(iPool->CreateEntriesFromSourceAndPath(source,path),aInsertPosition+i);
		//LOG("After append");
	};
	LOG(ELogGeneral,-1,"ReadLegacyInterval: end");
}
*/
TInt CPlaylist::LoadLegacyPlaylistV1(CMLauncherDocument &aDoc, TInt aLegacyCurrentElement, TInt aLegacyPosition)
{
	iCurrentElement=0;
	iCurrentPlaybackPosition=0;
	
	LOG(ELogGeneral,1,"LoadLegacyPlaylist: start");
	
	RFile fplaylist;
	TInt err=fplaylist.Open(aDoc.iFs,aDoc.iPreferences->PrivatePathPlus(KPlaylistFileName),EFileRead|EFileShareReadersOnly);
	if(err)return err;

	//we will read the entire file into memory and then parse it
	TInt size;
	err=fplaylist.Size(size);
	if(err)return err;
	LOG0("File opened, size=%d",size);

	HBufC8 *file=HBufC8::New(size);
	if(file==NULL)return KErrNoMemory;
	TPtr8 ptrFile(file->Des());
	fplaylist.Read(ptrFile);
	LOG0("File read into memory");

	TInt eolPos,start(0),length;
	//RPointerArray<TFileDirEntry> *playlist=new RArray<TFileDirEntry>;
	TFileName convertedFilename;
	TBuf<1> source;
	while((eolPos=ptrFile.Find(KEOL8))!=KErrNotFound)
	{
		if(start==0)
		{
			ptrFile[eolPos]='#';
			start=eolPos+2;
			continue;
		};
		length=eolPos-start;
		CnvUtfConverter::ConvertToUnicodeFromUtf8(convertedFilename,ptrFile.Mid(start,length));
		iPlaylist.Append(iPool->CreateEntriesFromSourceAndPath(source,convertedFilename));
		ptrFile[eolPos]='#';
		start=eolPos+2;	
	};
	iCurrentPlaybackPosition=aLegacyPosition;
	LOG0("Nr elements: %d, playback position: %d",iPlaylist.Count(),iCurrentPlaybackPosition);
	LOG0("Current element: %d",aLegacyCurrentElement);
	if(aLegacyCurrentElement<iPlaylist.Count())
		iCurrentElement=aLegacyCurrentElement;
	else iCurrentElement=0;

	//clean
	//CleanupStack::PopAndDestroy(file);
	delete file;
	fplaylist.Close();

	//we delete the legacy playlist
	aDoc.iFs.Delete(aDoc.iPreferences->PrivatePathPlus(KPlaylistFileName));
	LOG(ELogGeneral,-1,"LoadLegacyPlaylist: end");
	return KErrNone;
}

TFileDirEntry* CPlaylist::GetEntry(TInt aIndex, TBool aExpectValid)
{
	if(aExpectValid)
	{
		__ASSERT_ALWAYS(aIndex<iPlaylist.Count(),Panic(EPlaylistIndexOutsideRange));
		__ASSERT_ALWAYS(aIndex>=0,Panic(ENoValidPlaylistIndexFound));
		__ASSERT_ALWAYS(!(iPlaylist[aIndex]->iFlags&TFileDirEntry::EWasNotFound),Panic(EInvalidPlaylistElementFound));
	}
	else
		__ASSERT_ALWAYS(aIndex<iPlaylist.Count(),Panic(EPlaylistIndexOutsideRange));

	if(aIndex>=0)return iPlaylist[aIndex];
	else return NULL;
};

TFileDirEntry* CPlaylist::GetCurrentEntry(TBool aExpectValid)
{
	return GetEntry(iCurrentElement,aExpectValid);
};

/*
void CPlaylist::GetFilenameX(TInt aIndex,TFileName &aFilename)
{
	iPlaylist[aIndex]->GetFullPath(aFilename);
}*/

void CPlaylist::IncrementCurrent(TInt aIncrement, TBool aIncrementToNextValid, TBool *aWentOverTheTop, TBool *aNoValidElementsFound)
{
	TInt savedCurrentElement(iCurrentElement);
	IncrementIndex(iCurrentElement,aIncrement,aIncrementToNextValid,aWentOverTheTop,aNoValidElementsFound);
	if(aIncrement!=0 || savedCurrentElement!=iCurrentElement)
		iCurrentPlaybackPosition=0;
}

void CPlaylist::IncrementIndex(TInt &aIndex, TInt aIncrement, TBool aIncrementToNextValid, TBool *aWentOverTheTop, TBool *aNoValidElementsFound)
{
	LOG(ELogGeneral,1,"CPlaylist::IncrementIndex++");
	aIndex+=aIncrement;
	if(aIndex>=iPlaylist.Count())
	{
		aIndex=0;
		if(!iPlaylist.Count())aIndex=-1;
		if(aWentOverTheTop)*aWentOverTheTop=ETrue;
	}
	else if(aIndex<0)
	{
		aIndex=iPlaylist.Count()-1;
		if(aWentOverTheTop)*aWentOverTheTop=ETrue;
	}
	else if(aWentOverTheTop)*aWentOverTheTop=EFalse;
	
	if(aIndex==-1)
	{
		//the playlist does not contain elements (any more)
		if(aNoValidElementsFound)*aNoValidElementsFound=ETrue;
		return;
	}
	
	if(aIncrementToNextValid)
	{
		if(aNoValidElementsFound)*aNoValidElementsFound=EFalse; //we suppose we will find something
		
		if(!iPlaylist[aIndex] || iPlaylist[aIndex]->iFlags&TFileDirEntry::EWasNotFound)
		{
			TBool overTheTop(EFalse);
			aIndex=FirstValidIndex(aIndex,overTheTop); //this can be rather heavy function
			if(aIndex<0 && aNoValidElementsFound)*aNoValidElementsFound=ETrue;
			if(aWentOverTheTop)//we need to fill the over-the-top info
				*aWentOverTheTop|=overTheTop;
		};
			
	};
	LOG(ELogGeneral,-1,"CPlaylist::IncrementIndex--");
}

TInt CPlaylist::GetCurrentIndex()
{
	return iCurrentElement;
};



TInt CPlaylist::FirstValidIndex(TInt aIndex, TBool &aWentOverTheTop)
{
	LOG(ELogGeneral,1,"FirstValidIndex++ (aIndex=%d)",aIndex);
	__ASSERT_DEBUG(aIndex<iPlaylist.Count(),Panic(EPlaylistIndexOutsideRange));
	TInt savedIndex(aIndex);
	aWentOverTheTop=EFalse;
	while(!iPlaylist[aIndex] || iPlaylist[aIndex]->iFlags&TFileDirEntry::EWasNotFound)
	{
		/*
		//remove this element from the playlist
		iPlaylist.Remove(aIndex);
		iNrElements--;
		if(iCurrentElement>aIndex)
			iCurrentElement--;
		if(iPlaylist.Count()<=aIndex)
			aIndex=0;
			*/
		//increment aIndex
		aIndex++;
		LOG0("aIndex=%d",aIndex);
		if(aIndex>=iPlaylist.Count())
		{
			aIndex=0;
			aWentOverTheTop=ETrue;
		};
		if(aIndex==savedIndex)break; //means we have cycled the playlist once
	};
	//if(iPlaylist.Count())
	if(aIndex!=savedIndex)
	{
		LOG(ELogGeneral,-1,"FirstValidIndex-- (returning %d)",aIndex);
		return aIndex;
	}
	else 
	{
		LOG(ELogGeneral,-1,"FirstValidIndex-- (returning -1)");
		return -1;
	}
}

void CPlaylist::MetadataUpdated()
{
	if(iPool)
		iPool->iFlags|=CFileDirPool::EModified;
}

void CPlaylist::AddToPlaylist(TFileDirEntry *aCurrentEntry, TBool &aHasOggsOrFlacs)
{
    /* There are several posibilities:
     * -this is a selected file, so we add it
     * -this is a selected dir, so we go into the children
     * -this is a non-selected dir, we do not add it
     * -this is a selected dir with zero children (because we never went into it). We add it.
     */
    if(!(aCurrentEntry->iFlags & TFileDirEntry::EIsSelected) && !(aCurrentEntry->iFlags & TFileDirEntry::EIsPartiallySelected) )
        return; //nothing to add from here
	//LOG(ELogGeneral,1,"AddToPlaylist++");
    //so the element is selected or partially selected
    if(aCurrentEntry->iFlags & TFileDirEntry::EIsDir)
    {
    	//this is a dir 	
        if(!(aCurrentEntry->iFlags&TFileDirEntry::EWasExpanded))
        	iPool->MakeChildren(aCurrentEntry);//generate the children
        TInt i;
        TBool allChildrenAreFiles(ETrue);
        for(i=0;i<aCurrentEntry->ChildrenCount();i++)
        {
        	AddToPlaylist(&(*aCurrentEntry->iChildren)[i],aHasOggsOrFlacs);
        	if((*aCurrentEntry->iChildren)[i].iFlags&TFileDirEntry::EIsDir)
        		allChildrenAreFiles=EFalse;
        }
        //if all children are files, and threre is no playlist name, then name the playlist as the current dir
        //if the playlist has already a name, or if it had a name, then clear any existing name
        //LOG0("AddToPlaylist name++");
        if(allChildrenAreFiles)
        {
        	if(!iName)
        	{
        		//there is no name, we can set one
        		iName=HBufC::New(aCurrentEntry->iNameLength);
        		if(iName)
        			iName->Des().Copy(aCurrentEntry->iName,aCurrentEntry->iNameLength);
        	}
        	else
        		iName->Des().SetLength(0); //the name was not suitable because it is not unique (there are several folders selected)
        }
        //LOG0("AddToPlaylist name--");
    }
    else
    {
        //this is a file, create the song entry
    	if(aCurrentEntry->iFlags&TFileDirEntry::EIsOggOrFlac)
    		aHasOggsOrFlacs=ETrue;
    	Append(aCurrentEntry);
    };
    //LOG(ELogGeneral,-1,"AddToPlaylist--");
}

void CPlaylist::SetMetadata(TInt aNrElements, TInt aCurrentIndex, TInt aPlaybackPosition, TInt aTrackDuration, TInt aTotalPlaylistDuration, HBufC *aName, TInt aMyPlaylistIndex, TBool aIsCurrent) //transfers ownership of aName
{
	iCurrentElement=aCurrentIndex;
	iCurrentPlaybackPosition=aPlaybackPosition;
	iCurrentTrackDurationInSeconds=aTrackDuration;
	iTotalDurationInMiliseconds=(TInt64)aTotalPlaylistDuration*1000;
	if(iName)delete iName;
	iName=aName;
	iMyPlaylistIndex=aMyPlaylistIndex;
	isCurrent=aIsCurrent;
	iNrElementsWhenLoaded=aNrElements;
}

void CPlaylist::UpdatePosition(TTimeIntervalMicroSeconds &aPlaybackPosition)
{
	TInt newTime=(TInt)(aPlaybackPosition.Int64()/1000000);
	if(newTime!=iCurrentPlaybackPosition)
	{
		iCurrentPlaybackPosition=newTime;
		TInt i;
		for(i=0;i<iObservers.Count();i++)
			iObservers[i]->PlaylistTrackAndPositionUpdated(this);
	}
}

void CPlaylist::RemoveFromDisk(RFs &aFs)
{
	TBuf<KPlaylistFileNameFormatLength> playlistFilename;
	playlistFilename.Format(KPlaylistFileNameFormat,iMyPlaylistIndex);
	aFs.Delete(playlistFilename);
}

void CPlaylist::DecrementIndex(TInt aIndex,RFs &aFs)
{
	__ASSERT_ALWAYS(iMyPlaylistIndex-1==aIndex,Panic(EPlaylistDecrementIndexMismatch));
	TBuf<KPlaylistFileNameFormatLength> oldPlaylistFilename;
	TBuf<KPlaylistFileNameFormatLength> newPlaylistFilename;
	oldPlaylistFilename.Format(KPlaylistFileNameFormat,iMyPlaylistIndex);
	iMyPlaylistIndex--;
	newPlaylistFilename.Format(KPlaylistFileNameFormat,iMyPlaylistIndex);
	aFs.Rename(oldPlaylistFilename,newPlaylistFilename);
}

void CPlaylist::AddObserver(MPlaylistObserver *aObserver)
{
	iObservers.AppendL(aObserver);
}

void CPlaylist::RemoveObserver(MPlaylistObserver *aObserver)
{
	TInt i;
	for(i=0;i<iObservers.Count();i++)
		if(aObserver==iObservers[i])
		{
			iObservers.Remove(i);
			break;
		}
}

void CPlaylist::CheckForDeletedElements()
{
	LOG(ELogGeneral,1,"CPlaylist::CheckForDeletedElements++ (this=%x nrElements=%d)",this,this?iPlaylist.Count():-1);
	TInt i;
	for(i=0;i<iPlaylist.Count();i++)
		if(iPlaylist[i]->iFlags&TFileDirEntry::EDeleted)
		{
			iPlaylist.Remove(i);
			if(i<iCurrentElement)
				iCurrentElement--;
			i--;
		}
	LOG(ELogGeneral,-1,"CPlaylist::CheckForDeletedElements--");
}

void CPlaylist::RemoveOggAndFlacEntries() //TODO: this and the above can be combined
{
	LOG(ELogGeneral,1,"RemoveOggAndFlacEntries++");
	TInt i;
	for(i=0;i<iPlaylist.Count();i++)
		if(iPlaylist[i]->iFlags&TFileDirEntry::EIsOggOrFlac)
		{
			iPlaylist.Remove(i);
			if(i<iCurrentElement)
				iCurrentElement--;
			i--;
		}
	LOG(ELogGeneral,-1,"RemoveOggAndFlacEntries--");
}

void CPlaylist::RemoveCurrentElement()
{
	LOG(ELogGeneral,1,"RemoveCurrentElement++");
	if(iCurrentElement>=0 && iCurrentElement<iPlaylist.Count())
	{
		iPlaylist.Remove(iCurrentElement);
		iCurrentPlaybackPosition=0;
		if(iCurrentElement>=iPlaylist.Count())
			iCurrentElement=0;
		if(!iPlaylist.Count())
			iCurrentElement=-1;
	}
	LOG(ELogGeneral,-1,"RemoveCurrentElement-- (current element: %d/%d)",iCurrentElement+1,iPlaylist.Count());
}

void CPlaylist::GetPrevIndexesL(const TInt aNrElements, RPointerArray<HBufC> &aIndexStrings, RArray<TInt> &aIndexes)
{
	TInt indexElmMin=iCurrentElement-aNrElements;
	TInt i,indexesStrLen=0;
	if(indexElmMin<0)indexElmMin=0;
	//compute indexesStrLen 
	if(iCurrentElement<10)indexesStrLen=2;
	else if(iCurrentElement<100)indexesStrLen=3;
	else if(iCurrentElement<1000)indexesStrLen=4;
	else if(iCurrentElement<10000)indexesStrLen=5;
	else if(iCurrentElement<100000)indexesStrLen=6;
	else if(iCurrentElement<1000000)indexesStrLen=7;
	else FL_ASSERT(0);
	for(i=iCurrentElement-1;i>=indexElmMin;i--)
	{
		aIndexes.AppendL(i);
		//add the current index
		HBufC *is=HBufC::NewLC(indexesStrLen);
		is->Des().Format(_L("%d/"),i+1);
		aIndexStrings.AppendL(is);
		CleanupStack::Pop(is);
	}
}

void CPlaylist::GetNextIndexesL(const TInt aNrElements, RPointerArray<HBufC> &aIndexStrings, RArray<TInt> &aIndexes)
{
	TInt indexElmMax=iCurrentElement+aNrElements;
	TInt i,indexesStrLen=0;
	if(indexElmMax>=iPlaylist.Count())indexElmMax=iPlaylist.Count()-1;
	//compute indexesStrLen
	if(indexElmMax<10)indexesStrLen=2;
	else if(indexElmMax<100)indexesStrLen=3;
	else if(indexElmMax<1000)indexesStrLen=4;
	else if(indexElmMax<10000)indexesStrLen=5;
	else if(indexElmMax<100000)indexesStrLen=6;
	else if(indexElmMax<1000000)indexesStrLen=7;
	else FL_ASSERT(0);
	for(i=iCurrentElement+1;i<=indexElmMax;i++)
	{
		aIndexes.AppendL(i);
		//add the current index
		HBufC *is=HBufC::NewLC(indexesStrLen);
		is->Des().Format(_L("%d/"),i+1);
		aIndexStrings.AppendL(is);
		CleanupStack::Pop(is);
	}
}

void CPlaylist::GetNameAsString(const TInt aIndex, TPtr &aString)
{
	TFileDirEntry *fdr=GetEntry(aIndex,ETrue);
	aString.Set(fdr->iName,fdr->iNameLength,fdr->iNameLength);
}
