/*
 * common_cpp_defs.h
 *
 *  Created on: 18.1.2009
 *      Author: Florin
 */

#ifndef COMMON_CPP_DEFS_H_
#define COMMON_CPP_DEFS_H_

//_LIT(KDebugFolder,"E:\\MLauncherDebug\\");
_LIT(KSISFileName, "MLauncher.sisx");
_LIT(KSISFileNameInstalling, "MLauncher_installing.sisx");
_LIT(KSISFileName2Install, "MLauncher_2install.sisx");
_LIT(KOggPlayControllerSisx,"OggPlayController.sisx");
_LIT(KDirBackslash, "\\");

_LIT( KSelectionFileName, "Selection.xml");

_LIT(KCDrive,"C:");
_LIT(KDDrive,"D:");
_LIT(KEDrive,"E:");
_LIT(KFDrive,"F:");

_LIT(KPlsExtension, ".m3u");
_LIT(KEOL, "\r\n");
_LIT8(KEOL8, "\r\n");
_LIT8(KSpaces8, "  ");

_LIT(KTempPlaylist, "MLauncher Playlist");
_LIT(KPlaylistFolder,"c:\\Data\\Sounds\\");
//_LIT(KReceivedFilesDisplayName, "Received Files");
//_LIT(KVirtualFolderPrefix,"___");

_LIT(KPlaylistFileName,"Playlist.pls");
_LIT(KPlaylistFileNameBin,"Playlist.bin");
_LIT(KPlaylistFileNameFormat,"Playlist%02d.bin");
const TInt KPlaylistFileNameFormatLength=15;
const TInt KMaxNrPlaylists=10;

const TInt KPointerArrayPlaylistGranularity=256;
#ifdef __WINS__
const TInt KCacheFileBufferSize=5120; //can not be too small, otherwise app Panics when preferences are saved
#else
const TInt KCacheFileBufferSize=51200; //50 KB
#endif

#endif /* COMMON_CPP_DEFS_H_ */
