/*
============================================================================
 Name        : MLauncherAppUi.h
 Author      : Florin Lohan
 Copyright   : Florin Lohan
 Description : Declares UI class for application.
============================================================================
*/

#ifndef __MLAUNCHERAPPUI_h__
#define __MLAUNCHERAPPUI_h__

// INCLUDES
#include <aknviewappui.h>
#include <f32file.h>
#include <e32msgqueue.h>  //RMsgQueue
#include <aknserverapp.h> //MAknServerAppExitObserver
#include "MLauncherBT.h"
#include "MLauncherDocument.h"
//#include "TrackInfo.h"
#include "defs.h"

// FORWARD DECLARATIONS
class CFFListView;
class CPlaylistsView;
class CFiletransferView;
class CStopwatchView;
class CSourcesView;
class CMusicPlayerView;
class CMyFilesView;
class CAknMessageQueryDialog;
class CAknResourceNoteDialog;
class CMLauncherPreferences;
class CPlaylist;
class TFileDirEntry;
class CFileDirPool;
class CThreadObserver;
class MCurrentListObserver;
class CDocumentHandler;


// CLASS DECLARATION
/**
* CMLauncherAppUi application UI class.
* Interacts with the user through the UI and request message processing
* from the handler class
*/
class CMLauncherAppUi : public CAknViewAppUi, public MMLauncherTransferNotifier, public MAknServerAppExitObserver
{
public:
	enum TActiveViews
	{
		EFFListViewActive=1,
		EFiletransferViewActive=2,
		EStopwatchViewActive=4,
		ESourcesViewActive=8,
		EMusicPlayerViewActive=16,
		EPlaylistsViewActive=32,
		EMyFilesViewActive=64
	};
	enum TJobs
	{
		EJobClean=1,
		EJobDisplayMessage=2,
		EJobSubfolders=4, //check if the folder has subfolders
		EJobDiscoverSourcesAndParseMetadata=8, //add new folders containing music and parse metadata
		EJobStartDiscoveringSourcesAndParsingMetadata=16, //this displays a note if there is nothing
		EDoingJobs=0x10000000
	};
	enum TFlags
	{
		EAppShouldExit=1,
		EDeleteBtKeepListening=2,
		EListViewNeedsUpdatingProperties=4,
		EAppIsInBackground=8
	};
public: // Constructors and destructor
	/**
     * ConstructL.
     * 2nd phase constructor.
     */
    void ConstructL();

     /**
     * CMLauncherAppUi.
     * C++ default constructor. This needs to be public due to
     * the way the framework constructs the AppUi
     */
    CMLauncherAppUi();

     /**
     * ~CMLauncherAppUi.
     * Virtual Destructor.
     */
    virtual ~CMLauncherAppUi();


    virtual void DynInitMenuPaneL(TInt aResourceId, CEikMenuPane* aMenuPane);
    virtual void HandleCommandL(TInt aCommand);
    virtual void HandleForegroundEventL(TBool aForeground);
    /**
     * From MAknServerAppExitObserver
     */
    void HandleServerAppExit (TInt aReason);
    /**
	 * From MMLauncherBTNotifier
	 */
	virtual void CheckForUpdatesCompleteL(TInt /*aErr*/){};
	virtual void PlaylistTransferCompleteL();
	virtual void NewTransferL(const TInt aNrFiles, const TInt aTotalSize, const RArray<TInt> &aFileSizes);
	virtual void TransferingNewFileL(const TDirection aDirection, const TDes& aFilename, const TInt aSize);
	virtual void MoreDataTransfered(const TInt aBytes); //total sent/received so far
	virtual void UpdateView();
	virtual void SetDirection(TDirection aDirection);
	virtual TDirection Direction();
	CMLauncherPreferences *Preferences(){return MyDoc().iPreferences;};
	virtual void Clean(TBool aKeepListening);

	virtual void ShowErrorCleanCommL(TMsgType aErrorType, TInt aErrorStringResource, TInt aErr, TBool aClean, TBool aKeepListening=ETrue);
	virtual void ShowErrorCleanCommL(TMsgType aErrorType, HBufC* aErrorString, TInt aErr, TBool aClean, TBool aKeepListening=ETrue);

	virtual void Exit();
	// Own functions:
	TBool AppUiHandleCommandL(const TInt aCommand, const CAknView *aView); //returns True if it handled the command

	void CreateStopwatchViewL();
	void StopwatchDoneL();
	void CreateSourcesViewL();
	void SourcesViewDoneL();
	void UpdateSourcesView();
	
	void CreatePlaylistsViewL();
	void CreateMyFilesViewL();

	void CreateMusicPlayerViewL(CPlaylist *aPlaylist, TBool aIsPreview, TBool aStartPlaying); //aPlaylist ownership is NOT transfered
	void LoadPlaylistAndCreateMusicPlayerViewL();
	void CloseMusicPlayerViewL();


	TBool DisplayQueryOrNoteL(TMsgType aMsgType,TInt aTextResource,TInt aHeaderResource=-1,TInt aDlgResource=-1);
	void DisplayMessageL();

	void SwitchViewL(const CAknView *aCurrentView);
	//void ChangeExitWithNaviL(TInt aViewID);
	
	void ScheduleWorkerL(TUint aJobs);
	
	//from Engine
	//CPlaylist* GetNewPlaylist();
	CPlaylist* CurrentPlaylist(){return MyDoc().iCurrentPlaylist;};
	void LaunchExternalPlayerL();
	void CreatePlaylistFilename(TFileName &aPlaylist);

	/*
		 void AddToPlaylist(CPlaylist& aFiles4Playlist, TFileDirEntry *aCurrentEntry);
		        TInt CreatePlaylistL(TFileName &aPlaylist, TBool aShuffle); //returns the number of files in the playlist

		        //TInt CreatePlaylistListL(RPointerArray<HBufC>& aFiles4Playlist, TBool aShuffle); //returns the number of files in the playlist
		        TInt CreatePlaylistListL(CPlaylist &aFiles4Playlist, TBool aShuffle); //returns the number of files in the playlist
	 */
	void SetListObserver(MCurrentListObserver &aListObserver);
	
	//Playlist view
	void StartPlayingPlaylist(TInt aPlaylistIndex);
	void DeletePlaylist(TInt aPlaylistIndex);
	CMLauncherDocument& MyDoc(){return *(CMLauncherDocument*)Document();};
private:
	static TInt IdleWorkerStatic(TAny *aInstance);
	TInt IdleWorker();

public: //views:
	
private:
	CFFListView *iListView; //need access from sources view
	CFiletransferView *iFiletransferView;
	CPlaylistsView *iPlaylistsView;
	CStopwatchView *iStopwatchView;
	CSourcesView *iSourcesView;
	CMusicPlayerView *iMusicPlayerView;
	CMyFilesView *iMyFilesView;
	const CAknView *iCurrentView;//not owned
	TUint iActiveViews; //flag based

	//2 dialogs for displaying errors, warnings and notes after the view has changed
	CAknMessageQueryDialog *iDlgMsg;
	CAknResourceNoteDialog *iDlgNote;
	HBufC *iDlgNoteString; //the string for the iDlgNote

	TDirection iDirection;
	CIdle *iIdle;
	TUint iJobs;
	TUint iFlags;
	
	CDocumentHandler *iDocHandler;
	TInt iIi;
	TInt iNoteId;
};

#endif /*MLAUNCHERAPPUI_H_*/
