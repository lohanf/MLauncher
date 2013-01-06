/*
 ============================================================================
 Name		: MyFilesView.cpp
 Author	  : Florin Lohan
 Version	 : 1.0
 Copyright   : (C) 2011 Florin Lohan. All rights reserved.
 Description : CMyFilesView implementation
 ============================================================================
 */
#include <f32file.h>
#include <s32file.h>
#include <random.h>
#include <documenthandler.h> 
#include "MyFilesView.h"
#include "MyFilesContainer.h"
#include "MLauncherAppUi.h"
#include "MLauncher.hrh"
#include "MLauncher.pan"
#include <MLauncher.rsg>

#ifdef __WINS__
_LIT(KDir, ":\\Data\\Videos\\");
const TChar KDrives[]={'C'};
#else
_LIT(KDir, ":\\private\\E388D98A\\");
const TChar KDrives[]={'E','F'};
#endif

_LIT(KBakExt,".bak");
_LIT(KMp4Ext,".mp4");
_LIT(KAviExt,".avi");
//_LIT(KJpegBakExt,".bkj");
_LIT(KJpegExt,".jpg");

CMyFilesView::CMyFilesView()
{
	// No implementation required
}

CMyFilesView::~CMyFilesView()
{
	if(iMyFilesContainer)
	{
		DoDeactivate();
	};
	iFiles.ResetAndDestroy();
	iPaths.ResetAndDestroy();
	delete iDocHandler;
}

CMyFilesView* CMyFilesView::NewLC()
{
	CMyFilesView* self = new (ELeave) CMyFilesView();
	CleanupStack::PushL(self);
	self->ConstructL();
	return self;
}

CMyFilesView* CMyFilesView::NewL()
{
	CMyFilesView* self = CMyFilesView::NewLC();
	CleanupStack::Pop(); // self;
	return self;
}

void CMyFilesView::ConstructL()
{
	BaseConstructL(R_MYFILES_VIEW);
}

// -----------------------------------------------------------------------------
//  Called by the framework when the application status pane
//  size is changed.  Passes the new client rectangle to the
//  AppView
// -----------------------------------------------------------------------------
//
void CMyFilesView::HandleStatusPaneSizeChange()
{
    if(iMyFilesContainer)
    	iMyFilesContainer->SetRect( ClientRect() );
}

void CMyFilesView::DoDeactivate()
{
    if(iMyFilesContainer)
    {
        AppUi()->RemoveFromStack(iMyFilesContainer);
        delete iMyFilesContainer;
        iMyFilesContainer = NULL;
    }
}

void CMyFilesView::DoActivateL(const TVwsViewId& /*aPrevViewId*/,TUid /*aCustomMessageId*/, const TDesC8& /*aCustomMessage*/)
{
	CEikStatusPane *sp=StatusPane();
	if(sp)sp->MakeVisible(ETrue);
	UnhideL();
	LOG0("We have %d hidden files",iFiles.Count());
	if(!iMyFilesContainer)
    {
		iMyFilesContainer = CMyFilesContainer::NewL( ClientRect(), this);
    };
	iMyFilesContainer->SetMopParent(this);
    AppUi()->AddToStackL( *this, iMyFilesContainer );
}

TUid CMyFilesView::Id() const
{
    return TUid::Uid(KMyFilesViewId); //defined in hrh
}

void CMyFilesView::HandleCommandL(TInt aCommand)
{
	LOG(ELogGeneral,1,"CMyFilesView::HandleCommandL++");
	//try first to send it to the AppUi for handling
	if(MyAppUi()->AppUiHandleCommandL(aCommand,this))
		return; //AppUiHandleCommandL returned TRUE, it handled the command

	//AppUiHandleCommandL did not handle the command
	TInt index=iMyFilesContainer->CurrentIndex();
	//LOG0("current index: %d",index);
	switch(aCommand)
	{
	case ECommandMyFilesPlay:
	{
		TDataType dataType(_L8("video/mp4"));
		if(iDocHandler==NULL)
		{
			iDocHandler = CDocumentHandler::NewL();
			//iDocHandler->SetExitObserver(this);
		};
		iDocHandler->OpenFileEmbeddedL(*iPaths[index],dataType );
	};break;
	default:
		Panic( EMLauncherUi );
	};
	LOG(ELogGeneral,-1,"CMyFilesView::HandleCommandL--");
}

TInt CMyFilesView::Hide()
{
	TInt err,i;
	CDir *dir=NULL;
	TUint j;
	TFileName dirName;

	for(j=0;j<sizeof(KDrives)/sizeof(TChar);j++)
	{
		dirName.SetLength(0);
		dirName.Append(KDrives[j]);
		dirName.Append(KDir);
		//get a list of files from the target dir
		err=iEikonEnv->FsSession().GetDir(dirName,KEntryAttNormal,ESortByName,dir);
		if(err!=KErrNone)return err;
		CleanupDeletePushL(dir);

		TBuf8<1024> buf,save;
		//TInt len;
		buf.SetLength(1024);
		TRandom::Random(buf);

		//for each file, open the file, append data, rename file
		for(i=0;i<dir->Count();i++)
		{
			RFile file;
			TInt pos=0;
			TBuf<256> oldName(dirName);
			oldName.Append((*dir)[i].iName);

			//check for the right extension
			TBuf<4> ext;
			ext=oldName.Right(4);
			ext.LowerCase();

			if(ext==KMp4Ext)
			{
				//rename the file	
				TBuf<256> newName(oldName);
				newName.Delete(newName.Length()-KMp4Ext().Length(),KMp4Ext().Length());
				newName.Append(KBakExt);
				iEikonEnv->FsSession().Rename(oldName,newName);

				err=file.Open(iEikonEnv->FsSession(),newName,EFileWrite);
				err=file.Seek(ESeekStart,pos);
				err=file.Read(save);
				//len=save.Length();
				err=file.Seek(ESeekStart,pos);
				err=file.Write(buf);
				err=file.Seek(ESeekEnd,pos);
				err=file.Write(save);
				err=file.Write(buf);
				//len=buf.Length();
				//len=save.Length();
				file.Close();
			};

			if(ext==KJpegExt)
			{
				//rename the file	
				TBuf<256> newName(oldName);
				newName.Delete(newName.Length()-KJpegExt().Length(),KJpegExt().Length());
				//newName.Append(KJpegBakExt);
				iEikonEnv->FsSession().Rename(oldName,newName);

				err=file.Open(iEikonEnv->FsSession(),newName,EFileWrite);
				err=file.Seek(ESeekStart,pos);
				err=file.Read(save);
				//len=save.Length();
				err=file.Seek(ESeekStart,pos);
				err=file.Write(buf);
				err=file.Seek(ESeekEnd,pos);
				err=file.Write(save);
				err=file.Write(buf);
				//len=buf.Length();
				//len=save.Length();
				file.Close();
			};
		};

		//clean
		CleanupStack::PopAndDestroy(dir);	
	}

	return KErrNone;
}

TInt CMyFilesView::UnhideL()
{
	TInt err,i;
	CDir *dir=NULL;
	TUint j;
	TFileName dirName;
	TBuf8<1024> save;

	iFiles.ResetAndDestroy();
	LOG(ELogGeneral,1,"Unhide++ (%d) drives",sizeof(KDrives)/sizeof(TChar));
	for(j=0;j<sizeof(KDrives)/sizeof(TChar);j++)
	{
		dirName.SetLength(0);
		dirName.Append(KDrives[j]);
		dirName.Append(KDir);
		//get a list of files from the target dir
		LOG0("Looking into %S",&dirName);
		err=iEikonEnv->FsSession().GetDir(dirName,KEntryAttNormal,ESortByName,dir);
		if(err!=KErrNone)continue;
		CleanupDeletePushL(dir);
		

		//for each file, open the file, append data, rename file
		LOG0("Found %d files",dir->Count());
		for(i=0;i<dir->Count();i++)
		{
			RFile file;
			TInt pos;
			const TDesC &dName=(*dir)[i].iName;
			LOG0("Checking %S",&dName);
			
			//check for the right extension
			TBuf<4> ext;
			ext=dName.Right(4);
			ext.LowerCase();
			if(ext!=KBakExt && ext!=KMp4Ext && ext!=KAviExt && ext!=KJpegExt)
				continue;
			//fill in iFiles and iPaths
			
			//do stuff
			HBufC *path=HBufC::NewLC(dirName.Length()+dName.Length());
			path->Des().Copy(dirName);
			path->Des().Append(dName);
			iPaths.Append(path);
			CleanupStack::Pop(path);
			
			//get the real name
			if(ext==KBakExt)
			{
				file.Open(iEikonEnv->FsSession(),*path,EFileRead);
				pos=512;
				file.Seek(ESeekStart,pos);
				file.Read(save,32);
				file.Close();
				//get the name out of save
				save.TrimRight();
				HBufC *name=HBufC::NewLC(save.Length());
				name->Des().Copy(save);
				iFiles.Append(name);
			}
			else 
			{
				//the real name is dName
				HBufC *name=HBufC::NewLC(dName.Length());
				name->Des().Copy(dName);
				iFiles.Append(name);
				CleanupStack::Pop(name);
			}
		};

		//clean
		CleanupStack::PopAndDestroy(dir);	
	}
	LOG(ELogGeneral,-1,"Unhide-- (%d files added)",iFiles.Count());
	return KErrNone;
}
