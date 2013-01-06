/*
============================================================================
 Name        : MLauncherDocument.h
 Author      : Florin Lohan
 Copyright   : Florin Lohan
 Description : Declares document class for application.
============================================================================
*/

#ifndef __MLAUNCHERDOCUMENT_h__
#define __MLAUNCHERDOCUMENT_h__

// INCLUDES
#include <akndoc.h>

// FORWARD DECLARATIONS
class CMLauncherAppUi;
class CEikApplication;
class CFileDirPool;
class CMLauncherPreferences;
class CPlaylist;
class TFileDirEntry;

// CLASS DECLARATION

/**
* CMLauncherDocument application class.
* An instance of class CMLauncherDocument is the Document part of the
* AVKON application framework for the MLauncher example application.
*/
class CMLauncherDocument : public CAknDocument
    {
    public: // Constructors and destructor

        /**
        * NewL.
        * Two-phased constructor.
        * Construct a CMLauncherDocument for the AVKON application aApp
        * using two phase construction, and return a pointer
        * to the created object.
        * @param aApp Application creating this document.
        * @return A pointer to the created instance of CMLauncherDocument.
        */
        static CMLauncherDocument* NewL( CEikApplication& aApp );

        /**
        * NewLC.
        * Two-phased constructor.
        * Construct a CMLauncherDocument for the AVKON application aApp
        * using two phase construction, and return a pointer
        * to the created object.
        * @param aApp Application creating this document.
        * @return A pointer to the created instance of CMLauncherDocument.
        */
        static CMLauncherDocument* NewLC( CEikApplication& aApp );

        /**
        * ~CMLauncherDocument
        * Virtual Destructor.
        */
        virtual ~CMLauncherDocument();

    public: // Functions from base classes

        /**
        * CreateAppUiL
        * From CEikDocument, CreateAppUiL.
        * Create a CMLauncherAppUi object and return a pointer to it.
        * The object returned is owned by the Uikon framework.
        * @return Pointer to created instance of AppUi.
        */
        CEikAppUi* CreateAppUiL();

    private: // Constructors

        /**
        * ConstructL
        * 2nd phase constructor.
        */
        void ConstructL();

        /**
        * CMLauncherDocument.
        * C++ default constructor.
        * @param aApp Application creating this document.
        */
        CMLauncherDocument( CEikApplication& aApp );
    public: //own functions
        void AddVirtualFoldersL();
        CPlaylist* CreateTemporaryPlaylistL(TFileDirEntry *aEntry, TBool aTransferOwnership=EFalse); //if aTransferOwnership is TRUE, then the caller gets ownership of the list
        CPlaylist* BuildCurrentPlaylistL(); //returns the current playlist
        CPlaylist* SetPlaylistAsCurrent(TInt aPlaylistIndex);
    	void DeletePlaylist(TInt aPlaylistIndex);
    	void RenamePlaylist(TInt aPlaylistIndex, TDesC& aName);
    	
    	TInt DiscoverNewSources(); //returns 0 if the job is NOT done, 1 if the job is done but no changes, 2 if the job is done and there are changes
    	TBool AddToExistingSourcesL(); //returns ETrue if changes were made
    	void AddANewSource(TInt aSourceIndex, TUint extraFlags=0);
    	void SourcesChanged();
    public:
        CFileDirPool *iFilesTree;
        CMLauncherPreferences *iPreferences;
        RPointerArray<CPlaylist> iPlaylists;
    	RFs iFs;
    	TBool iCrashLogFound;

    	CPlaylist *iTempPlaylist; //helper
    	CPlaylist *iCurrentPlaylist; //do not delete, pointer to some elememt in iPlaylists
    	RPointerArray<HBufC> iGlobalDisplayNames;
    private:
    	//variables for finding new sources
    	CFileDirPool *iNewTree;
    	TChar iCurrentDrive;
    	RPointerArray<HBufC> iSources;
    	RArray<TUint> iSourcesCount;
    };

#endif // __MLAUNCHERDOCUMENT_h__

// End of File
