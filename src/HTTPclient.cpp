/*
============================================================================
 Name        : HTTPclient.cpp
 Author      : Florin Lohan
 Copyright   : Florin Lohan
 Description : HTTP functionality: check for updates on a web server
============================================================================
*/

#include <stringloader.h>
#include <aknmessagequerydialog.h>
#include <aknnotewrappers.h>
#include <httpstringconstants.h>
#include <http\mhttpdatasupplier.h>
#include <eikenv.h>
#include <hash.h> //md5
#include "MLauncherPreferences.h"
#include <MLauncher.rsg>
#include "MLauncher.hrh"
#include "MLauncher.pan"
#include "XMLparsing.h"
#include "HTTPclient.h"
#include "Observers.h"
#include "log.h"
#include "urls.h"

_LIT8(KVersionFile, "version.xml");
_LIT8(KSISXFile, "MLauncher.sisx");

_LIT8(KPostContentType, "multipart/form-data; boundary=AaB03x");
_LIT8(KBodyTagStart,"<body>");
_LIT8(KBodyTagEnd,"</body>");
const TInt KDataChunkLength=1024;

/* This is what we want to create in iFormData (note we start and end with newline):

--AaB03x
content-disposition: form-data; name="vmajor"

[Vmajor]
--AaB03x
content-disposition: form-data; name="vminor"

[Vminor]
--AaB03x
content-disposition: form-data; name="md5"

[md5 of the IMEI]
--AaB03x
content-disposition: form-data; name="MAX_FILE_SIZE"

2097152
--AaB03x
content-disposition: form-data; name="logfile"; filename="MLauncher_crashed.txt"
content-type: text/plain
content-transfer-encoding: binary

[binary data/content of the MLauncher_crashed.txt file]
--AaB03x--

So we will split it in 3 parts: part 1 from beginning to md5, then part 2 up to the file content, and part
3 after the end of the file*/
_LIT8(KFormDataPart1,
		"\n--AaB03x\ncontent-disposition: form-data; name=\"vmajor\"\n\n%d"
		"\n--AaB03x\ncontent-disposition: form-data; name=\"vminor\"\n\n%d"
		"\n--AaB03x\ncontent-disposition: form-data; name=\"md5\"\n\n");
_LIT8(KFormDataPart2,"\n--AaB03x\ncontent-disposition: form-data; name=\"MAX_FILE_SIZE\"\n\n2097152\n--AaB03x\ncontent-disposition: form-data; name=\"logfile\"; filename=\"MLauncher_crashed.txt\"\ncontent-type: text/plain\ncontent-transfer-encoding: binary\n\n");
_LIT8(KFormDataPart3,"\n--AaB03x--\n");

CEikonEnv* CHTTPclient::iee=NULL;

CHTTPclient* CHTTPclient::NewL(MHTTPclientObserver &aObserver)
{
	CHTTPclient* self = new ( ELeave ) CHTTPclient(aObserver);
    CleanupStack::PushL( self );
    TInt err=self->ConstructL();
    CleanupStack::Pop(self);
    if(err)
    {
    	delete self;
    	self=NULL;
    }
    return self;
}

CHTTPclient::CHTTPclient(MHTTPclientObserver &aObserver): CActive(CActive::EPriorityStandard), iObserver(aObserver)
{
    CActiveScheduler::Add(this);
}

CHTTPclient::~CHTTPclient()
{
	//
	delete iUserAgent;
	delete iReceivedData; //should be NULL already
	iSession.Close();//closes the transaction as well
	iConnection.Close();
	iSocketServ.Close();


	if(iSisFile)
	{
		iSisFile->Close();
		delete iSisFile;
	};
	if(iUploadFile)
	{
		iUploadFile->Close();
		delete iUploadFile;
	};
	delete iTelephony; //should be NULL already
	delete iDataPart1;
}

TInt CHTTPclient::ConstructL()
{
	__ASSERT_DEBUG(iee,Panic(EieeIsNull));
	iInUse=ETrue;
	iSession.OpenL();

	/* Attention, attention,
	 * We need all the **** below (RSocketServ & RConnection) to avoid a CONE 36 Panic when the application exists.
	 * for more info see:
	 * http://discussion.forum.nokia.com/forum/showthread.php?t=76778
	 */
	// initialize handles
	TInt err;
	User::LeaveIfError(iSocketServ.Connect());
	User::LeaveIfError(iConnection.Open(iSocketServ));
	err=iConnection.Start();
	if(err)
		return err;
	

	// set them for use with open http session represented by iSession
	RStringPool strP = iSession.StringPool();
	RHTTPConnectionInfo connInfo = iSession.ConnectionInfo();
	connInfo.SetPropertyL ( strP.StringF(HTTP::EHttpSocketServ, RHTTPSession::GetTable() ), THTTPHdrVal (iSocketServ.Handle()) );
	TInt connPtr = REINTERPRET_CAST(TInt, &(iConnection));
	connInfo.SetPropertyL ( strP.StringF(HTTP::EHttpSocketConnection, RHTTPSession::GetTable() ), THTTPHdrVal (connPtr) );

	//create the user agent string
	CArrayFixFlat<TInt> *version=new(ELeave)CArrayFixFlat<TInt>(2);
	CleanupStack::PushL(version);
	TInt ver=Vmajor;
	version->AppendL(ver);
	ver=Vminor;
	version->AppendL(ver);
	HBufC *userAgent=StringLoader::LoadLC(R_USER_AGENT,*version);
	iUserAgent=HBufC8::NewL(userAgent->Length());
	iUserAgent->Des().Copy(*userAgent);
	CleanupStack::PopAndDestroy(userAgent);
	CleanupStack::PopAndDestroy(version);
	return KErrNone;
}

TBool CHTTPclient::InUse()
{
	return iInUse;
}

void CHTTPclient::CheckForUpdatesL()
{
	//open the session
	LOG(ELogGeneral,1,"CheckForUpdatesL: Start");

	//creatre the URI
	TUriParser8 uri;
	HBufC8* urlstring=HBufC8::NewLC(KUpdateURL().Length()+KVersionFile().Length());
	urlstring->Des().Copy(KUpdateURL);
	urlstring->Des().Append(KVersionFile);
	User::LeaveIfError(uri.Parse(*urlstring));
	LOG0("CheckForUpdatesL: uri created");

	//create a GET transaction
	iTransaction=iSession.OpenTransactionL(uri,*this,iSession.StringPool().StringF(HTTP::EGET,RHTTPSession::GetTable()));
	CleanupStack::PopAndDestroy(urlstring);
	LOG0("CheckForUpdatesL: transaction opened now");

	//add the UserAgent header
	RHTTPHeaders headers=iTransaction.Request().GetHeaderCollection();
	AddHeaderL(headers,HTTP::EUserAgent,*iUserAgent);
	LOG0("CheckForUpdatesL: user-agent added");

	//submit the request
	iMyStatus=EStatusVersionReceived;
	iTransaction.SubmitL();
	LOG(ELogGeneral,-1,"CheckForUpdatesL: End");
}

void CHTTPclient::CheckForUpdates2L()
{
	//so we got the response
	//was this successfull?
	LOG(ELogGeneral,1,"CheckForUpdates2L: Start");
	if(iTransaction.Response().StatusCode()!=200)
	{
		//it was not successfull
		LOG0("Returned status: %d",iTransaction.Response().StatusCode());
		// TODO: something here
	};
	iTransaction.Close();
	
	CheckForUpdates2AskUserL(*iReceivedData,EFalse);
	delete iReceivedData;
	iReceivedData=NULL;
		
	LOG(ELogGeneral,-1,"CheckForUpdates2L: End");
}

TBool CHTTPclient::CheckForUpdates2AskUserL(TDesC8 &aReceivedData, TBool aLogFileUploaded)
{
	LOG(ELogGeneral,1,"CheckForUpdates2AskUserL++ (LogFileUploaded=%d)",aLogFileUploaded);
	//get the data from aReceivedData
	TInt vMajor,vMinor,size;
	HBufC *description(NULL);
	TBool userPrompted(EFalse);
	Xml::CPlsSelectionCH *plsSel=Xml::CPlsSelectionCH::NewLC();
	plsSel->ParseVersionBufferL(aReceivedData,vMajor,vMinor,size,description);
	CleanupStack::PopAndDestroy(plsSel);

	//we have the version and the description
	LOG0("Version on the server: %d.%d",vMajor,vMinor);
	LOG0("Description: %S",description);

	/*we can have 4 situations:
	 * 1. The client has the latest version and has a sis file in its private folder
	 * 2. The client can upgrade
	 * 3. No upgrade necessary, but there is no sis file in the private dir
	 * 4. The client rejected the upgrade, but sis file not found in the private dir. Ask again.
	 */

	TBool sisExists,canUpgrade,doUpgrade;
	//check if we have the sisx in the private folder
	TEntry entry;
	if(iee->FsSession().Entry(iObserver.Preferences()->PrivatePathPlus(KSISFileName),entry)!=KErrNone)
	{
		//file does not exist
		sisExists=EFalse;
	}
	else sisExists=ETrue;
	//check if we should upgrade
	if(vMajor>Vmajor || (vMajor==Vmajor && vMinor>Vminor))canUpgrade=ETrue;
	else canUpgrade=EFalse;

	//see if we will upgrade
	doUpgrade=EFalse;
	iDoInstall=EFalse;
	if(canUpgrade)
	{
		//ask the user if he/she wants to download
		CAknMessageQueryDialog* dlg = new (ELeave)CAknMessageQueryDialog;
		CleanupStack::PushL(dlg);
		HBufC* header=StringLoader::LoadLC(R_UPGRADE_HEADER);
		dlg->SetHeaderTextL(*header);
		CleanupStack::PopAndDestroy(header);

		CArrayFixFlat<TInt> *ints=new(ELeave)CArrayFixFlat<TInt>(5);
		CleanupStack::PushL(ints);
		TInt tmp=Vmajor;
		ints->AppendL(tmp);
		tmp=Vminor;
		ints->AppendL(tmp);
		ints->AppendL(vMajor);
		ints->AppendL(vMinor);
		ints->AppendL(size);

		CPtrC16Array *strings=new(ELeave)CPtrC16Array(1);
		CleanupStack::PushL(strings);
		strings->AppendL(*description);

		HBufC* text(NULL);
		if(aLogFileUploaded)text=StringLoader::LoadLC(R_LOG_UPLOADED_UPGRADE_TEXT,*strings,*ints);
		else text=StringLoader::LoadLC(R_UPGRADE_TEXT,*strings,*ints);
		dlg->SetMessageTextL(*text);
		CleanupStack::PopAndDestroy(text);
		CleanupStack::PopAndDestroy(strings);
		CleanupStack::PopAndDestroy(ints);
		CleanupStack::Pop(dlg);
		if(dlg->ExecuteLD(R_UPGRADE_QUERY)==ESoftkeyUpgrade)
		{
			//the user accepted the upgrade
			doUpgrade=ETrue;
			iDoInstall=ETrue;
		};
		userPrompted=ETrue;
		//
		if(!doUpgrade && !sisExists && !aLogFileUploaded)
		{
			//the user does not want to upgrade now, but he/she does not have the sis in the private folder
			//we recommend the download
			CAknMessageQueryDialog* dlg = new (ELeave)CAknMessageQueryDialog;
			CleanupStack::PushL(dlg);
			HBufC* header=StringLoader::LoadLC(R_SISDOWNLOAD_NOUPGRADE_HEADER);
			dlg->SetHeaderTextL(*header);
			CleanupStack::PopAndDestroy(header);

			HBufC* text=StringLoader::LoadLC(R_SISDOWNLOAD_NOUPGRADE_TEXT,size);
			dlg->SetMessageTextL(*text);
			CleanupStack::PopAndDestroy(text);
			CleanupStack::Pop(dlg);
			if(dlg->ExecuteLD(R_SISDOWNLOAD_QUERY)==ESoftkeyDownload)
			{
				//the user accepted the upgrade
				doUpgrade=ETrue;
			};

		};
	}
	else if(!sisExists && !aLogFileUploaded)
	{
		//sis file does not exist, we recommend the user to download the file
		CAknMessageQueryDialog* dlg = new (ELeave)CAknMessageQueryDialog;
		CleanupStack::PushL(dlg);
		HBufC* header=StringLoader::LoadLC(R_SISDOWNLOAD_LATESTV_HEADER);
		dlg->SetHeaderTextL(*header);
		CleanupStack::PopAndDestroy(header);

		CArrayFixFlat<TInt> *ints=new(ELeave)CArrayFixFlat<TInt>(5);
		CleanupStack::PushL(ints);
		ints->AppendL(vMajor);
		ints->AppendL(vMinor);
		ints->AppendL(size);

		HBufC* text=StringLoader::LoadLC(R_SISDOWNLOAD_LATESTV_TEXT,*ints);
		dlg->SetMessageTextL(*text);
		CleanupStack::PopAndDestroy(text);
		CleanupStack::PopAndDestroy(ints);
		CleanupStack::Pop(dlg);
		if(dlg->ExecuteLD(R_SISDOWNLOAD_QUERY)==ESoftkeyDownload)
		{
			//the user accepted the upgrade
			doUpgrade=ETrue;
		};
	}
	else if(!aLogFileUploaded)
	{
		//we notify the user that no upgrade is necessary
		CAknMessageQueryDialog* dlg = new (ELeave)CAknMessageQueryDialog;
		CleanupStack::PushL(dlg);
		HBufC* header=StringLoader::LoadLC(R_NOUPGRADE_NECESSARY_HEADER);
		dlg->SetHeaderTextL(*header);
		CleanupStack::PopAndDestroy(header);

		CArrayFixFlat<TInt> *ints=new(ELeave)CArrayFixFlat<TInt>(5);
		CleanupStack::PushL(ints);
		ints->AppendL(vMajor);
		ints->AppendL(vMinor);

		HBufC* text=StringLoader::LoadLC(R_NOUPGRADE_NECESSARY_TEXT,*ints);
		dlg->SetMessageTextL(*text);
		CleanupStack::PopAndDestroy(text);
		CleanupStack::PopAndDestroy(ints);
		CleanupStack::Pop(dlg);
		dlg->ExecuteLD(R_MESSAGE_DLG_OK_EMPTY);
	};
	//done, clean
	delete description;

	//check if we will go for the upgrate!
	if(doUpgrade)
	{
		//creatre the URI
		TUriParser8 uri;
		HBufC8* urlstring=HBufC8::NewLC(KUpdateURL().Length()+KSISXFile().Length());
		urlstring->Des().Copy(KUpdateURL);
		urlstring->Des().Append(KSISXFile);
		User::LeaveIfError(uri.Parse(*urlstring));
		LOG0("CheckForUpdates2L: uri created");

		//create a GET transaction
		iTransaction=iSession.OpenTransactionL(uri,*this,iSession.StringPool().StringF(HTTP::EGET,RHTTPSession::GetTable()));
		CleanupStack::PopAndDestroy(urlstring);
		LOG0("CheckForUpdates2L: transaction opened now");

		//add the UserAgent header
		RHTTPHeaders headers=iTransaction.Request().GetHeaderCollection();
		AddHeaderL(headers,HTTP::EUserAgent,*iUserAgent);
		LOG0("CheckForUpdates2L: user-agent added");

		//submit the request
		iMyStatus=EStatusSISXReceived;
		iTransaction.SubmitL();
	};
	LOG(ELogGeneral,-1,"CheckForUpdates2AskUserL--");
	
	return userPrompted;
}

void CHTTPclient::CheckForUpdates3L()
{
	//so we got the response
	//was this successfull?
	TInt status;
	LOG(ELogGeneral,1,"CheckForUpdates3L: Start");
	if(iTransaction.Response().StatusCode()!=200)
	{
		//it was not successfull
		LOG0("Returned status: %d",iTransaction.Response().StatusCode());
		// TODO: handle the error
		status=MHTTPclientObserver::EUpgradeFailed;
	};
	iTransaction.Close();
	if(iSisFile)
	{
		iSisFile->Close();
		delete iSisFile;
		iSisFile=NULL;
		//announce the user that the download is completed.
		ShowErrorL(R_MSG_SIS_DOWNLOADED_SUCCESSFULLY,2);
		if(iDoInstall)status=MHTTPclientObserver::ESisDownloadedInstall;
		else status=MHTTPclientObserver::ESisDownloaded;
	}
	else status=MHTTPclientObserver::EUpgradeFailed; //if there was no sis file, we can not install

	//announce the AppUi that the CheckForUpgrade ended
	iObserver.CheckForUpgradeEnded(status);
	iInUse=EFalse;
	LOG(ELogGeneral,-1,"CheckForUpdates3L: End");
}

void CHTTPclient::UploadCrashedLogFileL(TFileName &aFilename, TInt aSize)
{
	//open the session
	LOG(ELogGeneral,1,"UploadCrashedLogFileL: Start");

	//set file size
	iUploadFileSize=aSize;
	//open the file
	iUploadFile=new(ELeave)RFile;
	User::LeaveIfError(iUploadFile->Open(iee->FsSession(),aFilename,EFileRead|EFileShareReadersOnly));

	//get the IMEI and create its md5
	iMyStatus=EStatusGetIMEI;
	CTelephony::TPhoneIdV1Pckg phoneIdPckg( iPhoneId );
	iTelephony=CTelephony::NewL();
	iTelephony->GetPhoneId(iStatus, phoneIdPckg);
	SetActive(); // Tell scheduler a request is active
	LOG(ELogGeneral,-1,"UploadCrashedLogFileL: End");
}

void CHTTPclient::UploadCrashedLogFile2L()
{
	//we should have the IMEI in iPhoneId
	delete iTelephony;
	iTelephony=NULL;
	LOG(ELogGeneral,1,"UploadCrashedLogFile2L: Start (Phone Manufacturer: %S, Phone Model: %S)",&iPhoneId.iManufacturer,&iPhoneId.iModel);
	//LOG1("IMEI: %S",&iPhoneId.iSerialNumber); //for privacy reasons :-)

	//creatre the URI
	TUriParser8 uri;
	User::LeaveIfError(uri.Parse(KUploadURL));
	
	//create a POST transaction
	iTransaction=iSession.OpenTransactionL(uri,*this,iSession.StringPool().StringF(HTTP::EPOST,RHTTPSession::GetTable()));
	LOG0("UploadCrashedLogFile2L: transaction opened now");

	//add the UserAgent & content type headers
	RHTTPHeaders headers=iTransaction.Request().GetHeaderCollection();
	AddHeaderL(headers,HTTP::EUserAgent,*iUserAgent);
	AddHeaderL(headers,HTTP::EContentType,KPostContentType);
    LOG0("UploadCrashedLogFile2L: user-agent & content-type added");

    //add data supplier
    MHTTPDataSupplier* dataSupplier = this;
    iPartNumber=0;
    delete iReceivedData;iReceivedData=NULL; //just to be sure
    iTransaction.Request().SetBody(*dataSupplier);

	//submit the request
	iMyStatus=EStatusUploadRequestSent;
	iTransaction.SubmitL();
	LOG(ELogGeneral,-1,"UploadCrashedLogFile2L: End");
}

void CHTTPclient::UploadCrashedLogFile3L()
{
	LOG(ELogGeneral,1,"UploadCrashedLogFile3L: start");
	TInt status(MHTTPclientObserver::ELogUploadParsingFailed); //that would mean parsing error
	TBool userPrompted(EFalse);
	if(iTransaction.Response().StatusCode()!=200)
	{
		//it was not successfull
		LOG0("Returned status: %d",iTransaction.Response().StatusCode());
		// TODO: handle the error
		status=MHTTPclientObserver::ELogUploadTransactionFailed;
	}
	else
	{
		//transaction was successfull
		TBuf<10240> zz;
		zz.Copy(*iReceivedData);
		LOG0("Received data (%d bytes): %S",iReceivedData->Length(),&zz);
		//we have the webpage in iReceivedData. Interpret it.
		//we have a number after the <body> tag. Get that
		TInt start,end;
		start=iReceivedData->Find(KBodyTagStart);
		end=iReceivedData->Find(KBodyTagEnd);
		if(start==KErrNotFound)
			status=0;
		else
		{
			TPtrC8 numberStr(iReceivedData->Mid(start+KBodyTagStart().Length(),end-start-KBodyTagStart().Length()));
			TLex8 lex(numberStr);
			lex.Val(status);
			LOG0("Upload result: %d",status);
		}
	};
	iTransaction.Close();

	if(status==0)
		userPrompted=CheckForUpdates2AskUserL(*iReceivedData,ETrue);
	if(!userPrompted)
	{
		if(status)ShowErrorL(R_ERRMSG_LOG_UPLOAD_FAILED,0);
		else ShowErrorL(R_ERRMSG_LOG_UPLOADED_SUCCESSFULLY,2);
	}
	
	//interpret iReceivedData
	iObserver.CrashLogfileUploadEnded(status);
	delete iReceivedData;
	iReceivedData=NULL;
	iInUse=EFalse;
	LOG(ELogGeneral,-1,"UploadCrashedLogFile3L: end");
}

/*
void CHTTPclient::AddHeaderL(RHTTPHeaders& aHeaders,TInt aHeaderField, const TDesC& aHeaderValue)
{
	HBufC8 *headerValue=HBufC8::NewLC(aHeaderValue.Length());
	headerValue->Des().Copy(aHeaderValue);
	RStringPool stringPool = iSession.StringPool();
	RStringF valStr=stringPool.OpenFStringL(*headerValue);
	CleanupStack::PopAndDestroy(headerValue);
	THTTPHdrVal headerVal(valStr);
	aHeaders.SetFieldL(stringPool.StringF(aHeaderField,RHTTPSession::GetTable()),headerVal);
	valStr.Close();
}
*/
void CHTTPclient::AddHeaderL(RHTTPHeaders& aHeaders,TInt aHeaderField, const TDesC8& aHeaderValue)
{
	RStringPool stringPool = iSession.StringPool();
	RStringF valStr=stringPool.OpenFStringL(aHeaderValue);
	THTTPHdrVal headerVal(valStr);
	aHeaders.SetFieldL(stringPool.StringF(aHeaderField,RHTTPSession::GetTable()),headerVal);
	valStr.Close();
}

void CHTTPclient::WriteDataL(const TDesC8& aData)
{
    // some data already received
    if (iReceivedData)
	{
	    iReceivedData = iReceivedData->ReAllocL(iReceivedData->Length() + aData.Length());
		iReceivedData->Des() += aData;
	}
    else // no data yet received.
	{
	    iReceivedData = HBufC8::NewL(aData.Length());
	    iReceivedData->Des().Copy(aData);
	};
}

void CHTTPclient::WriteSISFileL(const TDesC8& aData)
{
	if(!iSisFile)
	{
		//we have to open this file
		iSisFile=new(ELeave)RFile;
		User::LeaveIfError(iSisFile->Replace(iee->FsSession(),iObserver.Preferences()->PrivatePathPlus(KSISFileName),EFileWrite));
	};
	iSisFile->Write(aData);
}

void CHTTPclient::ShowErrorL(TInt aErrorStringResource,TInt aErrorType)
{
	HBufC* noteText=StringLoader::LoadLC(aErrorStringResource);
	//CAknErrorNote* note=new(ELeave)CAknErrorNote;
	CAknResourceNoteDialog *note(NULL);
	switch(aErrorType)
	{
	case 0:note=new(ELeave)CAknErrorNote;break;
	case 1:note=new(ELeave)CAknWarningNote;break;
	case 2:note=new(ELeave)CAknInformationNote;break;
	}
	note->ExecuteLD(*noteText);
	CleanupStack::PopAndDestroy(noteText);
}

void CHTTPclient::MHFRunL(RHTTPTransaction aTransaction, const THTTPEvent &aEvent)
{
	LOG(ELogGeneral,1,"MHFRunL: Start, state=%d status=%d",iMyStatus,aEvent.iStatus);
	switch(iMyStatus)
	{
	case EStatusVersionReceived:
	{
		switch(aEvent.iStatus)
		{
		case THTTPEvent::EGotResponseBodyData:
		{
			MHTTPDataSupplier* body = aTransaction.Response().Body();
			TPtrC8 dataChunk;
			body->GetNextDataPart(dataChunk);
			WriteDataL(dataChunk);
			body->ReleaseData();
		};break;
		case THTTPEvent::EResponseComplete:
		{
			CheckForUpdates2L();
		};break;
		case THTTPEvent::EFailed:
		{
			LOG0("MHFRunL: transaction failed");
			CheckForUpdates2L();
		};break;
		};//aEvent switch
	};break;//EStatusVersionReceived
	case EStatusSISXReceived:
	{
		switch(aEvent.iStatus)
		{
		case THTTPEvent::EGotResponseBodyData:
		{
			MHTTPDataSupplier* body = aTransaction.Response().Body();
			TPtrC8 dataChunk;
			body->GetNextDataPart(dataChunk);
			WriteSISFileL(dataChunk);
			body->ReleaseData();
		};break;
		case THTTPEvent::EResponseComplete:
		{
			CheckForUpdates3L();
		};break;
		case THTTPEvent::EFailed:
		{
			LOG0("MHFRunL: transaction failed");
			CheckForUpdates3L();
		};break;
		};//aEvent switch
	};break;//EStatusSISXReceived
	case EStatusUploadRequestSent:
	{
		switch(aEvent.iStatus)
		{
		case THTTPEvent::EGotResponseBodyData:
		{
			MHTTPDataSupplier* body = aTransaction.Response().Body();
			TPtrC8 dataChunk;
			body->GetNextDataPart(dataChunk);
			WriteDataL(dataChunk);
			body->ReleaseData();
		};break;
		case THTTPEvent::EResponseComplete:
		{
			UploadCrashedLogFile3L();
		};break;
		case THTTPEvent::EFailed:
		{
			LOG0("MHFRunL: transaction failed");
			UploadCrashedLogFile3L();
		};break;
		};//aEvent switch
	};break;//EStatusUploadRequestSent
	};//iMyStatus switch
	LOG(ELogGeneral,-1,"MHFRunL: End");
}

TInt CHTTPclient::MHFRunError(TInt aError, RHTTPTransaction aTransaction, const THTTPEvent &aEvent)
{
	//
	return KErrNone;
}

void CHTTPclient::RunL()
{
	LOG(ELogGeneral,1,"CHTTPclient::RunL: Start, state=%d status=%d",iMyStatus,iStatus.Int());
	User::LeaveIfError(iStatus.Int());

	switch(iMyStatus)
	{
	case EStatusGetIMEI:UploadCrashedLogFile2L();
	}
	LOG(ELogGeneral,-1,"CHTTPclient::RunL: end");
}

void CHTTPclient::DoCancel()
{

}

TBool CHTTPclient::GetNextDataPart(TPtrC8 &aDataPart)
{
	LOG(ELogGeneral,1,"GetNextDataPart start (iPartNumber=%d, iDataReleased=%d)",iPartNumber,iDataReleased);
	TBool lastDataPart(EFalse);
	if(iPartNumber==0)iPartNumber=1; //we just started
	switch(iPartNumber)
	{
	case 1: //first part, up to the md5
	{
		delete iDataPart1;
		iDataPart1=HBufC8::NewL(KFormDataPart1().Length()+10);
		iDataPart1->Des().Format(KFormDataPart1,Vmajor,Vminor);
		aDataPart.Set(*iDataPart1);
	};break;
	case 2: //the md5 part
	{
		//make a md5 out of the entire iPhoneId buffer
		CMD5 *md5Obj=CMD5::NewL();
		TPtrC8 md5=md5Obj->Final(CTelephony::TPhoneIdV1Pckg(iPhoneId ));
		iMd5.Format(_L8("%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x"),md5[0],md5[1],md5[2],md5[3],md5[4],md5[5],md5[6],md5[7],md5[8],md5[9],md5[10],md5[11],md5[12],md5[13],md5[14],md5[15]);
		delete md5Obj;
		//LOG1("md5: %S",&iMd5);
		aDataPart.Set(iMd5);
	};break;
	case 3: //from the md5 to the file beginning
	{
		aDataPart.Set(KFormDataPart2);
	};break;
	case 4: //the file
	{
		//the file should be open
		__ASSERT_ALWAYS(iUploadFile,Panic(EHTTPUploadFileNull));
		if(!iReceivedData)
			iReceivedData=HBufC8::NewL(KDataChunkLength);
		if(iDataReleased)
		{
			//read into iReceivedData
			TPtr8 ptrData(iReceivedData->Des());
			iUploadFile->Read(ptrData);
		};
		LOG0("Data in iReceivedData: %d",iReceivedData->Length());
		if(iReceivedData->Length())
		{
			//we still have data in the file.
			aDataPart.Set(*iReceivedData);
		}
		else
		{
			//no more data in the file. Close the file
			iUploadFile->Close();
			// ... and send the last part
			aDataPart.Set(KFormDataPart3);
			lastDataPart=ETrue;
			iPartNumber=5;
		};

	};break;
	case 5: //from the file end to the end
	{
		aDataPart.Set(KFormDataPart3);
		lastDataPart=ETrue;
	};break;
	};//switch
	LOG(ELogGeneral,-1,"GetNextDataPart end (data: %d)",aDataPart.Size());
	return lastDataPart;
}

void CHTTPclient::ReleaseData()
{
	LOG(ELogGeneral,1,"ReleaseData: start");
	iDataReleased=ETrue;
	if(iPartNumber!=4)iPartNumber++; //4 is the file, it will increment by itself when the file is ready
	if(iPartNumber<6)
	{
		iTransaction.NotifyNewRequestBodyPartL();
		LOG0("New data available");
	};
	LOG(ELogGeneral,-1,"ReleaseData: end");
}

TInt CHTTPclient::OverallDataSize()
{
	TBuf<10> versionStr;
	versionStr.Format(_L("%d%d"),Vmajor,Vminor);
	TInt data=KFormDataPart1().Size()-4+versionStr.Length()+32+KFormDataPart2().Size()+iUploadFileSize+KFormDataPart3().Size();
	LOG0("OverallDataSize: %d",data);
	return data;
}

TInt CHTTPclient::Reset()
{
	LOG0("Reset called!");
	iPartNumber=0;
	iDataReleased=ETrue;
	delete iReceivedData;
	iReceivedData=NULL;
	TInt pos(0);
	iUploadFile->Seek(ESeekStart,pos);
	return KErrNone;
}
