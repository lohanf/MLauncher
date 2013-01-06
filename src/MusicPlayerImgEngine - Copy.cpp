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
#include <eikenv.h>
#include "MusicPlayerImgEngine.h"
#include "MusicPlayerView.h"
#include "MusicPlayerThemes.h"
#include "MLauncherPreferences.h"
#include "MLauncher.pan"
#include "log.h"

/*
_LIT(KAlbumCoverName176,"cover_176.jpg");
_LIT(KAlbumCoverName240,"cover_240.jpg");
_LIT(KAlbumCoverName352,"cover_352.jpg");
_LIT(KAlbumCoverName360,"cover_360.jpg");
*/
_LIT(KAlbumCoverName_,"cover_");
_LIT(KAlbumCoverExtension,".jpg");
_LIT(KAlbumCoverNameGeneric,"cover.jpg");
_LIT(KAlbumCoverNameGeneric2,"folder.jpg");
#ifdef __WINS__
_LIT(KGenericAlbumArtName,"Z:\\private\\E388D98A\\cover.jpg");
#else
_LIT(KGenericAlbumArtName,"cover.jpg");
#endif
_LIT(KAlternateGenericAlbumArtName,"E:\\Images\\MLauncherCover.jpg");
_LIT8(KJpegMime,"image/jpeg");
/*
const TSize KLegacySize(176,176);
const TSize KDoubleSize(352,352);
const TSize KQVGASize(240,240);
const TSize KnHDSize(360,360);
*/
const TInt KMaxImagesInCache=10;

CMusicPlayerImgEngine::CMusicPlayerImgEngine() : CActive(EPriorityStandard), iState(EIdle)
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
	delete iEncBitmap;
	delete iCurrentFilename;

	delete iBmpScaler;
	delete iImageEncoder;
	delete iEnqueuedFilename;
	
	//cached images
	iImages.ResetAndDestroy();
	iFilenames.ResetAndDestroy();
	iSizes.ResetAndDestroy();
}

void CMusicPlayerImgEngine::DoCancel()
{
	LOG(ELogGeneral,1,"CMusicPlayerImgEngine::DoCancel: start");
	
	//we cancel anything that can be canceled
	if(iImageDecoder)
		iImageDecoder->Cancel();
	if(iBmpScaler)
		iBmpScaler->Cancel();
	if(iImageEncoder)
		iImageEncoder->Cancel();
	
	Clean();
	iState=EIdle;
		
	LOG(ELogGeneral,-1,"CMusicPlayerImgEngine::DoCancel: end");
}

void CMusicPlayerImgEngine::PrepareAlbumArtL(const CMetadata &aMetadata, const TSize &aAlbumArtSize)
{
	if(aAlbumArtSize.iHeight==0 || aMetadata.iFileDirEntry==NULL)return;
	//get the path & size out of the song
	LOG(ELogGeneral,1,"PrepareAlbumArtL: start");

	TFileName cover;
	aMetadata.iFileDirEntry->GetFullPath(cover);
	TInt pos=cover.LocateReverse('\\');
	if(pos>=0)
		cover.Delete(pos+1,cover.Length()-pos-1);
	cover.Append(KAlbumCoverName_);
	TBuf<5> size;
	size.Format(_L("%d"),aAlbumArtSize.iWidth);
	cover.Append(size);
	cover.Append(KAlbumCoverExtension);

	//check to see if we already have the image in our cache
	TInt i;
	for(i=0;i<iFilenames.Count();i++)
		if(cover== *iFilenames[i] && aAlbumArtSize== *iSizes[i])
		{
			//yes, we have it in the cache
			//move the found elements in the beginning of their array
			UpdateElementsInCache(i);
			LOG(ELogGeneral,-1,"PrepareAlbumArtL: end: album art was found in cache");
			return;
		};
	//if we are here, the image is not in our cache

	//check if this is request that would need to be enqueued or it takes priority
	if(iState!=EIdle)
	{
		//we are going to enqueue this request, if there is no other request enqueued
		if(iEnqueuedTheme)
		{
			LOG(ELogGeneral,-1,"PrepareAlbumArtL: there is already an enqueued request, aborting");
			return;
		};
		//enqueue our request
		EnqueueRequestL(cover,aAlbumArtSize,NULL);
		LOG(ELogGeneral,-1,"PrepareAlbumArtL: end: current request replaced the enqueued request");
		return;		
	};

	//if we are here, we need to create the bitmap and transfer it
	TInt err=DoProcessingL(cover,aAlbumArtSize,NULL);
	if(err==KErrNotFound)
		PrepareGenericAlbumArtL(aAlbumArtSize);
	LOG(ELogGeneral,-1,"PrepareAlbumArtL: end");
}

TInt CMusicPlayerImgEngine::GetAlbumArtL(const CMetadata &aMetadata, CTheme &aTheme, TBool aDoNotEnqueue)
{
	if(aTheme.iAlbumArtSize.iHeight==0 || aMetadata.iFileDirEntry==NULL)return KErrNotFound;
	//get the path & size out of the song
	LOG(ELogGeneral,1,"GetAlbumArtL: start (iFileDirEntry: %x)",aMetadata.iFileDirEntry);

	TFileName cover;
	aMetadata.iFileDirEntry->GetFullPath(cover);
	LOG0("Request for file: %S",&cover)
	TInt pos=cover.LocateReverse('\\');
	if(pos>=0)
		cover.Delete(pos+1,cover.Length()-pos-1);
	cover.Append(KAlbumCoverName_);
	TBuf<5> size;
	size.Format(_L("%d"),aTheme.iAlbumArtSize.iWidth);
	cover.Append(size);
	cover.Append(KAlbumCoverExtension);
		
	//check to see if we already have the image in our cache
	TInt i;
	for(i=0;i<iFilenames.Count();i++)
		if(cover== *iFilenames[i] && aTheme.iAlbumArtSize== *iSizes[i])
		{
			//yes, we have it in the cache
			aTheme.iAlbumArt=iImages[i];

			//some LOG stuff
			TSize sz(iImages[i]->SizeInPixels());
			if(sz!=aTheme.iAlbumArtSize)
			{
				LOG0("ATTENTION: Returned size (%dx%d) different from requested size",sz.iWidth,sz.iHeight);
				LOG0("Requested size was %dx%d",aTheme.iAlbumArtSize.iWidth,aTheme.iAlbumArtSize.iHeight);
			};

			//move the found elements in the beginning of their array
			UpdateElementsInCache(i);
			LOG(ELogGeneral,-1,"GetAlbumArtL: end: album art was found in cache");
			return KErrNone;
		};
	//if we are here, the image is not in our cache
	
	//check if this is request that would need to be enqueued or it takes priority
	if(iState!=EIdle)
	{
		//we are going to enqueue this request
		//but first, we cancel any enqueued request, if it is not coming from the same Track 
		if(iEnqueuedTheme)
		{
			if(iEnqueuedTheme->iMySize==aTheme.iMySize && iEnqueuedTheme->iAlbumArtSize==aTheme.iAlbumArtSize)
			{
				LOG(ELogGeneral,-1,"GetAlbumArtL: end: request is the same as the enqueued request");
				return KErrNone; //same object
			}
			else
			{
				//cancel the enqueued request
				iEnqueuedTheme->iAlbumArt=NULL;
				iEnqueuedTheme=NULL;
				iEnqueuedFilename->Des().SetLength(0);
			};
		}
		// else e may have something enqueued, without a theme, but the iEnqueuedFilename will be overwritten anyway 
		//enqueue our request
		EnqueueRequestL(cover,aTheme.iAlbumArtSize,&aTheme);
		LOG(ELogGeneral,-1,"GetAlbumArtL: end: current request replaced the enqueued request");
		return KErrNone;		
	};
	
	//if we are here, we need to create the bitmap and transfer it
	aTheme.iAlbumArt=NULL;
	TInt err=DoProcessingL(cover,aTheme.iAlbumArtSize,&aTheme);
	if(err==KErrNotFound)
	{
		//try the generic album art
		LOG(ELogGeneral,-1,"GetAlbumArtL: end: get generic album art");
		return GetGenericAlbumArtL(aTheme);
	}
	else return err;
	LOG(ELogGeneral,-1,"GetAlbumArtL: end");
}

TInt CMusicPlayerImgEngine::GetAlbumArtFilename(const CMetadata &aMetadata, const TSize &aSize, TFileName &aCoverFilename) //returns KErrNotFound or KErrNone
{
	/* The algorithm for finding the album art image:
	 *  1. Create the filename TrackFolder/cover_SZ.jpg, SZ=requested size (e.g. cover_360.jpg)
	 *  2. Look for TrackFolder/cover_SZ.jpg in our cache. If found, return it, update cache
	 *  3. if not found in cache, we look for TrackFolder/cover_SZ.jpg in the file system. If found, return the filename
	 *  4. If we have the Album & Artist metadata (not null, more than 1 in length) we create the filename: Source/___Metadata/AlbumArtSized/SZ/Artist-Album.jpg
	 *  5. we look for the above filename in our cache. If found, return it, update cache
	 *  6. if not found in cache, we look for this filename in the file system. If found, return the filename
	 *  7. Create the filename TrackFolder/cover.jpg
	 *  8. Look for the filename in the file system. If found, return it. Flag that the image needs resizing, and fill in the target filename (TrackFolder/cover_SZ.jpg)
	 *  9. If filename not found and we have Album & Artist metadata, create the filename: Source/___Metadata/AlbumArtSized/Artist-Album.jpg
	 * 10. Look for the filename in the file system. if found, return it. Flag that the image needs resizing, and fill in the target filename (Source/___Metadata/AlbumArtSized/SZ/Artist-Album.jpg)
	 *     Add the above filename (Source/___Metadata/AlbumArtSized/Artist-Album.jpg) to Source/___Metadata/MissingAlbumArt.txt file 
	 *     (load the file's content into memory, add to memory, in destructor save file)
	 */
	aMetadata.iFileDirEntry->GetFullPath(aCoverFilename);

	/*
		TInt pos=aCoverFilename.LocateReverse('\\');
		if(pos>=0)
			aCoverFilename.Delete(pos+1,cover.Length()-pos-1);
		cover.Append(KAlbumCoverName_);
		TBuf<5> size;
		size.Format(_L("%d"),aTheme.iAlbumArtSize.iWidth);
		cover.Append(size);
		cover.Append(KAlbumCoverExtension);
			
		//check to see if we already have the image in our cache
		TInt i;
		for(i=0;i<iFilenames.Count();i++)
			if(cover== *iFilenames[i] && aTheme.iAlbumArtSize== *iSizes[i])
			{
				//yes, we have it in the cache
				aTheme.iAlbumArt=iImages[i];

				//some LOG stuff
				TSize sz(iImages[i]->SizeInPixels());
				if(sz!=aTheme.iAlbumArtSize)
				{
					LOG2("ATTENTION: Returned size (%dx%d) different from requested size",sz.iWidth,sz.iHeight);
					LOG2("Requested size was %dx%d",aTheme.iAlbumArtSize.iWidth,aTheme.iAlbumArtSize.iHeight);
				};

				//move the found elements in the beginning of their array
				UpdateElementsInCache(i);
				LOG("GetAlbumArtL: end: album art was found in cache");
				return KErrNone;
			};
		//if we are here, the image is not in our cache		
		 * 
		 */
}

TInt CMusicPlayerImgEngine::DoProcessingL(TFileName &aFilename, const TSize &aSize, CTheme *aTheme)
{
	LOG(ELogGeneral,1,"CMusicPlayerImgEngine::DoProcessingL: start (%S)",&aFilename);
	LOG0("Requested size: %dx%d",aSize.iWidth,aSize.iHeight);
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
	LOG0("iCurrenFilename set");
	
	// create the decoder
	TRAPD(err, iImageDecoder = CImageDecoder::FileNewL( iEikEnv->FsSession() , aFilename ));
	if(err)
	{
		//most probably the file has not been found. Try to convert a generic file
		LOG0("%S not found. Trying the generic name.",&aFilename);
		//first, construct the generic name
		TInt pos=aFilename.LocateReverse('\\');
		if(pos>=0)
			aFilename.Delete(pos+1,aFilename.Length()-pos-1);
		aFilename.Append(KAlbumCoverNameGeneric);
		//now try to open this
		TRAP(err, iImageDecoder = CImageDecoder::FileNewL( iEikEnv->FsSession(), aFilename ));
		if(err)
		{
			LOG0("Generic name (%S) not found. Trying second generic name.",&aFilename);
			//reuse previously computed pos
			if(pos>=0)
				aFilename.Delete(pos+1,aFilename.Length()-pos-1);
			aFilename.Append(KAlbumCoverNameGeneric2);
			//now try to open this
			TRAP(err, iImageDecoder = CImageDecoder::FileNewL( iEikEnv->FsSession(), aFilename ));
			if(err)
			{
				LOG(ELogGeneral,-1,"Generic name (%S) not found either. There will be no album art",&aFilename);
				iCurrentFilename->Des().SetLength(0);
				iCurrentTheme=NULL;
				iFlags=0;
				//if(aTheme)aTheme->iAlbumArt=NULL;
				return KErrNotFound; //there will be no cover art
			}
			else iFlags|=ENeedsScaling;
		}
		else iFlags|=ENeedsScaling;
	};
	if(iImageDecoder->FrameInfo().iOverallSizeInPixels != aSize)
		iFlags|=ENeedsScaling;

	LOG0("Creating bitmap for file %S",&aFilename);
	// create the destination bitmap
	//iBitmap = new CWsBitmap(iEikEnv->WsSession());
	iBitmap = new CFbsBitmap();
	if(!iBitmap)
	{
		LOG(ELogGeneral,-1,"No memory to instatntiate the bitmap! There will be no cover art.");
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
		LOG(ELogGeneral,-1,"No memory to Create() the bitmap! There will be no cover art.");
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
		iCurrentTheme->iFlags|=CTheme::EAlbumArtProcessing;
	if(iCurrentTheme)
		LOG0("!!!!! CACHE MISS !!!!!");
	LOG(ELogGeneral,-1,"CMusicPlayerImgEngine::DoProcessingL: end successfully");
	return KErrNone;
}

void CMusicPlayerImgEngine::PrepareGenericAlbumArtL(const TSize &aAlbumArtSize)
{
	LOG(ELogGeneral,1,"PrepareGenericAlbumArtL: start");
	
	if(!iGenericAlbumArtName.Length())
	{
		//asign one of the values: KGenericAlbumArtName or KAlternateGenericAlbumArtName
		TEntry entry;
		TInt err=iEikEnv->FsSession().Entry(KAlternateGenericAlbumArtName,entry);
		if(!err)iGenericAlbumArtName.Copy(KAlternateGenericAlbumArtName);
		else iGenericAlbumArtName.Copy(iPreferences->PrivatePathPlus(KGenericAlbumArtName));
	};

	//check to see if we already have the image in our cache
	TInt i;
	for(i=0;i<iFilenames.Count();i++)
		if(iGenericAlbumArtName == *iFilenames[i] && aAlbumArtSize== *iSizes[i])
		{
			//yes, we have it in the cache
			//move the found elements in the beginning of their array
			UpdateElementsInCache(i);
			LOG(ELogGeneral,-1,"PrepareGenericAlbumArtL: end: album art was found in cache");
			return;
		};
	//if we are here, the image is not in our cache

	//check if this is request that would need to be enqueued or it takes priority
	if(iState!=EIdle)
	{
		//we are going to enqueue this request, if there is no other request enqueued
		if(iEnqueuedTheme)
		{
			LOG(ELogGeneral,-1,"PrepareGenericAlbumArtL: there is already an enqueued request, aborting");
			return;
		};
		//enqueue our request
		EnqueueRequestL(iGenericAlbumArtName,aAlbumArtSize,NULL);
		LOG(ELogGeneral,-1,"PrepareGenericAlbumArtL: end: current request replaced the enqueued request");
		return;		
	};

	//if we are here, we need to create the bitmap and transfer it
	TFileName cover(iGenericAlbumArtName);
	DoProcessingL(cover,aAlbumArtSize,NULL);
	LOG(ELogGeneral,-1,"PrepareGenericAlbumArtL: end");
}

TInt CMusicPlayerImgEngine::GetGenericAlbumArtL(CTheme &aTheme)
{
	LOG(ELogGeneral,1,"GetGenericAlbumArtL: start");

	if(!iGenericAlbumArtName.Length())
	{
		//asign one of the values: KGenericAlbumArtName or KAlternateGenericAlbumArtName
		TEntry entry;
		TInt err=iEikEnv->FsSession().Entry(KAlternateGenericAlbumArtName,entry);
		if(!err)iGenericAlbumArtName.Copy(KAlternateGenericAlbumArtName);
		else iGenericAlbumArtName.Copy(iPreferences->PrivatePathPlus(KGenericAlbumArtName));
	};

	//check to see if we already have the image in our cache
	TInt i;
	for(i=0;i<iFilenames.Count();i++)
		if(iGenericAlbumArtName == *iFilenames[i] && aTheme.iAlbumArtSize== *iSizes[i])
		{
			//yes, we have it in the cache
			aTheme.iAlbumArt=iImages[i];

			//some LOG stuff
			TSize sz(iImages[i]->SizeInPixels());
			if(sz!=aTheme.iAlbumArtSize)
			{
				LOG0("ATTENTION: Returned size (%dx%d) different from requested size",sz.iWidth,sz.iHeight);
				LOG0("Requested size was %dx%d",aTheme.iAlbumArtSize.iWidth,aTheme.iAlbumArtSize.iHeight);
			};

			//move the found elements in the beginning of their array
			UpdateElementsInCache(i);
			LOG(ELogGeneral,-1,"GetGenericAlbumArtL: end: album art was found in cache");
			return KErrNone;
		};
	//if we are here, the image is not in our cache

	//check if this is request that would need to be enqueued or it takes priority
	TFileName cover(iGenericAlbumArtName);
	if(iState!=EIdle)
	{
		//we are going to enqueue this request
		//but first, we cancel any enqueued request, if it is not coming from the same Track 
		if(iEnqueuedTheme)
		{
			if(iEnqueuedTheme->iMySize==aTheme.iMySize && iEnqueuedTheme->iAlbumArtSize==aTheme.iAlbumArtSize)
			{
				LOG(ELogGeneral,-1,"GetGenericAlbumArtL: end: request is the same as the enqueued request");
				return KErrNone; //same object
			}
			else
			{
				//cancel the enqueued request
				iEnqueuedTheme->iAlbumArt=NULL;
				iEnqueuedTheme=NULL;
				iEnqueuedFilename->Des().SetLength(0);
			};
		}
		// else e may have something enqueued, without a theme, but the iEnqueuedFilename will be overwritten anyway 
		//enqueue our request
		EnqueueRequestL(cover,aTheme.iAlbumArtSize,&aTheme);
		LOG(ELogGeneral,-1,"GetGenericAlbumArtL: end: current request replaced the enqueued request");
		return KErrNone;		
	};

	//if we are here, we need to create the bitmap and transfer it
	aTheme.iAlbumArt=NULL;
	i=DoProcessingL(cover,aTheme.iAlbumArtSize,&aTheme);
	LOG(ELogGeneral,-1,"GetGenericAlbumArtL: end");
	return i;
}

void CMusicPlayerImgEngine::UpdateElementsInCache(TInt aIndex)
{
	//bring the aIndex elements in front, and push all the rest down
	//CWsBitmap *tmpBmp=iImages[aIndex];
	CFbsBitmap *tmpBmp=iImages[aIndex];
	TSize *tmpSize=iSizes[aIndex];
	HBufC *tmpFilename=iFilenames[aIndex];
	for(TInt i=aIndex-1;i>=0;i--)
	{
		iImages[i+1]=iImages[i];
		iSizes[i+1]=iSizes[i];
		iFilenames[i+1]=iFilenames[i];
	};
	iImages[0]=tmpBmp;
	iSizes[0]=tmpSize;
	iFilenames[0]=tmpFilename;
}

void CMusicPlayerImgEngine::CancelRequest(CTheme *aTheme)
{
	LOG(ELogGeneral,1,"CMusicPlayerImgEngine::CancelRequest: start");
	if(aTheme==iEnqueuedTheme)
	{
		iEnqueuedTheme=NULL;
		if(iEnqueuedFilename)iEnqueuedFilename->Des().SetLength(0);
	}
	else if(aTheme==iCurrentTheme)
	{
		LOG0("Canceling current request");
		Cancel();
		LOG0("Done canceling current request");
	};
	LOG(ELogGeneral,-1,"CMusicPlayerImgEngine::CancelRequest: end");
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
		{
			TFileName filename;
			//check if we should save the bitmap
			if(iPreferences->iFlags&CMLauncherPreferences::EPreferencesSaveScaledImages)
			{
				//we duplicate & save the bitmap
				if(iEncBitmap)delete iEncBitmap;
				iEncBitmap = new CFbsBitmap();
				if(iEncBitmap)iEncBitmap->Duplicate(iBitmap->Handle());
				//duplicate the filename (only needed if we encode the image)
				filename.Copy(*iCurrentFilename);
			}
			else iEncBitmap=NULL;

			//then, transfer & display the image
			TransferAlbumArt(ETrue);

			//now encode the image, so that next time we only need to decode it
			if(iEncBitmap)
			{
				if(iImageEncoder)delete iImageEncoder;
				iImageEncoder=CImageEncoder::FileNewL(iEikEnv->FsSession(),filename,KJpegMime);

				iImageEncoder->Convert(&iStatus,*iEncBitmap);
				iState = EEncoding;
				SetActive();
				//clean a bit
				delete iBmpScaler; iBmpScaler=NULL;
			}
			else 
			{
				//clean a bit
				delete iBmpScaler; iBmpScaler=NULL;
				ProcessNextRequest();
			};			
		}
		else
		{
			Clean();
			ProcessNextRequest();
		};
	};break;
	case EEncoding:
	{
		//encoding done, not much to do here
		delete iImageEncoder; iImageEncoder=NULL;
		delete iEncBitmap; iEncBitmap=NULL;
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
	if(iCurrentTheme)
	{
		iCurrentTheme->iAlbumArt=iBitmap;
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
	};
	TSize *sz=new TSize(iCurrentSize);
	if(!sz)return; //memory allocation did not succeed
	iSizes.Insert(sz,0);
	iImages.Insert(iBitmap,0);
	iBitmap=NULL;
	iFilenames.Insert(iCurrentFilename,0);
	iCurrentFilename=NULL;
	LOG(ELogGeneral,-1,"CMusicPlayerImgEngine::TransferAlbumArt: end");
}

void CMusicPlayerImgEngine::EnqueueRequestL(const TDesC& aFilename, const TSize &aSize, CTheme *aTheme)
{
	LOG(ELogGeneral,1,"CMusicPlayerImgEngine::EnqueueRequestL: start");
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
	LOG(ELogGeneral,-1,"CMusicPlayerImgEngine::EnqueueRequestL: end");
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
	TInt i;
	for(i=0;i<iFilenames.Count();i++)
		if(*iEnqueuedFilename== *iFilenames[i] && iEnqueuedSize == *iSizes[i])
		{
			//yes, we have it in the cache
			if(iEnqueuedTheme)
			{
				iEnqueuedTheme->iAlbumArt=iImages[i];
				iEnqueuedTheme->AlbumArtReady(KErrNone);
			};
			//move the found elements in the beginning of their array
			UpdateElementsInCache(i);
			//clean enqueued values
			iEnqueuedTheme=NULL;
			iEnqueuedFilename->Des().SetLength(0);
			LOG(ELogGeneral,-1,"CMusicPlayerImgEngine::ProcessNextRequest: request was found in the cache.");
			return;
		};
	//if we are here, the image is not in our cache
	TFileName tmpFilename(*iEnqueuedFilename);
	DoProcessingL(tmpFilename,iEnqueuedSize,iEnqueuedTheme);
	//clean enqueued values
	iEnqueuedTheme=NULL;
	iEnqueuedFilename->Des().SetLength(0);
	LOG(ELogGeneral,-1,"CMusicPlayerImgEngine::ProcessNextRequest: end (processing)");
}

void CMusicPlayerImgEngine::Clean()
{
	//clean
	delete iImageDecoder; iImageDecoder = NULL;
	delete iBitmap; iBitmap = NULL;
	delete iEncBitmap; iEncBitmap=NULL;
	if(iCurrentFilename)iCurrentFilename->Des().SetLength(0);
	if(iCurrentTheme)
	{
		iCurrentTheme->iAlbumArt=NULL;
		iCurrentTheme->iFlags&=~CTheme::EAlbumArtProcessing;
		iCurrentTheme->AlbumArtReady(KErrGeneral);
		iCurrentTheme=NULL;
	};
	delete iBmpScaler; iBmpScaler=NULL;
	delete iImageEncoder; iImageEncoder=NULL;
	iFlags=0;
}
