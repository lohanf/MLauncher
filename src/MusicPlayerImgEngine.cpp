/*
 ============================================================================
 Name		: MusicPlayerImgEngine.cpp
 Author	  : Florin Lohan
 Version	 : 1.0
 Copyright   : Florin Lohan
 Description : CMusicPlayerImgEngine implementation
 ============================================================================
 */
#include <imageconversion.h>
#include <BitmapTransforms.h>
#include <akniconutils.h>
#include <eikenv.h>
#include <MLauncher.mbg> //loading icons
#include "MusicPlayerImgEngine.h"
#include "MusicPlayerView.h"
#include "MusicPlayerThemes.h"
#include "MLauncherPreferences.h"
#include "MLauncher.pan"
#include "log.h"

_LIT(KAlbumCoverName_,"cover_");
_LIT(KAlbumCoverExtension,".jpg");
_LIT(KHintFileExtension,".hint"); //should not be bigger that this, otherwise we may not have space for it
const TInt KHintFileExtensionLength=5; //should match the above
_LIT(KAlbumCoverNameGeneric,"cover.jpg");
_LIT(KAlbumCoverNameGeneric2,"folder.jpg");
_LIT(KCoversFolderName,"__Covers\\");
_LIT(KIgnoreFolderName,"IGNORE\\");
_LIT(KAlternateGenericAlbumArtName,"E:\\Images\\MLauncherCover.jpg");
_LIT8(KJpegMime,"image/jpeg");
_LIT(KOwnIconFile, "\\resource\\apps\\MLauncher.mif");

const TInt KMaxImagesInCache=10;

CMusicPlayerImgEngine::CMusicPlayerImgEngine() : CActive(EPriorityStandard), iState(EIdle),
		iImages(KMaxImagesInCache), iFilenames(KMaxImagesInCache), iSizes(KMaxImagesInCache), iDominantColors(KMaxImagesInCache)
{
}

CMusicPlayerImgEngine* CMusicPlayerImgEngine::NewLC(CEikonEnv *aEikEnv, CMLauncherPreferences *aPreferences)
{
	CMusicPlayerImgEngine* self = new (ELeave) CMusicPlayerImgEngine();
	CleanupStack::PushL(self);
	self->ConstructL(aEikEnv,aPreferences);
	return self;
}

CMusicPlayerImgEngine* CMusicPlayerImgEngine::NewL(CEikonEnv *aEikEnv, CMLauncherPreferences *aPreferences)
{
	CMusicPlayerImgEngine* self = CMusicPlayerImgEngine::NewLC(aEikEnv,aPreferences);
	CleanupStack::Pop(); // self;
	return self;
}

void CMusicPlayerImgEngine::ConstructL(CEikonEnv *aEikEnv, CMLauncherPreferences *aPreferences)
{
	CActiveScheduler::Add(this); // Add to scheduler
	iEikEnv=aEikEnv;
	iPreferences=aPreferences;
}

CMusicPlayerImgEngine::~CMusicPlayerImgEngine()
{
	Cancel(); // Cancel any request, if outstanding
	// Delete instance variables if any
	delete iImageDecoder;
	delete iBitmap;
	delete iCurrentFilename;

	delete iBmpScaler;
	delete iEnqueuedFilename;
	delete iEnqueuedDecoder;
	
	//cached images
	iImages.ResetAndDestroy();
	iFilenames.ResetAndDestroy();
	iSizes.ResetAndDestroy();
	iDominantColors.Close();
	iGenericImages.ResetAndDestroy();
}

void CMusicPlayerImgEngine::DoCancel()
{
	LOG(ELogGeneral,1,"CMusicPlayerImgEngine::DoCancel: start");
	
	//we cancel anything that can be canceled
	if(iImageDecoder)
		iImageDecoder->Cancel();
	if(iBmpScaler)
		iBmpScaler->Cancel();
	/*if(iImageEncoder)
		iImageEncoder->Cancel();*/
	
	Clean();
	iState=EIdle;
		
	LOG(ELogGeneral,-1,"CMusicPlayerImgEngine::DoCancel: end");
}

void CMusicPlayerImgEngine::Clean()
{
	//LOG(ELogGeneral,1,"CMusicPlayerImgEngine::Clean++ (this=%x, iImageDecoder=%x)",this,iImageDecoder);
	//clean
	delete iImageDecoder; iImageDecoder = NULL;
	delete iBitmap; iBitmap = NULL;
	if(iCurrentFilename)iCurrentFilename->Des().SetLength(0);
	if(iCurrentTheme)
	{
		iCurrentTheme->iAlbumArt=NULL;
		iCurrentTheme->iFlags&=~CTheme::EAlbumArtProcessing;
		iCurrentTheme->AlbumArtReady(KErrGeneral);
		iCurrentTheme=NULL;
	};
	delete iBmpScaler; iBmpScaler=NULL;
	iFlags=0;
	//LOG(ELogGeneral,-1,"CMusicPlayerImgEngine::Clean--");
}

void CMusicPlayerImgEngine::PrepareAlbumArtL(const CMetadata &aMetadata, const TSize &aAlbumArtSize)
{
	if(aAlbumArtSize.iHeight==0 || aMetadata.iFileDirEntry==NULL)return;
	//get the path & size out of the song
	LOG(ELogGeneral,1,"PrepareAlbumArtL++");

	TFileName cover;
	CImageDecoder *dec;
	TInt err=GetAlbumArtFilename(aMetadata,aAlbumArtSize,cover,&dec);
	if(err<0)
	{
		LOG(ELogGeneral,-1,"PrepareAlbumArtL-- (album art not found)");
		return; //nothing to prepare
	}
	if(err>0)
	{
		LOG(ELogGeneral,-1,"PrepareAlbumArtL-- (album art found in cache)");
		return;
	}
	
	//if we are here, we need to load the album art from the file or from aMetadata.iCover
	//check if this request is already processing, or we need to enqueue it
	if(iState!=EIdle)
	{
		CheckAgainstCurrentAndEnqueued(EFalse,cover,dec,aAlbumArtSize,NULL);
		LOG(ELogGeneral,-1,"PrepareAlbumArtL-- (either already processing or enqueued or ignored)");
		return;
	}

	//if we are here, we need to create the bitmap and transfer it
	DoProcessingL(cover,dec,aAlbumArtSize,NULL);
	LOG(ELogGeneral,-1,"PrepareAlbumArtL: end");
}

void CMusicPlayerImgEngine::GetAlbumArtL(const CMetadata &aMetadata, CTheme &aTheme)
{
	if(aTheme.iAlbumArtSize.iHeight==0 || aMetadata.iFileDirEntry==NULL)return;
	//get the path & size out of the song
	LOG(ELogGeneral,1,"GetAlbumArtL++ (iFileDirEntry: %x)",aMetadata.iFileDirEntry);

	TFileName cover;
	CImageDecoder *dec;
	TInt err=GetAlbumArtFilename(aMetadata,aTheme.iAlbumArtSize,cover,&dec);
	if(err<0) {
		//no album art found, use generic based on the current theme color
		//first, compute the offset, based on size;
		const TInt KNrColors=5;
		TInt i,offset=0;
		if(aTheme.iAlbumArtSize==iGenericSize0)offset=0;
		else if(aTheme.iAlbumArtSize==iGenericSize1)offset=KNrColors;
		else {
			//we need to set the size and offset
			if(iGenericSize0.iHeight==0){
				iGenericSize0=aTheme.iAlbumArtSize;
				for(i=0;i<KNrColors;i++)
					iGenericImages.AppendL(NULL);			
			} 
			else if(iGenericSize1.iHeight==0){
				iGenericSize1=aTheme.iAlbumArtSize;
				FL_ASSERT(iGenericImages.Count()==KNrColors);
				for(i=0;i<KNrColors;i++)
					iGenericImages.AppendL(NULL);
			}
			else FL_ASSERT_TXT(0,"Both sizes full, something is wrong.");
		}
		
		switch(iPreferences->iMusicPlayerThemeData&0xFFF0)
		{
		case CColoredTheme::EColorGreen:
			if(!iGenericImages[offset]){
				//we need to load the green image/icon
				iGenericImages[offset]=AknIconUtils::CreateIconL(KOwnIconFile,EMbmMlauncherMlauncheraif);
				AknIconUtils::SetSize(iGenericImages[offset],aTheme.iAlbumArtSize,EAspectRatioNotPreserved );
			}
			aTheme.iAlbumArt=iGenericImages[offset];
			aTheme.iAlbumArtDominantColor=0x00FF00;//green
			break;
		case CColoredTheme::EColorBlue:
			if(!iGenericImages[offset+1]){
				//we need to load the blue image/icon
				iGenericImages[offset+1]=AknIconUtils::CreateIconL(KOwnIconFile,EMbmMlauncherMlauncheraifblue);
				AknIconUtils::SetSize(iGenericImages[offset+1],aTheme.iAlbumArtSize,EAspectRatioNotPreserved );
			}
			aTheme.iAlbumArt=iGenericImages[offset+1];
			aTheme.iAlbumArtDominantColor=0x0080FF;//blue
			break;
		case CColoredTheme::EColorOrange:
			if(!iGenericImages[offset+2]){
				//we need to load the orange image/icon
				iGenericImages[offset+2]=AknIconUtils::CreateIconL(KOwnIconFile,EMbmMlauncherMlauncheraiforange);
				AknIconUtils::SetSize(iGenericImages[offset+2],aTheme.iAlbumArtSize,EAspectRatioNotPreserved );
			}
			aTheme.iAlbumArt=iGenericImages[offset+2];
			aTheme.iAlbumArtDominantColor=0xFBB416;//orange
			break;
		case CColoredTheme::EColorRed:
			if(!iGenericImages[offset+3]){
				//we need to load the red image/icon
				iGenericImages[offset+3]=AknIconUtils::CreateIconL(KOwnIconFile,EMbmMlauncherMlauncheraifred);
				AknIconUtils::SetSize(iGenericImages[offset+3],aTheme.iAlbumArtSize,EAspectRatioNotPreserved );
			}
			aTheme.iAlbumArt=iGenericImages[offset+3];
			aTheme.iAlbumArtDominantColor=0xFF0000;//red
			break;
		case CColoredTheme::EColorViolet:
			if(!iGenericImages[offset+4]){
				//we need to load the violet image/icon
				iGenericImages[offset+4]=AknIconUtils::CreateIconL(KOwnIconFile,EMbmMlauncherMlauncheraifviolet);
				AknIconUtils::SetSize(iGenericImages[offset+4],aTheme.iAlbumArtSize,EAspectRatioNotPreserved );
			}
			aTheme.iAlbumArt=iGenericImages[offset+4];
			aTheme.iAlbumArtDominantColor=0xF107E7;//violet
			break;
		}//switch
		LOG(ELogGeneral,-1,"GetAlbumArtL-- (generic album art used)");
		return;
	}
	if(err>0)
	{
		LOG(ELogGeneral,-1,"GetAlbumArtL-- (album art found in cache)");
		aTheme.iAlbumArt=iImages[0];
		aTheme.iAlbumArtDominantColor=iDominantColors[0];
		return;
	}

	LOG0("GetAlbumArtL: checking if we are already loading this request");

	//check if this request is already processing, or we need to enqueue it
	if(iState!=EIdle)
	{
		CheckAgainstCurrentAndEnqueued(ETrue,cover,dec,aTheme.iAlbumArtSize,&aTheme);
		LOG(ELogGeneral,-1,"GetAlbumArtL-- (either already processing or enqueued)");
		return;
	}

	//if we are here, we need to create the bitmap and transfer it
	LOG0("GetAlbumArtL: CACHE MISS, we have to load the album art from file");
	aTheme.iAlbumArt=NULL;
	err=DoProcessingL(cover,dec,aTheme.iAlbumArtSize,&aTheme);
	LOG(ELogGeneral,-1,"GetAlbumArtL-- (request processing)");
}

TInt CMusicPlayerImgEngine::GetAlbumArtFilename(const CMetadata &aMetadata, const TSize &aSize, TFileName &aCoverFilename, CImageDecoder **aDecoder) 
{
	/* Function returns KErrNotFound if we find nothing, it returns 0 if we find a file, and it returns 1 if the elemnt is in cache
	 * In aCacheIndex returns the cache index, if found there, or -1. If found in cache, the cache is updated.
	 * If NOT found in cache, aCoverFilename contains the cover filename. If aMetadata.iCover is valid (non-NULL)
	 * then the aCoverFilename is non-existent, e.g. e:\\Music\\Something\\Else.mp3_360.jpg The purpose in this case
	 * would be to identify the image in the cache.
	 * 
	 * The algorithm for finding the album art:
	 * 1. Check if there is aMetadata.iCover. If it does, check the cache, update aCoverFilename, return.
	 * 2. Check for KAlbumCoverNameGeneric (cover.jpg) in song's folder.
	 * 3. Check for KAlbumCoverNameGeneric2 (folder.jpg) in song's folder.
	 * 4. If aMetadata artist and album are non-NULL, check the Source/__Covers/Artist-Album.jpg
	 * 5. If enabled in Preferences, create a "hint" Source/__Covers/Artist-Album.hint having 0 bytes
	 * 6. If here, return KErrNotFound
	 */
	
	aMetadata.iFileDirEntry->GetFullPath(aCoverFilename,ETrue);
	LOG(ELogGeneral,1,"GetAlbumArtFilename++ (%S), iCover=%x",&aCoverFilename,aMetadata.iCover);

	TInt r,err;
	TEntry entry;
	
	*aDecoder=NULL;
	if(aMetadata.iCover) {
		aMetadata.iFileDirEntry->GetFullPath(aCoverFilename);
		//check if we have this in cache
		if(IsInCache(aCoverFilename,aSize)) {
			LOG(ELogGeneral,-1,"GetAlbumArtFilename-- (we have embedded cover, in cache)");
			return 1; 
		}
		else {
			TRAP(err,*aDecoder=CImageDecoder::DataNewL(iEikEnv->FsSession(),*aMetadata.iCover));
			if(!err) {
				LOG(ELogGeneral,-1,"GetAlbumArtFilename-- (we have embedded cover)");
				return 0; 
			}
			else LOG0("Instantiating a CImageDecoder for the embedded cover failed. Checking other options.");
		}
	}
	
	//check for KAlbumCoverNameGeneric
	aMetadata.iFileDirEntry->iParent->GetFullPath(aCoverFilename,ETrue);
	TInt pathLength=aCoverFilename.Length();
	aCoverFilename.Append(KAlbumCoverNameGeneric);
	if((r=GetAlbumArtFilenameHelper(aSize,aCoverFilename,aDecoder))>=0)
		return r;
	
	//check for KAlbumCoverNameGeneric2
	aCoverFilename.SetLength(pathLength);
	aCoverFilename.Append(KAlbumCoverNameGeneric2);
	if((r=GetAlbumArtFilenameHelper(aSize,aCoverFilename,aDecoder))>=0)
		return r;
	
	//if we are here, we need to check if we have valid Artist and Album metadata
	if(aMetadata.iAlbum && aMetadata.iArtist)
	{
		//we can construct Source/__Covers/Artist-Album.jpg
		aMetadata.iFileDirEntry->GetSource()->GetFullPath(aCoverFilename,ETrue);
		aCoverFilename.Append(KCoversFolderName);
		pathLength=aCoverFilename.Length();
		if(pathLength+aMetadata.iArtist->Length()+aMetadata.iAlbum->Length()+6<=KMaxFileName) //6= the dash + length of .jpg or .jpeg or .hint
		{
			//check for __Covers/Artist-Album.jpg
			aCoverFilename.Append(*aMetadata.iArtist);
			aCoverFilename.Append('-');
			aCoverFilename.Append(*aMetadata.iAlbum);
			aCoverFilename.Append(KAlbumCoverExtension);
			if((r=GetAlbumArtFilenameHelper(aSize,aCoverFilename,aDecoder))>=0)
				return r;
			
			//if we are here, the file does not exist!
			if((iPreferences->iPFlags&CMLauncherPreferences::EPreferencesCreateCoverHintFiles) &&
					(pathLength+KIgnoreFolderName().Length()+aMetadata.iArtist->Length()+aMetadata.iAlbum->Length()+6<=KMaxFileName)) //filename size requirements
			{
				//check first if the hint file is in the "IGNORE" folder
				aCoverFilename.SetLength(pathLength);
				aCoverFilename.Append(KIgnoreFolderName);
				aCoverFilename.Append(*aMetadata.iArtist);
				aCoverFilename.Append('-');
				aCoverFilename.Append(*aMetadata.iAlbum);
				aCoverFilename.Append(KHintFileExtension);
				if(iEikEnv->FsSession().Entry(aCoverFilename,entry)){
					//the hint file was not found in the IGNORE folder
					//we should create a hint file
					aCoverFilename.SetLength(pathLength);
					aCoverFilename.Append(*aMetadata.iArtist);
					aCoverFilename.Append('-');
					aCoverFilename.Append(*aMetadata.iAlbum);
					aCoverFilename.Append(KHintFileExtension);
					LOG0("Creating hint file for %S",&aCoverFilename);
					//first, we try to make the folder
					if(!iEikEnv->FsSession().MkDir(aCoverFilename.Left(pathLength))){
						//set the entry as hidden
						TTime modifTime;
						modifTime.UniversalTime();
						iEikEnv->FsSession().SetEntry(aCoverFilename.Left(pathLength),modifTime,KEntryAttHidden,0);
					}
					//now we create the file, IF it does not exist
					if(iEikEnv->FsSession().Entry(aCoverFilename,entry)){
						RFile f;
						if((err=f.Create(iEikEnv->FsSession(),aCoverFilename,EFileWrite))){
							LOG0("Creating %S hint file failed with error %d",&aCoverFilename,err);
						}
						else {
							LOG0("Hint file %S created successfully!",&aCoverFilename);
							f.Close();
						};
					}
				}
			}
			
			//check __Covers/Album.jpg
			aCoverFilename.SetLength(pathLength);
			aCoverFilename.Append(*aMetadata.iAlbum);
			aCoverFilename.Append(KAlbumCoverExtension);
			if((r=GetAlbumArtFilenameHelper(aSize,aCoverFilename,aDecoder))>=0)
				return r;
						
			//check __Covers/Artist.jpg
			aCoverFilename.SetLength(pathLength);
			aCoverFilename.Append(*aMetadata.iArtist);
			aCoverFilename.Append(KAlbumCoverExtension);
			if((r=GetAlbumArtFilenameHelper(aSize,aCoverFilename,aDecoder))>=0)
				return r;
			
		}//if(pathLength+ ...
	}//if(aMetadata.iAlbum && aMetadata.iArtist)
	
	//if we are here, we did not find a cover art anywhere, so we will use the generic cover art
	//first, check if the user has specified their own album art, and try to decode that
	aCoverFilename.Copy(KAlternateGenericAlbumArtName);
	if((r=GetAlbumArtFilenameHelper(aSize,aCoverFilename,aDecoder))>=0)
		return r;
	
	//if we are here, we will use generic album art
	LOG(ELogGeneral,-1,"GetAlbumArtFilename-- (no match found, use generic)");
	*aDecoder=NULL;
	return KErrNotFound;
}

TInt CMusicPlayerImgEngine::GetAlbumArtFilenameHelper(const TSize &aSize, TFileName &aCoverFilename, CImageDecoder **aDecoder)
{
	//do we have this in cache?
	if(IsInCache(aCoverFilename,aSize)) {
		LOG(ELogGeneral,-1,"GetAlbumArtFilename-- (%S found in cache)",&aCoverFilename);
		return 1; //we found it in cache
	}
	//else we need to check if this file really exists
	TEntry entry;
	if(!iEikEnv->FsSession().Entry(aCoverFilename,entry)) {
		TRAPD(err,*aDecoder=CImageDecoder::FileNewL(iEikEnv->FsSession(),aCoverFilename));
		if(!err) {
			LOG(ELogGeneral,-1,"GetAlbumArtFilename-- (%S found)",&aCoverFilename);
			return 0; 
		}
		else LOG0("Instantiating a CImageDecoder for the found %S failed. Checking other options.",&aCoverFilename);
	}
	return -1;
}

TBool CMusicPlayerImgEngine::CheckAgainstCurrentAndEnqueued(TBool aForceEnqueue, const TDesC& aFilename, CImageDecoder *aImgDec, const TSize &aSize, CTheme *aTheme)
{
	LOG(ELogGeneral,1,"CheckAgainstCurrentAndEnqueued++");
	//first, check if the filename, size and theme are the same as those processed. If they are, disregard this request
	if(aSize==iCurrentSize && aTheme==iCurrentTheme && iCurrentFilename && aFilename==*iCurrentFilename)
	{
		LOG(ELogGeneral,-1,"CheckAgainstCurrentAndEnqueued-- (current request same as the one checked, we will not do it again)");
		delete aImgDec;
		return EFalse;
	}
	
	//check if we already have an enqueued request
	if(iEnqueuedFilename && iEnqueuedFilename->Length()>0)
	{
		//yes, we have an enqueued request
		//first check if it is the same as ours
		if(aSize==iEnqueuedSize && aTheme==iEnqueuedTheme && iEnqueuedFilename && aFilename==*iEnqueuedFilename)
		{
			LOG(ELogGeneral,-1,"CheckAgainstCurrentAndEnqueued-- (enqueued request same as the one checked, we will not enqueue it again)");
			delete aImgDec;
			return EFalse;
		}
		//if we are here, the enqueued request is different than ours
		if(aForceEnqueue)
		{
			//we will replace the enqueued request with this one
			//"cancel" the enqueued request
			if(iEnqueuedTheme)
			{
				iEnqueuedTheme->iAlbumArt=NULL;
				iEnqueuedTheme=NULL;
			}
			if(iEnqueuedFilename)
				iEnqueuedFilename->Des().SetLength(0);
			delete iEnqueuedDecoder;
			iEnqueuedDecoder=NULL;
			LOG0("Enqueued request cancelled.")
		}
		else
		{
			LOG(ELogGeneral,-1,"CheckAgainstCurrentAndEnqueued-- (there is an enqueued request already that we do not want to cancel)");
			delete aImgDec;
			return EFalse;
		}
	}
	
	//if we are here, we will enqueue our request
	if(iEnqueuedFilename && aFilename.Length()<=iEnqueuedFilename->Des().MaxLength())
		iEnqueuedFilename->Des().Copy(aFilename);
	else
	{
		delete iEnqueuedFilename;
		iEnqueuedFilename=HBufC::NewL(aFilename.Length());
		iEnqueuedFilename->Des().Copy(aFilename);
	};
	iEnqueuedTheme=aTheme; //can be NULL
	iEnqueuedSize=aSize;
	iEnqueuedDecoder=aImgDec;
	//done
	LOG(ELogGeneral,-1,"CheckAgainstCurrentAndEnqueued-- (the request was enqueued)");
	return ETrue;
}

TInt CMusicPlayerImgEngine::DoProcessingL(const TDesC &aFilename, CImageDecoder *aImgDec, const TSize &aSize, CTheme *aTheme)
{
	LOG(ELogGeneral,1,"CMusicPlayerImgEngine::DoProcessingL++ (%S - requested size: %dx%d)",&aFilename,aSize.iWidth,aSize.iHeight);
	delete iImageDecoder; iImageDecoder = NULL;
	delete iBitmap; iBitmap = NULL;

	//set image name (what it should be)
	if(iCurrentFilename && aFilename.Length()<=iCurrentFilename->Des().MaxLength())
		iCurrentFilename->Des().Copy(aFilename);
	else
	{
		delete iCurrentFilename;
		iCurrentFilename=HBufC::NewL(aFilename.Length());
		iCurrentFilename->Des().Copy(aFilename);
	};
	iImageDecoder=aImgDec;
	LOG0("iCurrenFilename and iImageDecoder (%x) set.",iImageDecoder);
	TInt err;
	
	if(iImageDecoder->FrameInfo().iOverallSizeInPixels != aSize)
		iFlags|=ENeedsScaling;

	LOG0("Creating bitmap for file %S",&aFilename);
	// create the destination bitmap
	//iBitmap = new CWsBitmap(iEikEnv->WsSession());
	iBitmap = new CFbsBitmap();
	if(!iBitmap)
	{
		LOG(ELogGeneral,-1,"CMusicPlayerImgEngine::DoProcessingL-- No memory to instatntiate the bitmap! There will be no cover art.");
		iCurrentFilename->Des().SetLength(0);
		iCurrentTheme=NULL;
		iFlags=0;
		//if(aTheme)aTheme->iAlbumArt=NULL;
		delete iImageDecoder;iImageDecoder=NULL;
		return KErrNoMemory; //there will be no cover art
	}
	if(iFlags&ENeedsScaling)
	{
		//we create the bitmap with the same size as the file
		LOG0("Image needs scaling");
		err=iBitmap->Create( iImageDecoder->FrameInfo().iOverallSizeInPixels, iImageDecoder->FrameInfo().iFrameDisplayMode );
	}
	else
		err=iBitmap->Create( aSize, iImageDecoder->FrameInfo().iFrameDisplayMode );
	if(err)
	{
		//clean and announce there will be no cover art
		LOG(ELogGeneral,-1,"CMusicPlayerImgEngine::DoProcessingL-- No memory to Create() the bitmap (err=%d)! There will be no cover art.",err);
		delete iImageDecoder; iImageDecoder = NULL;
		delete iBitmap; iBitmap = NULL;
		iFlags=0;
		//if(aTheme)aTheme->iAlbumArt=NULL;
		iCurrentFilename->Des().SetLength(0);
		iCurrentTheme=NULL;
		return err;
	};

	// start conversion to bitmap
	iState = EDecoding;
	iImageDecoder->Convert( &iStatus, *iBitmap );
	SetActive();
	iCurrentTheme=aTheme;
	iCurrentSize=aSize;
	if(iCurrentTheme)
	{
		iCurrentTheme->iFlags|=CTheme::EAlbumArtProcessing;
		LOG0("!!!!! CACHE MISS !!!!!");
	}
	LOG(ELogGeneral,-1,"CMusicPlayerImgEngine::DoProcessingL--");
	return KErrNone;
}

void CMusicPlayerImgEngine::UpdateElementsInCache(TInt aIndex)
{
	//bring the aIndex elements in front, and push all the rest down
	//CWsBitmap *tmpBmp=iImages[aIndex];
	CFbsBitmap *tmpBmp=iImages[aIndex];
	TSize *tmpSize=iSizes[aIndex];
	HBufC *tmpFilename=iFilenames[aIndex];
	TUint32 tmpDominantColor=iDominantColors[aIndex];
	for(TInt i=aIndex-1;i>=0;i--)
	{
		iImages[i+1]=iImages[i];
		iSizes[i+1]=iSizes[i];
		iFilenames[i+1]=iFilenames[i];
		iDominantColors[i+1]=iDominantColors[i];
	};
	iImages[0]=tmpBmp;
	iSizes[0]=tmpSize;
	iFilenames[0]=tmpFilename;
	iDominantColors[0]=tmpDominantColor;
}

TBool CMusicPlayerImgEngine::IsInCache(const TDesC &aFilename, const TSize &aSize) //returns an index or KErrNotFound. Also updates the cache
{
	TInt i;
	for(i=0;i<iFilenames.Count();i++)
		if(aFilename== *iFilenames[i] && aSize== *iSizes[i])
		{
			//yes, we have it in the cache. Update the elements
			UpdateElementsInCache(i);
			//update done
			return ETrue;
		};
	//if we are here, the image is not in our cache
	return EFalse;
}
void CMusicPlayerImgEngine::CancelRequest(CTheme *aTheme)
{
	if(aTheme==iEnqueuedTheme)
	{
		LOG0("CMusicPlayerImgEngine::CancelRequest: removing the enqueued theme");
		iEnqueuedTheme=NULL;
		if(iEnqueuedFilename)iEnqueuedFilename->Des().SetLength(0);
	}
	else if(aTheme==iCurrentTheme)
	{
		LOG0("CMusicPlayerImgEngine::CancelRequest: Canceling current request");
		Cancel();
		LOG0("CMusicPlayerImgEngine::CancelRequest: Done canceling current request");
	};
}

void CMusicPlayerImgEngine::RunL()
{
	LOG(ELogGeneral,1,"CMusicPlayerImgEngine::RunL: start");
	switch(iState)
	{
	case EDecoding:
	{
		//decoding
		if( iStatus == KErrNone )
		{
			if(iFlags&ENeedsScaling)
			{
				//start the scaling operation
				if(iBmpScaler)delete iBmpScaler;
				iBmpScaler=CBitmapScaler::NewL();
				iBmpScaler->Scale(&iStatus,*iBitmap,iCurrentSize,EFalse);
				iState = EScaling;
				iFlags=0;
				SetActive();
			}
			else
			{
				TransferAlbumArt(ETrue);
				ProcessNextRequest();
			};
		}
		else
		{
			Clean();
			ProcessNextRequest();
		};
	};break;
	case EScaling:
	{
		//scaling
		if( iStatus == KErrNone )
			TransferAlbumArt(ETrue);//transfer & display the image
		Clean();
		ProcessNextRequest();
	};break;
	};//switch
	LOG(ELogGeneral,-1,"CMusicPlayerImgEngine::RunL: end");
}

TInt CMusicPlayerImgEngine::RunError(TInt aError)
{
	Clean();
	return aError;
}

void CMusicPlayerImgEngine::TransferAlbumArt(TBool aWasActive)
{
	LOG(ELogGeneral,1,"CMusicPlayerImgEngine::TransferAlbumArt: start (wasActive: %d)",aWasActive);
	TUint32 dominantColor=GetDominantColor(iBitmap);
	if(iCurrentTheme)
	{
		iCurrentTheme->iAlbumArt=iBitmap;
		iCurrentTheme->iAlbumArtDominantColor=dominantColor;
		iCurrentTheme->iFlags&=~CTheme::EAlbumArtProcessing;
		iCurrentTheme->AlbumArtReady(KErrNone);
		
		//
		if(aWasActive && iCurrentTheme->iManager->iView->iFlags&CMusicPlayerView::EReactivateCIdleAfterImgProcessingEnds)
		{
			LOG0("Reactivating the ActiveIdle worker");
			iCurrentTheme->iManager->iView->iFlags-=CMusicPlayerView::EReactivateCIdleAfterImgProcessingEnds;
			iCurrentTheme->iManager->iView->ScheduleWorkerL(0);
		};
	};
	//add to cache
	TInt max(KMaxImagesInCache-1);
	__ASSERT_DEBUG(iImages.Count()<=KMaxImagesInCache,Panic(ETooManyImagesInCache));
	if(iImages.Count()==KMaxImagesInCache)
	{
		//delete the last element in the cache
		delete iImages[max];
		iImages.Remove(max);
		delete iSizes[max];
		iSizes.Remove(max);
		delete iFilenames[max];
		iFilenames.Remove(max);
		iDominantColors.Remove(max);
	};
	TSize *sz=new TSize(iCurrentSize);
	if(!sz)return; //memory allocation did not succeed
	iSizes.Insert(sz,0);
	iImages.Insert(iBitmap,0);
	iBitmap=NULL;
	iFilenames.Insert(iCurrentFilename,0);
	iCurrentFilename=NULL;
	iDominantColors.Insert(dominantColor,0);
	//clean our request
	iCurrentTheme=NULL;
	LOG(ELogGeneral,-1,"CMusicPlayerImgEngine::TransferAlbumArt: end");
}

void CMusicPlayerImgEngine::ProcessNextRequest()
{
	LOG(ELogGeneral,1,"CMusicPlayerImgEngine::ProcessNextRequest: start");
	iState=EIdle;
	if(!iEnqueuedFilename || iEnqueuedFilename->Length()==0)
	{
		LOG(ELogGeneral,-1,"CMusicPlayerImgEngine::ProcessNextRequest: no request to process");
		return; //no next request to process
	};
	
	LOG0("CMusicPlayerImgEngine::ProcessNextRequest: we have an enqueued request");
	//check to see if we already have the image in our cache
	if(IsInCache(*iEnqueuedFilename,iEnqueuedSize))
	{
		if(iEnqueuedTheme)
		{
			iEnqueuedTheme->iAlbumArt=iImages[0];
			iEnqueuedTheme->iAlbumArtDominantColor=iDominantColors[0];
			iEnqueuedTheme->AlbumArtReady(KErrNone);
		};
		LOG(ELogGeneral,-1,"CMusicPlayerImgEngine::ProcessNextRequest: request was found in the cache.");
		return;
	}
	
	//if we are here, the image is not in our cache
	DoProcessingL(*iEnqueuedFilename,iEnqueuedDecoder,iEnqueuedSize,iEnqueuedTheme);
	//clean enqueued values
	iEnqueuedTheme=NULL;
	iEnqueuedFilename->Des().SetLength(0);
	iEnqueuedDecoder=NULL; //DoProcessingL took ownership
	LOG(ELogGeneral,-1,"CMusicPlayerImgEngine::ProcessNextRequest: end (processing)");
}

TBool CMusicPlayerImgEngine::AreThereCoverHintFiles(TBool aDeleteThem) //returns ETrue if there are
{
	TInt i,j;
	TFileName coversDir;
	CDir *dir=NULL;
	TBuf<KHintFileExtensionLength> ext;
	TBool found(EFalse);
	__ASSERT_ALWAYS(KHintFileExtensionLength>=KHintFileExtension().Length(),Panic(EHintFileExtensionLengthTooSmall));
	for(i=0;i<iPreferences->iStartingDirs.Count();i++)
	{
		iPreferences->CreatePathFromSourceAndName(coversDir,i,KCoversFolderName,ETrue);
		if(KErrNone==iEikEnv->FsSession().GetDir(coversDir,KEntryAttNormal,0,dir))
		{
			//folder exists and operation was successful
			TInt coversDirLen=coversDir.Length();
			for(j=0;j<dir->Count();j++)
			{
				coversDir.SetLength(coversDirLen);
				coversDir.Append((*dir)[j].iName);
				//check for the right extension
				ext=coversDir.Right(KHintFileExtension().Length());
				ext.LowerCase();
				if(ext==KHintFileExtension)
				{
					//we found a hint file
					found=ETrue;
					if(aDeleteThem)//delete this hint file
						iEikEnv->FsSession().Delete(coversDir);			
					else
						break;
				}
			}
			delete dir;
			if(found && !aDeleteThem)break;
		}
	}
	return found;
}

TUint32 CMusicPlayerImgEngine::GetDominantColor(CFbsBitmap *aBmp)
{
	LOG0("GetDominantColor++ (bmp=%x)",aBmp);
	if(!aBmp)return 0;
	
	TInt i,j,c(0);
	const TUint8 KThreshold=20;
	TInt width=aBmp->SizeInPixels().iWidth;
	TInt height=aBmp->SizeInPixels().iHeight;
	if(!width || !height)return 0;
	
	TBitmapUtil bu(aBmp);
	TUint32 r(0),g(0),b(0),rgb;
	TUint8 rx,gx,bx;
	bu.Begin(TPoint(0,0));
	for(i=0;i<height;i++)
	{
		bu.SetPos(TPoint(0,i));
		for(j=0;j<width;j++)
		{
			rgb=bu.GetPixel();
			//we assume we have AARRGGBB (AA=alpha)
			bx=rgb&0xFF;
			rgb>>=8;
			gx=rgb&0xFF;
			rgb>>=8;
			rx=rgb&0xFF;
			
			if( (rx-gx>KThreshold && rx-bx>KThreshold) || (gx-rx>KThreshold && gx-bx>KThreshold) || (bx-rx>KThreshold && bx-gx>KThreshold) ||
				(gx-rx>KThreshold && bx-rx>KThreshold) || (rx-gx>KThreshold && bx-gx>KThreshold) || (rx-bx>KThreshold && gx-bx>KThreshold))
			{
				r+=rx;g+=gx;b+=bx;
				c++;
			}
			//LOG0("Pixel (%d,%d): %x, r=%d, g=%d, b=%d",j,i,rgb,r,g,b);
			bu.IncXPos();
		}
	}
	if(c)
	{
		r/=c;
		g/=c;
		b/=c;
	}
	rgb=(r<<16)+(g<<8)+b;
	bu.End();
	LOG0("GetDominantColor-- Dominant color: r=%d, g=%d, b=%d considered pixels: %d ignored: %d",r,g,b,c,width*height-c);
	return rgb;
}

