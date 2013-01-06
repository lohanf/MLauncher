#ifndef MLAUNCHERPREFERENCES_H_
#define MLAUNCHERPREFERENCES_H_

#include <f32file.h>
#include "defs.h"
/*
class CPlaylistMetadata : public CBase
{
public:
	HBufC *iName;
	TInt iNrElements;
	TInt iCurrentElement;
	TInt iCurrentPlaybackPosition;
};*/
class CPlaylist;
class CMLauncherPreferences : public CBase
{
public:
	enum TPreferencesFlags
	{
		EPreferencesKeepSelection=1,
		EPreferencesLaunchEmbedded=2,
		EPreferencesUseInternalMusicPlayer=4,
		EPreferencesUsePartialCheck=8,
		EPreferencesPlaylistLoop=16, 
		//EPreferencesSaveScaledImages=32, //not handled in Options menu yet
		EPreferencesUseCrossfading=64, 
		EPreferencesPlayerUseIconsInsteadOfDrag=128,
		EPreferencesEndKeyBackgroundOnly=256,
		EPreferencesPlayButtonNoShuffle=512,
		EPreferencesSongLoop=1024,
		EPreferencesUseMetadata=2048,
		EPreferencesCreateCoverHintFiles=4096, //0x1000
		EPreferencesMatchColors2AlbumArt=0x2000,
		EPreferencesUseBassBoost=0x4000,
		EPreferencesUseStereoWidening=0x8000,
		EPreferencesUseLoudness=0x10000
	};

	enum TComputedFlags
	{
		//these flags are computed, not saved (they are not user preferences)
		ENoPreferencesFileFound            =0x0001, //there was no preferences file found, so this is probably first time this app is launched
		ESavePreferences                   =0x0002,
		EMusicSearchInProgress             =0x0004,
		//HW related stuff
		EHwDeviceHasVolumeKeys             =0x0010,
		EHwDeviceIsSlow                    =0x0020, //one of the things we do not do if the device is slow is to prepare the next song while the current song is playing
		EHwDeviceHasMSK                    =0x0040,
		EHwDoNotUseDSA                     =0x0080, //We do not use Direct Screen Access on this device
		EHwDoNotUseCrossfading             =0x0100, //We do not use crossfading on this device
		EHwCanDoLandscapeWithoutTitlebar   =0x0200, //
		EHwTouchDevice                     =0x0400, //this is touch device or not
		EHwHasFmTx                         =0x0800, //device has FM Transmitter
		//EDeviceDoesNotHaveDeferredLightsOff=0x00000000, //when this is on, we do not need to defer stopping of the periodic worker in MusicPlayerContainer
		
		//sources flags
		EFlagSourceIsAudiobook =0x01000000,
		EFlagSourceAddedNow    =0x02000000
	};
	public:
		static CMLauncherPreferences* NewL(RFs &aFs, RPointerArray<CPlaylist> *aPlaylists);
        ~CMLauncherPreferences();
        void LoadPreferences();
        void SavePreferences();
        void DeletePreferences();
        void AddStartingDirL(TDesC& aStartingDir, TUint aNrFiles);
        void AddStartingDirL(HBufC *aStartingDir, TUint aNrFiles); //takes ownership of aStartingDir
        void AddBlacklistedDirL(HBufC *aBlacklistedDir, TUint aNrFiles); //takes ownership of aBlacklistedDir
        TInt GetVersionDescriptionSize();
        void CopyVersionDescription(TDes8 &aBuffer);
        void GetRecvFilesDirFullPath(TFileName &aPath,TInt aSourceIndex=-1);
        const TDesC& GetRecvFilesDir() const;
        void CreatePathFromSourceAndName(TFileName &aPath,TInt aSourceIndex,const TDesC &aName, TBool aAppendSlash);

        static void RemoveEndingBackslash(TDes& aPath);
        static void AddEndingBackslash(TDes& aPath);
        static void RemovePathComponent(TDes& aPath);
        
        void SetPrivatePath(TChar aDrive);
        TDesC& PrivatePathPlus(const TDesC& aStr);
	private:
	    CMLauncherPreferences();
	    void ConstructL(RFs &aFs, RPointerArray<CPlaylist> *aPlaylists);
	public:
	    //HBufC *iStartingDir;
	    RPointerArray<HBufC> iStartingDirs;
	    RPointerArray<HBufC> iBlacklistDirs;
	    RArray<TUint> iStartingDirsNrFiles; //most significant 8 bits are for flags
	    RArray<TUint> iBlacklistDirsNrFiles; //most significant 8 bits are for flags
	    RPointerArray<CPlaylist> *iPlaylists; //not owned

	    TUint iPFlags;
	    TUint iCFlags;
	    TInt iVolume;
	    TInt iMaxVolume; //computed
	    TUint iPlatformVersionMajor,iPlatformVersionMinor;
	    //TInt iPlaylistIndex;
	    //TInt iPosition; //position in seconds of the currently playing file
	    //RPointerArray<CPlaylistMetadata> iPlaylistMetadata;
	    TUint iMusicPlayerThemeData;
	    TInt iAudioEqualizerPreset;
	    TUint iCrossfadingTimeMs;
	    TUint iCrossfadingTimeVolRampUpMs;

	    RFs *iFs; //not owned
	private:
		TFileName iPrivatePath;
};

#endif /*MLAUNCHERPREFERENCES_H_*/
