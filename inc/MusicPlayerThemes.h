/*
 * MusicPlayerThemes.h
 *
 *  Created on: 18.10.2008
 *      Author: Florin Lohan
 */

#ifndef MUSIPLAYERTHEMES_H_
#define MUSIPLAYERTHEMES_H_

#include <coecntrl.h>
#include "defs.h"
#ifdef __TOUCH_ENABLED__
#include <AknToolbarObserver.h> 
#endif

class CTheme;
class CColoredTheme;
class CMusicPlayerView;
class CMusicPlayerContainer;
class CMusicPlayerImgEngine;
class CEikonEnv;
class CEikLabel;
class CMetadata;
class CDirectScreenBitmap;
class TMpPixel;
class CMLauncherPreferences;

class CThemeManager : public CBase
{
public:
	
	static CThemeManager* NewL(CMusicPlayerView *aView, CEikonEnv *aEikEnv);
	
	~CThemeManager();
	
	CTheme *GetThemeL(const TRect &aRect, CTheme *aCurrentTheme);
	
	CTheme *GetTheOtherTheme(CTheme *aCurrentTheme);

private:
	CThemeManager();
	
	void ConstructL(CMusicPlayerView *aView, CEikonEnv *aEikEnv);
	
public:
	RPointerArray<CTheme> iThemes;
	CMusicPlayerView *iView; //not owned
	CEikonEnv *iEikEnv; //not owned
	CMusicPlayerImgEngine *iImgEngine;
	TInt iFormerPosition; //in seconds. This value is shared by all themes
};



class CTheme : public CBase
{
public:
	enum TFlags //we can use one byte (8 flags)
	{
		EAlbumArtProcessing=1
	};
	
	virtual ~CTheme();
	
	virtual void Draw(const TRect& /*aRect*/, CWindowGc& /*gc*/) const {};
	
	virtual void UpdateVolumeOnScreen(){};
	
	virtual TInt CountComponentControls();

	virtual CCoeControl* ComponentControl( TInt aIndex );
	
	virtual void HandlePointerEventL (const TPointerEvent &/*aPointerEvent*/){};
	
	virtual void SetMetadataL(CMetadata &aMetadata,TTimeIntervalMicroSeconds &aCurrentPosition)=0;
	
	virtual void SetContainer(CMusicPlayerContainer *aContainer);
	
	virtual void ActivateL(){};
	
	virtual void Deactivate(){}; //means switching to another theme
	
	virtual void AddMenuItemL(CEikMenuPane */*aMenu*/, TInt /*aPreviousId*/){};
	
	virtual void HandleCommandL(TInt /*aCommand*/){};
	
	virtual TBool UpdatePlaybackPosition(TTimeIntervalMicroSeconds &aCurrentPosition, TBool aDraw=ETrue)=0; //returns ETrue if the progress bar was updated
		
	virtual void PeriodicUpdate(TTimeIntervalMicroSeconds &aCurrentPosition)=0;
	
	virtual void AlbumArtReady(TInt /*aErr*/){};
	
	virtual void AllowScrolling(TBool /*aAllow*/){};
	
	virtual void ContentIsPreviewed(TBool /*aPreviewed*/){};
	
protected:
	CTheme();
	void ConstructL(TSize &aSize);
public:
	//general
	TSize iMySize;
	TUint iFlags;
	//sizes
	TSize iAlbumArtSize;
	//CWsBitmap* iAlbumArt; //not owned, but owned by the ImgEngine
	CFbsBitmap* iAlbumArt; //not owned, but owned by the ImgEngine
	TUint32 iAlbumArtDominantColor;
	//other stuff
	static CMLauncherPreferences *iPreferences; //not owned, public so that the contaner can access them
	static CThemeManager *iManager; //not owned
	static CMusicPlayerContainer *iContainer; //not owned
};

/*
class CRendering : public CActive, public MDirectScreenAccess
{
public:
	static CRendering *New(CColoredTheme &aTheme);

public:
	~CRendering();
	
	void ProcessFrame();

public: // from MDirectScreenAccess

	virtual void Restart( RDirectScreenAccess::TTerminationReasons aReason );
	virtual void AbortNow( RDirectScreenAccess::TTerminationReasons aReason );

protected: // from CActive

	void DoCancel();
	void RunL();

private:

	CRendering();
	TBool Construct(CColoredTheme &aTheme); //if EFalse, the Construction was not successful
private:
	//TBool iStopDSA;

	CDirectScreenAccess* iDrawer;
	CDirectScreenBitmap* iDSBitmap;

	TAcceleratedBitmapInfo iBitmapInfo;
	CColoredTheme* iTheme;
};
*/
const TInt KBgColorMask=3;
class CColoredTheme : public CTheme
{
	friend class CRendering;
public:
	enum TColorFlags //first byte is reserved for CTheme's flags
	{
		ENoProgressBarBoundary=0x0100,
		EArtistTitleNeedsScrolling=0x0200,
		//EDSAActive=0x0400,
		EShowVolumeMode=0x0800,
		EAsianFonts=0x01000,
		
		EDragOperation=0x010000,
		ESeekDrag=0x020000,
		EVolumeDrag=0x040000
	};
	enum TColorSchemes
	{
		//first 4 bits used for bg color
		EBgColorWhite=1,
		EBgColorBlack=2,
		//next 12 bits are used for color
		EColorGreen=0x10,
		EColorBlue=0x20,
		EColorOrange=0x40,
		EColorRed=0x80,
		EColorViolet=0x100
	};
	~CColoredTheme();
protected:
	CColoredTheme();
	
	virtual void SetMetadataL(CMetadata &aMetadata,TTimeIntervalMicroSeconds &aCurrentPosition);
	
	virtual void AddMenuItemL(CEikMenuPane *aMenu, TInt aPreviousId);

	virtual void HandleCommandL(TInt aCommand);

	virtual void SetColorScheme(TUint aColors, TBool aRedraw=ETrue);
	
	virtual void Draw(const TRect& aRect, CWindowGc& gc) const;
	
	//virtual void PeriodicUpdate(TTimeIntervalMicroSeconds &aCurrentPosition);
	
	virtual void AlbumArtReady(TInt aErr);
	
	virtual void ActivateL();
		
	virtual void Deactivate(); //means switching to another theme
	
	void FormatTimeString(TDes& aString, TInt aSeconds, TInt aTotalDuration);
	
	virtual void AllowScrolling(TBool aAllow);
	
	void ComputeVolumeSize(TInt aDesiredSize);
	//void DrawVolumeL(const TRect& aRect, CWindowGc& gc, const TPoint &aPosition, const TInt aHorizontalLength, const TInt aMinBarHeight, const TInt aMaxBarHeight) const;
	//void DrawVolumeL(const TRect& aRect, CWindowGc& gc, const TRect &aVolumeRect, const TBool aForceSmall=EFalse) const;
	
	//void PrepareDsaL();
private:
	static void CreateBrushFills();
	void CreateArtistTitleScrollingBitmap();
	void DrawVolumeL(CWindowGc& gc) const;
	
	TBool SetMatchingTheme(); //returns ETrue if the theme has been changed
	static TInt ScrollFunction(TAny* aInstance);
	
	static void SetStaticColorScheme(TUint aColors, TBool &aRecreateATbitmap);
public:
	TUint GetColorScheme(){return iMyColors;}
	
protected:
	//size, positions
	TPoint iAlbumArtPosition;
	TPoint iArtistTitlePosition;
	TRect iArtistTitleRect;
	TRect iProgressBar;
	TInt iProgressBarRounded;
	//volume
	TRect iVolumeRect;
	TInt iSpeakerWidth; //new in 0.107
	TInt iLevelWidth; //new in 0.107
	//metadata
	TInt iDuration; //in seconds
	HBufC *iArtistTitle;
	//artist+title scrolling
	TInt iArtistTitleTotalLength;
	TInt iArtistTitleOffset;
	CWsBitmap *iArtistTitleBitmap;
	//TUint8 *iATBdata;
	//TUint8 *iATBdataAligned; //do not delete, points to somewhere in iATBdata;
	//TInt iATBdataSize;
	CPeriodic *iScrollingTimer;
	// Direct Screen Access
	//CRendering *iDSArendering;
	  
	//colors, font
	//the colors are static, so if we change them in one theme, they get changed everywhere
	static TRgb iColorBackground;
	static TRgb iColorText;
	static TUint32 iColorStartFillProgressBar;
	static TUint32 iColorStepFillProgressBar;
	static TRgb iColorProgressBarBoundary;
	static CFbsBitmap *iHugeFillV;
	static CFbsBitmap *iBigFillV;
	static CFbsBitmap *iSmallFillV;
	static CFbsBitmap *iSmallFillH;
	static TInt iNrInstances;
	static TUint iMyStaticColors;
	TUint iMyColors;
	const CFont *iFont; //not owned
private:
	TInt iVolumeSize; //this can be 10, 16 or 20
};

#ifdef __TOUCH_ENABLED__
class CNHDTheme : public CColoredTheme
{
protected:
	~CNHDTheme();
	static void SetStaticColorScheme(TUint aColors);
protected:
	static CFbsBitmap *iPlay;
	static CFbsBitmap *iPlayMask;
	static CFbsBitmap *iPause;
	static CFbsBitmap *iPauseMask;
	static CFbsBitmap *iFf;
	static CFbsBitmap *iFfMask;
	static CFbsBitmap *iFr;
	static CFbsBitmap *iFrMask;
private:
	static TUint iMyNHDStaticColors;
};

class CThemeNHDPortrait : public CNHDTheme
{
	friend class CThemeManager;
public:
	virtual ~CThemeNHDPortrait();

private:
	static CThemeNHDPortrait* NewL(TSize &aSize);
	CThemeNHDPortrait();
	void ConstructL(TSize &aSize);
public:

	virtual void Draw(const TRect& aRect, CWindowGc& gc) const;
	
	virtual TInt CountComponentControls();

	virtual CCoeControl* ComponentControl( TInt aIndex );
	
	virtual void HandlePointerEventL(const TPointerEvent &aPointerEvent);
	
	virtual void SetMetadataL(CMetadata &aMetadata,TTimeIntervalMicroSeconds &aCurrentPosition);
	
	virtual void SetContainer(CMusicPlayerContainer *aContainer);
	
	virtual TBool UpdatePlaybackPosition(TTimeIntervalMicroSeconds &aCurrentPosition, TBool aDraw=ETrue); //returns ETrue if the progress bar was updated
			
	virtual void PeriodicUpdate(TTimeIntervalMicroSeconds &aCurrentPosition);
	
	virtual void UpdateVolumeOnScreen();
	
	//virtual void ContentIsPreviewed(TBool aPreviewed);
	virtual void ActivateL();
	
protected:
	virtual void SetColorScheme(TUint aColors, TBool aRedraw=ETrue);
	
	static TInt ExitVolumeMode(TAny* aInstance);
	
private:
	CEikLabel *iLabelElapsedTime;
	CEikLabel *iTotalTime;
	CEikLabel *iPositionInPlaylist;
	CEikLabel *iButtonLeft;
	CEikLabel *iButtonRight;
	TInt iTimeLabelWidth;
	TBool iRightButtonShowsBack;

	CFbsBitmap *iArrows;
	CFbsBitmap *iArrowsMask;
	TPoint iDragStart;
	
	CPeriodic *iVolumeTimer;
	
	//
	HBufC *iTxtOptions;
	HBufC *iTxtShow;
	HBufC *iTxtBack;
};

class CThemeNHDLandscape : public CNHDTheme
{
	friend class CThemeManager;
public:
	virtual ~CThemeNHDLandscape();

private:
	static CThemeNHDLandscape* NewL(TSize &aSize);
	CThemeNHDLandscape();
	void ConstructL(TSize &aSize);
public:
	virtual TInt CountComponentControls();

	virtual CCoeControl* ComponentControl( TInt aIndex );
	
	virtual void HandlePointerEventL(const TPointerEvent &aPointerEvent);
	
	virtual void SetMetadataL(CMetadata &aMetadata,TTimeIntervalMicroSeconds &aCurrentPosition);
	
	virtual void SetContainer(CMusicPlayerContainer *aContainer);
	
	virtual TBool UpdatePlaybackPosition(TTimeIntervalMicroSeconds &aCurrentPosition, TBool aDraw=ETrue); //returns ETrue if the progress bar was updated
			
	virtual void PeriodicUpdate(TTimeIntervalMicroSeconds &aCurrentPosition);
	
	virtual void UpdateVolumeOnScreen();
	
	virtual void Draw(const TRect& aRect, CWindowGc& gc) const;
protected:
	virtual void SetColorScheme(TUint aColors, TBool aRedraw=ETrue);

private:
	CEikLabel *iLabelElapsedTime;
	CEikLabel *iTotalTime;
	CEikLabel *iPositionInPlaylist;
	//CEikLabel *iAlbumYear;
	TInt iTimeLabelWidth;
	
	TPoint iDragStart;
	CFont *iSmallFont; //not owned

	//volume change with touch
	TInt iVolInitialX;
};

class CThemeVGALandscape : public CNHDTheme
{
	friend class CThemeManager;
public:
	virtual ~CThemeVGALandscape();

private:
	static CThemeVGALandscape* NewL(TSize &aSize);
	CThemeVGALandscape();
	void ConstructL(TSize &aSize);
public:
	virtual TInt CountComponentControls();

	virtual CCoeControl* ComponentControl( TInt aIndex );
	
	virtual void HandlePointerEventL(const TPointerEvent &aPointerEvent);
	
	virtual void SetMetadataL(CMetadata &aMetadata,TTimeIntervalMicroSeconds &aCurrentPosition);
	
	virtual void SetContainer(CMusicPlayerContainer *aContainer);
	
	virtual TBool UpdatePlaybackPosition(TTimeIntervalMicroSeconds &aCurrentPosition, TBool aDraw=ETrue); //returns ETrue if the progress bar was updated
			
	virtual void PeriodicUpdate(TTimeIntervalMicroSeconds &aCurrentPosition);
	
	virtual void UpdateVolumeOnScreen();
	
	virtual void Draw(const TRect& aRect, CWindowGc& gc) const;
	
	virtual void ActivateL();
protected:
	virtual void SetColorScheme(TUint aColors, TBool aRedraw=ETrue);

private:
	CEikLabel *iLabelElapsedTime;
	CEikLabel *iTotalTime;
	CEikLabel *iPositionInPlaylist;
	CEikLabel *iButtonLeft;
	CEikLabel *iButtonRight;
	TInt iTimeLabelWidth;
	
	TBool iRightButtonShowsBack;
	
	TPoint iDragStart;
	CFont *iSmallFont; //not owned

	//volume change with touch
	TInt iVolInitialX;
	
	//
	HBufC *iTxtOptions;
	HBufC *iTxtShow;
	HBufC *iTxtBack;
};
#endif

class CThemeQVGAPortrait : public CColoredTheme
{
	friend class CThemeManager;
public:
	virtual ~CThemeQVGAPortrait();

private:
	static CThemeQVGAPortrait* NewL(TSize &aSize);
	CThemeQVGAPortrait();
	void ConstructL(TSize &aSize);
public:
	virtual TInt CountComponentControls();

	virtual CCoeControl* ComponentControl( TInt aIndex );
	
	virtual void SetMetadataL(CMetadata &aMetadata,TTimeIntervalMicroSeconds &aCurrentPosition);
	
	virtual void SetContainer(CMusicPlayerContainer *aContainer);
	
	virtual TBool UpdatePlaybackPosition(TTimeIntervalMicroSeconds &aCurrentPosition, TBool aDraw=ETrue); //returns ETrue if the progress bar was updated
			
	virtual void PeriodicUpdate(TTimeIntervalMicroSeconds &aCurrentPosition);
	
	virtual void UpdateVolumeOnScreen();

protected:
	virtual void SetColorScheme(TUint aColors, TBool aRedraw=ETrue);
private:
	CEikLabel *iLabelElapsedTime;
	CEikLabel *iTotalTime;
	CEikLabel *iPositionInPlaylist;
	TInt iTimeLabelWidth;
};

class CThemeDoublePortrait : public CColoredTheme
{
	friend class CThemeManager;
public:
	//virtual ~CThemeDoublePortrait();  //not needed

private:
	static CThemeDoublePortrait* NewL(TSize &aSize);
	CThemeDoublePortrait();
	void ConstructL(TSize &aSize);
public:
	
	virtual void SetMetadataL(CMetadata &aMetadata,TTimeIntervalMicroSeconds &aCurrentPosition);
	
	virtual TBool UpdatePlaybackPosition(TTimeIntervalMicroSeconds &aCurrentPosition, TBool aDraw=ETrue); //returns ETrue if the progress bar was updated
			
	virtual void PeriodicUpdate(TTimeIntervalMicroSeconds &aCurrentPosition);

protected:
	virtual void SetColorScheme(TUint aColors, TBool aRedraw=ETrue);
private:
	TInt iPlaybactTimePosition;
};


class CThemeGeneric : public CColoredTheme
{
	friend class CThemeManager;
public:
	virtual ~CThemeGeneric();  //not needed

private:
	static CThemeGeneric* NewL(TSize &aSize);
	CThemeGeneric();
	void ConstructL(TSize &aSize);
public:
	
	virtual TInt CountComponentControls();

	virtual CCoeControl* ComponentControl( TInt aIndex );

	virtual void SetContainer(CMusicPlayerContainer *aContainer);
		
	virtual void SetMetadataL(CMetadata &aMetadata,TTimeIntervalMicroSeconds &aCurrentPosition);
	
	virtual TBool UpdatePlaybackPosition(TTimeIntervalMicroSeconds &aCurrentPosition, TBool aDraw=ETrue); //returns ETrue if the progress bar was updated
			
	virtual void PeriodicUpdate(TTimeIntervalMicroSeconds &aCurrentPosition);

protected:
	virtual void SetColorScheme(TUint aColors, TBool aRedraw=ETrue);
	
private:
	CEikLabel *iLabelElapsedTime;
	CEikLabel *iTotalTime;
	CEikLabel *iPositionInPlaylist;
	CEikLabel *iAlbumYear;
	TInt iVerticalStep;
	//TInt iTimeLabelWidth;

};

class CThemeLandscapeGeneric : public CColoredTheme
{
	friend class CThemeManager;
public:
	virtual ~CThemeLandscapeGeneric();  //not needed

private:
	static CThemeLandscapeGeneric* NewL(TSize &aSize);
	CThemeLandscapeGeneric();
	void ConstructL(TSize &aSize);
public:
	virtual TInt CountComponentControls();

	virtual CCoeControl* ComponentControl( TInt aIndex );

	virtual void SetContainer(CMusicPlayerContainer *aContainer);
		
	virtual void SetMetadataL(CMetadata &aMetadata,TTimeIntervalMicroSeconds &aCurrentPosition);
	
	virtual TBool UpdatePlaybackPosition(TTimeIntervalMicroSeconds &aCurrentPosition, TBool aDraw=ETrue); //returns ETrue if the progress bar was updated
			
	virtual void PeriodicUpdate(TTimeIntervalMicroSeconds &aCurrentPosition);
	
	virtual void UpdateVolumeOnScreen();

protected:
	virtual void SetColorScheme(TUint aColors, TBool aRedraw=ETrue);
	
private:
	CEikLabel *iLabelElapsedTime;
	CEikLabel *iTotalTime;
	CEikLabel *iPositionInPlaylist;
	CEikLabel *iAlbumYear;
	TInt iTimeLabelWidth;
};


#endif /* MUSIPLAYERTHEMES_H_ */
