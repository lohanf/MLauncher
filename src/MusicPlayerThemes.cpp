/*
 * MusicPlayerThemes.cpp
 *
 *  Created on: 22.10.2008
 *      Author: Florin Lohan
 */

#include <coecntrl.h>
#include <eikenv.h>
#include <eiklabel.h>
#include <eikmenup.h>
#include <avkon.hrh>
#include <aknsutils.h> //loading icons

#include <gulfont.h>

#include <MLauncher.rsg>
#include <MLauncher.mbg> //loading icons
#include <avkon.mbg> //loading icons
#include <stringloader.h>

//DSA
#include <cdsb.h>

#include "MusicPlayerThemes.h"
#include "MLauncherPreferences.h"
#include "MusicPlayerContainer.h"
#include "MusicPlayerImgEngine.h"
#include "MusicPlayerView.h"
#include "MLauncher.hrh"
#include "MLauncher.pan"
#include "log.h"
#ifdef __TOUCH_ENABLED__
#include <akntoolbar.h>
#endif

const TInt KMemAlign=32; //align to L1 cache rows
const TInt KMemAlignPwr2=5; //the power of 2 for the previous number

const TSize KQVGAPortraitSize=TSize(240,293);
const TSize KQVGAPortraitSizeN958GB=TSize(240,295);
const TSize KDoublePortraitSize=TSize(352,376);
const TSize KnHDPortraitSize=TSize(360,640);
const TSize KnHDLandscapeSize=TSize(640,360);


const TInt KTextScrollPeriod=33333; //approx 30 times a second

_LIT(KEmpty,"");
_LIT(KSpace," ");
//_LIT(KSpaceAfterTitle,"     ");
const TInt KScrollPaddingInPixels=35;
const TInt KArtistTitleScrollStep=2;
_LIT(KArtistTitleSpace," - ");
_LIT(KTrackDetails,"[%d] ");
_LIT(KIndexInPlaylist, "[Song %d/%d]");
_LIT(KAlbumYear, " [%d]");
_LIT(KAlbumYearStandalone, "[Album year: %d]");
_LIT(KOwnIconFile, "\\resource\\apps\\MLauncher.mif");
_LIT(KTimeFormat,"%d:%02d");
_LIT(KTimeFormatHMS,"%d:%02d:%02d");
_LIT(KDoubleTimeFormat,"[%d:%02d/%d:%02d]"); 
_LIT(KDoubleTimeFormatHMS,"[%d:%02d:%02d/%d:%02d:%02d]"); 

CThemeManager* CThemeManager::NewL(CMusicPlayerView *aView, CEikonEnv *aEikEnv)
{
	CThemeManager* self = new (ELeave) CThemeManager();
	CleanupStack::PushL(self);
	self->ConstructL(aView,aEikEnv);
	CleanupStack::Pop(); // self;
	return self;
}

CThemeManager::CThemeManager()
{
	//no code necessary
}
	
void CThemeManager::ConstructL(CMusicPlayerView *aView, CEikonEnv *aEikEnv)
{
	iView=aView;
	iEikEnv=aEikEnv;
	iImgEngine=CMusicPlayerImgEngine::NewL(iEikEnv,aView->iPreferences);
	CTheme::iManager=this;
	CTheme::iPreferences=iView->iPreferences;
}

CThemeManager::~CThemeManager()
{
	iThemes.ResetAndDestroy();
    delete iImgEngine;
}

CTheme* CThemeManager::GetThemeL(const TRect &aRect, CTheme *aCurrentTheme)
{
	TSize size(aRect.Size());
	LOG0("CThemeManager::GetThemeL: requested size: %dx%d",size.iWidth,size.iHeight);
	//ma uit de ce se creaza mai multe theme aici
	if(aCurrentTheme && aCurrentTheme->iMySize!=size)
	{
		//theme is different
		//aCurrentTheme->Deactivate();
	};
	LOG0("Checking among instantiated themes (%d)",iThemes.Count());
	for(TInt i=0;i<iThemes.Count();i++)
	{
		LOG0("Checking with %dx%d",iThemes[i]->iMySize.iWidth,iThemes[i]->iMySize.iHeight);
		if(size==iThemes[i]->iMySize)
		{
			LOG0("Found!");
			return iThemes[i];
		};
	};
	
	LOG0("Theme not found, creating a new one!");
	//if we are here, it means that no theme was found
	CTheme *theme(NULL);
#ifdef __TOUCH_ENABLED__
	if(size==KnHDPortraitSize)
	{
		theme=CThemeNHDPortrait::NewL(size);
		iThemes.AppendL(theme);
	}
	else if(size==KnHDLandscapeSize)
	{
		theme=CThemeNHDLandscape::NewL(size);
		iThemes.AppendL(theme);
	}
	else 
#endif
		if(size==KQVGAPortraitSize)
	{
		theme=CThemeQVGAPortrait::NewL(size);
		iThemes.AppendL(theme);
	}
	else if(size==KQVGAPortraitSizeN958GB)
	{
		theme=CThemeQVGAPortrait::NewL(size);
		iThemes.AppendL(theme);
	}
	else if(size==KDoublePortraitSize)
	{
		theme=CThemeDoublePortrait::NewL(size);
		iThemes.AppendL(theme);
	}
	else if(size.iHeight<size.iWidth)
	{
		theme=CThemeLandscapeGeneric::NewL(size);
		iThemes.AppendL(theme);
	}
	else
	{
		//no theme found, get the generic theme
		LOG0("Size %dx%d not found, creating the generic theme!",size.iWidth,size.iHeight);
		theme=CThemeGeneric::NewL(size);
		iThemes.AppendL(theme);
	}
	return theme;
}

CTheme *CThemeManager::GetTheOtherTheme(CTheme *aCurrentTheme)
{
	TInt count(iThemes.Count());
	if(count<2)return NULL;
#ifndef __WINS__
	if(count>2)LOG0("ATTENTION: more than one theme available, so device has more than 2 screen sizes! (available themes: %d)",count);
#endif
	for(TInt i=0;i<count;i++)
		if(iThemes[i]!=aCurrentTheme)
			return iThemes[i];
	return NULL; //it should not reach here
}

///////////////////////////////////////////////////////////////////////	

CMLauncherPreferences *CTheme::iPreferences=NULL;
CThemeManager *CTheme::iManager=NULL;
CMusicPlayerContainer *CTheme::iContainer=NULL;

CTheme::CTheme()
{
	//no implementation necessary
}

void CTheme::ConstructL(TSize &aSize)
{
	iMySize=aSize;
	FL_ASSERT(iManager);
	FL_ASSERT(iPreferences);
}

CTheme::~CTheme()
{
	if(iFlags&EAlbumArtProcessing)
	{
		iManager->iImgEngine->CancelRequest(this);
	};
}

void CTheme::SetContainer(CMusicPlayerContainer *aContainer)
{
	iContainer=aContainer;
}

TInt CTheme::CountComponentControls()
{
	return 0;
}

CCoeControl* CTheme::ComponentControl( TInt /*aIndex*/ )
{
	return NULL;
}

///////////////////////////////////////////////////////////////////////	

#if 0
CRendering* CRendering::New(CColoredTheme &aTheme)
{
	if(aTheme.iPreferences->iFlags&CMLauncherPreferences::EHwDoNotUseDSA)
		return NULL;
	
	CRendering *self = new CRendering();
	if(!self)return NULL;
	if(!self->Construct(aTheme))
	{
		delete self;
		return NULL;
	};
	return self;
}

CRendering::~CRendering()
{
	// "Cancel" is a meaningless call, since the service 
	// (video driver's screen update process) is not cancellable.
	// When destroying this active object, you must make sure, 
	// that the last update request (CDirectScreenBitmap::EndUpdate()) is completed.
	// Assuming that LCD refresh rate is not less than 60 Hz,
	// the wait time should be more than 1/60 secs.
	// (Otherwise a stay signal comes.)
	
	if(IsActive())
	{
		//DeleteInstance();
		//return
		User::After(TTimeIntervalMicroSeconds32(100000));
		Cancel();	
	};
	
	delete iDrawer;
	iDSBitmap->Close();
	delete iDSBitmap;
	iTheme->iFlags&=~CColoredTheme::EDSAActive;
}


CRendering::CRendering(): CActive( CActive::EPriorityStandard )
{
}

TBool CRendering::Construct(CColoredTheme &aTheme)
{
	CActiveScheduler::Add( this );
	iTheme=&aTheme;
	if(!iTheme->iContainer)return EFalse;
	
	// Setting up direct screen access
	TInt err;
	CEikonEnv *eikEnv(iTheme->iManager->iEikEnv);
	TRAP(err,
			iDSBitmap = CDirectScreenBitmap::NewL();
	        iDrawer=CDirectScreenAccess::NewL(eikEnv->WsSession(), *eikEnv->ScreenDevice(), iTheme->iContainer->GetWindow(), *this);
	        eikEnv->WsSession().Flush();
	        iDrawer->StartL();
	);//TRAP
	if(err)return NULL;
	
	CFbsBitGc* gc = iDrawer->Gc();
	RRegion* region = iDrawer->DrawingRegion();
	gc->SetClippingRegion(region);

	// It may happen that a device does not support double buffering.
	err=iDSBitmap->Create(iTheme->iArtistTitleRect, CDirectScreenBitmap::EDoubleBuffer);
	if(err)
	{
		iTheme->iPreferences->iFlags|=CMLauncherPreferences::EHwDoNotUseDSA;
		return NULL;
	};
	
	iTheme->iFlags|=CColoredTheme::EDSAActive;
	
	//draw first frame
	ProcessFrame();
	return ETrue;
}
 
void CRendering::Restart( RDirectScreenAccess::TTerminationReasons /*aReason*/ )
    {
    TRAPD( err, iDrawer->StartL() ); 
    if(err)
    {
    	iTheme->iPreferences->iFlags|=CMLauncherPreferences::EHwDoNotUseDSA;
    	return;
    }
    CFbsBitGc* gc = iDrawer->Gc();
    RRegion* region = iDrawer->DrawingRegion();
    gc->SetClippingRegion(region);
    
    iDSBitmap->Create(iTheme->iArtistTitleRect, CDirectScreenBitmap::EDoubleBuffer); 
    
    // Put some code here to continue game engine	
    iTheme->iFlags|=CColoredTheme::EDSAActive;

    //draw first frame
    ProcessFrame();
    }
 
void CRendering::AbortNow( RDirectScreenAccess::TTerminationReasons /*aReason*/ )
    {
    // Put some code here to suspend game engine	
	iTheme->iFlags&=~CColoredTheme::EDSAActive;
    iDSBitmap->Close();
    }
 
void CRendering::RunL()
    {
    // Video driver finished to draw the last frame on the screen
    // You may initiate rendering the next frame from here, 
    // but it would be slow, since there is a delay between CDirectScreenBitmap::EndUpdate() 
    // and the completition of screen refresh by the video driver	
	ProcessFrame();
    }
 
void CRendering::DoCancel()
    {
    // Cancel not implemented in service provider, so we can't do anything here
    }
 
void CRendering::ProcessFrame()
    {
    // This method is responsible to render and draw a frame to the screen
	/*
	// Obtain the screen address every time before drawing the frame, 
	// since the address always changes
	iDSBitmap->BeginUpdate(iBitmapInfo);    
    
    // render the frame using iBitmapInfo.iAddress
	TInt i;
    TInt nrStrides=Min(iBitmapInfo.iSize.iHeight,iTheme->iArtistTitleBitmap->SizeInPixels().iHeight);
    TInt bytesPerPixel(TDisplayModeUtils::NumDisplayModeBitsPerPixel(iBitmapInfo.iDisplayMode )>>3);
    TInt offsetBytes(bytesPerPixel*iTheme->iArtistTitleOffset);
    TInt nrBytes=Min(iBitmapInfo.iLinePitch,iTheme->iArtistTitleBitmap->DataStride()-offsetBytes);
    
    TUint8 *dstStrideAddr=iBitmapInfo.iAddress;
    TUint8 *srcStrideAddr=iTheme->iATBdataAligned+offsetBytes;
    for(i=0;i<nrStrides;i++)
    {
    	Mem::Copy(dstStrideAddr,srcStrideAddr,nrBytes);
    	dstStrideAddr+=iBitmapInfo.iLinePitch;
    	srcStrideAddr+=iTheme->iArtistTitleBitmap->DataStride();
    };
    
    TInt remainingStringLength(iTheme->iArtistTitleRect.Width()+iTheme->iArtistTitleOffset-iTheme->iArtistTitleTotalLength);
    if(remainingStringLength>0)
    {
    	dstStrideAddr=iBitmapInfo.iAddress+(iTheme->iArtistTitleRect.Width()-remainingStringLength)*bytesPerPixel;
    	srcStrideAddr=iTheme->iATBdataAligned;
    	nrBytes=remainingStringLength*bytesPerPixel;
    	for(i=0;i<nrStrides;i++)
    	{
    		Mem::Copy(dstStrideAddr,srcStrideAddr,nrBytes);
    		dstStrideAddr+=iBitmapInfo.iLinePitch;
    		srcStrideAddr+=iTheme->iArtistTitleBitmap->DataStride();
    	};
    };
    //done rendering
    iTheme->iArtistTitleOffset+=KArtistTitleScrollStep;
    if(iTheme->iArtistTitleOffset>=iTheme->iArtistTitleTotalLength)
    	iTheme->iArtistTitleOffset-=iTheme->iArtistTitleTotalLength;
    
    //end draw
    if (IsActive())
    {
    	Cancel();
    }
    iDSBitmap->EndUpdate(iStatus);
    SetActive();
    */
    }
#endif
///////////////////////////////////////////////////////////////////////	

//const TInt KMaxVolumeBarWidth=16; //the length of the volume bar fill pattern
const TInt KMaxFillHeight=32; //the height of the progress bar fill pattern 

const TInt KVolumeSizeSmall=10;
const TInt KVolumeSizeMedium=16;
const TInt KVolumeSizeBig=20;

TRgb CColoredTheme::iColorBackground(0,0);
TRgb CColoredTheme::iColorText(0,0);
TUint32 CColoredTheme::iColorStartFillProgressBar(0);
TUint32 CColoredTheme::iColorStepFillProgressBar(0);
TRgb CColoredTheme::iColorProgressBarBoundary(0);
CFbsBitmap *CColoredTheme::iHugeFillV(NULL);
CFbsBitmap *CColoredTheme::iBigFillV(NULL);
CFbsBitmap *CColoredTheme::iSmallFillV(NULL);
CFbsBitmap *CColoredTheme::iSmallFillH(NULL);
TInt CColoredTheme::iNrInstances(0);
TUint CColoredTheme::iMyStaticColors(0);

CColoredTheme::CColoredTheme()
{
	iNrInstances++;
}

CColoredTheme::~CColoredTheme()
{
	delete iArtistTitle;
	delete iArtistTitleBitmap;
	/*
	if(iDSArendering)
	{
		delete iDSArendering;
	};
	delete iATBdata;*/
	iNrInstances--;
	if(iNrInstances==0)
	{
		delete iHugeFillV;iHugeFillV=NULL;
		delete iBigFillV;iBigFillV=NULL;
		delete iSmallFillV;iSmallFillV=NULL;
		delete iSmallFillH;iSmallFillH=NULL;
	}
}

void CColoredTheme::SetMetadataL(CMetadata &aMetadata,TTimeIntervalMicroSeconds &aCurrentPosition)
{
	LOG(ELogGeneral,1,"CColoredTheme::SetMetadataL: start");
	//create iArtistTitle
	if(aMetadata.iArtist && aMetadata.iArtist->Length()>0 && aMetadata.iTitle && aMetadata.iTitle->Length()>0)
	{
		//we can create the iArtistTitle from artist and title
		LOG0("Creating iArtistTitle from aMetadata.iArtist and aMetadata.iTitle");
		TInt len=aMetadata.iArtist->Length()+aMetadata.iTitle->Length()+KArtistTitleSpace().Length();//+KSpaceAfterTitle().Length();
		if(aMetadata.iTrack>0 && aMetadata.iTrack<100)
		{
			len+=4; //length of "[n] "
			if(aMetadata.iTrack>9)len++; //"[nn] "
		};
		if(!iArtistTitle || iArtistTitle->Des().MaxLength()<len)
		{
			delete iArtistTitle;iArtistTitle=NULL;
			iArtistTitle=HBufC::New(len);
		};
		if(iArtistTitle)
		{
			TPtr ptrArtistTitle(iArtistTitle->Des());
			if(aMetadata.iTrack>0 && aMetadata.iTrack<100)
			{
				ptrArtistTitle.Format(KTrackDetails,aMetadata.iTrack);
				ptrArtistTitle.Append(*aMetadata.iArtist);
			}
			else
				ptrArtistTitle.Copy(*aMetadata.iArtist);		
			ptrArtistTitle.Append(KArtistTitleSpace);
			ptrArtistTitle.Append(*aMetadata.iTitle);
			ptrArtistTitle.TrimAll();
		};
		LOG0("iArtistTitle created, length: %d",iArtistTitle?iArtistTitle->Length():-1);
	}
	else if(aMetadata.iFileDirEntry)//we have to use the filename, if it exists
	{
		//we have to use the filename
		LOG0("Creating iArtistTitle from filename");
		TFileName filename;
		aMetadata.iFileDirEntry->GetFullPath(filename);
		CMLauncherPreferences::RemovePathComponent(filename);
		filename.TrimAll();
		TInt len(filename.Length());//+KSpaceAfterTitle().Length());
		if(!iArtistTitle || iArtistTitle->Des().MaxLength()<len)
		{
			delete iArtistTitle;iArtistTitle=NULL;
			iArtistTitle=HBufC::New(len);
		};
		if(iArtistTitle)
			iArtistTitle->Des().Copy(filename);
		LOG0("iArtistTitle created, length: %d",iArtistTitle?iArtistTitle->Length():-1);
	};
	//set elapsed time to current position
	iManager->iFormerPosition=aCurrentPosition.Int64()/1000000;
	iDuration=aMetadata.iDuration;

	//check if the artist+title string fits in the screen or it needs scrolling
	iFlags&=~EArtistTitleNeedsScrolling; //by default, we do not scroll
	iArtistTitleOffset=0;
	if(iArtistTitle)
	{
		iArtistTitleTotalLength=iFont->TextWidthInPixels(*iArtistTitle);
		if(iArtistTitleTotalLength>iMySize.iWidth && iContainer)
		{
			//we need scrolling
			iArtistTitleTotalLength+=KScrollPaddingInPixels;
			CreateArtistTitleScrollingBitmap();
			//iDSArendering=CRendering::New(*this);
		};
	};
	LOG0("Getting the album art image...");
	//get the image
	iAlbumArtDominantColor=0;
	TInt err=iManager->iImgEngine->GetAlbumArtL(aMetadata,*this);
	if(iAlbumArt || !err) //if err is not KErrNone, there will be no album art
		iManager->iView->CurrentTrackInitializationComplete();
	if(!err)
		SetMatchingTheme();
	
	LOG(ELogGeneral,-1,"CColoredTheme::SetMetadataL: end");
}

void CColoredTheme::AddMenuItemL(CEikMenuPane *aMenu, TInt aPreviousId)
{
	aMenu->AddMenuItemsL(R_MENU_COLORED_THEME,aPreviousId);
}

void CColoredTheme::HandleCommandL(TInt aCommand)
{
	if(aCommand!=ECommandThemeMatchAlbumArt) //so that we do not repeat this for all the switch cases
		iPreferences->iPFlags&=~CMLauncherPreferences::EPreferencesMatchColors2AlbumArt;
	switch(aCommand)
	{
	case ECommandThemeMatchAlbumArt:
		iPreferences->iPFlags|=CMLauncherPreferences::EPreferencesMatchColors2AlbumArt;
		SetMatchingTheme();
		break;
	case ECommandThemeBlackAndGreen:SetColorScheme(EBgColorBlack|EColorGreen);break;
	case ECommandThemeBlackAndBlue:SetColorScheme(EBgColorBlack|EColorBlue);break;
	case ECommandThemeBlackAndOrange:SetColorScheme(EBgColorBlack|EColorOrange);break;
	case ECommandThemeBlackAndRed:SetColorScheme(EBgColorBlack|EColorRed);break;
	case ECommandThemeBlackAndViolet:SetColorScheme(EBgColorBlack|EColorViolet);break;
	case ECommandThemeWhiteAndGreen:SetColorScheme(EBgColorWhite|EColorGreen);break;
	case ECommandThemeWhiteAndBlue:SetColorScheme(EBgColorWhite|EColorBlue);break;
	case ECommandThemeWhiteAndOrange:SetColorScheme(EBgColorWhite|EColorOrange);break;
	case ECommandThemeWhiteAndRed:SetColorScheme(EBgColorWhite|EColorRed);break;
	case ECommandThemeWhiteAndViolet:SetColorScheme(EBgColorWhite|EColorViolet);break;
	default:Panic( EMLauncherUi );
	};
}

void CColoredTheme::SetStaticColorScheme(TUint aColors, TBool &aRecreateATbitmap)
{
	if(aColors==iMyStaticColors)
	{
		aRecreateATbitmap=EFalse;
		return;
	}
	
	//if here, there are some changes
	if(aColors&EBgColorBlack && !(iMyStaticColors&EBgColorBlack))
	{
		iColorBackground.SetInternal(0xFF000000);
		iColorText.SetInternal(0xFFFFFFFF);
		aRecreateATbitmap=ETrue;
	}
	else if(aColors&EBgColorWhite && !(iMyStaticColors&EBgColorWhite))
	{
		iColorBackground.SetInternal(0xFFFFFFFF);
		iColorText.SetInternal(0xFF000000);
		aRecreateATbitmap=ETrue;
	}
	else 
	{
		aRecreateATbitmap=EFalse;
		FL_ASSERT((aColors&KBgColorMask)==(iMyStaticColors&KBgColorMask)); //this means aColors and iMyStaticColors have the same background color
	}
	
	if(aColors&EColorGreen)
	{
		iColorStartFillProgressBar=0x00FF00;
		iColorStepFillProgressBar=0x500;
		iColorProgressBarBoundary.SetInternal(0xFF00FF00);
		CreateBrushFills();
	}
	else if(aColors&EColorBlue)
	{
		iColorStartFillProgressBar=0x80FF;
		iColorStepFillProgressBar=0x000307;
		iColorProgressBarBoundary.SetInternal(0xFF0080FF);
		CreateBrushFills();
	}
	else if(aColors&EColorOrange)
	{
		iColorStartFillProgressBar=0xFBB416;  //251, 180, 22
		iColorStepFillProgressBar=0x030500;  //245, 131, 22
		iColorProgressBarBoundary.SetInternal(0xFFFBB416);
		CreateBrushFills();
	}
	else if(aColors&EColorRed)
	{
		iColorStartFillProgressBar=0xFF0000;
		iColorStepFillProgressBar=0x70000;
		iColorProgressBarBoundary.SetInternal(0xFFFF0000);
		CreateBrushFills();
	}
	else if(aColors&EColorViolet)
	{
		iColorStartFillProgressBar=0xF107E7; //241, 7, 231
		iColorStepFillProgressBar=0x020005;
		iColorProgressBarBoundary.SetInternal(0xFFF107E7);
		CreateBrushFills();
	}
	else FL_ASSERT(0);
	iMyStaticColors=aColors;
}
void CColoredTheme::SetColorScheme(TUint aColors, TBool aRedraw/*=ETrue*/)
{
	LOG0("CColoredTheme::SetColorScheme+ (aColors=%d)",aColors);
	
	if(aColors && aColors==iPreferences->iMusicPlayerThemeData && iMyColors==aColors && !aRedraw )
		return;
	
	//set a default
	if(aColors==0)
	{
		aColors=EBgColorBlack|EColorGreen;
		iPreferences->iPFlags|=CMLauncherPreferences::EPreferencesMatchColors2AlbumArt;
	}
	TBool changeATbitmap(EFalse);
	SetStaticColorScheme(aColors,changeATbitmap);
	
	if(changeATbitmap && iArtistTitleBitmap && iFlags&EArtistTitleNeedsScrolling)
		CreateArtistTitleScrollingBitmap();
	
	iPreferences->iMusicPlayerThemeData=aColors;
	iMyColors=aColors;
	iPreferences->iCFlags|=CMLauncherPreferences::ESavePreferences;
	
	if(iContainer && aRedraw)
		iContainer->DrawDeferred();
	LOG0("CColoredTheme::SetColorScheme- (iMyColors=%d)",iMyColors);
}

void CColoredTheme::CreateBrushFills()
{
	//if the brushes are created, we need to reallocate them, otherwise jusst changing their value will not work
	CFbsBitmap *oldBmp(NULL);
	
	//create the "huge" layout
	oldBmp=iHugeFillV;
	iHugeFillV=new (ELeave) CFbsBitmap;
	User::LeaveIfError(iHugeFillV->Create(TSize(1,KMaxFillHeight<<1),EColor16MU));
	delete oldBmp;
		
	TBitmapUtil bu(iHugeFillV);
	bu.Begin(TPoint(0,0));
	TInt i;
	for(i=0;i<KMaxFillHeight;i++)
	{
		TUint32 color(iColorStartFillProgressBar-iColorStepFillProgressBar*i);
		bu.SetPixel(color);
		bu.IncYPos();
		bu.SetPixel(color);
		bu.IncYPos();
	};
	bu.End();
	
	//create the big layout
	oldBmp=iBigFillV;
	iBigFillV=new (ELeave) CFbsBitmap;
	User::LeaveIfError(iBigFillV->Create(TSize(1,KMaxFillHeight),EColor16MU));
	delete oldBmp;
	
	TBitmapUtil bub(iBigFillV);
    bub.Begin(TPoint(0,0));
	for(i=0;i<KMaxFillHeight;i++)
	{
		bub.SetPixel(iColorStartFillProgressBar-iColorStepFillProgressBar*i);
		bub.IncYPos();
	};
	bub.End();
	
	//create the small patterns
	oldBmp=iSmallFillV;
	iSmallFillV=new (ELeave) CFbsBitmap;
	User::LeaveIfError(iSmallFillV->Create(TSize(1,KMaxFillHeight>>1),EColor16MU));
	delete oldBmp;
	oldBmp=iSmallFillH;
	iSmallFillH=new (ELeave) CFbsBitmap;
	User::LeaveIfError(iSmallFillH->Create(TSize(KMaxFillHeight>>1,1),EColor16MU));
	delete oldBmp;
	oldBmp=NULL;
	TBitmapUtil buv(iSmallFillV);
	TBitmapUtil buh(iSmallFillH);
	buv.Begin(TPoint(0,0));
	buh.Begin(TPoint(0,0));
	for(i=0;i<KMaxFillHeight;i+=2)
	{
		TUint32 color(iColorStartFillProgressBar-iColorStepFillProgressBar*i);
		buv.SetPixel(color);
		buv.IncYPos();
		buh.SetPixel(color);
		buh.IncXPos();
	};
	buv.End();
	buh.End();
}

void CColoredTheme::CreateArtistTitleScrollingBitmap()
{
	//the idea is to create a CWsBitmap with the text, and then BitBlt it to the text area
	if(!iContainer)return;
	
	if(iArtistTitleBitmap)
		iArtistTitleBitmap->Reset();
	else
		iArtistTitleBitmap=new CWsBitmap(iManager->iEikEnv->WsSession());
	
	TSize bmpSize(iArtistTitleTotalLength,iArtistTitleRect.Height());
	iArtistTitleBitmap->Create(bmpSize, iContainer->SystemGc().Device()->DisplayMode() );
	
	//here we have the iArtistTitleBitmap creted
	
	//create the bitmap device
	CFbsBitmapDevice *bmpDevice=CFbsBitmapDevice::NewL(iArtistTitleBitmap);
	CleanupStack::PushL(bmpDevice);
	
	//create the graphics context
	CFbsBitGc *gc;
	bmpDevice->CreateContext(gc);
	CleanupStack::PushL(gc);
	
	//now draw the stuff
	gc->SetBrushColor(iColorBackground);
	gc->Clear();
	gc->UseFont(iFont);
	gc->SetPenColor(iColorText);
	gc->SetBrushStyle(CGraphicsContext::ENullBrush);
	gc->DrawText(*iArtistTitle,TPoint(0,iArtistTitlePosition.iY-iArtistTitleRect.iTl.iY));
	gc->DiscardFont();
	
	//clean
	CleanupStack::PopAndDestroy(2,bmpDevice);
	//done
	iFlags|=EArtistTitleNeedsScrolling;
	/*
	//fill the iATBdata
	iArtistTitleBitmap->LockHeap();
	TUint8 *addr=(TUint8*)iArtistTitleBitmap->DataAddress();
	TInt stride(iArtistTitleBitmap->DataStride());
	TInt nrRows(iArtistTitleBitmap->SizeInPixels().iHeight);
	if(iATBdataSize<stride*nrRows+KMemAlign)
	{
		//we need to allocate  data
		delete iATBdata;
		iATBdataSize=stride*nrRows+KMemAlign;
		iATBdata=(TUint8*)User::AllocL(iATBdataSize);
		iATBdataAligned=(TUint8*) (((((TUint)iATBdata)>>KMemAlignPwr2)+1)*KMemAlign);
		//if(iATBdataAligned<iATBdata)iATBdataAligned+=KMemAlign;
	};

	Mem::Copy(iATBdataAligned,addr,stride*nrRows);
	
	iArtistTitleBitmap->UnlockHeap();
	*/
}
#if 0
void CColoredTheme::PeriodicUpdate(TTimeIntervalMicroSeconds &/*aCurrentPosition*/)
{
	return;
	//TODO: we can get rid of this function after implementing DSA
	//if we are here, we make the label move....
	if((iFlags&EArtistTitleNeedsScrolling) && (iManager->iView->iFlags&CMusicPlayerView::EAllowScrolling))
	{
		//if we are here, we have to move the label
		/*
			TChar first=(*iArtistTitle)[0];
			iArtistTitle->Des().Delete(0,1);
			iArtistTitle->Des().Append(first);
		 */
		
		
		if(iDSArendering && iFlags&EDSAActive)
		{
			/*
			//direct draw
			TRect bmpRect(iArtistTitleOffset,0,iArtistTitleTotalLength,iArtistTitleRect.iBr.iY);
			iGc->BitBlt(iArtistTitleRect.iTl,iArtistTitleBitmap,bmpRect);		
			TInt remainingStringLength(iArtistTitleRect.Width()+iArtistTitleOffset-iArtistTitleTotalLength);
			if(remainingStringLength>0)
			{
				bmpRect.iTl.iX=0;
				//bmpRect.iBr.iX=remainingStringLength; //this should not be necessary
				iGc->BitBlt(TPoint(iArtistTitleRect.iBr.iX-remainingStringLength,iArtistTitleRect.iTl.iY),iArtistTitleBitmap,bmpRect);
			};
			iDSA->ScreenDevice()->Update();
			//end of direct draw
			 */
		}
		else //we draw the "classic" way
		{
			iArtistTitleOffset+=KArtistTitleScrollStep;
			if(iArtistTitleOffset>=iArtistTitleTotalLength)
				iArtistTitleOffset-=iArtistTitleTotalLength;
					
			iContainer->DrawNow(iArtistTitleRect);
			
			iFps++;
		}
		
		
	};
}
#endif

void CColoredTheme::Draw(const TRect& aRect, CWindowGc& gc) const
{	
	if(aRect==iArtistTitleRect && iArtistTitleBitmap && (iFlags&EArtistTitleNeedsScrolling)) //we need EArtistTitleNeedsScrolling, because we do not delete the bitmap, so we can reuse it
	{
		TRect bmpRect(iArtistTitleOffset,0,iArtistTitleTotalLength,iArtistTitleRect.iBr.iY);
		gc.BitBlt(iArtistTitleRect.iTl,iArtistTitleBitmap,bmpRect);		
		TInt remainingStringLength(iArtistTitleRect.Width()+iArtistTitleOffset-iArtistTitleTotalLength);
		if(remainingStringLength>0)
		{
			bmpRect.iTl.iX=0;
			//bmpRect.iBr.iX=remainingStringLength; //this should not be necessary
			gc.BitBlt(TPoint(iArtistTitleRect.iBr.iX-remainingStringLength,iArtistTitleRect.iTl.iY),iArtistTitleBitmap,bmpRect);
		};
		return;
	}
	//LOG(ELogGeneral,1,"CColoredTheme::Draw++ (%d,%d-%d,%d)",aRect.iTl.iX,aRect.iTl.iY,aRect.iBr.iX,aRect.iBr.iY);
	gc.SetBrushColor(iColorBackground);
	gc.Clear(aRect);
	
	//draw the album art, if we have it and if the cleared region needs it
	if(aRect.iTl.iY<(iAlbumArtPosition.iY+iAlbumArtSize.iHeight) && aRect.iBr.iY>iAlbumArtPosition.iY)
	{
		//we should draw album art
		if(iAlbumArt)
		{
			gc.BitBlt(iAlbumArtPosition, iAlbumArt );
			//LOG0("BitBlted album art");
		}
		//else LOG0("CColoredTheme::Draw: We have no album art (iAlbumArt is NULL)");
	}
		

	//draw the progress bar, if it falls into the region
	if(iDuration && aRect.iTl.iY<iProgressBar.iBr.iY && aRect.iBr.iY>iProgressBar.iTl.iY)
	{
		//draw the duration rectangle
		//LOG0("Drawing duration bar");
		gc.SetPenColor(iColorProgressBarBoundary);
		gc.SetBrushStyle(CGraphicsContext::ENullBrush);
		gc.SetPenStyle(CGraphicsContext::ESolidPen);
		if(!(iFlags&ENoProgressBarBoundary))
			gc.DrawRoundRect(iProgressBar,TSize(iProgressBarRounded,iProgressBarRounded));

		TInt rectangleWidth(iProgressBar.Width()*iManager->iFormerPosition/iDuration);
		if(rectangleWidth>2) //no sese to draw anything for less than 2
		{
			if(iProgressBar.Height()<=(KMaxFillHeight>>1))
				gc.UseBrushPattern(iSmallFillV);
			else
				gc.UseBrushPattern(iBigFillV);
			gc.SetBrushStyle(CGraphicsContext::EPatternedBrush);
			//gc.SetPenStyle(CGraphicsContext::ENullPen);
			gc.SetPenSize(TSize(0,0));
			gc.SetBrushOrigin(iProgressBar.iTl);

			TRect rectangle(iProgressBar);
			rectangle.SetWidth(rectangleWidth);
			
			TInt delta(iProgressBarRounded-rectangleWidth);
			if(delta>0)
				rectangle.Shrink(0,delta);
			else delta=0;

			//gc.SetBrushOrigin(rectangle.iTl);
			//gc.SetPenColor(iCurrentTheme->iColorText);

			gc.DrawRoundRect(rectangle,TSize(iProgressBarRounded-delta,iProgressBarRounded-delta));
		};
	};

	//draw text, if it falls into the region
	if(iFont && iArtistTitle && aRect.iTl.iY<iArtistTitleRect.iBr.iY && aRect.iBr.iY>iArtistTitleRect.iTl.iY)
	{
		if(iArtistTitleBitmap && (iFlags&EArtistTitleNeedsScrolling))
		{
			/*if(!(iFlags&EDSAActive)) // if DSA is active, we let it draw on the screen
			{*/
				//LOG0("Drawing Artist title/BitBlt");
				TRect bmpRect(iArtistTitleOffset,0,iArtistTitleTotalLength,iArtistTitleRect.iBr.iY);
				gc.BitBlt(iArtistTitleRect.iTl,iArtistTitleBitmap,bmpRect);		
				TInt remainingStringLength(iArtistTitleRect.Width()+iArtistTitleOffset-iArtistTitleTotalLength);
				if(remainingStringLength>0)
				{
					bmpRect.iTl.iX=0;
					//bmpRect.iBr.iX=remainingStringLength; //this should not be necessary
					gc.BitBlt(TPoint(iArtistTitleRect.iBr.iX-remainingStringLength,iArtistTitleRect.iTl.iY),iArtistTitleBitmap,bmpRect);
				};
			//};
		}
		else
		{
			gc.UseFont(iFont);
			gc.SetPenColor(iColorText);
			gc.SetPenSize(TSize(1,1));
			gc.SetBrushStyle(CGraphicsContext::ENullBrush);
			gc.DrawText(*iArtistTitle,iArtistTitlePosition);
			/*
		    gc.DrawText(*iArtistTitle,iArtistTitleRect,iArtistTitlePosition.iY-iArtistTitleRect.iTl.iY,CGraphicsContext::ELeft,-iArtistTitleOffset);
		    if(iFlags&EArtistTitleNeedsScrolling)
		    {
			    TInt remainingStringLength(iArtistTitleRect.Width()+iArtistTitleOffset-iArtistTitleTotalLength);
			    if(remainingStringLength>0)
				    gc.DrawText(*iArtistTitle,iArtistTitleRect,iArtistTitlePosition.iY-iArtistTitleRect.iTl.iY,CGraphicsContext::ELeft,iArtistTitleTotalLength-iArtistTitleOffset);
		    };
			 */
			gc.DiscardFont();
			//LOG0("Drawing Artist title (text)");
		};
	};
	
	//draw volume
	if(aRect.Intersects(iVolumeRect) && (iFlags&EShowVolumeMode))
	{
		DrawVolumeL(gc);
	}
	//LOG(ELogGeneral,-1,"CColoredTheme::Draw--");
}

void CColoredTheme::AlbumArtReady(TInt aErr)
{
	if(aErr==KErrNone && iContainer)
	{
		if(SetMatchingTheme())
			iContainer->DrawDeferred(); //we redraw everything
		else
			iContainer->DrawNow(TRect(iAlbumArtPosition,iAlbumArtSize));
	}
	iManager->iView->CurrentTrackInitializationComplete();
}

void CColoredTheme::ActivateL()
{
	/*if(iFlags&EArtistTitleNeedsScrolling && iATBdataAligned && !iDSArendering)
	{
		iDSArendering=CRendering::New(*this);
	};*/
	if(iMyColors!=iPreferences->iMusicPlayerThemeData)
		SetColorScheme(iPreferences->iMusicPlayerThemeData);
}
		
void CColoredTheme::Deactivate()
{
	/*if(iDSArendering)
	{
		delete iDSArendering;
		iDSArendering=NULL;
	};*/
	iContainer=NULL;
}

void CColoredTheme::FormatTimeString(TDes& aString, TInt aSeconds, TInt aTotalDuration)
{
	if(aTotalDuration>=3600)
	{
		TInt h(aSeconds/3600);
		TInt m((aSeconds-h*3600)/60);
		aString.Format(KTimeFormatHMS,h,m,aSeconds%60);
	}
	else
		aString.Format(KTimeFormat,aSeconds/60,aSeconds%60);
}

void CColoredTheme::ComputeVolumeSize(TInt aDesiredSize)
{
	iVolumeSize=aDesiredSize;
	if(iVolumeRect.Width()<42 || iVolumeRect.Height()<20)
	{
		iVolumeSize=0;
		iSpeakerWidth=0;
		return; //area too small
	}
	if(iVolumeRect.Width()<75 || iVolumeRect.Height()<40)
		iVolumeSize=KVolumeSizeSmall;
	else if(iVolumeRect.Width()<100 || iVolumeRect.Height()<60)
		iVolumeSize=KVolumeSizeMedium;
	
	switch(iVolumeSize)
	{
	case KVolumeSizeSmall:iSpeakerWidth=12;iLevelWidth=0;break;
	case KVolumeSizeMedium:iSpeakerWidth=18;break;
	case KVolumeSizeBig:iSpeakerWidth=25;break;
	default:FL_ASSERT(0);
	}//switch

	iLevelWidth=(iVolumeRect.Width()-iSpeakerWidth)/iPreferences->iMaxVolume;
}

void CColoredTheme::DrawVolumeL(CWindowGc& gc) const
{
	//check for the minimum sizes:
	//the "small" speaker is 12x20. The minimum size for the volume would be 30x20, so in total we have 42x20
	//the "big" speaker is 25x40. The minimum size for the volume should be 50x40, so in total we have 75x50
	if(!iVolumeSize)return;
	FL_ASSERT(iVolumeSize==KVolumeSizeSmall || iVolumeSize==KVolumeSizeMedium || iVolumeSize==KVolumeSizeBig);

	//Draw here the volume 
	TInt i;
	TInt remainingWidth(iVolumeRect.Width());
	TUint32 startColor( ((iColorStartFillProgressBar&0xFF0000)>>16) + (iColorStartFillProgressBar&0xFF00) + ((iColorStartFillProgressBar&0xFF)<<16));
	TUint32 stepColor( ((iColorStepFillProgressBar&0xFF0000)>>16) + (iColorStepFillProgressBar&0xFF00) + ((iColorStepFillProgressBar&0xFF)<<16));

	TUint32 halfStepColor( (((stepColor&0xFF)*3)>>2)+((((stepColor&0xFF00)*3)>>2)&0xFF00)+((((stepColor&0xFF0000)*3)>>2)&0xFF0000) );
	gc.SetPenStyle(CGraphicsContext::ESolidPen);
	gc.SetPenSize(TSize(1,1));
	if(iVolumeSize==KVolumeSizeSmall)
	{
		TPoint pos(iVolumeRect.iTl.iX,iVolumeRect.iBr.iY-15);
		for(i=0;i<iVolumeSize;i++)
		{
			gc.SetPenColor(startColor-i*2*halfStepColor);
			gc.DrawLine(TPoint(pos.iX,pos.iY+i),TPoint(pos.iX+5,pos.iY+i));

			gc.DrawLine(TPoint(pos.iX+6,pos.iY+i),TPoint(pos.iX+12,pos.iY-5+i*2));
			gc.SetPenColor(startColor-i*2*halfStepColor-halfStepColor);
			gc.DrawLine(TPoint(pos.iX+6,pos.iY+i),TPoint(pos.iX+12,pos.iY-4+i*2));
		};
		remainingWidth-=12;
	}
	else if(iVolumeSize==KVolumeSizeMedium)
	{
		TPoint pos(iVolumeRect.iTl.iX,iVolumeRect.iBr.iY-24);
		for(i=0;i<iVolumeSize;i++)
		{
			gc.SetPenColor(startColor-i*2*halfStepColor);
			gc.DrawLine(TPoint(pos.iX,pos.iY+i),TPoint(pos.iX+8,pos.iY+i));

			gc.DrawLine(TPoint(pos.iX+9,pos.iY+i),TPoint(pos.iX+18,pos.iY-8+i*2));
			gc.SetPenColor(startColor-i*2*halfStepColor-halfStepColor);
			gc.DrawLine(TPoint(pos.iX+9,pos.iY+i),TPoint(pos.iX+18,pos.iY-7+i*2));
		};
		remainingWidth-=18;
	}
	else if(iVolumeSize==KVolumeSizeBig)
	{
		TPoint pos(iVolumeRect.iTl.iX,iVolumeRect.iBr.iY-30);
		for(i=0;i<iVolumeSize;i++)
		{
			gc.SetPenColor(startColor-i*2*halfStepColor);
			gc.DrawLine(TPoint(pos.iX,pos.iY+i),TPoint(pos.iX+10,pos.iY+i));

			gc.DrawLine(TPoint(pos.iX+12,pos.iY+i),TPoint(pos.iX+25,pos.iY-10+i*2));
			gc.SetPenColor(startColor-i*2*halfStepColor-halfStepColor);
			gc.DrawLine(TPoint(pos.iX+12,pos.iY+i),TPoint(pos.iX+25,pos.iY-9+i*2));
		};
		remainingWidth-=25;
	};
	
	//draw the volume lines
	/*
	if(iManager->iView->iMaxVolume>0 && iManager->iView->iVolumeStep>0)
	{
		nrSteps=iManager->iView->iMaxVolume/iManager->iView->iVolumeStep;
		if(nrSteps>0)
			stepH=remainingWidth/nrSteps;
	};*/
	
	if(iLevelWidth<=3 || iManager->iView->iPreferences->iVolume<0)
		return; //we can not show anything

	TInt widthVolumeSpace(iLevelWidth>>2);
	if(widthVolumeSpace==0)widthVolumeSpace=1;
	TInt widthVolumeBar(iLevelWidth-widthVolumeSpace);
	if(widthVolumeBar>(KMaxFillHeight>>1))
	{
		widthVolumeBar=KMaxFillHeight>>1;
		widthVolumeSpace=iLevelWidth-widthVolumeBar;
	};
	
	TInt round(widthVolumeBar>=3?(widthVolumeBar>>1):0);
	TInt maxBar(iVolumeRect.Height());
	if(iVolumeSize==KVolumeSizeSmall)maxBar-=5;
	else if(iVolumeSize==KVolumeSizeMedium)maxBar-=8;
	else maxBar-=10;
	TInt minBar(5);
	TInt stepV((maxBar-minBar)/(iPreferences->iMaxVolume-1));
	
	TSize roundS(round,round);

	gc.SetPenColor(startColor);
	gc.UseBrushPattern(iSmallFillH);
	gc.SetBrushStyle(CGraphicsContext::EPatternedBrush);
	//gc.SetPenStyle(CGraphicsContext::ENullPen);
	gc.SetPenSize(TSize(0,0));
	TBool drawFullBars(ETrue);
	TRect r;
	TInt extra(0);
	if(iVolumeSize==KVolumeSizeSmall)
	{
		if(stepV<2)
		{
			extra=3;
			stepV=(maxBar+extra-minBar)/(iPreferences->iMaxVolume-1);
			if(stepV<1)
			{
				//time for drastic measures
				extra=5;
				minBar=1;
				stepV=(maxBar+extra-minBar)/(iPreferences->iMaxVolume-1);
			};
		};
		r.SetRect(iVolumeRect.iTl.iX+12+widthVolumeSpace,iVolumeRect.iBr.iY-5+extra-minBar,iVolumeRect.iTl.iX+12+iLevelWidth,iVolumeRect.iBr.iY-5+extra);
	}
	else if(iVolumeSize==KVolumeSizeMedium)
	{
		if(stepV<3)
		{
			extra=6;
			stepV=(maxBar+extra-minBar)/(iPreferences->iMaxVolume-1);
		};
		r.SetRect(iVolumeRect.iTl.iX+18+widthVolumeSpace,iVolumeRect.iBr.iY-8+extra-minBar,iVolumeRect.iTl.iX+18+iLevelWidth,iVolumeRect.iBr.iY-8+extra);
	}
	else
	{
		if(stepV<4)
		{
			extra=8;
			stepV=(maxBar+extra-minBar)/(iPreferences->iMaxVolume-1);
		};
		r.SetRect(iVolumeRect.iTl.iX+25+widthVolumeSpace,iVolumeRect.iBr.iY-10+extra-minBar,iVolumeRect.iTl.iX+25+iLevelWidth,iVolumeRect.iBr.iY-10+extra);
	};

	for(i=0;i<iPreferences->iMaxVolume;i++)
	{
		if(drawFullBars && i>=iManager->iView->iPreferences->iVolume)
		{
			gc.SetBrushStyle(CGraphicsContext::ENullBrush);
			gc.SetPenStyle(CGraphicsContext::ESolidPen);
			gc.SetPenSize(TSize(1,1));
			//r.Shrink(1,1);
			drawFullBars=EFalse;
			/*
			if(round>0)
			{
				round--;
				roundS.SetSize(round,round);
			};*/
		}
		else
			gc.SetBrushOrigin(r.iTl);
		gc.DrawRoundRect(r,roundS);
		r.iTl.iX+=iLevelWidth;
		r.iTl.iY-=stepV;
		r.iBr.iX+=iLevelWidth;
	};
}

void CColoredTheme::AllowScrolling(TBool aAllow)
{
	if(aAllow) 
	{
		if(iFlags&EArtistTitleNeedsScrolling)
			/*if(iDSArendering && iFlags&EDSAActive)
			{
				//DSA stuff
			}
			else*/
			{
				//we need to start the periodic scroller
				if(!iScrollingTimer)
					iScrollingTimer=CPeriodic::New(CActive::EPriorityLow);
				else iScrollingTimer->Cancel(); //so we do not start it twice
				if(iScrollingTimer)
				{
					TCallBack cb(&ScrollFunction,this);
					iScrollingTimer->Start(KTextScrollPeriod,KTextScrollPeriod,cb);
				};
			};
	}
	else
	{
		if(iScrollingTimer)
			iScrollingTimer->Cancel();
		//TODO: stop DSA
	}
}

TInt CColoredTheme::ScrollFunction(TAny* aInstance)
{
	CColoredTheme *self=(CColoredTheme*)aInstance;
	self->iArtistTitleOffset+=KArtistTitleScrollStep;
	if(self->iArtistTitleOffset>=self->iArtistTitleTotalLength)
		self->iArtistTitleOffset-=self->iArtistTitleTotalLength;

	if(self->iContainer)
		self->iContainer->DrawNow(self->iArtistTitleRect);

	return 1;
}

TBool CColoredTheme::SetMatchingTheme()
{
	if(!iAlbumArtDominantColor || !(iPreferences->iPFlags&CMLauncherPreferences::EPreferencesMatchColors2AlbumArt))
		return EFalse; //we do not need to change the theme
	TUint r,g,b;
	r=(iAlbumArtDominantColor>>16)&0xFF;
	g=(iAlbumArtDominantColor>>8)&0xFF;
	b=(iAlbumArtDominantColor)&0xFF;
	LOG(ELogGeneral,1,"SetMatchingTheme++ (r=%d, g=%d, b=%d)",r,g,b);
	TUint distance2Green=r*r+(0xFF-g)*(0xFF-g)+b*b;
	TUint min=distance2Green;
	TUint minColor=EColorGreen;
	
	TUint distance2Blue=r*r+(0x80-g)*(0x80-g)+(0xFF-b)*(0xFF-b);
	if(distance2Blue<min){min=distance2Blue;minColor=EColorBlue;}
	
	TUint distance2Orange=(0xFB-r)*(0xFB-r)+(0xB4-g)*(0xB4-g)+(0x16-b)*(0x16-b);
	if(distance2Orange<min){min=distance2Orange;minColor=EColorOrange;}
	
	TUint distance2Red=(0xFF-r)*(0xFF-r)+g*g+b*b;
	if(distance2Red<min){min=distance2Red;minColor=EColorRed;}
			
	TUint distance2Violet=(0xF1-r)*(0xF1-r)+(0x07-g)*(0x07-g)+(0xE7-b)*(0xE7-b);
	if(distance2Violet<min){min=distance2Violet;minColor=EColorViolet;}	
	
	LOG0("d2Green=%d, d2Blue=%d, d2Orange=%d, d2Red=%d, d2Violet=%d",distance2Green,distance2Blue,distance2Orange,distance2Red,distance2Violet);
			
	//set the theme
	TBool themeChanged(EFalse);
	LOG0("iMyColors&White=%d, iMyColors&Black=%d",iMyColors&EBgColorWhite,iMyColors&EBgColorBlack);
	if(iMyColors&EBgColorWhite)
		LOG0("minColor=%d, themeColor=%d",minColor,iMyColors-EBgColorWhite);
	if(iMyColors&EBgColorBlack)
			LOG0("minColor=%d, themeColor=%d",minColor,iMyColors-EBgColorBlack);
	if(iMyColors&EBgColorWhite && minColor!=(iMyColors-EBgColorWhite))
	{
		LOG0("SCS white");
		SetColorScheme(minColor|EBgColorWhite);
		themeChanged=ETrue;
	}
	if(iMyColors&EBgColorBlack && minColor!=(iMyColors-EBgColorBlack))
	{
		LOG0("SCS black");
		SetColorScheme(minColor|EBgColorBlack);	
		themeChanged=ETrue;
	}
	LOG(ELogGeneral,-1,"SetMatchingTheme-- (min=%d, color=%d)",min,minColor);
	return themeChanged;
}
			
/*
void CColoredTheme::PrepareDsaL()
{
	//not ready at the moment
	return;
	
	if(!iContainer)return;
	iDSA=CDirectScreenAccess::NewL(iManager->iEikEnv->WsSession(), *iManager->iEikEnv->ScreenDevice(), iContainer->GetWindow(), *this);
	iManager->iEikEnv->WsSession().Flush();
	// Start direct screen access
	iDSA->StartL();
}
*/
	
///////////////////////////////////////////////////////////////////////	
#ifdef __TOUCH_ENABLED__ 

CFbsBitmap *CNHDTheme::iPlay(NULL);
CFbsBitmap *CNHDTheme::iPlayMask(NULL);
CFbsBitmap *CNHDTheme::iPause(NULL);
CFbsBitmap *CNHDTheme::iPauseMask(NULL);
CFbsBitmap *CNHDTheme::iFf(NULL);
CFbsBitmap *CNHDTheme::iFfMask(NULL);
CFbsBitmap *CNHDTheme::iFr(NULL);
CFbsBitmap *CNHDTheme::iFrMask(NULL);

TUint CNHDTheme::iMyNHDStaticColors(0);

const TInt KNHDXPlayPause=60;
const TInt KNHDYPlayPause=60;
const TInt KNHDXFFFR=70;
const TInt KNHDYFFFR=55;

CNHDTheme::~CNHDTheme()
{
	if(iNrInstances==0)
	{
		delete iPlay;iPlay=NULL;
		delete iPause;iPause=NULL;
		delete iFf;iFf=NULL;
		delete iFr;iFr=NULL;
	}
}

void CNHDTheme::SetStaticColorScheme(TUint aColors)
{
	if((iMyNHDStaticColors&KBgColorMask)==(aColors&KBgColorMask))return; //nothing to change
	
	//load play, pause ff and fr icons
	TInt err(0);
	if(aColors&EBgColorWhite && !(iMyNHDStaticColors&EBgColorWhite))
	{
		//we need to load the white buttons (opposite color to the background)
		delete iPlay;iPlay=NULL;
		delete iPause;iPause=NULL;
		delete iFf;iFf=NULL;
		delete iFr;iFr=NULL;	
		
		AknIconUtils::CreateIconLC(iPlay,iPlayMask,KOwnIconFile,EMbmMlauncherPlay_black,EMbmMlauncherPlay_black_mask);
		CleanupStack::Pop(2,iPlay);
		err=AknIconUtils::SetSize(iPlay,TSize(KNHDXPlayPause,KNHDYPlayPause) );

		AknIconUtils::CreateIconLC(iPause,iPauseMask,KOwnIconFile,EMbmMlauncherPause_black,EMbmMlauncherPause_black_mask);
		CleanupStack::Pop(2,iPause);
		err=AknIconUtils::SetSize(iPause,TSize(KNHDXPlayPause,KNHDYPlayPause) );	
		
		AknIconUtils::CreateIconLC(iFf,iFfMask,KOwnIconFile,EMbmMlauncherFf_black,EMbmMlauncherFf_black_mask);
		CleanupStack::Pop(2,iFf);
		err=AknIconUtils::SetSize(iFf,TSize(KNHDXFFFR,KNHDYFFFR) );

		AknIconUtils::CreateIconLC(iFr,iFrMask,KOwnIconFile,EMbmMlauncherFr_black,EMbmMlauncherFr_black_mask);
		CleanupStack::Pop(2,iFr);
		err=AknIconUtils::SetSize(iFr,TSize(KNHDXFFFR,KNHDYFFFR) );
	}
	else if(aColors&EBgColorBlack && !(iMyNHDStaticColors&EBgColorBlack))
	{
		//we need to load the white buttons (opposite color to the background)
		delete iPlay;iPlay=NULL;
		delete iPause;iPause=NULL;
		delete iFf;iFf=NULL;
		delete iFr;iFr=NULL;	

		AknIconUtils::CreateIconLC(iPlay,iPlayMask,KOwnIconFile,EMbmMlauncherPlay_white,EMbmMlauncherPlay_white_mask);
		CleanupStack::Pop(2,iPlay);
		err=AknIconUtils::SetSize(iPlay,TSize(KNHDXPlayPause,KNHDYPlayPause) );

		AknIconUtils::CreateIconLC(iPause,iPauseMask,KOwnIconFile,EMbmMlauncherPause_white,EMbmMlauncherPause_white_mask);
		CleanupStack::Pop(2,iPause);
		err=AknIconUtils::SetSize(iPause,TSize(KNHDXPlayPause,KNHDYPlayPause) );	

		AknIconUtils::CreateIconLC(iFf,iFfMask,KOwnIconFile,EMbmMlauncherFf_white,EMbmMlauncherFf_white_mask);
		CleanupStack::Pop(2,iFf);
		err=AknIconUtils::SetSize(iFf,TSize(KNHDXFFFR,KNHDYFFFR) );

		AknIconUtils::CreateIconLC(iFr,iFrMask,KOwnIconFile,EMbmMlauncherFr_white,EMbmMlauncherFr_white_mask);
		CleanupStack::Pop(2,iFr);
		err=AknIconUtils::SetSize(iFr,TSize(KNHDXFFFR,KNHDYFFFR) );
	}
	else FL_ASSERT(0); //we should have returned in the beginning of the function	
}


///////////////////////////////////////////////////////////////////////	

const TInt KNHDX=360;
const TInt KNHDXMargin=3;
const TInt KNHDYArtistTitleStart=0;
const TInt KNHDYArtistTitleEnd=30;
const TInt KNHDYArtistTitleDraw=25;
const TInt KNHDYAlbumArtStart=30;
const TInt KNHDYAlbumArtEnd=390;
const TInt KNHDYProgressBarStart=396;
const TInt KNHDYProgressBarEnd=424;
const TInt KNHDYTimeLabelsEnd=454;
const TInt KNHDYPosPlsLabelsEnd=579;
const TInt KNHDYArrowsStart=429;
const TInt KNHDYArrowsEnd=579;
const TInt KNHDXFFFRStart=10;

const TInt KDragFactor=5;
const TInt KDragMinimumDistance=150;
const TInt KDragSeekXMargin=50;
const TInt KDragSeekYMarginUp=20;
const TInt KDragSeekYMarginDown=50;
const TInt KDragXMargin=30;
const TInt KDragYMarginUp=10;
const TInt KDragYMarginDown=30;

const TInt KVolumeOnScreen=2500000; //in microseconds
const TInt KNHDXVolumeStart=30;
const TInt KNHDYVolumeStart=469;
const TInt KNHDXVolumeLength=300;
const TInt KNHDVolumeHeight=90;

const TRect KButtonLeftRect=TRect(2,580,179,638);
const TRect KButtonRightRect=TRect(181,580,358,638);
const TSize KButtonsRoundSize=TSize(7,7);
const TRect KLabelLeftRect=TRect(12,580,179,638);
const TRect KLabelRightRect=TRect(181,580,348,638);


CThemeNHDPortrait* CThemeNHDPortrait::NewL(TSize &aSize)
{
	CThemeNHDPortrait* self = new (ELeave) CThemeNHDPortrait();
	CleanupStack::PushL(self);
	self->ConstructL(aSize);
	CleanupStack::Pop(); // self;
	return self;
}

CThemeNHDPortrait::CThemeNHDPortrait()
{
	iAlbumArtPosition.SetXY(0,KNHDYAlbumArtStart);
	iAlbumArtSize.SetSize(KNHDX,KNHDX);
	iArtistTitlePosition.SetXY(KNHDXMargin,KNHDYArtistTitleDraw);
	iArtistTitleRect.SetRect(0,0,KNHDX,KNHDYArtistTitleEnd);
	iProgressBar.SetRect(KNHDXMargin,KNHDYProgressBarStart,KNHDX-KNHDXMargin,KNHDYProgressBarEnd);
	iProgressBarRounded=5;
	iVolumeRect.SetRect(KNHDXVolumeStart,KNHDYVolumeStart,KNHDXVolumeStart+KNHDXVolumeLength,KNHDYVolumeStart+KNHDVolumeHeight);
}

void CThemeNHDPortrait::ConstructL(TSize &aSize)
{
	CTheme::ConstructL(aSize);
	ComputeVolumeSize(KVolumeSizeBig);
	
	//get texts
	iTxtOptions=StringLoader::LoadL(R_LABEL_OPTIONS);
	iTxtShow=StringLoader::LoadL(R_LABEL_SHOW);
	iTxtBack=StringLoader::LoadL(R_LABEL_BACK);
	//get colors
	SetColorScheme(iPreferences->iMusicPlayerThemeData);
	//get the font we want to use
	iFont=iManager->iEikEnv->DenseFont();
		
	iLabelElapsedTime=new (ELeave) CEikLabel;
	iLabelElapsedTime->SetTextL(KEmpty);
	//iLabelElapsedTime->SetLabelAlignment(ELayoutAlignRight);
	iLabelElapsedTime->OverrideColorL(EColorLabelText,iColorText);
	iTotalTime=new (ELeave) CEikLabel;
	iTotalTime->SetTextL(KEmpty);
	iTotalTime->SetLabelAlignment(ELayoutAlignRight);
	iTotalTime->OverrideColorL(EColorLabelText,iColorText);
	iPositionInPlaylist=new (ELeave) CEikLabel;
	iPositionInPlaylist->SetTextL(KEmpty);
	iPositionInPlaylist->SetLabelAlignment(ELayoutAlignRight);
	iPositionInPlaylist->OverrideColorL(EColorLabelText,iColorText);
	iPositionInPlaylist->SetFont(iFont);
	
	TRect r;
	iButtonLeft=new (ELeave) CEikLabel;
	iButtonLeft->SetTextL(*iTxtOptions);
	iButtonLeft->SetLabelAlignment(ELayoutAlignLeft);
	iButtonLeft->OverrideColorL(EColorLabelText,iColorText);
	//iButtonLeft->SetFont(iFont);
	r=KLabelLeftRect;
	r.iTl.iY+=(r.Height()-iButtonLeft->MinimumSize().iHeight)>>1;
	iButtonLeft->SetRect(r);
	iButtonRight=new (ELeave) CEikLabel;
	if(iManager->iView->iFlags&CMusicPlayerView::ESongPreview)
	{
		iButtonRight->SetTextL(*iTxtBack);
		iRightButtonShowsBack=ETrue;
	}
	else
	{
		iButtonRight->SetTextL(*iTxtShow);
		iRightButtonShowsBack=EFalse;
	};
	iButtonRight->SetLabelAlignment(ELayoutAlignRight);
	iButtonRight->OverrideColorL(EColorLabelText,iColorText);
	//iButtonRight->SetFont(iFont);
	r=KLabelRightRect;
	r.iTl.iY+=(r.Height()-iButtonRight->MinimumSize().iHeight)>>1;
	iButtonRight->SetRect(r);
}

void CThemeNHDPortrait::SetContainer(CMusicPlayerContainer *aContainer)
{
	CTheme::SetContainer(aContainer);
	iLabelElapsedTime->SetContainerWindowL(*(CCoeControl*)aContainer);
	iTotalTime->SetContainerWindowL(*(CCoeControl*)aContainer);
	iPositionInPlaylist->SetContainerWindowL(*(CCoeControl*)aContainer);
	iButtonLeft->SetContainerWindowL(*(CCoeControl*)aContainer);
	iButtonRight->SetContainerWindowL(*(CCoeControl*)aContainer);
}

CThemeNHDPortrait::~CThemeNHDPortrait()
{
	delete iLabelElapsedTime;
	delete iTotalTime;
	delete iPositionInPlaylist;
	delete iButtonLeft;
	delete iButtonRight;

	delete iArrows;
	delete iArrowsMask;
	
	if(iVolumeTimer)
	{
		iVolumeTimer->Cancel();
		delete iVolumeTimer;
	};
	delete iTxtOptions;
	delete iTxtShow;
	delete iTxtBack;
}


void CThemeNHDPortrait::Draw(const TRect& aRect, CWindowGc& gc) const 
{
	CColoredTheme::Draw(aRect,gc);
	
	if(aRect.iBr.iY>=KNHDYArrowsStart)
	{
		//we draw arrow or the volume
		if(!(iFlags&EShowVolumeMode) && iArrows)
		{
			gc.SetBrushStyle(CGraphicsContext::ENullBrush);
			if(!(iPreferences->iPFlags&CMLauncherPreferences::EPreferencesPlayerUseIconsInsteadOfDrag && iFf))
				gc.BitBltMasked( TPoint(0,KNHDYArrowsStart), iArrows, TRect(0,0,KNHDX,KNHDYArrowsEnd-KNHDYArrowsStart), iArrowsMask, ETrue);
			CTrack *currentTrack(iManager->iView->iTrack);
			TBool drawPause(EFalse); //if iTrack is NULL, we will draw a Pause
			if(currentTrack)
			{
				if(currentTrack->iState==CTrack::EStatePlaying)
					drawPause=ETrue;
				else if(currentTrack->iState==CTrack::EStateInitializing && currentTrack->iFlags&CTrack::EStartPlaying)
					drawPause=ETrue;
			};
			if(drawPause)
				gc.BitBltMasked( TPoint((KNHDX-KNHDXPlayPause)>>1,(KNHDYArrowsEnd+KNHDYArrowsStart-KNHDYPlayPause)>>1), iPause, TRect(0,0,KNHDXPlayPause,KNHDYPlayPause), iPauseMask, ETrue);
			else
				gc.BitBltMasked( TPoint((KNHDX-KNHDXPlayPause)>>1,(KNHDYArrowsEnd+KNHDYArrowsStart-KNHDYPlayPause)>>1), iPlay, TRect(0,0,KNHDXPlayPause,KNHDYPlayPause), iPlayMask, ETrue);
			
			//draw the ff & fr 
			if(iPreferences->iPFlags&CMLauncherPreferences::EPreferencesPlayerUseIconsInsteadOfDrag && iFf)
			{	
				gc.BitBltMasked( TPoint(KNHDXFFFRStart,(KNHDYArrowsEnd+KNHDYArrowsStart-KNHDYFFFR)>>1), iFr, TRect(0,0,KNHDXFFFR,KNHDYFFFR), iFrMask, ETrue);
				gc.BitBltMasked( TPoint(KNHDX-KNHDXFFFRStart-KNHDXFFFR,(KNHDYArrowsEnd+KNHDYArrowsStart-KNHDYFFFR)>>1), iFf, TRect(0,0,KNHDXFFFR,KNHDYFFFR), iFfMask, ETrue);
			};
		};
	};
	
	if(aRect.iBr.iY>=KNHDYPosPlsLabelsEnd)
	{
		//we draw the botom buttons
		gc.UseBrushPattern(iHugeFillV);
		gc.SetBrushStyle(CGraphicsContext::EPatternedBrush);
		gc.SetPenSize(TSize(0,0));
		//left button
		gc.SetBrushOrigin(KButtonLeftRect.iTl);
		gc.DrawRoundRect(KButtonLeftRect,KButtonsRoundSize);
					
		//right button
		gc.SetBrushOrigin(KButtonRightRect.iTl);
		gc.DrawRoundRect(KButtonRightRect,KButtonsRoundSize);		
	}
	
}

	
TInt CThemeNHDPortrait::CountComponentControls()
{
	return 3+2;
}

CCoeControl* CThemeNHDPortrait::ComponentControl( TInt aIndex )
{
	switch(aIndex)
	{
	case 0:return iLabelElapsedTime;
	case 1:return iTotalTime;
	case 2:return iPositionInPlaylist;
	
	case 3:return iButtonLeft;
	case 4:return iButtonRight;
	};
	Panic(ECoeControlIndexTooBig);
	return NULL; //to avoid a warning
}

void CThemeNHDPortrait::HandlePointerEventL(const TPointerEvent &aPointerEvent)
{
	//LOG0("nHDportrait::HandlePointerEventL++");
	CTrack *track(iManager->iView->iTrack);
	if(!track)return;

	if(aPointerEvent.iType==TPointerEvent::EButton1Down && aPointerEvent.iPosition.iY>KNHDYProgressBarEnd+KDragSeekYMarginDown /*KNHDYArrowsStart*/ )
	{
		iDragStart=aPointerEvent.iPosition;
	}
	else if(aPointerEvent.iType==TPointerEvent::EButton1Down && aPointerEvent.iPosition.iY>KNHDYProgressBarStart-KDragSeekYMarginUp && aPointerEvent.iPosition.iY<KNHDYProgressBarEnd+KDragSeekYMarginDown)
	{
		//candidate to seek. Check X position
		TInt xpos(iManager->iFormerPosition*iProgressBar.Width()/iDuration);
		if(xpos-KDragSeekXMargin<aPointerEvent.iPosition.iX && xpos+KDragSeekXMargin>aPointerEvent.iPosition.iX)
			iFlags|=ESeekDrag;
	}
	else if(aPointerEvent.iType==TPointerEvent::EDrag)
	{
		iFlags|=EDragOperation;
		if(iFlags&ESeekDrag)
		{
			//iFormerPosition=aPointerEvent.iPosition.iX*iMusicPlayerView->iTrack->iMetadata.iDuration/KNHDX;
			TTimeIntervalMicroSeconds playbackPosition((aPointerEvent.iPosition.iX-iProgressBar.iTl.iX)*iDuration/iProgressBar.Width()*(TInt64)1000000);
			if(playbackPosition<0)playbackPosition=0;
			TInt64 duration(iDuration*(TInt64)1000000);
			if(playbackPosition>duration)playbackPosition=duration;
			UpdatePlaybackPosition(playbackPosition);
		};
	}
	else if(aPointerEvent.iType==TPointerEvent::EButton1Up)
	{
		//was this a drag operation?
		LOG0("Drag operation?");
		if(iFlags&EDragOperation)
		{
			//yes it was.
			//was it seek drag?
			if(iFlags&ESeekDrag)
			{
				//yes, it was seek
				TTimeIntervalMicroSeconds playbackPosition((aPointerEvent.iPosition.iX-iProgressBar.iTl.iX)*iDuration/iProgressBar.Width()*(TInt64)1000000);
				if(playbackPosition<0)playbackPosition=0;
				TInt64 duration(iDuration*(TInt64)1000000);
				if(playbackPosition>duration)playbackPosition=duration;
				track->SetPosition(playbackPosition);
			}
			else
			{
				if(iPreferences->iPFlags&CMLauncherPreferences::EPreferencesPlayerUseIconsInsteadOfDrag)
				{
					//if we have a small drag inside the previous area, we consider it click
					TRect drawRect(KNHDXFFFRStart,(KNHDYArrowsEnd+KNHDYArrowsStart-KNHDYFFFR)>>1,KNHDXFFFRStart+KNHDXFFFR,(KNHDYArrowsEnd+KNHDYArrowsStart+KNHDYFFFR)>>1);
					TRect touchRect(drawRect);
					touchRect.Grow(KDragXMargin,KDragYMarginUp);
					touchRect.iBr.iY+=KDragYMarginDown-KDragYMarginUp;
					if(touchRect.Contains(aPointerEvent.iPosition) && touchRect.Contains(iDragStart))
					{
						//go to previous
						iManager->iView->NextPreviousL(-1);
						//iContainer->DrawNow(drawRect);
					};
					
					//if we have a small drag inside the previous area, we consider it click
					drawRect.SetRect(KNHDX-KNHDXFFFRStart-KNHDXFFFR,(KNHDYArrowsEnd+KNHDYArrowsStart-KNHDYFFFR)>>1,KNHDX-KNHDXFFFRStart,(KNHDYArrowsEnd+KNHDYArrowsStart+KNHDYFFFR)>>1);
					touchRect=drawRect;
					touchRect.Grow(KDragXMargin,KDragYMarginUp);
					touchRect.iBr.iY+=KDragYMarginDown-KDragYMarginUp;
					if(touchRect.Contains(aPointerEvent.iPosition) && touchRect.Contains(iDragStart))
					{
						//go to previous
						iManager->iView->NextPreviousL(1);
						//iContainer->DrawNow(drawRect);
					};
				}
				else
				{
					//this was drag (swipe) next/previous, perhaps
					TInt dx=aPointerEvent.iPosition.iX-iDragStart.iX;
					TInt dy=aPointerEvent.iPosition.iY-iDragStart.iY;
					if(Abs(dy)*KDragFactor<Abs(dx) && Abs(dx)>=KDragMinimumDistance && aPointerEvent.iPosition.iY>KNHDYArrowsStart)
					{
						//this is the right pattern to move either to the left or right
						if(dx>0)
							iManager->iView->NextPreviousL(1);
						else
							iManager->iView->NextPreviousL(-1);
					};
				};

				//if we have a small drag inside the play/pause area, we consider it click
				//TRect touchRect((KNHDX-KNHDXPlayPause>>1)-KDragSeekXMargin,(KNHDYArrowsEnd+KNHDYArrowsStart-KNHDYPlayPause>>1)-KDragSeekYMarginUp,(KNHDX+KNHDXPlayPause>>1)+KDragSeekXMargin,(KNHDYArrowsEnd+KNHDYArrowsStart+KNHDYPlayPause>>1)+KDragSeekYMarginDown);
				TRect drawRect(KNHDX-KNHDXPlayPause>>1,KNHDYArrowsEnd+KNHDYArrowsStart-KNHDYPlayPause>>1,KNHDX+KNHDXPlayPause>>1,KNHDYArrowsEnd+KNHDYArrowsStart+KNHDYPlayPause>>1);
				TRect touchRect(drawRect);
				touchRect.Grow(KDragXMargin,KDragYMarginUp);
				touchRect.iBr.iY+=KDragYMarginDown-KDragYMarginUp;
				if(touchRect.Contains(aPointerEvent.iPosition) && touchRect.Contains(iDragStart))
				{
					//select the item
					iManager->iView->PlayPauseStop(EFalse);
					iContainer->DrawNow(drawRect);
				};
			};
			LOG0("Checking for button pushes");
			if(aPointerEvent.iPosition.iY>=KLabelLeftRect.iTl.iY && aPointerEvent.iPosition.iX<KLabelLeftRect.iBr.iX)
			{
				//left button was pushed
				LOG0("Left button was pushed");
				iManager->iView->HandleCommandL(EAknSoftkeyOptions);
			}
			else if(aPointerEvent.iPosition.iY>=KLabelRightRect.iTl.iY && aPointerEvent.iPosition.iX>KLabelRightRect.iTl.iX)
			{
				//right button was pushed
				LOG0("Right button was pushed");
				if(iManager->iView->iFlags&CMusicPlayerView::ESongPreview)
					iManager->iView->HandleCommandL(EAknSoftkeyBack);
				else
					iManager->iView->HandleCommandL(ESoftkeyNavi);
			}

			iFlags&=~EDragOperation;
		}
		else
		{
			//no drag operation, so at most a play/pause or button pushed
			LOG0("No drag, so play/pause button pushed");
			if(aPointerEvent.iPosition.iY>=KLabelLeftRect.iTl.iY && aPointerEvent.iPosition.iX<KLabelLeftRect.iBr.iX)
			{
				//left button was pushed
				LOG0("Left button was pushed");
				iManager->iView->HandleCommandL(EAknSoftkeyOptions);
			}
			else if(aPointerEvent.iPosition.iY>=KLabelRightRect.iTl.iY && aPointerEvent.iPosition.iX>KLabelRightRect.iTl.iX)
			{
				//right button was pushed
				LOG0("Right button was pushed");
				if(iManager->iView->iFlags&CMusicPlayerView::ESongPreview)
					iManager->iView->HandleCommandL(EAknSoftkeyBack);
				else
					iManager->iView->HandleCommandL(ESoftkeyNavi);
			}
			else
			{
				TRect drawRect(KNHDX-KNHDXPlayPause>>1,KNHDYArrowsEnd+KNHDYArrowsStart-KNHDYPlayPause>>1,KNHDX+KNHDXPlayPause>>1,KNHDYArrowsEnd+KNHDYArrowsStart+KNHDYPlayPause>>1);
				TRect touchRect(drawRect);
				touchRect.Grow(KDragXMargin,KDragYMarginUp);
				touchRect.iBr.iY+=KDragYMarginDown-KDragYMarginUp;
				if(touchRect.Contains(aPointerEvent.iPosition))
				{
					//select the item
					iManager->iView->PlayPauseStop(EFalse);
					iContainer->DrawNow(drawRect);
				};

				if(iPreferences->iPFlags&CMLauncherPreferences::EPreferencesPlayerUseIconsInsteadOfDrag)
				{
					//check for previous
					drawRect.SetRect(KNHDXFFFRStart,(KNHDYArrowsEnd+KNHDYArrowsStart-KNHDYFFFR)>>1,KNHDXFFFRStart+KNHDXFFFR,(KNHDYArrowsEnd+KNHDYArrowsStart+KNHDYFFFR)>>1);
					touchRect=drawRect;
					touchRect.Grow(KDragXMargin,KDragYMarginUp);
					touchRect.iBr.iY+=KDragYMarginDown-KDragYMarginUp;
					if(touchRect.Contains(aPointerEvent.iPosition) && touchRect.Contains(iDragStart))
					{
						//go to previous
						iManager->iView->NextPreviousL(-1);
						//iContainer->DrawNow(drawRect);
					};

					//check for next
					drawRect.SetRect(KNHDX-KNHDXFFFRStart-KNHDXFFFR,(KNHDYArrowsEnd+KNHDYArrowsStart-KNHDYFFFR)>>1,KNHDX-KNHDXFFFRStart,(KNHDYArrowsEnd+KNHDYArrowsStart+KNHDYFFFR)>>1);
					touchRect=drawRect;
					touchRect.Grow(KDragXMargin,KDragYMarginUp);
					touchRect.iBr.iY+=KDragYMarginDown-KDragYMarginUp;
					if(touchRect.Contains(aPointerEvent.iPosition) && touchRect.Contains(iDragStart))
					{
						//go to previous
						iManager->iView->NextPreviousL(1);
						//iContainer->DrawNow(drawRect);
					};
				};
			}
		};
		//clear seek drag anyway
		iFlags&=~ESeekDrag;
	}; 
	//LOG0("nHDportrait::HandlePointerEventL--");
}

void CThemeNHDPortrait::SetMetadataL(CMetadata &aMetadata,TTimeIntervalMicroSeconds &aCurrentPosition)
{
	
	LOG(ELogGeneral,1,"CThemeNHDPortrait::SetMetadataL: start");
	//set artist/title string & current position
	CColoredTheme::SetMetadataL(aMetadata,aCurrentPosition);

	//set time stuff
	TBuf<25> time;
	FormatTimeString(time,iManager->iFormerPosition,iDuration);
	iLabelElapsedTime->SetTextL(time);

	//set total duration
	FormatTimeString(time,iDuration,iDuration);
	iTotalTime->SetTextL(time);
	//set iTimeLabelWidth
	iTimeLabelWidth = iTotalTime->MinimumSize().iWidth;
	
	//set index in playlist
	time.Format(KIndexInPlaylist,iManager->iView->iPlaylist->GetCurrentIndex()+1,iManager->iView->iPlaylist->Count());
	iPositionInPlaylist->SetTextL(time);

	//set labels position on the screen
	TSize labelSize;
	TRect rect;
	//label 1: elapsed time
	labelSize = iLabelElapsedTime->MinimumSize();
	labelSize.iWidth=iTimeLabelWidth;
	rect.iTl.iX=KNHDXMargin;
	rect.iTl.iY=KNHDYTimeLabelsEnd-labelSize.iHeight;
	rect.iBr = rect.iTl + labelSize;
	iLabelElapsedTime->SetRect(rect);

	//label 2: total time
	labelSize = iTotalTime->MinimumSize();
	rect.iTl.iX=KNHDX-KNHDXMargin-iTimeLabelWidth;
	rect.iTl.iY=KNHDYTimeLabelsEnd-labelSize.iHeight;
	rect.iBr = rect.iTl + labelSize;
	iTotalTime->SetRect(rect);
	
	//label 3: song in playlist
	labelSize = iPositionInPlaylist->MinimumSize();
	labelSize.iHeight+=3;
	rect.SetRect(iMySize.iWidth-labelSize.iWidth,KNHDYPosPlsLabelsEnd-labelSize.iHeight,iMySize.iWidth,KNHDYPosPlsLabelsEnd); //bottom right corner
	iPositionInPlaylist->SetRect(rect);
	
	LOG(ELogGeneral,-1,"CThemeNHDPortrait::SetMetadataL: end");
}

TBool CThemeNHDPortrait::UpdatePlaybackPosition(TTimeIntervalMicroSeconds &aCurrentPosition, TBool aDraw/*=ETrue*/) //returns ETrue if the progress bar was updated
{
	TInt64 deltaUs(aCurrentPosition.Int64()-iManager->iFormerPosition*(TInt64)1000000);
	if(deltaUs>=1000000 || deltaUs<0)
	{
		TInt playbackSeconds(aCurrentPosition.Int64()/1000000);
		//if(!(iFlags&EDragOperation) || !(iFlags&ESeekDrag))
		iManager->iFormerPosition=playbackSeconds;

		//update current position label
		TBuf<10> time;
		FormatTimeString(time,playbackSeconds,iDuration);
		iLabelElapsedTime->SetTextL(time);

		if(aDraw)
		{
			iContainer->DrawNow(iProgressBar);
			iContainer->DrawNow(iLabelElapsedTime->Rect());
			//iLabelElapsedTime->DrawNow();
		};
		return ETrue;
	};

	return EFalse;
}

void CThemeNHDPortrait::PeriodicUpdate(TTimeIntervalMicroSeconds &aCurrentPosition)
{
	//CColoredTheme::PeriodicUpdate(aCurrentPosition);
	
	if(!(iFlags&EDragOperation) || !(iFlags&ESeekDrag))
		UpdatePlaybackPosition(aCurrentPosition); //if we have seek drag, we do not update the position
}
	
void CThemeNHDPortrait::SetColorScheme(TUint aColors, TBool aRedraw/*=ETrue*/)
{
	CColoredTheme::SetColorScheme(aColors,EFalse);
	aColors=iPreferences->iMusicPlayerThemeData; //can be that CColoredTheme changed/updated the aColors
	SetStaticColorScheme(aColors);
				
	if(iLabelElapsedTime)
		iLabelElapsedTime->OverrideColorL(EColorLabelText,iColorText);
	if(iTotalTime)
		iTotalTime->OverrideColorL(EColorLabelText,iColorText);
	if(iPositionInPlaylist)
		iPositionInPlaylist->OverrideColorL(EColorLabelText,iColorText);
	if(iButtonLeft)
		iButtonLeft->OverrideColorL(EColorLabelText,iColorText);
	if(iButtonRight)
		iButtonRight->OverrideColorL(EColorLabelText,iColorText);
	TInt err(0);
	TInt bmpId(-1),maskId(-1);
	if(aColors&EColorGreen)
	{
		bmpId=EMbmMlauncherArrows_left_right_green;
		maskId=EMbmMlauncherArrows_left_right_green_mask;
	}
	else if(aColors&EColorBlue)
	{
		bmpId=EMbmMlauncherArrows_left_right_blue;
		maskId=EMbmMlauncherArrows_left_right_blue_mask;
	}
	else if(aColors&EColorOrange)
	{
		bmpId=EMbmMlauncherArrows_left_right_orange;
		maskId=EMbmMlauncherArrows_left_right_orange_mask;
	}
	else if(aColors&EColorRed)
	{
		bmpId=EMbmMlauncherArrows_left_right_red;
		maskId=EMbmMlauncherArrows_left_right_red_mask;
	}
	else if(aColors&EColorViolet)
	{
		bmpId=EMbmMlauncherArrows_left_right_violet;
		maskId=EMbmMlauncherArrows_left_right_violet_mask;
	};
	
	if(bmpId>=0)
	{
		delete iArrows;delete iArrowsMask;
		AknIconUtils::CreateIconLC(iArrows,iArrowsMask,KOwnIconFile,bmpId,maskId);
		CleanupStack::Pop(2,iArrows);
		err=AknIconUtils::SetSize(iArrows,TSize(360,150),EAspectRatioNotPreserved );
	};
	
	if(iContainer && aRedraw)
		iContainer->DrawDeferred();
}

void CThemeNHDPortrait::UpdateVolumeOnScreen()
{
	//here we show the volume for a period of time
	if(iVolumeTimer)iVolumeTimer->Cancel();
	else iVolumeTimer = CPeriodic::New(CActive::EPriorityLow);
	
	if(iVolumeTimer)
	{
		TCallBack cb(&ExitVolumeMode,this);
		iVolumeTimer->Start(KVolumeOnScreen,KVolumeOnScreen,cb);
	};
	
	iFlags|=EShowVolumeMode;
	if(iContainer)
		iContainer->DrawDeferred();	
}

void CThemeNHDPortrait::ActivateL()
{
	if(iManager->iView->iFlags&CMusicPlayerView::ESongPreview)
	{
		if(!iRightButtonShowsBack)
		{
			iButtonRight->SetTextL(*iTxtBack);
			iRightButtonShowsBack=ETrue;
		}
	}
	else
	{
		if(iRightButtonShowsBack)
		{
			iButtonRight->SetTextL(*iTxtShow);
			iRightButtonShowsBack=EFalse;
		}
	}
	CColoredTheme::ActivateL();
}

TInt CThemeNHDPortrait::ExitVolumeMode(TAny* aInstance)
{
	CThemeNHDPortrait *self=(CThemeNHDPortrait*)aInstance;
	self->iFlags&=~EShowVolumeMode;
	if(self->iContainer)
		self->iContainer->DrawDeferred();
	//cancel the volume
	self->iVolumeTimer->Cancel();
	return 0; 
}

//////////////////////////////////////////////////////////////

const TInt KNHDLX=640;
const TInt KNHDLY=360;
const TInt KNHDLXMargin=3;
const TInt KNHDLYArtistTitleStart=0;
const TInt KNHDLYArtistTitleEnd=30;
const TInt KNHDLYArtistTitleDraw=25;

const TInt KNHDLXAlbumArtStart=170;
const TInt KNHDLYAlbumArtStart=30;
const TInt KNHDLAlbumArtHeight=300;
const TInt KNHDLYProgressBarStart=333;
const TInt KNHDLYProgressBarEnd=357;
const TInt KNHDLYTimeLabelsEnd=355;
const TInt KNHDLYSongInPlaylistStart=40;
const TInt KNHDLYSongInPlaylistEnd=70;
const TInt KNHDLXVolumeStart=0;
const TInt KNHDLYVolumeStart=30;
const TInt KNHDLVolumeLength=170;
const TInt KNHDLVolumeHeight=40;

const TInt KNHDLSmallFontSize=17;

const TInt KNHDLXPrevPlaylistStart=0;
const TInt KNHDLYPrevPlaylistStart=70;
const TInt KNHDLXPrevPlaylistEnd  =170;
const TInt KNHDLYPrevPlaylistEnd  =330;

const TInt KNHDLXNextPlaylistStart=470;
const TInt KNHDLYNextPlaylistStart=70;
const TInt KNHDLXNextPlaylistEnd  =640;
const TInt KNHDLYNextPlaylistEnd  =330; 

/*
const TInt KNHDLPrevPlaylistHeight=KNHDLYAlbumArtStart+KNHDLAlbumArtHeight-KNHDLYPrevPlaylistStart; //330-70=260
const TInt KNHDLNrSongsInPrevList=KNHDLPrevPlaylistHeight/KNHDLSmallFontSize; //260/15=17(+5)
const TInt KNHDLYNextPlaylistStart=KNHDLYSongInPlaylistEnd;
const TInt KNHDLXNextPlaylistStart=KNHDLXAlbumArtStart+KNHDLAlbumArtHeight;
const TInt KNHDLNextPlaylistHeight=330-KNHDLYNextPlaylistStart; 
const TInt KNHDLNrSongsInNextList=KNHDLNextPlaylistHeight/KNHDLSmallFontSize; //270/15=18
*/

const TInt KNHDLListMargin=2;

CThemeNHDLandscape* CThemeNHDLandscape::NewL(TSize &aSize)
{
	CThemeNHDLandscape* self = new (ELeave) CThemeNHDLandscape();
	CleanupStack::PushL(self);
	self->ConstructL(aSize);
	CleanupStack::Pop(); // self;
	return self;
}

CThemeNHDLandscape::CThemeNHDLandscape()
{
	iAlbumArtPosition.SetXY(KNHDLXAlbumArtStart,KNHDLYAlbumArtStart);
	iAlbumArtSize.SetSize(KNHDLAlbumArtHeight,KNHDLAlbumArtHeight);
	iArtistTitlePosition.SetXY(KNHDLXMargin,KNHDLYArtistTitleDraw);
	iArtistTitleRect.SetRect(0,0,KNHDLX,KNHDLYArtistTitleEnd);
	iProgressBar.SetRect(0,KNHDLYProgressBarStart,0,KNHDLYProgressBarEnd);
	iProgressBarRounded=5;
	
	iVolumeRect.SetRect(KNHDLXVolumeStart,KNHDLYVolumeStart,KNHDLXVolumeStart+KNHDLVolumeLength,KNHDLYVolumeStart+KNHDLVolumeHeight);
	iFlags|=EShowVolumeMode;
}

void CThemeNHDLandscape::ConstructL(TSize &aSize)
{
	CTheme::ConstructL(aSize);
	ComputeVolumeSize(KVolumeSizeBig);
	
	//get the font we want to use
	iFont=iManager->iEikEnv->DenseFont();

	//get the small font for the lists
	CWsScreenDevice* screenDevice=iManager->iEikEnv->ScreenDevice();
	//TInt nrTypefaces=screenDevice->NumTypefaces(); -> we use font with index zero anyway
	TTypefaceSupport tf;
	screenDevice->TypefaceSupport(tf,0);
	TFontSpec fs(tf.iTypeface.iName,KNHDLSmallFontSize);
	screenDevice->GetNearestFontInPixels(iSmallFont,fs);

		
	iLabelElapsedTime=new (ELeave) CEikLabel;
	iLabelElapsedTime->SetTextL(KEmpty);
	//iLabelElapsedTime->SetLabelAlignment(ELayoutAlignRight);
	iLabelElapsedTime->SetFont(iFont);
	
	iTotalTime=new (ELeave) CEikLabel;
	iTotalTime->SetTextL(KEmpty);
	iTotalTime->SetLabelAlignment(ELayoutAlignRight);
	iTotalTime->SetFont(iFont);
	
	iPositionInPlaylist=new (ELeave) CEikLabel;
	iPositionInPlaylist->SetTextL(KEmpty);
	iPositionInPlaylist->SetLabelAlignment(ELayoutAlignCenter);  // was ELayoutAlignRight
	iPositionInPlaylist->SetFont(iFont);
}

void CThemeNHDLandscape::SetContainer(CMusicPlayerContainer *aContainer)
{
	CTheme::SetContainer(aContainer);
	iLabelElapsedTime->SetContainerWindowL(*(CCoeControl*)aContainer);
	iTotalTime->SetContainerWindowL(*(CCoeControl*)aContainer);
	iPositionInPlaylist->SetContainerWindowL(*(CCoeControl*)aContainer);
}

CThemeNHDLandscape::~CThemeNHDLandscape()
{
	delete iLabelElapsedTime;
	delete iTotalTime;
	delete iPositionInPlaylist;
	//release the small font
	iManager->iEikEnv->ScreenDevice()->ReleaseFont(iSmallFont);
}

void CThemeNHDLandscape::UpdateVolumeOnScreen()
{
	iContainer->DrawNow(iVolumeRect);
}
	
TInt CThemeNHDLandscape::CountComponentControls()
{
	return 3;
}

CCoeControl* CThemeNHDLandscape::ComponentControl( TInt aIndex )
{
	switch(aIndex)
	{
	case 0:return iLabelElapsedTime;
	case 1:return iTotalTime;
	case 2:return iPositionInPlaylist;
	};
	return NULL;
}

void CThemeNHDLandscape::HandlePointerEventL(const TPointerEvent &aPointerEvent)
{
	CTrack *track(iManager->iView->iTrack);
	if(!track)return;
	
	//LOG(ELogGeneral,1,"CThemeNHDLandscape::HandlePointerEventL++");

	if(aPointerEvent.iType==TPointerEvent::EButton1Down)
	{
		if(aPointerEvent.iPosition.iY>KNHDLYProgressBarStart-KDragSeekYMarginUp && aPointerEvent.iPosition.iY<KNHDLYProgressBarEnd+KDragSeekYMarginDown)
		{
			//candidate to seek. Check X position
			TInt xpos(iProgressBar.iTl.iX+iManager->iFormerPosition*iProgressBar.Width()/iDuration);
			if(xpos-KDragSeekXMargin<aPointerEvent.iPosition.iX && xpos+KDragSeekXMargin>aPointerEvent.iPosition.iX)
				iFlags|=ESeekDrag;
		}
		else if(iVolumeRect.Contains(aPointerEvent.iPosition))
		{
			if(aPointerEvent.iPosition.iX>iVolumeRect.iTl.iX+iSpeakerWidth)
			{
				iFlags|=EVolumeDrag;
				iVolInitialX=aPointerEvent.iPosition.iX;
			}
		}
		else iDragStart=aPointerEvent.iPosition;
	}
	else if(aPointerEvent.iType==TPointerEvent::EDrag)
	{
		if(iFlags&ESeekDrag)
		{
			TTimeIntervalMicroSeconds playbackPosition((aPointerEvent.iPosition.iX-iProgressBar.iTl.iX)*iDuration/iProgressBar.Width()*(TInt64)1000000);
			if(playbackPosition<0)playbackPosition=0;
			TInt64 duration(iDuration*(TInt64)1000000);
			if(playbackPosition>duration)playbackPosition=duration;
			UpdatePlaybackPosition(playbackPosition);
		};
		if(iFlags&EVolumeDrag)
		{
			TInt i,steps=(aPointerEvent.iPosition.iX-iVolInitialX)/iLevelWidth;
			if(steps>0)for(i=0;i<steps;i++)
			{
				iManager->iView->VolumeUpDown(ETrue);
				iVolInitialX+=iLevelWidth;
			}
			else for(i=steps;i<0;i++)
			{
				iManager->iView->VolumeUpDown(EFalse);
				iVolInitialX-=iLevelWidth;
			}
		}//done with volume
	}
	else if(aPointerEvent.iType==TPointerEvent::EButton1Up)
	{
		//check for drag operations
		if(iFlags&ESeekDrag)
		{
			TTimeIntervalMicroSeconds playbackPosition((aPointerEvent.iPosition.iX-iProgressBar.iTl.iX)*iDuration/iProgressBar.Width()*(TInt64)1000000);
			if(playbackPosition<0)playbackPosition=0;
			TInt64 duration(iDuration*(TInt64)1000000);
			if(playbackPosition>duration)playbackPosition=duration;
			track->SetPosition(playbackPosition);
			iFlags&=~ESeekDrag;
		}
		else if(iFlags&EVolumeDrag)
		{
			TInt i,steps=(aPointerEvent.iPosition.iX-iVolInitialX)/iLevelWidth;
			if(steps>0)for(i=0;i<steps;i++)
			{
				iManager->iView->VolumeUpDown(ETrue);
				iVolInitialX+=iLevelWidth;
			}
			else for(i=steps;i<0;i++)
			{
				iManager->iView->VolumeUpDown(EFalse);
				iVolInitialX-=iLevelWidth;
			}
			iFlags&=~EVolumeDrag;
		}//done with volume
		else 
		{
			//checking for push buttons
			TRect clickRect(iDragStart,iDragStart);
			clickRect.Grow(KDragXMargin,KDragYMarginUp);
			clickRect.iBr.iY+=KDragYMarginDown-KDragYMarginUp;
			if(clickRect.Contains(aPointerEvent.iPosition))
			{
				//no drag operation, so at most a play/pause or button pushed
				LOG0("No drag, so perhaps play/pause or next or prev button pushed");
				//check for play/pause
				TRect touchRect(KNHDLXAlbumArtStart,KNHDLYAlbumArtStart,KNHDLXAlbumArtStart+KNHDLAlbumArtHeight,KNHDLYAlbumArtStart+KNHDLAlbumArtHeight);
				if(touchRect.Contains(aPointerEvent.iPosition))
				{
					iManager->iView->PlayPauseStop(EFalse);
					iContainer->DrawNow(touchRect);
				}
				else
				{
					//check for next
					touchRect.SetRect(KNHDLXNextPlaylistStart,KNHDLYNextPlaylistStart,KNHDLXNextPlaylistEnd,KNHDLYNextPlaylistEnd);
					if(touchRect.Contains(aPointerEvent.iPosition))
					{
						iManager->iView->NextPreviousL(1);
					}
					else
					{
						//check for previous
						touchRect.SetRect(KNHDLXPrevPlaylistStart,KNHDLYPrevPlaylistStart,KNHDLXPrevPlaylistEnd,KNHDLYPrevPlaylistEnd);
						if(touchRect.Contains(aPointerEvent.iPosition))
						{
							iManager->iView->NextPreviousL(-1);
						}
					}
				}
				//
				LOG0("End of checking for button clicks");
			}
		}
	}; //button up
	//LOG(ELogGeneral,-1,"CThemeNHDLandscape::HandlePointerEventL--");
}

void CThemeNHDLandscape::SetMetadataL(CMetadata &aMetadata,TTimeIntervalMicroSeconds &aCurrentPosition)
{
	//set artist/title string & current position
	CColoredTheme::SetMetadataL(aMetadata,aCurrentPosition);

	//set time stuff
	TBuf<25> time;
	FormatTimeString(time,iManager->iFormerPosition,iDuration);
	iLabelElapsedTime->SetTextL(time);

	//set total duration
	FormatTimeString(time,iDuration,iDuration);
	iTotalTime->SetTextL(time);
	//set iTimeLabelWidth
	iTimeLabelWidth = iTotalTime->MinimumSize().iWidth;
	//set progress bar size
	iProgressBar.SetRect(KNHDLXMargin+iTimeLabelWidth+KNHDLXMargin,KNHDLYProgressBarStart,KNHDLX-KNHDLXMargin-iTimeLabelWidth-KNHDLXMargin,KNHDLYProgressBarEnd);
	
	//set index in playlist
	time.Format(KIndexInPlaylist,iManager->iView->iPlaylist->GetCurrentIndex()+1,iManager->iView->iPlaylist->Count());
	iPositionInPlaylist->SetTextL(time);
	
	//set labels position on the screen
	TSize labelSize;
	TRect rect;
	//label 1: elapsed time
	labelSize = iLabelElapsedTime->MinimumSize();
	labelSize.iWidth=iTimeLabelWidth;
	rect.iTl.iX=KNHDLXMargin;
	rect.iTl.iY=KNHDLYTimeLabelsEnd-labelSize.iHeight;
	rect.iBr = rect.iTl + labelSize;
	iLabelElapsedTime->SetRect(rect);

	//label 2: total time
	labelSize = iTotalTime->MinimumSize();
	rect.iTl.iX=KNHDLX-KNHDLXMargin-iTimeLabelWidth;
	//rect.iTl.iY=KNHDLYTimeLabelsEnd-labelSize.iHeight;
	rect.iBr = rect.iTl + labelSize;
	iTotalTime->SetRect(rect);
	
	//label 3: song in playlist
	rect.SetRect(KNHDLXAlbumArtStart+KNHDLAlbumArtHeight+KNHDLXMargin,KNHDLYSongInPlaylistStart,KNHDLX,KNHDLYSongInPlaylistEnd);
	iPositionInPlaylist->SetRect(rect);
}

TBool CThemeNHDLandscape::UpdatePlaybackPosition(TTimeIntervalMicroSeconds &aCurrentPosition, TBool aDraw) // aDraw=ETrue , returns ETrue if the progress bar was updated
{
	TInt playbackSeconds(aCurrentPosition.Int64()/1000000);

	if(playbackSeconds!=iManager->iFormerPosition)
	{
		iManager->iFormerPosition=playbackSeconds;

		//update current position label
		TBuf<10> time;
		FormatTimeString(time,playbackSeconds,iDuration);
		iLabelElapsedTime->SetTextL(time);

		if(aDraw)
		{
			TRect r(iProgressBar);
			r.iTl.iX=0;
			iContainer->DrawNow(r);
			//iContainer->DrawNow(iLabelElapsedTime->Rect());
		};
		return ETrue;
	};
	return EFalse;
}

void CThemeNHDLandscape::PeriodicUpdate(TTimeIntervalMicroSeconds &aCurrentPosition)
{
	//CColoredTheme::PeriodicUpdate(aCurrentPosition);
	if(!(iFlags&EDragOperation) || !(iFlags&ESeekDrag))
		UpdatePlaybackPosition(aCurrentPosition);
}
	
void CThemeNHDLandscape::SetColorScheme(TUint aColors, TBool aRedraw) //aRedraw=ETrue
{
	LOG0("CThemeNHDLandscape::SetColorScheme+ (iContainer=%x aRedraw=%d)",iContainer,aRedraw);
	CColoredTheme::SetColorScheme(aColors,EFalse);
	aColors=iPreferences->iMusicPlayerThemeData; //can be that CColoredTheme changed/updated the aColors
	SetStaticColorScheme(aColors);

	if(iLabelElapsedTime)
		iLabelElapsedTime->OverrideColorL(EColorLabelText,iColorText);
	if(iTotalTime)
		iTotalTime->OverrideColorL(EColorLabelText,iColorText);
	if(iPositionInPlaylist)
		iPositionInPlaylist->OverrideColorL(EColorLabelText,iColorProgressBarBoundary);
	
	if(iContainer && aRedraw)
	{
		iContainer->DrawDeferred();
		LOG0("DD");
	}
	LOG0("CThemeNHDLandscape::SetColorScheme-");
}

void CThemeNHDLandscape::Draw(const TRect& aRect, CWindowGc& gc) const
{	
	CColoredTheme::Draw(aRect,gc);
	if(iArtistTitleRect==aRect)return;
	
	gc.UseFont(iSmallFont);
	gc.SetPenColor(iColorText);
	gc.SetPenSize(TSize(1,1));
	gc.SetBrushStyle(CGraphicsContext::ENullBrush);
	
	TUint32  step(0x001A1A1A);
	TUint32 currentColor(iColorText.Value());
	TBool decreaseColor(currentColor);
	TInt height=iSmallFont->HeightInPixels();
	TInt descent=iSmallFont->DescentInPixels();
	
	RPointerArray<HBufC> indexStrings;
	RArray<TInt> indexes;
	TPtr name(NULL,0);
	TInt i,l,indexWidth,w;
		
	if(aRect.Intersects(TRect(KNHDLXPrevPlaylistStart,KNHDLYPrevPlaylistStart,KNHDLXPrevPlaylistEnd,KNHDLYPrevPlaylistEnd)))
	{
		//we draw the prev part
		l=KNHDLXPrevPlaylistEnd-KNHDLListMargin;
		const TInt nrPrevElements=(KNHDLXPrevPlaylistEnd-KNHDLXPrevPlaylistStart)/KNHDLSmallFontSize;

		if(iManager->iView->iPlaylist)
			iManager->iView->iPlaylist->GetPrevIndexesL(nrPrevElements,indexStrings,indexes);
		//
		indexWidth=0;
		for(i=0;i<indexes.Count();i++)
		{
			FL_ASSERT(indexStrings[i]);
			w=iSmallFont->TextWidthInPixels(*indexStrings[i]);
			if(w>indexWidth)indexWidth=w;
		}
		//
		for(i=0;i<indexes.Count();i++)
		{
			//draw the index
			gc.DrawTextVertical(*indexStrings[i],TRect(l-height-descent,KNHDLYPrevPlaylistEnd-indexWidth,l,KNHDLYPrevPlaylistEnd),height,ETrue,CGraphicsContext::ERight);
			//draw the name
			iManager->iView->iPlaylist->GetNameAsString(indexes[i],name);
			gc.DrawTextVertical(name,TRect(l-height-descent,KNHDLYPrevPlaylistStart,l,KNHDLYPrevPlaylistEnd-indexWidth),height,ETrue);
			l-=KNHDLSmallFontSize;
			if(decreaseColor)currentColor-=step;
			else currentColor+=step;
			gc.SetPenColor(TRgb(currentColor));
		}
		//clear
		indexStrings.ResetAndDestroy();
		indexes.Reset();
		//fr sign on top
		gc.BitBltMasked( TPoint(KNHDLXPrevPlaylistStart+((KNHDLXPrevPlaylistEnd-KNHDLXPrevPlaylistStart-KNHDXFFFR)>>1), KNHDLYPrevPlaylistStart+((KNHDLYPrevPlaylistEnd-KNHDLYPrevPlaylistStart-KNHDYFFFR)>>1)), iFr, TRect(0,0,KNHDXFFFR,KNHDYFFFR), iFrMask, ETrue);
	}
	
	if(aRect.Intersects(TRect(KNHDLXNextPlaylistStart,KNHDLYNextPlaylistStart,KNHDLXNextPlaylistEnd,KNHDLYNextPlaylistEnd)))
	{
		//we draw the next part
		l=KNHDLXNextPlaylistStart;
		gc.SetPenColor(iColorText);
		currentColor=iColorText.Value();
		const TInt nrNextElements=(KNHDLXNextPlaylistEnd-KNHDLXNextPlaylistStart)/KNHDLSmallFontSize;
		if(iManager->iView->iPlaylist)
			iManager->iView->iPlaylist->GetNextIndexesL(nrNextElements,indexStrings,indexes);
		//
		indexWidth=0;
		for(i=0;i<indexes.Count();i++)
		{
			FL_ASSERT(indexStrings[i]);
			w=iSmallFont->TextWidthInPixels(*indexStrings[i]);
			if(w>indexWidth)indexWidth=w;
		}
		//
		for(i=0;i<indexes.Count();i++)
		{
			//draw the index
			gc.DrawTextVertical(*indexStrings[i],TRect(l,KNHDLYNextPlaylistEnd-indexWidth,l+height+descent,KNHDLYNextPlaylistEnd),height,ETrue,CGraphicsContext::ERight);
			//draw the name
			iManager->iView->iPlaylist->GetNameAsString(indexes[i],name);
			gc.DrawTextVertical(name,TRect(l,KNHDLYNextPlaylistStart,l+height+descent,KNHDLYNextPlaylistEnd-indexWidth),height,ETrue);
			l+=KNHDLSmallFontSize;
			if(decreaseColor)currentColor-=step;
			else currentColor+=step;
			gc.SetPenColor(TRgb(currentColor));
		}
		//clear
		indexStrings.ResetAndDestroy();
		indexes.Reset();
		//ff sign
		gc.BitBltMasked( TPoint(KNHDLXNextPlaylistStart+((KNHDLXNextPlaylistEnd-KNHDLXNextPlaylistStart-KNHDXFFFR)>>1), KNHDLYNextPlaylistStart+((KNHDLYNextPlaylistEnd-KNHDLYNextPlaylistStart-KNHDYFFFR)>>1)), iFf, TRect(0,0,KNHDXFFFR,KNHDYFFFR), iFfMask, ETrue);
	}
	//overall clear
	gc.DiscardFont();
	
	
	/*
	TInt i,top=KNHDLYPrevPlaylistStart+(KNHDLNrSongsInPrevList-1)*KNHDLSmallFontSize;
	
	
	for(i=0;i<KNHDLNrSongsInPrevList;i++)
	{
		gc.DrawText(_L("Metallica - Master of Puppets"),TRect(KNHDLListMargin,top,KNHDLXAlbumArtStart-KNHDLListMargin,top+KNHDLSmallFontSize+height+height),height);
		top-=KNHDLSmallFontSize;
		currentColor-=step;
		gc.SetPenColor(TRgb(currentColor));
		//gc.DrawText(_L("Cucu bubu Miau - Fade to Black"),TPoint(0,top));

	}
	
	top=KNHDLYNextPlaylistStart;
	gc.SetPenColor(iColorText);
	currentColor=iColorText.Value();
	for(i=0;i<KNHDLNrSongsInNextList;i++)
	{
		gc.DrawText(_L("Metallica - Master of Puppets"),TRect(KNHDLXNextPlaylistStart+KNHDLListMargin,top,640-KNHDLListMargin,top+KNHDLSmallFontSize+height+height),height);
		top+=KNHDLSmallFontSize;
		currentColor-=step;
		gc.SetPenColor(TRgb(currentColor));
		//gc.DrawText(_L("Cucu bubu Miau - Fade to Black"),TPoint(0,top));

	}*/
	
	
	
	/*
	gc.UseFont(iSmallFont);
	gc.SetPenColor(iColorText);
	gc.SetPenSize(TSize(1,1));
	gc.SetBrushStyle(CGraphicsContext::ENullBrush);
	for(i=0;i<KNHDLNrSongsInPrevList;i++)
	{
		gc.DrawText(_L("Cucu bubu "),TRect(0,top,170,top+KNHDLSmallFontSize+baseline+baseline),baseline);
		top+=KNHDLSmallFontSize;
		//gc.DrawText(_L("Cucu bubu Miau - Fade to Black"),TPoint(0,top));
				
	}
	*/
	
	if(aRect.Intersects(TRect(iAlbumArtPosition,iAlbumArtSize)))
	{
		CTrack *currentTrack(iManager->iView->iTrack);
		TBool drawPause(EFalse); //if iTrack is NULL, we will draw a Pause
		if(currentTrack)
		{
			if(currentTrack->iState==CTrack::EStatePlaying)
				drawPause=ETrue;
			else if(currentTrack->iState==CTrack::EStateInitializing && currentTrack->iFlags&CTrack::EStartPlaying)
				drawPause=ETrue;
		};
		if(drawPause)
			gc.BitBltMasked( TPoint((KNHDLX-KNHDXPlayPause)>>1,(KNHDLY-KNHDYPlayPause)>>1), iPause,TRect(0,0,KNHDXPlayPause,KNHDYPlayPause), iPauseMask,ETrue);
		else
			gc.BitBltMasked( TPoint((KNHDLX-KNHDXPlayPause)>>1,(KNHDLY-KNHDYPlayPause)>>1), iPlay, TRect(0,0,KNHDXPlayPause,KNHDYPlayPause), iPlayMask, ETrue);
	}
}

#endif

///////////////////////////////////////////////////////////////////////	
/* Old QVGA Portrait
const TInt KQVGAX=240;
const TInt KQVGAXMargin=3;
const TInt KQVGAYArtistTitleStart=0;
const TInt KQVGAYArtistTitleEnd=26;
const TInt KQVGAYArtistTitleDraw=20;
const TInt KQVGAYAlbumArtStart=26;
const TInt KQVGAYAlbumArtEnd=266;
const TInt KQVGAYProgressBarStart=270;
const TInt KQVGAYProgressBarEnd=287;
const TInt KQVGAYTimeLabelsEnd=287;

CThemeQVGAPortrait* CThemeQVGAPortrait::NewL(CThemeManager *aManager, TSize &aSize)
{
	CThemeQVGAPortrait* self = new (ELeave) CThemeQVGAPortrait();
	CleanupStack::PushL(self);
	self->ConstructL(aManager,aSize);
	CleanupStack::Pop(); // self;
	return self;
}

CThemeQVGAPortrait::CThemeQVGAPortrait()
{
	iAlbumArtPosition.SetXY(0,KQVGAYAlbumArtStart);
	iAlbumArtSize.SetSize(KQVGAX,KQVGAX);
	iArtistTitlePosition.SetXY(KQVGAXMargin,KQVGAYArtistTitleDraw);
	iArtistTitleRect.SetRect(0,0,KQVGAX,KQVGAYArtistTitleEnd);
	iProgressBar.SetRect(0,KQVGAYProgressBarStart,0,KQVGAYProgressBarEnd);
	iProgressBarRounded=5;
	//iVerticalFillSize=27;
}

void CThemeQVGAPortrait::ConstructL(CThemeManager *aManager, TSize &aSize)
{
    CTheme::ConstructL(aManager,aSize);
	
	//get colors
	SetColorScheme(iPreferences->iMusicPlayerThemeData);
		
	iLabelElapsedTime=new (ELeave) CEikLabel;
	iLabelElapsedTime->SetTextL(KEmpty);
	//iLabelElapsedTime->SetLabelAlignment(ELayoutAlignRight);
	iLabelElapsedTime->OverrideColorL(EColorLabelText,iColorText);
	iTotalTime=new (ELeave) CEikLabel;
	iTotalTime->SetTextL(KEmpty);
	iTotalTime->SetLabelAlignment(ELayoutAlignRight);
	iTotalTime->OverrideColorL(EColorLabelText,iColorText);

	//get the font we want to use
	iFont=iManager->iEikEnv->DenseFont();
}

void CThemeQVGAPortrait::SetContainer(CMusicPlayerContainer *aContainer)
{
	CTheme::SetContainer(aContainer);
	iLabelElapsedTime->SetContainerWindowL(*(CCoeControl*)aContainer);
	iTotalTime->SetContainerWindowL(*(CCoeControl*)aContainer);
}

CThemeQVGAPortrait::~CThemeQVGAPortrait()
{
	delete iLabelElapsedTime;
	delete iTotalTime;
}

TInt CThemeQVGAPortrait::CountComponentControls()
{
	return 2;
}

CCoeControl* CThemeQVGAPortrait::ComponentControl( TInt aIndex )
{
	switch(aIndex)
	{
	case 0:return iLabelElapsedTime;
	case 1:return iTotalTime;
	};
	return NULL;
}

void CThemeQVGAPortrait::SetMetadataL(CMetadata &aMetadata,TTimeIntervalMicroSeconds &aCurrentPosition)
{
	//set artist/title string & current position
	CColoredTheme::SetMetadataL(aMetadata,aCurrentPosition);

	//set time stuff
	TBuf<10> time;
	FormatTimeString(time,iManager->iFormerPosition,iDuration);
	iLabelElapsedTime->SetTextL(time);

	//set total duration
	FormatTimeString(time,iDuration,iDuration);
	iTotalTime->SetTextL(time);
	//set iTimeLabelWidth
	iTimeLabelWidth = iTotalTime->MinimumSize().iWidth;
	//set progress bar size
	iProgressBar.SetRect(KQVGAXMargin+iTimeLabelWidth+KQVGAXMargin,KQVGAYProgressBarStart,KQVGAX-KQVGAXMargin-iTimeLabelWidth-KQVGAXMargin,KQVGAYProgressBarEnd);
	
	//set labels position on the screen
	TSize labelSize;
	TRect rect;
	//label 1: elapsed time
	labelSize = iLabelElapsedTime->MinimumSize();
	labelSize.iWidth=iTimeLabelWidth;
	rect.iTl.iX=KQVGAXMargin;
	rect.iTl.iY=KQVGAYTimeLabelsEnd-labelSize.iHeight;
	rect.iBr = rect.iTl + labelSize;
	iLabelElapsedTime->SetRect(rect);

	//label 2: total time
	labelSize = iTotalTime->MinimumSize();
	rect.iTl.iX=KQVGAX-KQVGAXMargin-iTimeLabelWidth;
	rect.iTl.iY=KQVGAYTimeLabelsEnd-labelSize.iHeight;
	rect.iBr = rect.iTl + labelSize;
	iTotalTime->SetRect(rect);
}

TBool CThemeQVGAPortrait::UpdatePlaybackPosition(TTimeIntervalMicroSeconds &aCurrentPosition, TBool aDraw) //returns ETrue if the progress bar was updated
{
	TInt playbackSeconds(aCurrentPosition.Int64()/1000000);

	if(playbackSeconds!=iManager->iFormerPosition)
	{
		//set elapsed time to current position
		iManager->iFormerPosition=playbackSeconds;
		//update current position label
		TBuf<10> time;
		FormatTimeString(time,playbackSeconds,iDuration);
		iLabelElapsedTime->SetTextL(time);

		if(aDraw)
		{
			TRect r(iProgressBar);
			r.iTl.iX=0;
			iContainer->DrawNow(r);
			//iLabelElapsedTime->DrawNow();
		};
		LOG1("Fps: %d",iFps);
		iFps=0;
		return ETrue;
	};
	return EFalse;
}

void CThemeQVGAPortrait::PeriodicUpdate(TTimeIntervalMicroSeconds &aCurrentPosition)
{
	CColoredTheme::PeriodicUpdate(aCurrentPosition);
	UpdatePlaybackPosition(aCurrentPosition);
}

void CThemeQVGAPortrait::SetColorScheme(TUint aColors, TBool aRedraw)
{
	CColoredTheme::SetColorScheme(aColors,EFalse);
	if(iLabelElapsedTime)
		iLabelElapsedTime->OverrideColorL(EColorLabelText,iColorText);
	if(iTotalTime)
		iTotalTime->OverrideColorL(EColorLabelText,iColorText);
	
	if(iContainer && aRedraw)
		iContainer->DrawDeferred();
}
*/


const TInt KQVGAXMargin=2;
const TInt KQVGAYMargin=2;
/*
const TInt KQVGAYArtistTitleStart=0;
const TInt KQVGAYArtistTitleEnd=26;
const TInt KQVGAYArtistTitleDraw=20;
const TInt KQVGAYAlbumArtStart=26;
const TInt KQVGAYAlbumArtEnd=266;
const TInt KQVGAYProgressBarStart=270;
const TInt KQVGAYProgressBarEnd=287;
const TInt KQVGAYTimeLabelsEnd=287;
*/

CThemeQVGAPortrait* CThemeQVGAPortrait::NewL(TSize &aSize)
{
	CThemeQVGAPortrait* self = new (ELeave) CThemeQVGAPortrait();
	CleanupStack::PushL(self);
	self->ConstructL(aSize);
	CleanupStack::Pop(); // self;
	return self;
}

CThemeQVGAPortrait::CThemeQVGAPortrait()
{	
}

void CThemeQVGAPortrait::ConstructL(TSize &aSize)
{
	CTheme::ConstructL(aSize);
	
	//get the font we want to use
	iFont=iManager->iEikEnv->DenseFont();
	
	//create time labels
	iLabelElapsedTime=new (ELeave) CEikLabel;
	iLabelElapsedTime->SetTextL(KEmpty);
	iLabelElapsedTime->SetFont(iFont);
	//iLabelElapsedTime->SetLabelAlignment(ELayoutAlignRight);
	iTotalTime=new (ELeave) CEikLabel;
	iTotalTime->SetTextL(KEmpty);
	iTotalTime->SetFont(iFont);
	iTotalTime->SetLabelAlignment(ELayoutAlignRight);
	TInt textAscent=iLabelElapsedTime->Font()->AscentInPixels();//numbers do not have descent
	TInt textHeight(iFont->HeightInPixels());
	if(textAscent==textHeight)
	{
		//we have Asian fonts
		iFlags|=EAsianFonts;
		//textAscent=(textAscent/3)<<1;
		textAscent=11; //above line would be 12
	}
	else textHeight+=(KQVGAYMargin<<1);
		
	LOG0("QVGA Portrait theme font: height=%d, ascent=%d",iFont->HeightInPixels(),textAscent);
	if(textAscent>iFont->HeightInPixels())
	{
		LOG0("Strange, ascent is bigger than the height!");
		textAscent=iFont->HeightInPixels();
	};
	
	//get the sizes
	iArtistTitleRect.SetRect(0,0,aSize.iWidth,textHeight);
	if(iFlags&EAsianFonts)
		iArtistTitlePosition.SetXY(KQVGAXMargin,textHeight);
	else
		iArtistTitlePosition.SetXY(KQVGAXMargin,KQVGAYMargin+textAscent+KQVGAYMargin);
	
	iAlbumArtSize.SetSize(aSize.iWidth,aSize.iWidth);
	iAlbumArtPosition.SetXY(0,textHeight);

	if(textAscent>KMaxFillHeight)
		textAscent=KMaxFillHeight;
	iProgressBar.SetRect(0,textHeight+aSize.iWidth+KQVGAYMargin,0,textHeight+aSize.iWidth+KQVGAYMargin+textAscent);
	if((textAscent>>1)<5)
		iProgressBarRounded=textAscent>>1;
	else iProgressBarRounded=5;

	//set volume
	iVolumeRect.SetRect(KQVGAXMargin,iProgressBar.iBr.iY,(iMySize.iWidth>>1)+KQVGAXMargin,iMySize.iHeight);
	ComputeVolumeSize(KVolumeSizeSmall);
	iFlags|=EShowVolumeMode;
	
	iPositionInPlaylist=new (ELeave) CEikLabel;
	iPositionInPlaylist->SetTextL(KEmpty);
	iPositionInPlaylist->SetLabelAlignment(ELayoutAlignRight);
	iPositionInPlaylist->SetFont(iFont);
	
	//get colors
	SetColorScheme(iPreferences->iMusicPlayerThemeData);
}

void CThemeQVGAPortrait::SetContainer(CMusicPlayerContainer *aContainer)
{
	CTheme::SetContainer(aContainer);
	iLabelElapsedTime->SetContainerWindowL(*(CCoeControl*)aContainer);
	iTotalTime->SetContainerWindowL(*(CCoeControl*)aContainer);
	iPositionInPlaylist->SetContainerWindowL(*(CCoeControl*)aContainer);
}

CThemeQVGAPortrait::~CThemeQVGAPortrait()
{
	delete iLabelElapsedTime;
	delete iTotalTime;
	delete iPositionInPlaylist;
}

TInt CThemeQVGAPortrait::CountComponentControls()
{
	return 3;
}

CCoeControl* CThemeQVGAPortrait::ComponentControl( TInt aIndex )
{
	switch(aIndex)
	{
	case 0:return iLabelElapsedTime;
	case 1:return iTotalTime;
	case 2:return iPositionInPlaylist;
	};
	return NULL;
}

void CThemeQVGAPortrait::SetMetadataL(CMetadata &aMetadata,TTimeIntervalMicroSeconds &aCurrentPosition)
{
	//set artist/title string & current position
	CColoredTheme::SetMetadataL(aMetadata,aCurrentPosition);

	//set time stuff
	TBuf<25> time;
	FormatTimeString(time,iManager->iFormerPosition,iDuration);
	iLabelElapsedTime->SetTextL(time);

	//set total duration
	FormatTimeString(time,iDuration,iDuration);
	iTotalTime->SetTextL(time);
	//set iTimeLabelWidth
	iTimeLabelWidth = iTotalTime->MinimumSize().iWidth;
	//set progress bar size
	iProgressBar.iTl.iX=KQVGAXMargin+iTimeLabelWidth+KQVGAXMargin;
	iProgressBar.iBr.iX=iMySize.iWidth-KQVGAXMargin-iTimeLabelWidth-KQVGAXMargin;
	//iProgressBar.SetRect(KQVGAXMargin+iTimeLabelWidth+KQVGAXMargin,KQVGAYProgressBarStart,KQVGAX-KQVGAXMargin-iTimeLabelWidth-KQVGAXMargin,KQVGAYProgressBarEnd);
	
	//set index in playlist
	time.Format(KIndexInPlaylist,iManager->iView->iPlaylist->GetCurrentIndex()+1,iManager->iView->iPlaylist->Count());
	iPositionInPlaylist->SetTextL(time);
	
	//set labels position on the screen
	TSize labelSize;
	TRect rect;
	//label 1: elapsed time
	//labelSize = iLabelElapsedTime->MinimumSize();
	//labelSize.iWidth=iTimeLabelWidth;
	rect.iTl.iX=KQVGAXMargin;
	rect.iTl.iY=iProgressBar.iTl.iY-KQVGAYMargin-2;
	rect.iBr.iX=KQVGAXMargin+iTimeLabelWidth;
	rect.iBr.iY=rect.iTl.iY+iFont->AscentInPixels()+KQVGAYMargin+2;
	//rect.iBr = rect.iTl + labelSize;
	iLabelElapsedTime->SetRect(rect);

	//label 2: total time
	//labelSize = iTotalTime->MinimumSize();
	rect.iTl.iX=iMySize.iWidth-KQVGAXMargin-iTimeLabelWidth;
	//rect.iTl.iY=KQVGAYTimeLabelsEnd-labelSize.iHeight;
	//rect.iBr = rect.iTl + labelSize;
	rect.iBr.iX=iMySize.iWidth-KQVGAXMargin;
	iTotalTime->SetRect(rect);
	
	//label 3: position in playlist
	labelSize=iPositionInPlaylist->MinimumSize();
	rect.iTl.iX=iMySize.iWidth-KQVGAXMargin-labelSize.iWidth;
	rect.iBr.iX=iMySize.iWidth-KQVGAXMargin;
	if(iFlags&EAsianFonts)
		rect.iTl.iY=iMySize.iHeight-labelSize.iHeight-KQVGAYMargin;
	else
		rect.iTl.iY=iMySize.iHeight-labelSize.iHeight-KQVGAYMargin-KQVGAYMargin;
	rect.iBr.iY=iMySize.iHeight;
	iPositionInPlaylist->SetRect(rect);
}

TBool CThemeQVGAPortrait::UpdatePlaybackPosition(TTimeIntervalMicroSeconds &aCurrentPosition, TBool aDraw/*=ETrue*/) //returns ETrue if the progress bar was updated
{
	TInt playbackSeconds(aCurrentPosition.Int64()/1000000);

	if(playbackSeconds!=iManager->iFormerPosition)
	{
		//set elapsed time to current position
		iManager->iFormerPosition=playbackSeconds;
		//update current position label
		TBuf<10> time;
		FormatTimeString(time,playbackSeconds,iDuration);
		iLabelElapsedTime->SetTextL(time);

		if(aDraw)
		{
			TRect r(iProgressBar);
			r.iTl.iX=0;
			iContainer->DrawNow(r);
			//iLabelElapsedTime->DrawNow();
		};
		return ETrue;
	};
	return EFalse;
}

void CThemeQVGAPortrait::PeriodicUpdate(TTimeIntervalMicroSeconds &aCurrentPosition)
{
	//CColoredTheme::PeriodicUpdate(aCurrentPosition);
	UpdatePlaybackPosition(aCurrentPosition);
}

void CThemeQVGAPortrait::SetColorScheme(TUint aColors, TBool aRedraw/*=ETrue*/)
{
	CColoredTheme::SetColorScheme(aColors,EFalse);
	if(iLabelElapsedTime)
		iLabelElapsedTime->OverrideColorL(EColorLabelText,iColorText);
	if(iTotalTime)
		iTotalTime->OverrideColorL(EColorLabelText,iColorText);
	if(iPositionInPlaylist)
		iPositionInPlaylist->OverrideColorL(EColorLabelText,iColorText);
	
	if(iContainer && aRedraw)
		iContainer->DrawDeferred();
}

void CThemeQVGAPortrait::UpdateVolumeOnScreen()
{
	iContainer->DrawNow(iVolumeRect);
}


///////////////////////////////////////////////////////////////////////	

const TInt KDoubleX=352;
const TInt KDoubleXMargin=2;
const TInt KDoubleYArtistTitleStart=352;
const TInt KDoubleYArtistTitleEnd=376;
const TInt KDoubleYArtistTitleDraw=372;
const TInt KDoubleYAlbumArtStart=0;
const TInt KDoubleYAlbumArtEnd=352;
const TInt KDoubleYProgressBarStart=352;
const TInt KDoubleYProgressBarEnd=372;


CThemeDoublePortrait* CThemeDoublePortrait::NewL(TSize &aSize)
{
	CThemeDoublePortrait* self = new (ELeave) CThemeDoublePortrait();
	CleanupStack::PushL(self);
	self->ConstructL(aSize);
	CleanupStack::Pop(); // self;
	return self;
}

CThemeDoublePortrait::CThemeDoublePortrait()
{
	iAlbumArtPosition.SetXY(0,KDoubleYAlbumArtStart);
	iAlbumArtSize.SetSize(KDoubleX,KDoubleX);
	iArtistTitlePosition.SetXY(KDoubleXMargin,KDoubleYArtistTitleDraw);
	iArtistTitleRect.SetRect(0,KDoubleYArtistTitleStart,KDoubleX,KDoubleYArtistTitleEnd);
	iProgressBar.SetRect(0,KDoubleYArtistTitleStart,KDoubleX,KDoubleYArtistTitleEnd);
	//iVerticalFillSize=29;
	iFlags|=ENoProgressBarBoundary;
}

void CThemeDoublePortrait::ConstructL(TSize &aSize)
{
	CTheme::ConstructL(aSize);
	
	//get colors
	SetColorScheme(iPreferences->iMusicPlayerThemeData);
		
	//get the font we want to use
	iFont=iManager->iEikEnv->DenseFont();
}

/*
CThemeDoublePortrait::~CThemeDoublePortrait()
{
}
*/

void CThemeDoublePortrait::SetMetadataL(CMetadata &aMetadata,TTimeIntervalMicroSeconds &aCurrentPosition)
{
	//set artist/title string & current position
	CColoredTheme::SetMetadataL(aMetadata,aCurrentPosition);
	
	//set time stuff
	TBuf<25> time;
	TInt maxLen(0);
	TInt extraSpaces;
	//set elapsed time to current position
	iManager->iFormerPosition=aCurrentPosition.Int64()/1000000;
	if(iDuration>=3600)
	{
		TInt h(iManager->iFormerPosition/3600);
		TInt m((iManager->iFormerPosition-h*3600)/60);
		TInt h2(iDuration/3600);
		TInt m2((iDuration-h2*3600)/60);
		time.Format(KDoubleTimeFormatHMS,h,m,iManager->iFormerPosition%60,h2,m2,iDuration%60);
		maxLen=19;
		extraSpaces=(maxLen-time.Length())*KSpace().Length();
		maxLen+=KSpace().Length();
	}
	else
	{
		time.Format(KDoubleTimeFormat,iManager->iFormerPosition/60,iManager->iFormerPosition%60,iDuration/60,iDuration%60);
		maxLen=13;
		extraSpaces=(maxLen-time.Length())*KSpace().Length();
		maxLen+=KSpace().Length();
	};
	
	//add time info to iArtistTitle
	TInt length(iArtistTitle->Length()/*+KSpaceAfterTitle().Length()*/+maxLen);
	if(iArtistTitle->Des().MaxLength()<length)
	{
		//iArtistTitle->ReAllocL(length);
		//seems tha HBufC::ReAllocL() does not work always, so we need to do it manually
		HBufC *newArtistTitle=HBufC::NewL(length);
		newArtistTitle->Des().Copy(*iArtistTitle);
		delete iArtistTitle;
		iArtistTitle=newArtistTitle;
	};
		
	iArtistTitle->Des().Append(KSpace);
	iPlaybactTimePosition=iArtistTitle->Length();
	iArtistTitle->Des().Append(time);
	for(TInt i=0;i<extraSpaces;i++)
		iArtistTitle->Des().Append(KSpace);
		
	/*
	//set artist/title string
	TInt length(aArtistTitle.Length()+KSpaceAfterTitle().Length()+maxLen);
	if(!iArtistTitle)
		iArtistTitle=HBufC::NewL(length);
	else if(iArtistTitle->Des().MaxLength()<length)
	{
		delete iArtistTitle;
		iArtistTitle=HBufC::NewL(length);
	};
	iArtistTitle->Des().Copy(aArtistTitle);
	iArtistTitle->Des().Append(KSpace);
	iPlaybactTimePosition=iArtistTitle->Length();
	iArtistTitle->Des().Append(time);
	for(TInt i=0;i<extraSpaces;i++)
		iArtistTitle->Des().Append(KSpace);
		*/
	
	//check if the artist+title string fits in the screen or it needs scrolling
	TInt width=iFont->TextWidthInPixels(*iArtistTitle);
	if(width>KDoubleX)
	{
		//we need scrolling
		//iArtistTitle->Des().Append(KSpaceAfterTitle);
		iFlags|=EArtistTitleNeedsScrolling;
	}
	else
		iFlags&=~EArtistTitleNeedsScrolling;	
}

TBool CThemeDoublePortrait::UpdatePlaybackPosition(TTimeIntervalMicroSeconds &aCurrentPosition, TBool aDraw/*=ETrue*/) //returns ETrue if the progress bar was updated
{
	TInt playbackSeconds(aCurrentPosition.Int64()/1000000);

	if(playbackSeconds!=iManager->iFormerPosition)
	{
		//set elapsed time to current position
		iManager->iFormerPosition=playbackSeconds;
		//update current position label
		TBuf<25> time;
		if(iDuration>=3600)
		{
			TInt h(iManager->iFormerPosition/3600);
			TInt m((iManager->iFormerPosition-h*3600)/60);
			TInt h2(iDuration/3600);
			TInt m2((iDuration-h*3600)/60);
			time.Format(KDoubleTimeFormatHMS,h,m,iManager->iFormerPosition%60,h2,m2,iDuration%60);
		}
		else
		{
			time.Format(KDoubleTimeFormat,iManager->iFormerPosition/60,iManager->iFormerPosition%60,iDuration/60,iDuration%60);
		};
		//update artist title string
		TInt tl(time.Length());
		if(iPlaybactTimePosition+time.Length() > iArtistTitle->Length())
		{
			//we need to copy the data in 2 steps
			TInt partialLength(iArtistTitle->Length()-iPlaybactTimePosition);
			iArtistTitle->Des().Replace(iPlaybactTimePosition,partialLength,time.Left(partialLength));
			iArtistTitle->Des().Replace(0,tl-partialLength,time.Right(tl-partialLength));
			
			/*
			TPtr ptr(iArtistTitle->Des().MidTPtr(iPlaybactTimePosition));
			ptr.Copy(time.Left(partialLength));
			ptr.Set(iArtistTitle->Des());
			ptr.Copy(time.Right(time.Length()-partialLength));*/
		}
		else
		{
			//we can copy all the data in one go
			iArtistTitle->Des().Replace(iPlaybactTimePosition,tl,time);
			//TPtr ptr(iArtistTitle->Des().MidTPtr(iPlaybactTimePosition));
			//ptr.Copy(time);
		};

		if(aDraw)
		{
			iContainer->DrawNow(iArtistTitleRect);
		};
		return ETrue;
	};
	return EFalse;
}

void CThemeDoublePortrait::PeriodicUpdate(TTimeIntervalMicroSeconds &aCurrentPosition)
{
	UpdatePlaybackPosition(aCurrentPosition,EFalse);
	//CColoredTheme::PeriodicUpdate(aCurrentPosition);
	
		
	/*
	if(iFlags&EArtistTitleNeedsScrolling )
	{
		UpdatePlaybackPosition(aCurrentPosition,EFalse);
		//if we are here, we have to move the label
		TChar first=(*iArtistTitle)[0];
		iArtistTitle->Des().Delete(0,1);
		iArtistTitle->Des().Append(first);
		iPlaybactTimePosition--;
		if(iPlaybactTimePosition<0)
			iPlaybactTimePosition=iArtistTitle->Length()-1;
		
		iContainer->DrawNow(iArtistTitleRect);
	}
	else
		UpdatePlaybackPosition(aCurrentPosition);	
		*/
}

void CThemeDoublePortrait::SetColorScheme(TUint aColors, TBool aRedraw/*=ETrue*/)
{
	CColoredTheme::SetColorScheme(aColors,aRedraw);
}

///////////////////////////////////////////////////////////////////////	

const TInt KGenericXMargin=2;
const TInt KGenericYMargin=3;
const TInt KGenericMaxProgressBarWidth=20;

CThemeGeneric* CThemeGeneric::NewL(TSize &aSize)
{
	CThemeGeneric* self = new (ELeave) CThemeGeneric();
	CleanupStack::PushL(self);
	self->ConstructL(aSize);
	CleanupStack::Pop(); // self;
	return self;
}

CThemeGeneric::CThemeGeneric()
{
	
}

void CThemeGeneric::ConstructL(TSize &aSize)
{
	CTheme::ConstructL(aSize);
	
	//get the sizes
	iVerticalStep=aSize.iHeight/5;
	iArtistTitlePosition.SetXY(KGenericXMargin,(iVerticalStep<<2)/5);
	iArtistTitleRect.SetRect(0,0,aSize.iWidth,iVerticalStep);
	
	//the progress bar should be max 30 pixels wide
	TInt yMargin(KGenericYMargin);
	TInt width(iVerticalStep-(KGenericYMargin<<1));
	if(width>KGenericMaxProgressBarWidth)
	{
		width=KGenericMaxProgressBarWidth;
		yMargin=(iVerticalStep-width)>>1;
		iProgressBarRounded=5;
	}
	else
	{
		iProgressBarRounded=width>>2;
	};
	iProgressBar.SetRect(KGenericXMargin,(iVerticalStep<<1)+yMargin,aSize.iWidth-KGenericXMargin,iVerticalStep*3-yMargin);
	iProgressBarRounded=5;

	iAlbumArtSize.SetSize(0,0);
	iAlbumArtPosition.SetXY(0,0);
	
	//get colors
	SetColorScheme(iPreferences->iMusicPlayerThemeData);
		
	//get the font we want to use
	iFont=iManager->iEikEnv->DenseFont();
	
	//create labels
	iLabelElapsedTime=new (ELeave) CEikLabel;
	iLabelElapsedTime->SetTextL(KEmpty);
	//iLabelElapsedTime->SetLabelAlignment(ELayoutAlignRight);
	iLabelElapsedTime->OverrideColorL(EColorLabelText,iColorText);
	iTotalTime=new (ELeave) CEikLabel;
	iTotalTime->SetTextL(KEmpty);
	iTotalTime->SetLabelAlignment(ELayoutAlignRight);
	iTotalTime->OverrideColorL(EColorLabelText,iColorText);
	
	iPositionInPlaylist=new (ELeave) CEikLabel;
	iPositionInPlaylist->SetTextL(KEmpty);
	iPositionInPlaylist->SetLabelAlignment(ELayoutAlignCenter);
	iPositionInPlaylist->OverrideColorL(EColorLabelText,iColorText);
	iPositionInPlaylist->SetFont(iFont);
	iAlbumYear=new (ELeave) CEikLabel;
	iAlbumYear->SetTextL(KEmpty);
	iAlbumYear->SetLabelAlignment(ELayoutAlignCenter);
	iAlbumYear->OverrideColorL(EColorLabelText,iColorText);
	iAlbumYear->SetFont(iFont);	
}


CThemeGeneric::~CThemeGeneric()
{
	//delete the labels
	delete iLabelElapsedTime;
	delete iTotalTime;
	delete iPositionInPlaylist;
	delete iAlbumYear;
}


void CThemeGeneric::SetContainer(CMusicPlayerContainer *aContainer)
{
	CTheme::SetContainer(aContainer);
	iLabelElapsedTime->SetContainerWindowL(*(CCoeControl*)aContainer);
	iTotalTime->SetContainerWindowL(*(CCoeControl*)aContainer);
	iPositionInPlaylist->SetContainerWindowL(*(CCoeControl*)aContainer);
	iAlbumYear->SetContainerWindowL(*(CCoeControl*)aContainer);
}

TInt CThemeGeneric::CountComponentControls()
{
	return 4;
}

CCoeControl* CThemeGeneric::ComponentControl( TInt aIndex )
{
	switch(aIndex)
	{
	case 0:return iLabelElapsedTime;
	case 1:return iTotalTime;
	case 2:return iPositionInPlaylist;
	case 3:return iAlbumYear;
	};
	return NULL;
}

void CThemeGeneric::SetMetadataL(CMetadata &aMetadata,TTimeIntervalMicroSeconds &aCurrentPosition)
{
	//set artist/title string & current position
	CColoredTheme::SetMetadataL(aMetadata,aCurrentPosition);

	//set time stuff
	TBuf<25> time;
	FormatTimeString(time,iManager->iFormerPosition,iDuration);
	iLabelElapsedTime->SetTextL(time);

	//set total duration
	FormatTimeString(time,iDuration,iDuration);
	iTotalTime->SetTextL(time);
	
	//set index in playlist
	time.Format(KIndexInPlaylist,iManager->iView->iPlaylist->GetCurrentIndex()+1,iManager->iView->iPlaylist->Count());
	iPositionInPlaylist->SetTextL(time);
	if(aMetadata.iAlbum && aMetadata.iAlbum->Length()>0)
		if(aMetadata.iYear>0 && aMetadata.iYear<3000) //I don't believe anyone will be using this sw in a thousand years
		{
			//we have both album and year
			HBufC *album=HBufC::NewL(aMetadata.iAlbum->Length()+8);
			album->Des().Copy(*aMetadata.iAlbum);
			time.Format(KAlbumYear,aMetadata.iYear);
			album->Des().Append(time);
			iAlbumYear->SetTextL(*album);
			delete album;
		}
		else
			iAlbumYear->SetTextL(*aMetadata.iAlbum); //only album name is available
	else iAlbumYear->SetTextL(KEmpty);
	
	//set timeLabelWidth
	TInt timeLabelWidth(iTotalTime->MinimumSize().iWidth);

	//set labels position on the screen
	TSize labelSize;
	TRect rect;
	//label 1: elapsed time
	labelSize = iLabelElapsedTime->MinimumSize();
	labelSize.iWidth=timeLabelWidth;
	rect.iTl.iX=KGenericXMargin;
	rect.iTl.iY=iProgressBar.iBr.iY+(KGenericYMargin<<1);
	rect.iBr = rect.iTl + labelSize;
	iLabelElapsedTime->SetRect(rect);

	//label 2: total time
	labelSize = iTotalTime->MinimumSize();
	rect.iTl.iX=iMySize.iWidth-KGenericXMargin-timeLabelWidth;
	rect.iBr = rect.iTl + labelSize;
	iTotalTime->SetRect(rect);
	
	//label 3: song in playlist
	rect.SetRect(0,iVerticalStep,iMySize.iWidth,iVerticalStep<<1);
	iPositionInPlaylist->SetRect(rect);
	//label 4: album [year]
	rect.SetRect(0,iVerticalStep<<2,iMySize.iWidth,iVerticalStep*5);
	iAlbumYear->SetRect(rect);
}

TBool CThemeGeneric::UpdatePlaybackPosition(TTimeIntervalMicroSeconds &aCurrentPosition, TBool aDraw/*=ETrue*/) //returns ETrue if the progress bar was updated
{
	TInt playbackSeconds(aCurrentPosition.Int64()/1000000);

	if(playbackSeconds!=iManager->iFormerPosition)
	{
		//set elapsed time to current position
		iManager->iFormerPosition=playbackSeconds;
		//update current position label
		TBuf<10> time;
		FormatTimeString(time,playbackSeconds,iDuration);
		iLabelElapsedTime->SetTextL(time);

		if(aDraw)
		{
			iContainer->DrawNow(iProgressBar);
			iContainer->DrawNow(iLabelElapsedTime->Rect());
			//iLabelElapsedTime->DrawNow();
		};
		return ETrue;
	};
	return EFalse;
}

void CThemeGeneric::PeriodicUpdate(TTimeIntervalMicroSeconds &aCurrentPosition)
{
	//CColoredTheme::PeriodicUpdate(aCurrentPosition);
	UpdatePlaybackPosition(aCurrentPosition);
}

void CThemeGeneric::SetColorScheme(TUint aColors, TBool aRedraw/*=ETrue*/)
{
	CColoredTheme::SetColorScheme(aColors,EFalse);
	if(iLabelElapsedTime)
		iLabelElapsedTime->OverrideColorL(EColorLabelText,iColorText);
	if(iTotalTime)
		iTotalTime->OverrideColorL(EColorLabelText,iColorText);
	if(iPositionInPlaylist)
		iPositionInPlaylist->OverrideColorL(EColorLabelText,iColorText);
	if(iAlbumYear)
		iAlbumYear->OverrideColorL(EColorLabelText,iColorText);
	
	if(iContainer && aRedraw)
		iContainer->DrawDeferred();
}

///////////////////////////////////////////////////////////////////////	

const TInt KLandscapeGenericXMargin=2;
const TInt KLandscapeGenericYMargin=2;
const TInt KLandscapeGenericMaxVolumeHeight=45;
//const TInt KGenericMaxProgressBarWidth=20;


CThemeLandscapeGeneric* CThemeLandscapeGeneric::NewL(TSize &aSize)
{
	CThemeLandscapeGeneric* self = new (ELeave) CThemeLandscapeGeneric();
	CleanupStack::PushL(self);
	self->ConstructL(aSize);
	CleanupStack::Pop(); // self;
	return self;
}

CThemeLandscapeGeneric::CThemeLandscapeGeneric()
{
	
}

void CThemeLandscapeGeneric::ConstructL(TSize &aSize)
{
	CTheme::ConstructL(aSize);
	
	//get the font we want to use
	iFont=iManager->iEikEnv->DenseFont();
	
	//create time labels
	iLabelElapsedTime=new (ELeave) CEikLabel;
	iLabelElapsedTime->SetTextL(KEmpty);
	//iLabelElapsedTime->SetLabelAlignment(ELayoutAlignRight);
	iTotalTime=new (ELeave) CEikLabel;
	iTotalTime->SetTextL(KEmpty);
	iTotalTime->SetLabelAlignment(ELayoutAlignRight);
	TInt labelHeight=iLabelElapsedTime->Font()->AscentInPixels();//numbers do not have descent
	TInt textHeight(KLandscapeGenericYMargin+iFont->HeightInPixels()+KLandscapeGenericYMargin);
	
	//get the sizes
	iArtistTitleRect.SetRect(0,0,aSize.iWidth,textHeight);
	iArtistTitlePosition.SetXY(KLandscapeGenericXMargin,KLandscapeGenericYMargin+iFont->AscentInPixels()+KLandscapeGenericYMargin);
	
	if(labelHeight>KMaxFillHeight)
		labelHeight=KMaxFillHeight;
	iProgressBar.SetRect(0,aSize.iHeight-labelHeight-KLandscapeGenericYMargin,0,aSize.iHeight-KLandscapeGenericYMargin);
	if((labelHeight>>2)<5)
		iProgressBarRounded=labelHeight>>2;
	else iProgressBarRounded=5;
	
	TInt albumArtSize(aSize.iHeight-textHeight-labelHeight-KLandscapeGenericYMargin-KLandscapeGenericYMargin);
	iAlbumArtSize.SetSize(albumArtSize,albumArtSize);
	iAlbumArtPosition.SetXY(0,textHeight);
	
	TInt height(iAlbumArtSize.iHeight-((textHeight+(KLandscapeGenericYMargin<<2))<<1));
	if(height>KLandscapeGenericMaxVolumeHeight)height=KLandscapeGenericMaxVolumeHeight;
	iVolumeRect.SetRect(iAlbumArtSize.iWidth+KLandscapeGenericXMargin,iProgressBar.iTl.iY-KLandscapeGenericYMargin-textHeight-KLandscapeGenericYMargin-height,iMySize.iWidth-KLandscapeGenericXMargin,iProgressBar.iTl.iY-KLandscapeGenericYMargin-textHeight-KLandscapeGenericYMargin);
	ComputeVolumeSize(iMySize.iWidth<=320?KVolumeSizeMedium:KVolumeSizeBig);
	iFlags|=EShowVolumeMode;
	
	//the other labels, if we still have space
	TInt remainingWidth(aSize.iWidth-albumArtSize);
	if(remainingWidth>iFont->TextWidthInPixels(KIndexInPlaylist) && textHeight<=albumArtSize) //TODO: we need something else than KIndexInPlaylist here
	{
		iPositionInPlaylist=new (ELeave) CEikLabel;
		iPositionInPlaylist->SetTextL(KEmpty);
		iPositionInPlaylist->SetLabelAlignment(ELayoutAlignRight);
		iPositionInPlaylist->SetFont(iFont);
	};

	if(remainingWidth>iFont->TextWidthInPixels(KAlbumYearStandalone) && //TODO: we need something else than KAlbumYearStandalone
			((iPositionInPlaylist && textHeight<=(albumArtSize>>1)) || (!iPositionInPlaylist && textHeight<=albumArtSize))) 
	{
		iAlbumYear=new (ELeave) CEikLabel;
		iAlbumYear->SetTextL(KEmpty);
		iAlbumYear->SetLabelAlignment(ELayoutAlignCenter);
		iAlbumYear->SetFont(iFont);	
	};
	
	//get colors (at the end, so we set colors for labels as well)
	SetColorScheme(iPreferences->iMusicPlayerThemeData);
}


CThemeLandscapeGeneric::~CThemeLandscapeGeneric()
{
	//delete the labels
	delete iLabelElapsedTime;
	delete iTotalTime;
	delete iPositionInPlaylist;
	delete iAlbumYear;
}


void CThemeLandscapeGeneric::SetContainer(CMusicPlayerContainer *aContainer)
{
	CTheme::SetContainer(aContainer);
	iLabelElapsedTime->SetContainerWindowL(*(CCoeControl*)aContainer);
	iTotalTime->SetContainerWindowL(*(CCoeControl*)aContainer);
	if(iPositionInPlaylist)
		iPositionInPlaylist->SetContainerWindowL(*(CCoeControl*)aContainer);
	if(iAlbumYear)
		iAlbumYear->SetContainerWindowL(*(CCoeControl*)aContainer);
}

TInt CThemeLandscapeGeneric::CountComponentControls()
{
	TInt count(2);
	if(iPositionInPlaylist)count++;
	if(iAlbumYear)count++;
	return count;
}

CCoeControl* CThemeLandscapeGeneric::ComponentControl( TInt aIndex )
{
	switch(aIndex)
	{
	case 0:return iLabelElapsedTime;
	case 1:return iTotalTime;
	case 2:if(iPositionInPlaylist)return iPositionInPlaylist;
	       else return iAlbumYear;
	case 3:return iAlbumYear;
	};
	return NULL;
}

void CThemeLandscapeGeneric::SetMetadataL(CMetadata &aMetadata,TTimeIntervalMicroSeconds &aCurrentPosition)
{
	//set artist/title string & current position
	CColoredTheme::SetMetadataL(aMetadata,aCurrentPosition);
	
	//set time stuff
	TRect rect;
	TBuf<25> time;
	FormatTimeString(time,iManager->iFormerPosition,iDuration);
	iLabelElapsedTime->SetTextL(time);

	//set total duration
	FormatTimeString(time,iDuration,iDuration);
	iTotalTime->SetTextL(time);
	//set iTimeLabelWidth
	iTimeLabelWidth = iTotalTime->MinimumSize().iWidth;
	//set progress bar size
	iProgressBar.iTl.iX=iTimeLabelWidth+KLandscapeGenericXMargin;
	iProgressBar.iBr.iX=iMySize.iWidth-iTimeLabelWidth-KLandscapeGenericXMargin;
	
	//set index in playlist
	TInt textHeight((KLandscapeGenericYMargin<<2)+iFont->HeightInPixels());
	if(iPositionInPlaylist)
	{
		time.Format(KIndexInPlaylist,iManager->iView->iPlaylist->GetCurrentIndex()+1,iManager->iView->iPlaylist->Count());
		iPositionInPlaylist->SetTextL(time);
		//rect.SetRect(iAlbumArtSize.iWidth+KLandscapeGenericXMargin,textHeight,iMySize.iWidth,textHeight<<1);
		rect.SetRect(iAlbumArtSize.iWidth+KLandscapeGenericXMargin,iProgressBar.iTl.iY-textHeight,iMySize.iWidth,iProgressBar.iTl.iY-(KLandscapeGenericYMargin<<1));
		iPositionInPlaylist->SetRect(rect);
	};
	
	if(iAlbumYear)
	{
		if(aMetadata.iAlbum && aMetadata.iAlbum->Length()>0)
			if(aMetadata.iYear>0 && aMetadata.iYear<3000) //I don't believe anyone will be using this sw in a thousand years
			{
				//we have both album and year
				HBufC *album=HBufC::NewL(aMetadata.iAlbum->Length()+8);
				album->Des().Copy(*aMetadata.iAlbum);
				time.Format(KAlbumYear,aMetadata.iYear);
				album->Des().Append(time);
				iAlbumYear->SetTextL(*album);
				delete album;
			}
			else
				iAlbumYear->SetTextL(*aMetadata.iAlbum); //only album name is available
		else iAlbumYear->SetTextL(KEmpty);
		
		//label 4: album [year]
		rect.SetRect(iAlbumArtSize.iWidth+KLandscapeGenericXMargin,textHeight<<1,iMySize.iWidth,textHeight*3);
		iAlbumYear->SetRect(rect);
	};

	//set labels position on the screen
	TSize labelSize;
	//label 1: elapsed time
	labelSize = iLabelElapsedTime->MinimumSize();
	labelSize.iWidth=iTimeLabelWidth;
	rect.iTl.iX=0;
	rect.iBr.iX=iTimeLabelWidth;
	rect.iBr.iY=iMySize.iHeight-KLandscapeGenericYMargin+1;
	rect.iTl.iY=rect.iBr.iY-labelSize.iHeight;
	//rect.iTl.iY=iProgressBar.iTl.iY-3;
	//rect.iBr = rect.iTl + labelSize;
	iLabelElapsedTime->SetRect(rect);

	//label 2: total time
	labelSize = iTotalTime->MinimumSize();
	rect.iTl.iX=iMySize.iWidth-iTimeLabelWidth;
	rect.iBr = rect.iTl + labelSize;
	iTotalTime->SetRect(rect);
}

TBool CThemeLandscapeGeneric::UpdatePlaybackPosition(TTimeIntervalMicroSeconds &aCurrentPosition, TBool aDraw/*=ETrue*/) //returns ETrue if the progress bar was updated
{
	TInt playbackSeconds(aCurrentPosition.Int64()/1000000);

	if(playbackSeconds!=iManager->iFormerPosition)
	{
		//set elapsed time to current position
		iManager->iFormerPosition=playbackSeconds;
		//update current position label
		TBuf<10> time;
		FormatTimeString(time,playbackSeconds,iDuration);
		iLabelElapsedTime->SetTextL(time);

		if(aDraw)
		{
			iContainer->DrawNow(iProgressBar);
			iContainer->DrawNow(iLabelElapsedTime->Rect());
			//iLabelElapsedTime->DrawNow();
		};
		return ETrue;
	};
	return EFalse;
}

void CThemeLandscapeGeneric::PeriodicUpdate(TTimeIntervalMicroSeconds &aCurrentPosition)
{
	//CColoredTheme::PeriodicUpdate(aCurrentPosition);
	UpdatePlaybackPosition(aCurrentPosition);
}

void CThemeLandscapeGeneric::SetColorScheme(TUint aColors, TBool aRedraw/*=ETrue*/)
{
	CColoredTheme::SetColorScheme(aColors,EFalse);
	if(iLabelElapsedTime)
		iLabelElapsedTime->OverrideColorL(EColorLabelText,iColorText);
	if(iTotalTime)
		iTotalTime->OverrideColorL(EColorLabelText,iColorText);
	if(iPositionInPlaylist)
		iPositionInPlaylist->OverrideColorL(EColorLabelText,iColorText);
	if(iAlbumYear)
		iAlbumYear->OverrideColorL(EColorLabelText,iColorText);
	
	if(iContainer && aRedraw)
		iContainer->DrawDeferred();
}

void CThemeLandscapeGeneric::UpdateVolumeOnScreen()
{
	iContainer->DrawNow(iVolumeRect);
}
