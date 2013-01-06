#ifndef HTTPCLIENT_H_
#define HTTPCLIENT_H_

#include <http\mhttptransactioncallback.h>
#include <http\mhttpdatasupplier.h>
#include <http\rhttpheaders.h>
#include <http\rhttpsession.h>
#include <http\rhttptransaction.h>
#include <Etel3rdParty.h>
#include <es_sock.h>

class CEikonEnv;
class MHTTPclientObserver;

class CHTTPclient: public CActive, public MHTTPTransactionCallback, public MHTTPDataSupplier
{
public:
	static CHTTPclient* NewL(MHTTPclientObserver &aObserver);
	~CHTTPclient();
	void CheckForUpdatesL();
	void UploadCrashedLogFileL(TFileName &aFilename, TInt aSize);
	TBool InUse(); //returns true if the instance is doing something. If not in use, the object can be deleted
private: //own functions
	CHTTPclient(MHTTPclientObserver &aObserver);
	TInt ConstructL();
	void AddHeaderL(RHTTPHeaders& aHeaders,TInt aHeaderField, const TDesC8& aHeaderValue);
	void WriteDataL(const TDesC8& aData);
	void WriteSISFileL(const TDesC8& aData);
	void ShowErrorL(TInt aErrorStringResource,TInt aErrorType=0); //aErrorType: 0=error note, 1=warning note, 2=info note
	void CheckForUpdates2L();
	TBool CheckForUpdates2AskUserL(TDesC8 &aReceivedData, TBool aLogFileUploaded); //returns ETrue if there was a user prompt
	void CheckForUpdates3L();

	void UploadCrashedLogFile2L();
	void UploadCrashedLogFile3L();
protected: //from CActive
	virtual void RunL();
	virtual void DoCancel();
private: //from MHTTPTransactionCallback
	virtual void MHFRunL(RHTTPTransaction aTransaction, const THTTPEvent &aEvent);
	virtual TInt MHFRunError(TInt aError, RHTTPTransaction aTransaction, const THTTPEvent &aEvent);

private: //from MHTTPDataSupplier
	/* We can not use CHTTPFormEncoder because the file can be rather big */
	virtual TBool GetNextDataPart(TPtrC8 &aDataPart);
	virtual void ReleaseData();
	virtual TInt OverallDataSize();
	virtual TInt Reset();

private:
	//for check for upgrades
	TBool iDoInstall;
	RFile *iSisFile;

	//for upload crash file
	CTelephony* iTelephony;
	CTelephony::TPhoneIdV1 iPhoneId;
	RFile *iUploadFile;
	TInt iUploadFileSize;
	TInt iPartNumber; //from 1 to 5
	TBool iDataReleased;
	TBuf8<32> iMd5;

	//general
	TBool iInUse;
	RSocketServ iSocketServ;
	RConnection iConnection;
	RHTTPSession iSession;
	RHTTPTransaction iTransaction;
	TInt iMyStatus;
	HBufC8 *iReceivedData;

	HBufC8* iUserAgent;
	HBufC8* iDataPart1;
	enum TMyStatusIds
	{
		EStatusVersionReceived=1,
		EStatusSISXReceived,
		//Upload statuses
		EStatusGetIMEI,
		EStatusUploadRequestSent
	};
	MHTTPclientObserver &iObserver;
public:
	static CEikonEnv *iee; //not owned
};

#endif /*HTTPCLIENT_H_*/
