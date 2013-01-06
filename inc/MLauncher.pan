/*
============================================================================
 Name        : MLauncher.pan
 Author      : Florin Lohan
 Copyright   : Florin Lohan
 Description : This file contains panic codes.
============================================================================
*/

#ifndef __MLAUNCHER_PAN__
#define __MLAUNCHER_PAN__

#include "log.h"
#include "common_cpp_defs.h"

/** MLauncher application panic codes */
enum TMLauncherPanics
    {
    EMLauncherUi = 1,
    // add further panics here
    EBTConnectWrongValue=101,
    EBTCurrentCommandWrongValue,
    EBTFileNotFound,
    EBTCurrentCommandNotVersion,
    EBTCurrentCommandNotZeroTerminated,
    EBTNotImplemented,
    EBTInvalidView,
    EBTiDataBTNULL,
    EBTiDataFileNotAllocated,
    EBTiReadingWritingNotFalse,
    EBTWrongNumberOfFileSizes,
    EBTFileNotNULL,
    EBTListeningSocketNotNull,
    EBTListeningSocketNull,
    EBTSocketNotNull,
    EBTDataBTNotNull,
    EBTDataFileNotNull,
    EBTFile2SendRecvNotNull,
    EBTSendAsNotNull,
    EBTSendAsMessageNotNull,
    EBTFilesInPlaylistNotNull,
    EBTNotListening,
    EBTInvalidCancelCommand,
    EBTDataDissapeared,
    EBTErrorAndNoClean,
    EBTNoObserver,
    EBTNoError2Display,
    EBTBufferTooSmall,

    EParentIsNull=201,
    ERequestedSizeIsZero,
    ENrChildrenNotKChildrenGranularity,
    EiNExtIsNULL,
    EIndexTooBig,
    EIndexTooBig2,
    EIndexTooBig3,
    ECountsNotSame,
    ENotEnoughMemory,
    ESourceInconsistency,//210
    ESourceInconsistency2,
    EElementShouldNotBeRoot,
    EPlaylistJobInconsistency,
    EParentIsNull2,
    ENameHasZeroLength,
    EIndexTooBig4,
    EChildIsNull,
    EBadCode,
    EBadCode2,
    EBadCode3,//20
    EBadCode4,
    EBadCode5,
    ETrackFileIsDeleted,//23
    
    EFileBufLenLessThan2=300,
    ENrOfElementsMismatch,
    EiMark1Negative,
    EPlaylistIndexOutsideRange,
    EUnknownPlaylistMethod,
    EBadIndex1,
    EBadIndex2,
    EiMark3Positive,
    ENoValidPlaylistIndexFound,
    EInvalidPlaylistElementFound,
    EiFileDirBufIsNull,
    
    EEntryNotPath,
    ESwitchValueNotHandled,

    EConnectionTypeUnknown,
    EIPAddressIsNULL,

    ELogInstanceNotNull,
    ELogInstanceNull,
    EHTTPUploadFileNull,

    EieeIsNull, //TODO: remove iee & thid id??
    EiFsIsNull,
    ECurrentParentIsNull,
    ENameIsNull,
    EBothIsSelectedAndIsPartiallySelected,

    EParentParentNotNull,
    EParentNameNotNull,
    EInvalidSourceIndex,
    ESourcesViewNotNull,
    EUnrecognizedMsgType,

    EMusicPlayerAlbumArtNotNull,
    EMusicPlayerAlbumArtNextPointerIsNull,
    EMusicPlayerAlbumArtNextNotNull,
    EFileIndexIsInvalid,
    EMusicTrackWrongState,
    EMusicFilenameEndsWithEOL,
    EJumpValueIsInvalid,
    ETooManyImagesInCache,
    EUnhandledSize,
    EMusicPlayerNoTheme,
    EDSAInstanceNotNull,
    EAudioOutputNotNull,
    EWrongAudioOutput,
    ETempTreeNotNull,
    
    EMoreStartingDirsThanSources,
    EPlaylistNotFullyLoaded, //0.103
    EToolbarShuffleInconsistency1, //0.103, Toolbar shows "no shuffle" icon, but in preferences is set the shuffle mode
    EToolbarShuffleInconsistency2, //0.103, Toolbar shows "shuffle" icon, but in preferences is set the non-shuffle mode
    ECurrentParentNotTheSame, //0.104, used for music files counting (finding source folders)
    EUnknownDriveLetter, //0.104
    
    //500: metadata related stuff
    EOurArtistIsNull=500,
    EOurArtistAlbumsIsNull,
    EOurAlbumIsNull,
    EOurAlbumSongsIsNull,
    EOurSongIsNull,
    EArtistsHolderParentNotRoot,
    
    ESourcesAndSourcesCountDifferent=600,
    
    //700: engine-related panics
    EPlaylistIsNull=700,
    ECurrentPlaylistIsNull,
    EPlaylistDecrementIndexMismatch,
    EHintFileExtensionLengthTooSmall, //0.105
    
    //effects & equalizer
    EThisShouldNotBeCalled=800,
    
    //Assert
    EAssertionFailed=1000,
    
    ENotImplemented=10000,
    ECoeControlIndexTooBig,
    ECoeControlIsNull,
    ECurrentViewIsNull
    };

inline void Panic(TMLauncherPanics aReason)
    {
    //_LIT(applicationName,"MLauncher");
    LOG0("Application Panic: %d",aReason);
    LOG_END_PANIC;
    User::Panic(applicationName, aReason);
    }

#endif // __MLAUNCHER_PAN__
