/*
 ============================================================================
 Name		 : MusicPlayerImgEngine.h
 Author	     : Florin Lohan
 Version	 : 1.0
 Copyright   : Florin Lohan
 Description : CMusicPlayerImgEngine declaration
 ============================================================================
 */

#ifndef MUSICPLAYERIMGENGINE_H
#define MUSICPLAYERIMGENGINE_H

#include <e32base.h>	// For CActive, link against: euser.lib
#include <e32std.h>		// For RTimer, link against: euser.lib

class CImageDecoder;
class CImageEncoder;
class CBitmapScaler;
class CEikonEnv;

class CTheme;
class CMetadata;
class CMLauncherPreferences;

class CMusicPlayerImgEngine : public CActive
{
public:
	// Cancel and destroy
	~CMusicPlayerImgEngine();

	// Two-phased constructor.
	static CMusicPlayerImgEngine* NewL(CEikonEnv *aEikEnv, CMLauncherPreferences *aPreferences);

	// Two-phased constructor.
	static CMusicPlayerImgEngine* NewLC(CEikonEnv *aEikEnv, CMLauncherPreferences *aPreferences);

public:
	// New functions
	// Function for making the initial request
	void PrepareAlbumArtL(const CMetadata &aMetadata, const TSize &aAlbumArtSize);
	
	void GetAlbumArtL(const CMetadata &aMetadata, CTheme &aTheme); //If it returns and image is NOT available, it will call CTheme::AlbumArtReady when the image is ready
	
	void CancelRequest(CTheme *aTheme);
	
	TBool AreThereCoverHintFiles(TBool aDeleteThem=EFalse); //returns ETrue if there are

private: //own functions
	TInt GetAlbumArtFilenameHelper(const TSize &aSize, TFileName &aCoverFilename, CImageDecoder **aDecoder);
	TInt GetAlbumArtFilename(const CMetadata &aMetadata, const TSize &aSize, TFileName &aCoverFilename, CImageDecoder **aDecoder); //returns 1 if request found in cache, 0 if the request was found and we have a decoder, KErrNotFound if there is no decoder
	TBool CheckAgainstCurrentAndEnqueued(TBool aForceEnqueue, const TDesC& aFilename, CImageDecoder *aImgDec, const TSize &aSize, CTheme *aTheme); //returns ETrue if the request was enqueued, EFalse if it was discarded. 
	
	void TransferAlbumArt(TBool aWasActive);
	
	//inline void EnqueueRequestL(const TDesC& aFilename, CImageDecoder *aImgDec, const TSize &aSize, CTheme *aTheme); //aTheme can be NULL
	
	void ProcessNextRequest();

	void Clean();
	
	void UpdateElementsInCache(TInt aIndex);
	TBool IsInCache(const TDesC &aFilename, const TSize &aSize); //returns ETrue if found. Also updates the cache, so the searched element has index 0
	
	TInt DoProcessingL(const TDesC &aFilename, CImageDecoder *aImgDec, const TSize &aSize, CTheme *aTheme); //aTheme can be NULL
	
	TUint32 GetDominantColor(CFbsBitmap *aBmp);
	/*
	void PrepareGenericAlbumArtL(const TSize &aAlbumArtSize);
	
	TInt GetGenericAlbumArtL(CTheme &aTheme);*/
private:
	// C++ constructor
	CMusicPlayerImgEngine();

	// Second-phase constructor
	void ConstructL(CEikonEnv *aEikEnv, CMLauncherPreferences *aPreferences);

private:
	// From CActive
	// Handle completion
	void RunL();

	// How to cancel me
	void DoCancel();

	// Override to handle leaves from RunL(). Default implementation causes
	// the active scheduler to panic.
	TInt RunError(TInt aError);

private:
	enum TMusicPlayerImgEngineState
	{
		EIdle, // Uninitialized
		EDecoding, // Decoding an image
		EScaling
	};
	enum TFlags
	{
		ENeedsScaling=1
	};

private:
	TInt iState; // State of the active object
	TUint iFlags;
	//RFs *iFs; //not owned
	CEikonEnv *iEikEnv; //not owned
	CMLauncherPreferences *iPreferences;//not owned
	//TFileName iGenericAlbumArtName;

	//image vars
	//CWsBitmap* iBitmap; // decoded image (CWsBitmap does not work if we need e.g. scaling)
	CFbsBitmap* iBitmap; // decoded image
	
	CImageDecoder* iImageDecoder; // decoder from ICL API
	CBitmapScaler* iBmpScaler; //scaler from ICL API
	//current vars
	HBufC *iCurrentFilename; //the filename that corresponds to the current image, owned
	CTheme *iCurrentTheme; //not owned
	TSize iCurrentSize;

	//enqueued vars
	HBufC *iEnqueuedFilename; //owned
	CTheme *iEnqueuedTheme; //not owned, might be NULL
	TSize iEnqueuedSize;
	CImageDecoder *iEnqueuedDecoder;
	
	//image cache
	//RPointerArray<CWsBitmap> iImages;
	RPointerArray<CFbsBitmap> iImages;
	RPointerArray<HBufC> iFilenames;
	RPointerArray<TSize> iSizes;
	RArray<TUint32> iDominantColors;
	RPointerArray<CFbsBitmap> iGenericImages;
	TSize iGenericSize0,iGenericSize1;
};

#endif // MUSICPLAYERIMGENGINE_H
