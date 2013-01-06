/*
============================================================================
 Name        : MLauncherPreferences.cpp
 Author      : Florin Lohan
 Copyright   : Florin Lohan
 Description : Preferences, singleton class
============================================================================
*/
#include <hal.h>
#include <eikenv.h>
#include <s32file.h>
#include <apgcli.h>
#include <utf.h>
#include <aknutils.h>

//#include <centralrepository.h> //to check for MSK
//#include <avkoninternalcrkeys.h>  // KAknMiddleSoftkeyEnabled
#include "MLauncherPreferences.h"
#include "XMLparsing.h"
#include "TrackInfo.h"
#include "MLauncher.pan"
#include "defs.h"
#include "common_cpp_defs.h"
#include "log.h"

const TInt KPreferencesVersion=2;
const TInt KPreferencesLineLength=512;
_LIT(KDefaultRecvFilesDir, "___ReceivedMusic"); //all dirs starting with 3 underscores are not shown in the list

_LIT(KOurPrivateFolder,"C:\\private\\E388D98A\\");

_LIT( KPreferencesFileNameOld, "Preferences.bin" );
_LIT( KPreferencesFileName, "Preferences.xml" );
_LIT( KVersionDescriptionFile, "version.xml" );
_LIT8(KXmlStart, "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\r\n");
_LIT8(KXmlPreferencesStart,"<preferences>\r\n");
_LIT8(KXmlPreferencesEnd,"</preferences>\r\n");
_LIT8(KYes8, "yes");
_LIT8(KNo8, "no");

const TInt KCrossfadingVolumeRampMs=5000; //in miliseconds
const TInt KCrossfadingTimeMs=10000; //in miliseconds

const TUid KFmTxAppUid={0x10282BEF};
//const TUid KOggPlayControllerUid={0xF01FF683};
_LIT(KOggPlayControllerRSCPath,"C:\\Resource\\Plugins\\OggPlayController.RSC");

_LIT(KAudioBookName,"audiobook");

CMLauncherPreferences* CMLauncherPreferences::NewL(RFs &aFs, RPointerArray<CPlaylist> *aPlaylists)
{
	CMLauncherPreferences* self = new (ELeave) CMLauncherPreferences();
	CleanupStack::PushL(self);
	self->ConstructL(aFs,aPlaylists);
	CleanupStack::Pop(self);
	return self;
}

CMLauncherPreferences::CMLauncherPreferences() : iPFlags(0),iCFlags(0),iVolume(-1),iAudioEqualizerPreset(-1)
{
}

void CMLauncherPreferences::SetPrivatePath(TChar aDrive)
{
	iPrivatePath.Copy(KOurPrivateFolder);
	iPrivatePath[0]=aDrive;
}

TDesC& CMLauncherPreferences::PrivatePathPlus(const TDesC& aStr)
{
	iPrivatePath.SetLength(KOurPrivateFolder().Length());
	iPrivatePath.Append(aStr);
	return iPrivatePath;
}

void CMLauncherPreferences::ConstructL(RFs &aFs, RPointerArray<CPlaylist> *aPlaylists)
{
	TInt err;
	RApaLsSession ls;
	TApaAppInfo info;
	iFs=&aFs;
	iPlaylists=aPlaylists;

	__ASSERT_DEBUG(iFs,Panic(EiFsIsNull));

	User::LeaveIfError(ls.Connect());
	ls.GetAllApps();
	err=ls.GetAppInfo(info, TUid::Uid(0x102072c3)); //this is the UID of the S60 Music Player AND the UID of the mpx music player (replaces the S60 Music Player in some Nokia devices)
	if(!err)
	{
		//all functions were successful up to here!
	  	LOG0("app name: %S",&info.iFullName)
	   	if(info.iFullName.Find(_L("mpx.exe"))!=KErrNotFound)
	   	{
	   		iPFlags|=EPreferencesLaunchEmbedded;
	   		LOG0("mpx player found, will launch it embedded by default");
	   	};
	};
	//get FM Transmitter info, if we have an opened session
	err=ls.GetAppInfo(info,KFmTxAppUid);
	if(err)
	{
		iCFlags&=~EHwHasFmTx;
		LOG0("Device does NOT have FM Transmitter");
	}
	else
	{
		iCFlags|=EHwHasFmTx;
		LOG0("Device has FM Transmitter");
	};
	/* this does not always work, because at least in Belle, the Controller is not an app (exe), so it is not found
	err=ls.GetAppInfo(info,KOggPlayControllerUid);
	if(err)
	{
		TFileDirEntry::iStaticFlags=0;
		LOG0("The OggPlayController is NOT installed (err=%d), support for Ogg and FLAC deactivated",err);
	}
	else
	{
		TFileDirEntry::iStaticFlags=TFileDirEntry::ESupportsOggAndFlac;
		LOG0("The OggPlayController is installed, Ogg and FLAC supported");
	};
	*/
	ls.Close();
	TEntry entry;
	TChar drives[]={'C','E','F'};
	TBool controllerFound(EFalse);
	TFileName path(KOggPlayControllerRSCPath);
	TUint i;
	for(i=0;i<sizeof(drives)/sizeof(TChar);i++)
	{
		path[0]=drives[i];
		if(iFs->Entry(path,entry)==KErrNone)
		{
			controllerFound=ETrue;
			break;
		}
	}
	if(controllerFound)
	{
		TFileDirEntry::iStaticFlags=TFileDirEntry::ESupportsOggAndFlac;
		LOG0("The OggPlayController is installed, Ogg and FLAC supported");
	}
	else
	{
		TFileDirEntry::iStaticFlags=0;
		LOG0("The OggPlayController is NOT installed, support for Ogg and FLAC deactivated");
	}
		

	// Get phone UID. Tweak some of the preferences based on the phone UID.
	// You can get product UIDs from:
	// http://www.newlc.com/Common-products-UIDs.html
	// or from forum.nokia.com
	TInt halData;
	HAL::Get(HALData::EMachineUid,halData);
	switch(halData)
	{
	// 3rd ed, HW1
	case 0x200005FC: //Nokia N91, N91 8GB
	{
		iCFlags|=EPreferencesLaunchEmbedded;
		iCFlags|=EHwDeviceHasVolumeKeys;
		//iCFlags|=EHwDeviceIsSlow;
		//iCFlags|=EHwDeviceHasMSK;
		//iCFlags|=EHwDoNotUseDSA;
		iCFlags|=EHwDoNotUseCrossfading;
		//iCFlags|=EHwCanDoLandscapeWithoutTitlebar;
	};break;
	case 0x200005FA: //Nokia N92
	{
		//iPFlags|=EPreferencesLaunchEmbedded;
		iCFlags|=EHwDeviceHasVolumeKeys;
		//iCFlags|=EHwDeviceIsSlow;
		//iCFlags|=EHwDeviceHasMSK;
		//iCFlags|=EHwDoNotUseDSA;
		iCFlags|=EHwDoNotUseCrossfading;
		//iCFlags|=EHwCanDoLandscapeWithoutTitlebar;
	};break;
	case 0x200005F9: //Nokia N80
	{
		iPFlags|=EPreferencesLaunchEmbedded;
		//iCFlags|=EHwDeviceHasVolumeKeys;
		//iCFlags|=EHwDeviceIsSlow;
		//iCFlags|=EHwDeviceHasMSK;
		//iCFlags|=EHwDoNotUseDSA;
		iCFlags|=EHwDoNotUseCrossfading;
		//iCFlags|=EHwCanDoLandscapeWithoutTitlebar;
	};break;
	case 0x20001856: //Nokia E60
	{
		//iPFlags|=EPreferencesLaunchEmbedded;
		iCFlags|=EHwDeviceHasVolumeKeys;
		//iCFlags|=EHwDeviceIsSlow;
		//iCFlags|=EHwDeviceHasMSK;
		//iCFlags|=EHwDoNotUseDSA;
		iCFlags|=EHwDoNotUseCrossfading;
		//iCFlags|=EHwCanDoLandscapeWithoutTitlebar;
	};break;
	case 0x20001858: //Nokia E61
	{
		//iPFlags|=EPreferencesLaunchEmbedded;
		iCFlags|=EHwDeviceHasVolumeKeys;
		//iCFlags|=EHwDeviceIsSlow;
		//iCFlags|=EHwDeviceHasMSK;
		//iCFlags|=EHwDoNotUseDSA;
		iCFlags|=EHwDoNotUseCrossfading;
		iCFlags|=EHwCanDoLandscapeWithoutTitlebar;
	};break;
	case 0x20002D7F: //Nokia E61i
	{
		//iPFlags|=EPreferencesLaunchEmbedded;
		iCFlags|=EHwDeviceHasVolumeKeys;
		//iCFlags|=EHwDeviceIsSlow;
		//iCFlags|=EHwDeviceHasMSK;
		//iCFlags|=EHwDoNotUseDSA;
		iCFlags|=EHwDoNotUseCrossfading;
		iCFlags|=EHwCanDoLandscapeWithoutTitlebar;
	};break;
	case 0x200005FB: //Nokia N73
	{
		//iPFlags|=EPreferencesLaunchEmbedded;
		iCFlags|=EHwDeviceHasVolumeKeys;
		//iCFlags|=EHwDeviceIsSlow;
		//iCFlags|=EHwDeviceHasMSK;
		//iCFlags|=EHwDoNotUseDSA;
		iCFlags|=EHwDoNotUseCrossfading;
		//iCFlags|=EHwCanDoLandscapeWithoutTitlebar;
	};break;
	case 0x20000604: //Nokia E65
	{
		//iPFlags|=EPreferencesLaunchEmbedded;
		iCFlags|=EHwDeviceHasVolumeKeys;
		//iCFlags|=EHwDeviceIsSlow;
		//iCFlags|=EHwDeviceHasMSK;
		//iCFlags|=EHwDoNotUseDSA;
		iCFlags|=EHwDoNotUseCrossfading;
		//iCFlags|=EHwCanDoLandscapeWithoutTitlebar;
	};break;
	case 0x20000601: //Nokia N77
	{
		//iPFlags|=EPreferencesLaunchEmbedded;
		iCFlags|=EHwDeviceHasVolumeKeys;
		//iCFlags|=EHwDeviceIsSlow;
		//iCFlags|=EHwDeviceHasMSK;
		//iCFlags|=EHwDoNotUseDSA;
		iCFlags|=EHwDoNotUseCrossfading;
		//iCFlags|=EHwCanDoLandscapeWithoutTitlebar;
	};break;
	case 0x200005FE: //Nokia N75
	{
		//iPFlags|=EPreferencesLaunchEmbedded;
		iCFlags|=EHwDeviceHasVolumeKeys;
		//iCFlags|=EHwDeviceIsSlow;
		//iCFlags|=EHwDeviceHasMSK;
		//iCFlags|=EHwDoNotUseDSA;
		iCFlags|=EHwDoNotUseCrossfading;
		//iCFlags|=EHwCanDoLandscapeWithoutTitlebar;
	};break;
	case 0x20001857: //Nokia E70
	{
		//iPFlags|=EPreferencesLaunchEmbedded;
		//iCFlags|=EHwDeviceHasVolumeKeys;
		//iCFlags|=EHwDeviceIsSlow;
		//iCFlags|=EHwDeviceHasMSK;
		//iCFlags|=EHwDoNotUseDSA;
		iCFlags|=EHwDoNotUseCrossfading;
		//iCFlags|=EHwCanDoLandscapeWithoutTitlebar;
	};break;
	case 0x200005FF: //Nokia N71
	{
		//iPFlags|=EPreferencesLaunchEmbedded;
		//iCFlags|=EHwDeviceHasVolumeKeys;
		//iCFlags|=EHwDeviceIsSlow;
		//iCFlags|=EHwDeviceHasMSK;
		//iCFlags|=EHwDoNotUseDSA;
		iCFlags|=EHwDoNotUseCrossfading;
		//iCFlags|=EHwCanDoLandscapeWithoutTitlebar;
	};break;
	
	//3rd ed, HW2
	case 0x20000600: //Nokia N93
	{
		iPFlags|=EPreferencesLaunchEmbedded;
		//iCFlags|=EHwDeviceHasVolumeKeys;
		//iCFlags|=EHwDeviceIsSlow;
		//iCFlags|=EHwDeviceHasMSK;
		//iCFlags|=EHwDoNotUseDSA;
		iCFlags|=EHwDoNotUseCrossfading;
		//iCFlags|=EHwCanDoLandscapeWithoutTitlebar;
	};break;
	case 0x20000605: //Nokia N93i
	{
		iPFlags|=EPreferencesLaunchEmbedded;
		//iCFlags|=EHwDeviceHasVolumeKeys;
		//iCFlags|=EHwDeviceIsSlow;
		//iCFlags|=EHwDeviceHasMSK;
		//iCFlags|=EHwDoNotUseDSA;
		iCFlags|=EHwDoNotUseCrossfading;
		//iCFlags|=EHwCanDoLandscapeWithoutTitlebar;
	};break;
	
	//3rd ed, HW3
	case 0x200005F8: //Nokia 3250
	{
		//iPFlags|=EPreferencesLaunchEmbedded;
		iCFlags|=EHwDeviceHasVolumeKeys;
		//iCFlags|=EHwDeviceIsSlow;
		//iCFlags|=EHwDeviceHasMSK;
		//iCFlags|=EHwDoNotUseDSA;
		//iCFlags|=EHwDoNotUseCrossfading;
		//iCFlags|=EHwCanDoLandscapeWithoutTitlebar;
	};break;
	case 0x20000602: //Nokia 5500
	{
		//iPFlags|=EPreferencesLaunchEmbedded;
		iCFlags|=EHwDeviceHasVolumeKeys;
		//iCFlags|=EHwDeviceIsSlow;
		//iCFlags|=EHwDeviceHasMSK;
		//iCFlags|=EHwDoNotUseDSA;
		//iCFlags|=EHwDoNotUseCrossfading;
		//iCFlags|=EHwCanDoLandscapeWithoutTitlebar;
	};break;
	case 0x20001859: //Nokia E62
	{
		//iPFlags|=EPreferencesLaunchEmbedded;
		iCFlags|=EHwDeviceHasVolumeKeys;
		//iCFlags|=EHwDeviceIsSlow;
		//iCFlags|=EHwDeviceHasMSK;
		//iCFlags|=EHwDoNotUseDSA;
		//iCFlags|=EHwDoNotUseCrossfading;
		iCFlags|=EHwCanDoLandscapeWithoutTitlebar;
	};break;
	case 0x20002495: //Nokia E50
	{
		//iPFlags|=EPreferencesLaunchEmbedded;
		iCFlags|=EHwDeviceHasVolumeKeys;
		//iCFlags|=EHwDeviceIsSlow;
		//iCFlags|=EHwDeviceHasMSK;
		//iCFlags|=EHwDoNotUseDSA;
		//iCFlags|=EHwDoNotUseCrossfading;
		//iCFlags|=EHwCanDoLandscapeWithoutTitlebar;
	};break;
	
	//////////////
	// 3rd ed, FP1, HW1
	case 0x2000060A: //Nokia N76
	{
		iPFlags|=EPreferencesLaunchEmbedded;
		iCFlags|=EHwDeviceHasVolumeKeys;
		//iCFlags|=EHwDeviceIsSlow;
		//iCFlags|=EHwDeviceHasMSK;
		//iCFlags|=EHwDoNotUseDSA;
		//iCFlags|=EHwDoNotUseCrossfading;
		//iCFlags|=EHwCanDoLandscapeWithoutTitlebar;
	};break;
	case 0x20000606: //Nokia 6290
	{
		//iPFlags|=EPreferencesLaunchEmbedded;
		iCFlags|=EHwDeviceHasVolumeKeys;
		//iCFlags|=EHwDeviceIsSlow;
		//iCFlags|=EHwDeviceHasMSK;
		//iCFlags|=EHwDoNotUseDSA;
		//iCFlags|=EHwDoNotUseCrossfading;
		//iCFlags|=EHwCanDoLandscapeWithoutTitlebar;
	};break;
	case 0x20002D7B: //Nokia 6110n
	{
		//iPFlags|=EPreferencesLaunchEmbedded;
		iCFlags|=EHwDeviceHasVolumeKeys;
		//iCFlags|=EHwDeviceIsSlow;
		//iCFlags|=EHwDeviceHasMSK;
		//iCFlags|=EHwDoNotUseDSA;
		//iCFlags|=EHwDoNotUseCrossfading;
		//iCFlags|=EHwCanDoLandscapeWithoutTitlebar;
	};break;
	case 0x20002D7C: //Nokia 5700
	{
		//iPFlags|=EPreferencesLaunchEmbedded;
		iCFlags|=EHwDeviceHasVolumeKeys;
		//iCFlags|=EHwDeviceIsSlow;
		//iCFlags|=EHwDeviceHasMSK;
		//iCFlags|=EHwDoNotUseDSA;
		//iCFlags|=EHwDoNotUseCrossfading;
		//iCFlags|=EHwCanDoLandscapeWithoutTitlebar;
	};break;
	case 0x20002D7E: //Nokia 6120c, 6121c
	{
		//iPFlags|=EPreferencesLaunchEmbedded;
		iCFlags|=EHwDeviceHasVolumeKeys;
		//iCFlags|=EHwDeviceIsSlow;
		//iCFlags|=EHwDeviceHasMSK;
		//iCFlags|=EHwDoNotUseDSA;
		//iCFlags|=EHwDoNotUseCrossfading;
		//iCFlags|=EHwCanDoLandscapeWithoutTitlebar;
	};break;
	case 0x2000DA55: //Nokia 6124c
	{
		//iPFlags|=EPreferencesLaunchEmbedded;
		iCFlags|=EHwDeviceHasVolumeKeys;
		//iCFlags|=EHwDeviceIsSlow;
		//iCFlags|=EHwDeviceHasMSK;
		//iCFlags|=EHwDoNotUseDSA;
		//iCFlags|=EHwDoNotUseCrossfading;
		//iCFlags|=EHwCanDoLandscapeWithoutTitlebar;
	};break;
	case 0x20002D83: //Nokia N81 (& N81 8GB)
	{
		//iPFlags|=EPreferencesLaunchEmbedded;
		iCFlags|=EHwDeviceHasVolumeKeys;
		//iCFlags|=EHwDeviceIsSlow;
		//iCFlags|=EHwDeviceHasMSK;
		//iCFlags|=EHwDoNotUseDSA;
		//iCFlags|=EHwDoNotUseCrossfading;
		//iCFlags|=EHwCanDoLandscapeWithoutTitlebar;
	};break;
	case 0x20002498: //Nokia E51	
	{
		//iPFlags|=EPreferencesLaunchEmbedded;
		iCFlags|=EHwDeviceHasVolumeKeys;
		//iCFlags|=EHwDeviceIsSlow;
		//iCFlags|=EHwDeviceHasMSK;
		//iCFlags|=EHwDoNotUseDSA;
		//iCFlags|=EHwDoNotUseCrossfading;
		//iCFlags|=EHwCanDoLandscapeWithoutTitlebar;
	};break;
	case 0x2000249b: //Nokia E71
	{
		//iPFlags|=EPreferencesLaunchEmbedded;
		iCFlags|=EHwDeviceHasVolumeKeys;
		//iCFlags|=EHwDeviceIsSlow;
		//iCFlags|=EHwDeviceHasMSK;
		//iCFlags|=EHwDoNotUseDSA;
		//iCFlags|=EHwDoNotUseCrossfading;
		iCFlags|=EHwCanDoLandscapeWithoutTitlebar;
	};break;
	case 0x2000249C: //Nokia E66
	{
		//iPFlags|=EPreferencesLaunchEmbedded;
		iCFlags|=EHwDeviceHasVolumeKeys;
		//iCFlags|=EHwDeviceIsSlow;
		//iCFlags|=EHwDeviceHasMSK;
		//iCFlags|=EHwDoNotUseDSA;
		//iCFlags|=EHwDoNotUseCrossfading;
		//iCFlags|=EHwCanDoLandscapeWithoutTitlebar;
	};break;
	case 0x200025C3: //Nokia E63
	{
		//iPFlags|=EPreferencesLaunchEmbedded;
		//iCFlags|=EHwDeviceHasVolumeKeys;
		//iCFlags|=EHwDeviceIsSlow;
		//iCFlags|=EHwDeviceHasMSK;
		//iCFlags|=EHwDoNotUseDSA;
		//iCFlags|=EHwDoNotUseCrossfading;
		iCFlags|=EHwCanDoLandscapeWithoutTitlebar;
	};break;
		
	
	// 3rd ed, FP1, HW2 (N95, N95 8GB, E90, N82)
	case 0x2000060b: //Nokia N95
	{
		iPFlags|=EPreferencesLaunchEmbedded;
		iCFlags|=EHwDeviceHasVolumeKeys;
		//iCFlags|=EHwDeviceIsSlow;
		//iCFlags|=EHwDeviceHasMSK;
		//iCFlags|=EHwDoNotUseDSA;
		iCFlags|=EHwDoNotUseCrossfading;
		//iCFlags|=EHwCanDoLandscapeWithoutTitlebar;
	};break;
	case 0x20002D84: //Nokia N95 8GB
	{
		iPFlags|=EPreferencesLaunchEmbedded;
		iCFlags|=EHwDeviceHasVolumeKeys;
		//iCFlags|=EHwDeviceIsSlow;
		//iCFlags|=EHwDeviceHasMSK;
		//iCFlags|=EHwDoNotUseDSA;
		iCFlags|=EHwDoNotUseCrossfading;
		//iCFlags|=EHwCanDoLandscapeWithoutTitlebar;
	};break;
	case 0x20002D85: //Nokia N82
	{
		//iPFlags|=EPreferencesLaunchEmbedded;
		iCFlags|=EHwDeviceHasVolumeKeys;
		//iCFlags|=EHwDeviceIsSlow;
		//iCFlags|=EHwDeviceHasMSK;
		//iCFlags|=EHwDoNotUseDSA;
		iCFlags|=EHwDoNotUseCrossfading;
		//iCFlags|=EHwCanDoLandscapeWithoutTitlebar;
	};break;
	case 0x20002496 : //Nokia E90
	{
		//iPFlags|=EPreferencesLaunchEmbedded;
		//iCFlags|=EHwDeviceHasVolumeKeys;
		//iCFlags|=EHwDeviceIsSlow;
		//iCFlags|=EHwDeviceHasMSK;
		//iCFlags|=EHwDoNotUseDSA;
		iCFlags|=EHwDoNotUseCrossfading;
		//iCFlags|=EHwCanDoLandscapeWithoutTitlebar;
	};break;
	
	//////////////
	// 3rd ed, FP1, HW1
	case 0x20002D82: //Nokia N96
	{
		//iPFlags|=EPreferencesLaunchEmbedded;
		iCFlags|=EHwDeviceHasVolumeKeys;
		//iCFlags|=EHwDeviceIsSlow;
		iCFlags|=EHwDeviceHasMSK;
		//iCFlags|=EHwDoNotUseDSA;
		//iCFlags|=EHwDoNotUseCrossfading;
		//iCFlags|=EHwCanDoLandscapeWithoutTitlebar;
	};break;

	// 3rd ed, FP1, HW2
	case 0x2000DA54: //Nokia 6210n
	{
		//iPFlags|=EPreferencesLaunchEmbedded;
		iCFlags|=EHwDeviceHasVolumeKeys;
		//iCFlags|=EHwDeviceIsSlow;
		iCFlags|=EHwDeviceHasMSK;
		//iCFlags|=EHwDoNotUseDSA;
		//iCFlags|=EHwDoNotUseCrossfading;
		//iCFlags|=EHwCanDoLandscapeWithoutTitlebar;
	};break;
	case 0x20002D81: //Nokia N78
	{
		//iPFlags|=EPreferencesLaunchEmbedded;
		iCFlags|=EHwDeviceHasVolumeKeys;
		//iCFlags|=EHwDeviceIsSlow;
		iCFlags|=EHwDeviceHasMSK;
		//iCFlags|=EHwDoNotUseDSA;
		//iCFlags|=EHwDoNotUseCrossfading;
		//iCFlags|=EHwCanDoLandscapeWithoutTitlebar;
	};break;
	case 0x2000DA5A: //Nokia 5320
	{
		//iPFlags|=EPreferencesLaunchEmbedded;
		iCFlags|=EHwDeviceHasVolumeKeys;
		//iCFlags|=EHwDeviceIsSlow;
		iCFlags|=EHwDeviceHasMSK;
		//iCFlags|=EHwDoNotUseDSA;
		//iCFlags|=EHwDoNotUseCrossfading;
		//iCFlags|=EHwCanDoLandscapeWithoutTitlebar;
	};break;
	case 0x2000DA57: //Nokia 6650
	{
		//iPFlags|=EPreferencesLaunchEmbedded;
		iCFlags|=EHwDeviceHasVolumeKeys;
		//iCFlags|=EHwDeviceIsSlow;
		iCFlags|=EHwDeviceHasMSK;
		//iCFlags|=EHwDoNotUseDSA;
		//iCFlags|=EHwDoNotUseCrossfading;
		//iCFlags|=EHwCanDoLandscapeWithoutTitlebar;
	};break;


	// 3rd ed, FP1, HW3
	case 0x20002D86: //Nokia N85
	{
		//iPFlags|=EPreferencesLaunchEmbedded;
		iCFlags|=EHwDeviceHasVolumeKeys;
		//iCFlags|=EHwDeviceIsSlow;
		iCFlags|=EHwDeviceHasMSK;
		//iCFlags|=EHwDoNotUseDSA;
		//iCFlags|=EHwDoNotUseCrossfading;
		//iCFlags|=EHwCanDoLandscapeWithoutTitlebar;
	};break;
	case 0x2000DA52: //Nokia 6220c
	{
		//iPFlags|=EPreferencesLaunchEmbedded;
		iCFlags|=EHwDeviceHasVolumeKeys;
		//iCFlags|=EHwDeviceIsSlow;
		iCFlags|=EHwDeviceHasMSK;
		//iCFlags|=EHwDoNotUseDSA;
		//iCFlags|=EHwDoNotUseCrossfading;
		//iCFlags|=EHwCanDoLandscapeWithoutTitlebar;
	};break;
	case 0x2000DA64: //Nokia N79
	{
		//iPFlags|=EPreferencesLaunchEmbedded;
		iCFlags|=EHwDeviceHasVolumeKeys;
		//iCFlags|=EHwDeviceIsSlow;
		iCFlags|=EHwDeviceHasMSK;
		//iCFlags|=EHwDoNotUseDSA;
		//iCFlags|=EHwDoNotUseCrossfading;
		//iCFlags|=EHwCanDoLandscapeWithoutTitlebar;
	};break;

	//////////////
	// 5th ed, HW1
	case 0x2000DA56 : //Nokia 5800
	{
		//iPFlags|=EPreferencesLaunchEmbedded;
		iCFlags|=EHwDeviceHasVolumeKeys;
		//iCFlags|=EHwDeviceIsSlow;
		//iCFlags|=EHwDeviceHasMSK;
		//iCFlags|=EHwDoNotUseDSA;
		//iCFlags|=EHwDoNotUseCrossfading;
		iCFlags|=EHwCanDoLandscapeWithoutTitlebar;
	};break;

	default: //we suppose it is a powerful device
	{
		//iPFlags|=EPreferencesLaunchEmbedded;
		iCFlags|=EHwDeviceHasVolumeKeys;
		//iCFlags|=EHwDeviceIsSlow;
		//iCFlags|=EHwDeviceHasMSK;
		//iCFlags|=EHwDoNotUseDSA;
		//iCFlags|=EHwDoNotUseCrossfading;
		iCFlags|=EHwCanDoLandscapeWithoutTitlebar;
	};
	};//switch
	
#ifdef __WINS__
	iCFlags|=EHwDeviceIsSlow;
#endif
	
	//check for touch support
#ifdef _FLL_SDK_VERSION_50_
	if(AknLayoutUtils::PenEnabled())
		iCFlags|=EHwTouchDevice;
	else 
		iCFlags&=~EHwTouchDevice;
	
	//get platform version
	iPlatformVersionMajor=iPlatformVersionMinor=0; //in case we are unable to get the platform version
	_LIT(KS60ProductIDFile, "Series60v*.sis");
	_LIT(KROMInstallDir, "z:\\system\\install\\");
	TFindFile ff(*iFs);
	CDir* result;
	err=ff.FindWildByDir(KS60ProductIDFile,KROMInstallDir,result);
	if(!err)
	{
		err=result->Sort(ESortByName|EDescending);
		if(!err)
		{
			iPlatformVersionMajor=(*result)[0].iName[9]-'0';
			iPlatformVersionMinor=(*result)[0].iName[11]-'0';
		}
	    delete result;
	}
	
	//use the platform version to get the max volume
	if(iPlatformVersionMajor<5)iMaxVolume=10; //this should not happen, because we are doing this check for touch devices only
	else if(iPlatformVersionMajor==5)
	{
		if(iPlatformVersionMinor>=2) //S^3 and Anna
			iMaxVolume=20;
		else iMaxVolume=10;
	}
	else iMaxVolume=20; //iPlatformVersionMajor>=6
	LOG0("Detected platform version: %d.%d, maxVolume set to %d",iPlatformVersionMajor,iPlatformVersionMinor,iMaxVolume);
#else
	iCFlags&=~EHwTouchDevice;
	iMaxVolume=10;
#endif
	
	//log
	if(iPFlags&EPreferencesLaunchEmbedded)
	{
	    LOG0("Phone UID: %x, by default the player is launched embedded",halData);
	    //LOG("By default the player is launched embedded");
	}
	else
	{
	  	LOG0("Phone UID: %x, by default the player is launched standalone (not embedded)",halData);
	  	//LOG("By default the player is launched standalone (not embedded)");
	};
	
	//other flags, these can be reset by preferences
	iPFlags|=(EPreferencesUseInternalMusicPlayer|EPreferencesUsePartialCheck);
	if(!(iCFlags&EHwDoNotUseCrossfading))
	{
		iPFlags|=EPreferencesUseCrossfading; //if we support crossfading, use it by default
		iCrossfadingTimeMs=KCrossfadingTimeMs;
		iCrossfadingTimeVolRampUpMs=KCrossfadingVolumeRampMs;
	};
	iCFlags|=EHwDoNotUseDSA; //we will remove this when we make DSA to work
}

CMLauncherPreferences::~CMLauncherPreferences()
{
	if(iCFlags&ESavePreferences)
		SavePreferences();
	iStartingDirs.ResetAndDestroy();
	iBlacklistDirs.ResetAndDestroy();
	iStartingDirsNrFiles.Close();
	iBlacklistDirsNrFiles.Close();
	//iPlaylistMetadata.ResetAndDestroy();
}

void CMLauncherPreferences::DeletePreferences()
{
	iFs->Delete(PrivatePathPlus(KPreferencesFileName));
	iFs->Delete(PrivatePathPlus(KPlaylistFileName));
	iFs->Delete(PrivatePathPlus(KPlaylistFileNameBin));
	TBuf<KPlaylistFileNameFormatLength> playlistFilename;
	TInt i;
	for(i=0;i<KMaxNrPlaylists;i++)
	{
		playlistFilename.Format(KPlaylistFileNameFormat,i);
		iFs->Delete(playlistFilename);
	}
	iCFlags&=~ESavePreferences;
}

void CMLauncherPreferences::LoadPreferences()
{
	//check if the log is enabled
	//iFlags|=EPreferencesLogEnabled;

	TInt err,i;
	//check if we have the old preferences
	RFileReadStream frs;
	err = frs.Open(*iFs, PrivatePathPlus(KPreferencesFileNameOld), EFileRead);
	if(err==KErrNone)
	{
		//we have the old preferences file. Read preferences, save them, delete the file.
		TFileName startingDir;
		//read preferences
		frs.PushL();
		frs >> startingDir;
		AddStartingDirL(startingDir,0xFFFFFF);
		//keep selection
		frs.ReadInt32L()?iPFlags|=EPreferencesKeepSelection:iPFlags&=~EPreferencesKeepSelection;
		//launch embedded
		frs.ReadInt32L()?iPFlags|=EPreferencesLaunchEmbedded:iPFlags&=~EPreferencesLaunchEmbedded;
		frs.Pop();
		frs.Release();

		//save them
		SavePreferences();

		//delete the old preferences file
		iFs->Delete(PrivatePathPlus(KPreferencesFileNameOld));
	};

	Xml::CPreferencesCH* pch=Xml::CPreferencesCH::NewLC();
	TRAP(err,pch->ParsePreferencesFileL(PrivatePathPlus(KPreferencesFileName),this));//we may not have a preferences file, and that's OK
	CleanupStack::PopAndDestroy(pch);

	for(i=0;i<iStartingDirs.Count();i++)
	{
		TPtr ptrStartingDir(iStartingDirs[i]->Des());
		RemoveEndingBackslash(ptrStartingDir);
	};
}

void CMLauncherPreferences::SavePreferences()
{
	LOG(ELogGeneral,1,"SavePreferences++");
	TInt err,i;
	iFs->MkDirAll(PrivatePathPlus(KPreferencesFileName));

	RFile prefs;
	err=prefs.Replace(*iFs,PrivatePathPlus(KPreferencesFileName),EFileWrite);
	if(err!=KErrNone)return;//can not create the file, for some reason

	TUint8 *fileBuf=(TUint8*)User::AllocL(KCacheFileBufferSize);
	TPtr8 fileBufPtr(fileBuf,KCacheFileBufferSize);
	
	//write header + start tag
	fileBufPtr.Copy(KXmlStart);
	fileBufPtr.Append(KXmlPreferencesStart);

	//TBuf8<KPreferencesLineLength> line;
	TBuf8<256> dirUtf8;

	//write the version of the preferences
	fileBufPtr.AppendFormat(_L8("<version v=\"%d\"/>\r\n"),KPreferencesVersion);

	//write the starting dirs
	for(i=0;i<iStartingDirs.Count();i++)
	{
		CnvUtfConverter::ConvertFromUnicodeToUtf8(dirUtf8,*iStartingDirs[i]);
		fileBufPtr.AppendFormat(_L8("<startingdir dir=\"%S\" nrfiles=\"%d\"/>\r\n"),&dirUtf8,iStartingDirsNrFiles[i]);
	};

	//write the blacklisted dirs
	for(i=0;i<iBlacklistDirs.Count();i++)
	{
		CnvUtfConverter::ConvertFromUnicodeToUtf8(dirUtf8,*iBlacklistDirs[i]);
		fileBufPtr.AppendFormat(_L8("<blacklisteddir dir=\"%S\" nrfiles=\"%d\"/>\r\n"),&dirUtf8,iBlacklistDirsNrFiles[i]);
	};

	//write flags (only the first 2 bytes)
	fileBufPtr.AppendFormat(_L8("<flags f=\"%b\"/>\r\n"),iPFlags);

	//write some comments what the flags mean
	fileBufPtr.AppendFormat(_L8("<!-- What the flags mean (from right to left):\r\n"));
	
	//bit 1: write "keep selection"
	fileBufPtr.AppendFormat(_L8("  Keep selection : \"%S\" (saves file selection when exiting the player and reloads it next time it is launched)\r\n"),iPFlags&EPreferencesKeepSelection?&KYes8:&KNo8);

	//bit 2: write "launch embedded"
	fileBufPtr.AppendFormat(_L8("  Launch external player embedded: \"%S\"\r\n"),iPFlags&EPreferencesLaunchEmbedded?&KYes8:&KNo8);

	//bit 3: write "use internal music player"
	fileBufPtr.AppendFormat(_L8("  Use internal music player: \"%S\"\r\n"),iPFlags&EPreferencesUseInternalMusicPlayer?&KYes8:&KNo8);
	
	//bit 4: write "use partial check"
	fileBufPtr.AppendFormat(_L8("  Use partial check: \"%S\"\r\n"),iPFlags&EPreferencesUsePartialCheck?&KYes8:&KNo8);
	
	//bit 5: write "Loop through playlist"
	fileBufPtr.AppendFormat(_L8("  Repeat songs in the playlist: \"%S\"\r\n"),iPFlags&EPreferencesPlaylistLoop?&KYes8:&KNo8);

	//bit 6: write "Saved scaled images" - ALWAYS NO
	fileBufPtr.AppendFormat(_L8("  Save scaled images: \"%S\"\r\n"),&KNo8);

	//bit 7: write "use crossfading"
	fileBufPtr.AppendFormat(_L8("  Song crossfading: \"%S\"\r\n"),iPFlags&EPreferencesUseCrossfading?&KYes8:&KNo8);

	//bit 8: write "icons instead of drag"
	fileBufPtr.AppendFormat(_L8("  Use next and prev icons instead of drag/arrows: \"%S\"\r\n"),iPFlags&EPreferencesPlayerUseIconsInsteadOfDrag?&KYes8:&KNo8);

	//bit 9: write "End key sends only sends app to background"
	fileBufPtr.AppendFormat(_L8("  End key sends only sends app to background: \"%S\"\r\n"),iPFlags&EPreferencesEndKeyBackgroundOnly?&KYes8:&KNo8);

	//bit 10: write "Play button on touch screens does not shuffle playlist"
	fileBufPtr.AppendFormat(_L8("  Play button on touch screens does not shuffle playlist: \"%S\"\r\n"),iPFlags&EPreferencesPlayButtonNoShuffle?&KYes8:&KNo8);

	//bit 11: write "Loop current song"
	fileBufPtr.AppendFormat(_L8("  Loop current song: \"%S\"\r\n"),iPFlags&EPreferencesSongLoop?&KYes8:&KNo8);
		
	//bit 12: write "Use metadata" EPreferencesUseMetadata
	fileBufPtr.AppendFormat(_L8("  Use metadata: \"%S\"\r\n"),iPFlags&EPreferencesUseMetadata?&KYes8:&KNo8);
	
	//bit 13: write "EPreferencesCreateCoverHintFiles"
	fileBufPtr.AppendFormat(_L8("  Create cover hint files: \"%S\"\r\n"),iPFlags&EPreferencesCreateCoverHintFiles?&KYes8:&KNo8);
	
	//bit 14: write "EPreferencesMatchColors2AlbumArt"
	fileBufPtr.AppendFormat(_L8("  Match music player's color theme to the dominant color of the album art: \"%S\"\r\n"),iPFlags&EPreferencesMatchColors2AlbumArt?&KYes8:&KNo8);
	
	//bit 15: write "EPreferencesUseBassBoost"
	fileBufPtr.AppendFormat(_L8("  Use Bass Boost: \"%S\"\r\n"),iPFlags&EPreferencesUseBassBoost?&KYes8:&KNo8);
	
	//bit 16: write "EPreferencesUseStereoWidening"
	fileBufPtr.AppendFormat(_L8("  Use Stereo Widening: \"%S\"\r\n"),iPFlags&EPreferencesUseStereoWidening?&KYes8:&KNo8);
	
	//bit 17: write "EPreferencesUseLoudness"
	fileBufPtr.AppendFormat(_L8("  Use Loudness: \"%S\"\r\n"),iPFlags&EPreferencesUseLoudness?&KYes8:&KNo8);
		
	//done with comments
	fileBufPtr.AppendFormat(_L8("-->\r\n"));


    //write the volume for the internal music player
	fileBufPtr.AppendFormat(_L8("<volume v=\"%d\"/>\r\n"),iVolume);

    //write the music player themes data
    fileBufPtr.AppendFormat(_L8("<musicplayerthemesdata mptd=\"%x\"/>\r\n"),iMusicPlayerThemeData);

    //write crossfading parameters
    fileBufPtr.AppendFormat(_L8("<crossfading timems=\"%d\" volrampupms=\"%d\"/>\r\n"),iCrossfadingTimeMs,iCrossfadingTimeVolRampUpMs);
    
    //write equalizer preset value (-1 means we do not use any equalizer preset)
    fileBufPtr.AppendFormat(_L8("<equalizer preset=\"%d\"/>\r\n"),iAudioEqualizerPreset);
    
    LOG0("Before saving playlists");
    //write the index & position
    TBuf8<128> plsName8;
    for(i=0;i<iPlaylists->Count();i++)
    {
    	CPlaylist *cp=(*iPlaylists)[i];
    	TBuf8<128> plsName;
    	if(cp->iName)
    		CnvUtfConverter::ConvertFromUnicodeToUtf8(plsName8,cp->iName->Des());
    	else
    		plsName8.SetLength(0);
    	fileBufPtr.AppendFormat(_L8("<playlist nrelements=\"%d\" index=\"%d\" position=\"%d\" duration=\"%d\" totalduration=\"%d\" name=\"%S\" iscurrent=\"%S\"/>\r\n"),cp->NrElementsWhenLoaded(),cp->GetCurrentIndex(),cp->iCurrentPlaybackPosition,cp->CurrentTrackDuration(),(TInt)(cp->iTotalDurationInMiliseconds/1000),&plsName8,cp->IsCurrent()?&KYes8:&KNo8);
    };
    LOG0("After saving playlists");

    //end tag
    fileBufPtr.Append(KXmlPreferencesEnd);
    
    //done writing to buffer, dump it to file
    prefs.Write(fileBufPtr);
    prefs.Close();
    User::Free(fileBuf);
    iCFlags&=~ENoPreferencesFileFound;
    LOG(ELogGeneral,-1,"SavePreferences--");
}

void CMLauncherPreferences::AddStartingDirL(TDesC& aStartingDir, TUint aNrFiles)
{
	HBufC *startingDir=HBufC::NewLC(aStartingDir.Length());
	startingDir->Des().Copy(aStartingDir);
	AddStartingDirL(startingDir,aNrFiles);
	CleanupStack::Pop(startingDir);
}

void CMLauncherPreferences::AddStartingDirL(HBufC *aStartingDir, TUint aNrFiles)
{
	//we take ownership of aStartingDir
	TPtr ptrStartingDir(aStartingDir->Des());
	RemoveEndingBackslash(ptrStartingDir);
	iStartingDirs.AppendL(aStartingDir);
	//here we check if we mark this starting dir as an "audiobook" type of source
	TFileName stdir;
	stdir.Copy(*aStartingDir);
	stdir.LowerCase();
	if(stdir.Find(KAudioBookName)!=KErrNotFound)
	{
		//mark this as an audiobook type of source
		aNrFiles|=EFlagSourceIsAudiobook;
	}
	iStartingDirsNrFiles.AppendL(aNrFiles);
}

void CMLauncherPreferences::AddBlacklistedDirL(HBufC *aBlacklistedDir, TUint aNrFiles)
{
	//we take ownership of aStartingDir
	TPtr ptrBlacklistedDir(aBlacklistedDir->Des());
	RemoveEndingBackslash(ptrBlacklistedDir);
	iBlacklistDirs.AppendL(aBlacklistedDir);
	iBlacklistDirsNrFiles.AppendL(aNrFiles);
}

TInt CMLauncherPreferences::GetVersionDescriptionSize()
{
	TInt length=0;
	TEntry entry;
	TInt err=iFs->Entry(PrivatePathPlus(KVersionDescriptionFile),entry);
	if(err)
	{
		//most probably the file does not exist! Lets create it
		RFile vdf;
		TInt err;
		err=vdf.Replace(*iFs,PrivatePathPlus(KVersionDescriptionFile),EFileWrite);
		if(err!=KErrNone)return 0;//can not create the file, for some reason

		//write header
		vdf.Write(KXmlStart);

		//write the version
		TBuf8<KPreferencesLineLength> line;
		line.Format(_L8("<version major=\"%d\" minor=\"%d\">"),Vmajor,Vminor);
		vdf.Write(line);
		vdf.Write(KEOL8);
		vdf.Write(_L8("</version>"));
		vdf.Write(KEOL8);

		//write the description
		vdf.Write(_L8("<description>"));
		vdf.Write(KEOL8);
		vdf.Write(_L8("None"));
		vdf.Write(KEOL8);
		vdf.Write(_L8("</description>"));
		vdf.Write(KEOL8);

		//done writing
		vdf.Close();

		//now get the length
		length=GetVersionDescriptionSize();
	}
	else length=entry.iSize;
	return length;
}

void CMLauncherPreferences::CopyVersionDescription(TDes8 &aBuffer)
{
	RFile vdf;
	TInt err;
	err=vdf.Open(*iFs,PrivatePathPlus(KVersionDescriptionFile),EFileRead|EFileShareReadersOnly);
	if(err!=KErrNone)return;//can not open the file, for some reason

	TInt size;
	vdf.Size(size);
	vdf.Read(aBuffer,size);
	vdf.Close();
}

void CMLauncherPreferences::GetRecvFilesDirFullPath(TFileName &aPath,TInt aSourceIndex/*=-1*/)
{
	if(aSourceIndex<0)
	{
		//we need to select where to put the music by ourselves
		//TODO: make a proper selection
		TInt i;
		
		//try first the source that starts with e:	
		for(i=0;i<iStartingDirs.Count();i++)
		{
			TBuf<5> drive(iStartingDirs[i]->Left(KEDrive().Length()));
			drive.LowerCase();
			if(drive==KEDrive)
			{
				aSourceIndex=i;
				LOG0("We found a source for e: drive");
				break;
			};
		};
	};
	if(aSourceIndex<0)aSourceIndex=0;
	
	__ASSERT_ALWAYS(aSourceIndex>=0 && aSourceIndex<iStartingDirs.Count(),Panic(EInvalidSourceIndex));
	aPath.Copy(*iStartingDirs[aSourceIndex]);
	AddEndingBackslash(aPath);
	aPath.Append(KDefaultRecvFilesDir);
	AddEndingBackslash(aPath);
}

void CMLauncherPreferences::CreatePathFromSourceAndName(TFileName &aPath,TInt aSourceIndex,const TDesC &aName, TBool aAppendSlash)
{
	aPath.Copy(*iStartingDirs[aSourceIndex]);
	AddEndingBackslash(aPath);
	aPath.Append(aName);
	if(aAppendSlash)
		AddEndingBackslash(aPath);
}
/*
void CMLauncherPreferences::SavePlaylistFileL(CPlaylist& aPlaylist, TInt aPosition)
{
	//first, create the playlist filename
	TInt err,i,nrFiles(aPlaylist.Count());
	iFs->MkDirAll(PrivatePathPlus(KPlaylistFileName));

	RFile playlist;
	err=playlist.Replace(*iFs,PrivatePathPlus(KPlaylistFileName),EFileWrite);
	if(!err)
	{
		//file created, write into it
		playlist.Write(_L8("#EXTM3U"));
		playlist.Write(KEOL8);
		
		TBuf8<KMaxFileName+2> song8; //+2 from KEOL8().Length()
		TFileName song16;
		//add the files
		for(i=0;i<nrFiles;i++)
		{
			aPlaylist.GetEntry(i)->GetFullPath(song16);
			CnvUtfConverter::ConvertFromUnicodeToUtf8(song8,song16);
			if(song8.Right(KEOL8().Length()) != KEOL8)
				song8.Append(KEOL8);
			    	
			playlist.Write(song8);
		};
		//close playlist
		playlist.Close();

		//save the index & position
		iPlaylistIndex=aPlaylist.GetCurrentIndex();
		if(iPlaylistIndex<0)iPlaylistIndex=0;
		if(iPlaylistIndex>=nrFiles)iPlaylistIndex=nrFiles-1;
		iPosition=aPosition;
		iFlags|=ESavePreferences;
	};
}
*/

/* function not used at the moment
void CMLauncherPreferences::LoadPlaylistL(RPointerArray<HBufC8>& aFiles4Playlist)
{
	RFile playlist;
	TInt err=playlist.Open(*iFs,PrivatePathPlus(KPlaylistFileName),EFileRead|EFileShareReadersOnly);
	if(err)return;

	//we will read the entire file into memory and then parse it
	TInt size;
	err=playlist.Size(size);
	if(err)return;
	HBufC8 *file=HBufC8::NewLC(size);
	TPtr8 ptrFile(file->Des());
	playlist.Read(ptrFile);
	TInt eolPos,start(0),length;
	HBufC8 *songFilename;
	while((eolPos=ptrFile.Find(KEOL8))!=KErrNotFound)
	{
		if(start==0)
		{
			ptrFile[eolPos]='#';
			start=eolPos+2;
			continue;
		};
		length=eolPos-start;
		songFilename=HBufC8::NewLC(length);
		songFilename->Des().Copy(ptrFile.Mid(start,length));
		aFiles4Playlist.AppendL(songFilename);
		CleanupStack::Pop(songFilename);
		ptrFile[eolPos]='#';
		start=eolPos+2;
	};

	//clean
	CleanupStack::PopAndDestroy(file);
	playlist.Close();
	//we also need to delete the playlist file
	iFs->Delete(PrivatePathPlus(KPlaylistFileName));
}
*/

const TDesC& CMLauncherPreferences::GetRecvFilesDir() const
{
	return KDefaultRecvFilesDir();
}

void CMLauncherPreferences::RemoveEndingBackslash(TDes& aPath)
{
	TInt len(KDirBackslash().Length()); //not sure if compiler would optimize this ...
	if(aPath.Right(len) == KDirBackslash)
		aPath.Delete(aPath.Length()-len,len);
}

void CMLauncherPreferences::AddEndingBackslash(TDes& aPath)
{
	if(aPath.Right(KDirBackslash().Length()) != KDirBackslash)
		aPath.Append(KDirBackslash);
}

void CMLauncherPreferences::RemovePathComponent(TDes& aPath)
{
	TInt pos=aPath.LocateReverse('\\');
	if(pos>=0)
		aPath.Delete(0,pos+1);
}
