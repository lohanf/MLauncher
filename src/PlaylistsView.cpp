/*
 ============================================================================
 Name		: PlaylistsView.cpp
 Author	  : Florin Lohan
 Version	 : 1.0
 Copyright   : (C) 2010 Florin Lohan. All rights reserved.
 Description : CPlaylistsView implementation
 ============================================================================
 */

#include <aknquerydialog.h> 
#include <stringloader.h>
#include "PlaylistsView.h"
#include "PlaylistsContainer.h"
#include "MLauncherAppUi.h"
#include "TrackInfo.h"
#include "MLauncher.hrh"
#include "MLauncher.pan"
#include <MLauncher.rsg>

CPlaylistsView::CPlaylistsView()
{
	// No implementation required
}

CPlaylistsView::~CPlaylistsView()
{
	if(iPlaylistsContainer)
	{
		DoDeactivate();
	};
}

CPlaylistsView* CPlaylistsView::NewLC()
{
	CPlaylistsView* self = new (ELeave) CPlaylistsView();
	CleanupStack::PushL(self);
	self->ConstructL();
	return self;
}

CPlaylistsView* CPlaylistsView::NewL()
{
	CPlaylistsView* self = CPlaylistsView::NewLC();
	CleanupStack::Pop(); // self;
	return self;
}

void CPlaylistsView::ConstructL()
{
	BaseConstructL(R_PLAYLISTS_VIEW);
	//iPreferences=MyAppUi()->iPreferences;
}

// -----------------------------------------------------------------------------
//  Called by the framework when the application status pane
//  size is changed.  Passes the new client rectangle to the
//  AppView
// -----------------------------------------------------------------------------
//
void CPlaylistsView::HandleStatusPaneSizeChange()
{
    if(iPlaylistsContainer)
    	iPlaylistsContainer->SetRect( ClientRect() );
}

void CPlaylistsView::DoDeactivate()
{
    if(iPlaylistsContainer)
    {
        AppUi()->RemoveFromStack(iPlaylistsContainer);
        delete iPlaylistsContainer;
        iPlaylistsContainer = NULL;
    }
}

void CPlaylistsView::DoActivateL(const TVwsViewId& /*aPrevViewId*/,TUid /*aCustomMessageId*/, const TDesC8& /*aCustomMessage*/)
{
	CEikStatusPane *sp=StatusPane();
	if(sp)sp->MakeVisible(ETrue);
	if(!iPlaylistsContainer)
    {
    	iPlaylistsContainer = CPlaylistsContainer::NewL( ClientRect(), this);
    };
    iPlaylistsContainer->SetMopParent(this);
    AppUi()->AddToStackL( *this, iPlaylistsContainer );
}

TUid CPlaylistsView::Id() const
{
    return TUid::Uid(KPlaylistsViewId); //defined in hrh
}

void CPlaylistsView::HandleCommandL(TInt aCommand)
{
	LOG(ELogGeneral,1,"CPlaylistsView::HandleCommandL++");
	//try first to send it to the AppUi for handling
	if(MyAppUi()->AppUiHandleCommandL(aCommand,this))
	{
		LOG(ELogGeneral,-1,"CPlaylistsView::HandleCommandL end (AppUi handled the command)");
		return; //AppUiHandleCommandL returned TRUE, it handled the command
	}

	//AppUiHandleCommandL did not handle the command
	TInt index=iPlaylistsContainer->CurrentIndex();
	if(index<0)
	{
		LOG(ELogGeneral,-1,"CPlaylistsView::HandleCommandL-- (index<0)");
		return;
	}
	else LOG0("current index: %d",index);

	switch(aCommand)
	{
	case ECommandPlaylistPlay:
		MyAppUi()->StartPlayingPlaylist(index);
		break;
	case ECommandPlaylistRename:
	{
		TBuf<64> name;//that 64 is the same as maxlen from r_dataquery_input_playlist_name
		CMLauncherDocument *myDoc=(CMLauncherDocument*)MyAppUi()->Document();
		if(myDoc->iPlaylists[index]->iName)
			name.Copy(*myDoc->iPlaylists[index]->iName);

		CAknTextQueryDialog* renameQuery=CAknTextQueryDialog::NewL(name);
		CleanupStack::PushL(renameQuery);
		HBufC* prompt=StringLoader::LoadLC(R_DATA_QUERY_RENAME_PROMPT);
		renameQuery->SetPromptL(*prompt);
		CleanupStack::PopAndDestroy(prompt);
		CleanupStack::Pop(renameQuery);
		if(renameQuery->ExecuteLD(R_DATAQUERY_INPUT_PLAYLIST_NAME))
		{
			myDoc->RenamePlaylist(index,name);
			iPlaylistsContainer->AddListElementsL();
		};
	};break;
	case ECommandPlaylistDelete:
		MyAppUi()->DeletePlaylist(index);
		iPlaylistsContainer->AddListElementsL(ETrue);
		break;


	default:
		Panic( EMLauncherUi );
	};

	LOG(ELogGeneral,-1,"CPlaylistsView::HandleCommandL end");
}
