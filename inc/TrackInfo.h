/*
 * TrackInfo.h
 *
 *  Created on: 18.12.2008
 *      Author: Florin Lohan
 */

#ifndef TRACKINFO_H_
#define TRACKINFO_H_

#include <e32base.h>
#include <MetaDataField.hrh>

const TUint KMetadataMinYear=1000;
const TUint KMetadataMaxYear=2100;
const TUint KMetadataMinTrack=1;
const TUint KMetadataMaxTrack=1000;

//class TChildrenArray;
class TFileDirEntry;
class CFileDirPool;
class CEikonEnv;
class CMetaDataUtility;
class CMLauncherDocument;
class CPlaylist;


/*
template <class T>
class TChildrenArray
{
public:
	void Initialize();
	void Append(T *aChild,CFileDirPool *aPool);
	void InsertIntoPosition(T *aChild,CFileDirPool *aPool,TInt aPosition);
	TInt Delete(T *aChild); //returns true if the child was deleted
	void Delete(TInt aIndex);
	T &operator[](TInt anIndex);
	inline TInt ChildrenCount(){return iNrChildren;};
	void MarkAll(TBool aMark);
private:
	T *ShiftChildren();
private:

	TInt iNrChildren; //this is the nr of children in this array and in the next arrays
	T* iChildren[KChildrenGranularity];
	TChildrenArray *iNext;
};*/

template <class T>
class TChildrenArray
{
public:
	void Initialize(TInt aNrChildren, CFileDirPool *aPool);
	void AddMoreChildren(TInt aNrChildren, CFileDirPool *aPool);
	T &operator[](TInt anIndex);
	TInt ChildrenCount();
	void MarkAsDeleted(T *aChild);
	void MarkAsDeleted(TInt aIndex);
	void MarkAllAsDeleted();
	void UnmarkAllAsDeleted();
	void Append(T *aChild, CFileDirPool *aPool, TInt aAllocateIfNoSpace=1); //0 means we should have space!
	void InsertIntoPosition(T *aChild,CFileDirPool *aPool,TInt aPosition, TInt aAllocateIfNoSpace=1);
	void ReviewChildren(); //this rearranges children, as some deleted children can be inserted between existing children
private:
	TUint16 iNrChildren;
	TUint16 iNrAllocatedChildren;
	T** iChildren;
	TChildrenArray *iNext;
};

class TFileDirEntry
{
public:
	TFileDirEntry();
	void Initialize(TUint8 *aName, TInt aNameLength, TUint aFlags, TFileDirEntry *aParent);
	void InitializeAndCopy(TUint8 *aName, const TDesC& aNameDes, TUint aFlags, TFileDirEntry *aParent);
	inline TInt ChildrenCount() const{if(iChildren)return iChildren->ChildrenCount();else return 0;};
	//void CreateChildL(TDesC &aName, TInt aFlags, TDesC *aDisplayName=NULL, TBool aFirstInList=EFalse);
	void GetFullPath(TDes &aPath,TBool aAppendSlash=ETrue) const;
	void GetPathWithoutSource(TDes &aPath) const;
	static TBool IsMusicFile(const TDesC16& aFilename, TBool &aIsOggOrFlac);
	
	void SetSelection4Subtree(TBool aSelected);
	void SaveSelection(RFile& aSelection, TBuf8<KMaxFileName> &aTemp8FileName, TInt aDepth);
	void GetStartingDirL(RPointerArray<HBufC>& aList, RArray<TUint>& aListCount, TFileName &aTmpPath); //aTmpPath saves memory: if not passed, this function would need to alloc it on stack, recursively
	
	//TInt DeleteChild(TFileDirEntry *aChild); //remove the aChild entry from the list of children
	//TInt DeleteChild(TInt aIndex);
	TInt AddAndSortChild(CFileDirPool *aPool, TFileDirEntry *aChild, TBool aMakeCopy, TInt aChildrenToAllocate); //add aChild to the list of children
	//TInt AddDisplayName(CFileDirPool *aPool, TDesC& aDisplayName);
	TInt AppendMetadataDisplayName(TFileName &aFileName) const;
	TBool AllChildrenAndSubchildrenSelected(); //returns ETrue if all children and subchildren are selected
	TFileDirEntry* GetProperParent(CFileDirPool *aPool);
	TFileDirEntry *GetSource();
	TInt GetSourceLength();
	TInt GetLength();
	void SetModificationTime(const TTime &aModificationTime);
	TBool HasSameModificationTime(const TTime &aModificationTime); //returns True if modification times are the same and not zero
	void MarkAsDeleted(); //including children/subtree
	void UnmarkAsDeleted(); //including children/subtree
	void ReviewChildren();
public:
	enum TFlags
	{
		EIsDir=1,
		EIsSelected=2, //selected = all or some of the children are selected. If not selected, not going into children
		EIsPartiallySelected=4, //selected = all or some of the children are selected. If not selected, not going into children
		EWasSelected=8,
		EWasPartiallySelected=16,
		EHasSubdirs=32,
		EHasMedia=64,
		EIsSpecial=128,
		EIsOggOrFlac=0x100,
		EIsSource=0x200,
		EWasExpanded=0x400, //this means that we looked into the folder for files and folders
		EMarked=0x800,
		EWasNotFound=0x1000,
		EIsArtist=0x2000,
		EIsAlbum=0x4000,
		EIsSong=0x8000,
		EIsMetadata=0x10000,
		EMetadataUnknown=0x20000, //this is so we do not need to test agains KUnknown string
		EAudiobookSource=0x40000,
		EDeleted=0x80000
	};
	enum TStaticFlags
	{
		ESupportsOggAndFlac=1,
	};

	TUint16 *iName; //allocated on the buffer
	TUint8 iNameLength;
	TUint8 iDisplayNameLeft; //also used for computing the nr of music files
	TUint8 iDisplayNameRight; //if this is >0, the "display name" is in use
	TUint8 iGlobalDisplayNameIndex; //the array is defined in CMLauncherDocument
	TUint iFlags;
	TInt iSelectedPosition; //the selected position in folder/corresponding song/file for a file/song. When counting the music files, it represents the current child index 
	TInt iNrMusicFiles; //we use this to compute the number of music files from an entry and its children. For music files it holds the duration, in miliseconds.
	TUint iModificationTime; 
	static TUint iStaticFlags;

	TFileDirEntry *iParent;
	TChildrenArray<TFileDirEntry> *iChildren;
};


class CSegmentedBuffer : public CBase
{
public:
	CSegmentedBuffer(TInt aSegmentLength=20480);
	~CSegmentedBuffer();
	
	TUint8* ExpandL(TInt aSize);
	void Reset();
	void GetMemoryInfo(TInt &aNrSegments, TInt &aSegmentLength);
private:
	RPointerArray<TUint8> iSegments;
	RArray<TUint> iSegmentLengths;
	TInt iSegLen;
	TInt iLastSegmentLength;
	TUint8 *iLastSegment;
};


class CFileDirPool : public CBase
{
public:
	enum TFlags
	{
		EModified=1,
		EGetNrMusicFilesFast=2,
	};
public:
	static CFileDirPool* NewL(TInt aPoolSize=20480); //20k, also creates the root node
	static CFileDirPool* NewLC(TInt aPoolSize=20480); //20k, also creates the root node
	~CFileDirPool();
	void Recycle();
	
	TUint8* GetNewSpace(TInt size);
	TFileDirEntry* NewFileDirEntry(const TDesC &aName, TUint aFlags, TFileDirEntry *aParent, TInt aPosition=-1); //returned entry still owned
	TFileDirEntry* CreateEntriesFromSourceAndPath(const TDesC &aSource, const TDesC &aPath);
	TBool MakeChildren(TFileDirEntry *aParent, TInt aLevels=1); //returns true if children were created
	void UpdateEntry(TFileDirEntry *aEntry); //something was added
	void UpdateEntry2(TFileDirEntry *aEntry, TBool aRecursive, TBool &aCurrentParentWasUpdated); //something was added
	void UpdateTree(TBool aFilesReceivedInSpecialFolder, TBool &aCurrentFolderNeedsUpdate);
	
	void GetNrMusicFilesL(TFileDirEntry *aSource, TBool aFast, TBool &aShouldExit);
	int GetNrMusicFiles_ActiveIdleModeL();
	
	void ApplySelection(CFileDirPool *aSelection);
	void SetSelection(TFileDirEntry *aEntry, TInt aNrSelectedChildren=-1); //if -1 then it is unknown
	
	/*metadata functions - not used at the moment
	void GetMetadata(TFileDirEntry *aEntry, TFileName &aTmpPath, TBool &aShouldExit);
	void AddMetadataEntry(TPtrC aArtist, TPtrC aAlbum, TPtrC aYear, TPtrC aSong, TPtrC aTrackNr, TFileDirEntry *aEntry, TFileName &aTmpPath);
	void CleanMetadataVariables();
	void SetCallback(TCallBack *aCallback, TInt aTotalNrFiles); //transfers ownership
	*/
	/*
	TInt SaveToFile(TFileName &aFilename);
	TInt LoadFromFile(TFileName &aFilename);
	*/
	void MarkSourceAsDeleted(TDesC& aSource);
	
	void GetMemoryInfo(TInt &aNrSegments, TInt &aSegmentLength);
private:
	CFileDirPool();
	void ConstructL(TInt aPoolSize);
public:
	TFileDirEntry *iRoot; //not owned directly
	TFileDirEntry *iCurrentParent; //not owned parent 
	RPointerArray<TFileDirEntry> iSources; //pointers not owned directly
	TFileDirEntry *iArtistsHolder; //not owned directly, used for metadata
	static RFs *iFs; //not owned
	//public metadata stuff
	TInt iCurrentFileNr;
	TInt iTotalNrFiles;
	TUint iFlags;
private:
	CSegmentedBuffer *iFileDirBuf;
	//TInt iCurrentPosition;
	/*metadata stuff - not used at the moment
	CMetaDataUtility *iMDUtil;
	RArray<TMetaDataFieldId> *iFields;
	TCallBack *iMetadataCallback;*/
};

class MPlaylistObserver
{
public:
	virtual void PlaylistTrackAndPositionUpdated(CPlaylist *aPlaylist)=0;
};

class CPlaylist : public CBase
{
public:
	CPlaylist();
	~CPlaylist();
	CPlaylist(TInt aGranularity);
	void AddObserver(MPlaylistObserver *aObserver);
	void RemoveObserver(MPlaylistObserver *aObserver);
	
	inline TInt Count() {return iPlaylist.Count();};
	inline TInt NrElementsWhenLoaded() {return iPlaylist.Count()?iPlaylist.Count():iNrElementsWhenLoaded;};
	//void GetFilenameX(TInt aIndex,TFileName &aFilename);
	TFileDirEntry* GetEntry(TInt aIndex, TBool aExpectValid); //entry can be NULL or not valid
	TFileDirEntry* GetCurrentEntry(TBool aExpectValid); //entry can be NULL or not valid
	TInt GetCurrentIndex(); 
	void IncrementCurrent(TInt aIncrement, TBool aIncrementToNextValid, TBool *aWentOverTheTop, TBool *aNoValidElementsFound);
	void IncrementIndex(TInt &aIndex, TInt aIncrement, TBool aIncrementToNextValid, TBool *aWentOverTheTop, TBool *aNoValidElementsFound);
	TInt Shuffle(TBool aFromCurrent=EFalse);
	void Recycle();
	TInt ExportPlaylist(RFs& aFs, const TDesC& aFilename);
	TInt SaveCurrentPlaylist(CMLauncherDocument &aDoc);
	void LoadPlaylistL(CMLauncherDocument &aDoc);
	TInt LoadLegacyPlaylistV1(CMLauncherDocument &aDoc, TInt aLegacyCurrentElement, TInt aLegacyPosition);
	void MetadataUpdated();
	inline void Initialize(CFileDirPool *aPool, TInt aMyPlaylistIndex)
	    {iPool=aPool;iMyPlaylistIndex=aMyPlaylistIndex;};
	void AddToPlaylist(TFileDirEntry *aCurrentEntry, TBool &aHasOggsOrFlacs);
	TBool IsCurrent(){return isCurrent;};
	void SetCurrent(TBool aIsCurrent){isCurrent=aIsCurrent;};
	void SetMetadata(TInt aNrElements, TInt aCurrentIndex, TInt aPlaybackPosition, TInt aTrackDuration, TInt aTotalPlaylistDuration, HBufC *aName, TInt aMyPlaylistIndex, TBool aIsCurrent); //transfers ownership of aName
	void UpdatePosition(TTimeIntervalMicroSeconds &aPlaybackPosition);
	inline void SetDurationForCurrentTrack(TInt aDurationInSeconds)
	    {iCurrentTrackDurationInSeconds=aDurationInSeconds;};
	inline TInt CurrentTrackDuration()
	{return iCurrentTrackDurationInSeconds;};
	void RemoveFromDisk(RFs &aFs);
	void DecrementIndex(TInt aIndex,RFs &aFs);
	void CheckForDeletedElements();
	void RemoveOggAndFlacEntries();
	void RemoveCurrentElement();
	void GetPrevIndexesL(const TInt aNrElements, RPointerArray<HBufC> &aIndexStrings, RArray<TInt> &aIndexes);
	void GetNextIndexesL(const TInt aNrElements, RPointerArray<HBufC> &aIndexStrings, RArray<TInt> &aIndexes);
	void GetNameAsString(const TInt aIndex, TPtr &aString);
private:
	TInt Append(TFileDirEntry *aEntry);
	TInt FirstValidIndex(TInt aIndex, TBool &aWentOverTheTop);
public:
	// idle worker
	TInt iCurrentPlaybackPosition;
	HBufC *iName;
	TBool iIsAudiobook;
	TInt64 iTotalDurationInMiliseconds;
private: //vars
	RPointerArray<TFileDirEntry> iPlaylist; //we do not own the pointers
	TInt iNrElementsWhenLoaded; //informative number of elements. This is set even if the playlist is not loaded. so we can display the number of elements in Playlist view
	TInt iCurrentElement;

	CFileDirPool *iPool; //not owned
	TBool isCurrent;
	TInt iMyPlaylistIndex;
	TInt iCurrentTrackDurationInSeconds;
	RPointerArray<MPlaylistObserver> iObservers; //not owned
};
#endif /* TRACKINFO_H_ */
