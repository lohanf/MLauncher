/*
============================================================================
 Name        : MLauncherListView.h
 Author      : Florin Lohan
 Copyright   : Florin Lohan
 Description : Declares UI class for application.
============================================================================
*/

#ifndef __FFLISTVIEW_h__
#define __FFLISTVIEW_h__

// INCLUDES
#include <aknview.h>
#include <aknviewappui.h> 
#include <eiklbo.h>  //MEikListBoxObserver
#include "Observers.h"
#include "defs.h"
#ifdef __TOUCH_ENABLED__
#include <AknToolbarObserver.h>
#endif


// FORWARD DECLARATIONS
class CFFListContainer;
class CMLauncherPreferences;
class CMLauncherSelectDlg;
class CHTTPclient;
class CThreadObserver;
class CMLauncherAppUi;
class CMLauncherDocument;

const TInt KMaxTexLengthInSelectionText=20;
// CLASS DECLARATION
/**
* CMLauncherAppUi application UI class.
* Interacts with the user through the UI and request message processing
* from the handler class
*/
class CFFListView : public CAknView, 
#ifdef __TOUCH_ENABLED__
                           public MAknToolbarObserver,
#endif
                           public MHTTPclientObserver/*, public MProgressDialogCallback*/
    {
    public:
    	enum TIdleWorkerJobs
    	{
    	   	EJobEraseSubstrings=4, //errase common substrings, e.g. the artist in a "nn - Artist - Songname.mp3" type of file. Will become: "nn Songname.mp3"
    	   	EDeleteHTTPclient=8, //delete the iHTTPclient variable
    	};
    	enum TFlags
    	{
    		EShouldUpgrade=1,
    		EUploadCrashedLogFile=2,
    		EHasMessage2Display=4,
    		EHasBeenActivatedAtLeastOnce=16,
    		EDoActivateInProgress=32,
    		EWaitForFirstDraw=64,
    		EContainerHasProgressBar=128,
    		EToolbarDisplaysPlaysong=256
    	};
    	enum TWhatChanged
    	{
    		EProperties=1,
    		EElements,
    		EMetadataParsingStarted,
    		EMetadataParsingEnded,
    		EMetadataParsingInProgress,
    		EFilesTree,
    	};
    public: // Constructors and destructor

        /**
        * ConstructL.
        * 2nd phase constructor.
        */
        void ConstructL();

        /**
        * CFFListView.
        * C++ default constructor. This needs to be public due to
        * the way the framework constructs the AppUi
        */
        CFFListView();

        /**
        * ~CFFListView.
        * Virtual Destructor.
        */
        virtual ~CFFListView();

        static CFFListView* NewLC();

    public:  // Functions from base classes

        /**
        * From CEikAppUi, HandleCommandL.
        * Takes care of command handling.
        * @param aCommand Command to be handled.
        */
        void HandleCommandL( TInt aCommand );


    public:  // Functions from base classes

		/**
		 * From MHTTPclientObserver
		 */
		virtual void CheckForUpgradeEnded(TInt aStatus);
		virtual void CrashLogfileUploadEnded(TInt aStatus);
		virtual CMLauncherPreferences* Preferences();
		
        // from CAknView:
        TUid Id() const;
        void DoActivateL(const TVwsViewId& aPrevViewId,TUid aCustomMessageId, const TDesC8& aCustomMessage);
        void DoDeactivate();

        // from MEikMenuObserver
        void DynInitMenuPaneL(TInt aResourceId, CEikMenuPane *aMenuPane);

#ifdef __TOUCH_ENABLED__
        //from MAknToolbarObserver
        void OfferToolbarEventL(TInt aCommand);
#endif
        // from MProgressDialogCallBack
        //virtual void  DialogDismissedL (TInt aButtonId);


    public: //own functions
        void CurrentListChanged(TWhatChanged aWhatChanged);
        void CurrentItemChanged(TInt aNewItemIndex=-1);
        
    	void CheckAndUpgradeL(TBool aCheck);
    	//void ChangeExitWithNaviL(TBool aShowNavi);
    	void UpdateEntriesL();
        void SaveSelection();
    	void UploadCrashLogFileL();//accessed from AppUi
    	void FirstDrawDone();
    	CMLauncherAppUi* MyAppUi(){return (CMLauncherAppUi*)AppUi();};
    	CMLauncherDocument *MyDoc(){return (CMLauncherDocument*)AppUi()->Document();};
    	void GetSelectionText(TDes &aText);
#ifdef __MSK_ENABLED__
    	void SetMSKSelectUnselect(TBool aSelected, TBool aHasSelection=ETrue);
#endif
    	//idle worker
    	TInt IdleWorker();
    	void ScheduleWorkerL(TInt aJobs, TBool aWaitForFirstDraw=ETrue);
    private:
        void LaunchPlayerL(TBool aShuffle);

        //TBool InputStartingDirManuallyL(TBool &aFolderExists, TBool &aRetry); //return: EFalse=user canceled the dialog, ETrue otherwise
        //TBool SelectStartingDirL(); //return: ETrue: path has been set

        //TFileDirEntry* SelectStartingDirL(TBool aUserRequest, TBool &aPathChanged); //aUserRequest==ETrue when this was called by the user from menu
        //void SaveStartingDirL(TDesC& aStartingDir);
        TBool InstallNowOrLaterL(const TDesC& aSisFilename);//returns ETrue if sis was installed

        //moved from Container
        void RenameCurrentEntryL();
        void DeleteSelectionL();
        TBool DeleteSelectedEntriesL(TFileDirEntry &aCurrentEntry);
        void MoveSelectionL();
        void MoveSelectionHereL();
        void RestartEraseSubstrings();
        // Idle worker
    	//static TInt IdleWorker(TAny *aInstance);
    	static TInt PeriodicWorker(TAny *aInstance);
    	

    public:
        void MoveRightL(TInt aIndex);
        void MoveLeftL();
        void ResetViewL();
    private: // Data
        //CFileDirPool *iFilesTree; //not owned
        CHTTPclient *iHTTPclient; //owned

#ifdef __MSK_ENABLED__
        //MSK labels
        HBufC *iMskSelect;
        HBufC *iMskUnselect;
#endif
        CPeriodic *iPeriodic;
        TInt iEraseSubstringStart;
        TInt iEraseSubstringLength;
        TInt iErasedLength;

    public:
        TUint iFlags;
        TUint iJobs; //jobs for the idle worker, see flags above

         CFFListContainer* iListContainer;
         //CMLauncherPreferences *iPreferences;//not owned
         RPointerArray<TFileDirEntry> iMovingFiles; //used to move files, pointers not owned
         TFileDirEntry *iOldParent; //not owned
         TTime iMetadataParseStart;
    };


#endif // __FFLISTVIEW_h__

// End of File
