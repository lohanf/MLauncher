#ifndef MLAUNCHERBT_H_
#define MLAUNCHERBT_H_

#include <BTExtNotifiers.h>
#include <es_sock.h>
#include <bt_sock.h>
#include <btsdp.h>

class RSendAs;
class RSendAsMessage;
class RNotifier;
class CPlaylist;
class CMLauncherPreferences;
//class TBTDeviceResponseParams;
//class TBTDeviceResponseParamsPckg;

#define __USE_L2CAP  //undefine to use RFCOMM

typedef enum
{
	EMsgNote=1,
	EMsgWarning,
	EMsgError,
	EMsgNotePersistent,
	EMsgWarningPersistent,
	EMsgErrorPersistent,
	EMsgQueryYesNo,
	EMsgBigNote
} TMsgType;

class MMLauncherTransferNotifier
{
public:
	enum TDirection
    {
 		EDirectionSending=1,
 		EDirectionReceiving
 	};


	virtual void PlaylistTransferCompleteL()=0;
	virtual void NewTransferL(const TInt aNrFiles, const TInt aTotalSize, const RArray<TInt> &aFileSizes)=0;
	virtual void TransferingNewFileL(const TDirection aDirection, const TDes& aFilename, const TInt aSize)=0;
	virtual void MoreDataTransfered(const TInt aBytes)=0; //total sent/received so far
	virtual void Clean(TBool aKeepListening)=0;
	virtual void UpdateView()=0;

	virtual void SetDirection(TDirection aDirection)=0;
	virtual TDirection Direction()=0;
	
	virtual CMLauncherPreferences *Preferences()=0;

	/* The following 2 functions display an error message, LOG it, handle the eventual view switching
	 * and take care of cleaning the Communication (calling ::Clean() functions).
	 * They are in the observer because a message is teared down when we do view switching
	 *
	 * Second function should be used when the message contains also data.
	 * Second function takes ownership of aErrorString
	 */
	virtual void ShowErrorCleanCommL(TMsgType aErrorType, TInt aErrorStringResource, TInt aErr, TBool aClean, TBool aKeepListening=ETrue)=0;
	virtual void ShowErrorCleanCommL(TMsgType aErrorType, HBufC* aErrorString, TInt aErr, TBool aClean, TBool aKeepListening=ETrue)=0;
};

class CCommCommon;

class MMLauncherSocket
{
public:
	MMLauncherSocket(CCommCommon* aNotifier) : iNotifier(aNotifier){};
	virtual ~MMLauncherSocket(){};
	virtual void Clean(TBool aKeepListening)=0;
	virtual TInt Listen()=0;
	virtual TInt Accept()=0;
	virtual TInt Connect(const TDesC8 *addr=NULL)=0;
	virtual TInt Send(const TDesC8 &aDesc, TUint someFlags)=0;
	virtual TInt Read(TDes8 &aDesc)=0;
	virtual TInt Shutdown(RSocket::TShutdown aHow)=0;
	virtual void CancelAll()=0;
	virtual TInt GetMTU(TInt &aSMtu, TInt &aRMtu)=0;
	CCommCommon *iNotifier; //not owned
};

class CMLauncherBTSocket : public CBase, public MMLauncherSocket
{
public:
	static CMLauncherBTSocket* NewL(CCommCommon* aNotifier);
	virtual ~CMLauncherBTSocket();
	virtual void Clean(TBool aKeepListening);

	virtual TInt Listen();
	virtual TInt Accept();
	virtual TInt Connect(const TDesC8 *addr=NULL);
	virtual TInt Send(const TDesC8 &aDesc, TUint someFlags);
	virtual TInt Read(TDes8 &aDesc);
	virtual TInt Shutdown(RSocket::TShutdown aHow);
	virtual void CancelAll();
	virtual TInt GetMTU(TInt &aSMtu, TInt &aRMtu);

	static void GetDeviceAddressL(TInt aService, TBTDevAddr& aAddr,TBTDeviceName& aName,TBTDeviceClass& aClass);
private:
	CMLauncherBTSocket(CCommCommon* aNotifier);
public:
	RSocketServ iSocksrv;
	CBluetoothSocket *iSocket;
	CBluetoothSocket *iListener;
	TSdpServRecordHandle iRech;
	TBool iListening;

#ifndef __USE_L2CAP
	TSockXfrLength iXfr;
#endif
};

class CMLauncherIPSocket : public CActive, public MMLauncherSocket
{
public:
	static CMLauncherIPSocket* NewL(CCommCommon* aNotifier);
	virtual ~CMLauncherIPSocket();
	virtual void Clean(TBool aKeepListening);

	virtual TInt Listen();
	virtual TInt Accept();
	virtual TInt Connect(const TDesC8 *addr=NULL);
	virtual TInt Send(const TDesC8 &aDesc, TUint someFlags);
	virtual TInt Read(TDes8 &aDesc);
	virtual TInt Shutdown(RSocket::TShutdown aHow);
	virtual void CancelAll();
	virtual TInt GetMTU(TInt &aSMtu, TInt &aRMtu);
protected:
	virtual void RunL();
	virtual void DoCancel();
	virtual TInt RunError(TInt aError);
private:
	CMLauncherIPSocket(CCommCommon* aNotifier);
	TInt GetIP(const TDesC8& aAddress,TSockAddr& aAddr);
public:
	enum TState
	{
		ENotConnected,
		EResolving,
		EConnecting,
		ESendingRequest,
		EReceivingResponse,
		EDisconnecting
		/*
		EAnswerOK,
		EAnswerError,
		EData,
		*/
	};

    // Current engine state
    TState iState;

	RSocketServ iSocksrv;
	RSocket iSocket;
	RSocket iListener;
	TSockXfrLength iXfr;
};

/* Communication independent class
 */
class CCommCommon : public CActive, public MBluetoothSocketNotifier
{
	friend class CMLauncherIPSocket; //to have access to Handle* functions
public:
	enum TConnectionType
	{
		EBTConnection=1,
		EIPConnection
	};
	enum TActiveRequest
	{
		EActiveRequestTransferPlaylist=201,
		EActiveRequestTransferApplication,
		EActiveRequestSendURL
	};
protected:
	enum TBTCommands
	{
		ECmdGetVersion=101,
		ECmdVersionFollows,
		ECmdGetSISX,
		ECmdSISXFollows,
		ECmdDoneSending,
		ECmdDone,
		ECmdFileFollows,
		ECmdReceivedOK,
		ECmdSendAgain,
		//commands & status as well
		ECmdCancelFile,
		ECmdCancelAll,
		ECmdCancelNoSpace,
		//status
		EPeerCanceledFile,
		EPeerCanceledAll,
		EPeerCanceledNoSpace
		//ECmdContinue
	};
	enum TBTStatus
	{
		EStatusGetVersionSent=1001,
		EStatusStartSendFile,
		EStatusSendingFile,
		EStatusReceiveOKorSendAgain,
		EStatusDone,
		EStatusReceiveCommand,
		EStatusReceiveGetVersion,
		EStatusReceiveVersionFollows,
		EStatusInterpretCommand,
		EStatusGotMoreData,
		EStatusReceivedFileData,
		EStatusReceiveDoneSending,
		EStatusFileReceivedOK,
		EStatusSendFileAgain,
		EStatusSentErrorCode,
		EStatusRecvErrorCode,
		EStatusSendingMessageMultiplePackets
	};
protected:
	CCommCommon();
	virtual ~CCommCommon();
	virtual void Clean(TBool aKeepListening);

	void AllocateBTDataL();

	void PackAndSendMessageL(const TInt aCommand,const TDesC8& aMessage,const TInt aStatus);
	void SendNextPacketL();
	void SendVersionL(TInt aCommand);
	void SendFileL(RFile* aFile,const TDesC8& aMessage);
	void SendFile2L();
	void SendZeroTerminatedCommandL(TInt aCommand);
	void SendCancelL(TUint8 aError,TInt aBytesFree=0,TInt aBytesNeeded=0);
	TInt InterpretCancel();
	void SendingFileDoneL();
	void DoCancelTransfer(); //this is called when the socket is closed

	void RecvDataL(TInt aNextStatus);
	void RecvMoreDataL();
	void ParseCommandL(TInt aExpectedCommand=-1);
	void RecvFileL();

	void InterpretVersionL(TPtr8& aVersionXML);
	void OpenReceivingFileL(TPtr8& aFileData);

	virtual void HandleConnectCompleteL(TInt /*aErr*/) {};
	virtual void HandleAcceptCompleteL(TInt /*aErr*/) {};
	virtual void HandleShutdownCompleteL(TInt /*aErr*/);
	virtual void HandleSendCompleteL(TInt aErr);
	virtual void HandleReceiveCompleteL(TInt aErr);
	virtual void HandleIoctlCompleteL(TInt /*aErr*/) {};
	virtual void HandleActivateBasebandEventNotifierCompleteL(TInt /*aErr*/, TBTBasebandEventNotification& /*aEventNotification*/) {};

    virtual void CommCommandComplete(TInt aErr=0)=0;

public:
	void ShowErrorL(TInt aErrorStringResource,TMsgType aErrorType, TInt aErr);
	void CancelTransferTimeout();
protected:
    MMLauncherSocket *iSocket;
    TConnectionType iConnectionType;
	TInt iMyStatus;
	TInt iTargetStatus; //save iMyStatus here when a message needs to be sent in several packets
	TUint8 *iDataBT;
	TUint8 *iDataFile;
	TPtr8 iPtrDataBT;
	TPtr8 iPtrDataFile;
	TInt iRMTU; //the receiving MTU
	TInt iSMTU; //the sending MTU
	TBool iReadingWriting; //TRUE when we have an active reading or writing operation
	//TRequestStatus iFileStatus;
	//TInt iDataLength;
	TInt iDataFilledLength; //how much of iDataBT is already filled
	TInt iCommand;
	TInt iCancelStatus; //continue, cancel current file, cancel the entire transfer
	RFile* iFile2SendRecv;
	TInt iFile2SendRecvLength;
	TInt iFile2SendRecvBytesReceived;
	TInt iTotalNrFiles;
	TInt iTotalSize;
	TUint iCrc;
	TUint iLast4Bytes; //these are the last 4 bytes received from the peer. Used for error checking
	TInt iErrorMBytesFree; //used only when one of the peer has not enough space for the transfer
	TInt iErrorMBytesNeeded; //used only when one of the peer has not enough space for the transfer

	//TInt iCurrentChunk;
	TInt iSRDataInBuffer;

public:
    MMLauncherTransferNotifier *iObserver; //not owned
	TInt iActiveRequest;
	static CEikonEnv *iee; //not owned
};

class CCommSender : public CCommCommon
{
public:
	static CCommSender* GetInstanceL(); //singleton pattern
	static void DeleteInstance();
	virtual ~CCommSender();
	virtual void Clean(TBool aKeepListening);

	void SendApplicationL(MMLauncherTransferNotifier *aObserver);
	void SendUrlSMSL(MMLauncherTransferNotifier *aObserver);
	void SendPlaylistL(TConnectionType aConnectionType, MMLauncherTransferNotifier *aObserver, CPlaylist *aFilesInPlaylist, const TDesC8 *aConnectionData=NULL);
	void CancelTransfer(TBool aAllFiles); //if aAllFiles is FALSE, we cancel only the current file
protected:
	virtual void RunL();
	virtual void DoCancel();
	virtual TInt RunError(TInt aError);
private:
	CCommSender();
	static CCommSender* instance;
	void SendApplication2L();

	void SendPlaylist2L(); //sends the version
	void SendPlaylist3L(); //sends files, one by one

	void ConnectToBTDeviceL();
	void ConnectToIpL(const TDesC8 *aConnectionData);
	virtual void CommCommandComplete(TInt aErr=0);

private:
	void HandleConnectCompleteL(TInt aErr);
    //void HandleAcceptCompleteL(TInt /*aErr*/) {};
    //void HandleShutdownCompleteL(TInt aErr);
    //void HandleSendCompleteL(TInt aErr);
    //void HandleReceiveCompleteL(TInt aErr);
    //void HandleIoctlCompleteL(TInt aErr) {};
    //void HandleActivateBasebandEventNotifierCompleteL(TInt aErr, TBTBasebandEventNotification& aEventNotification) {};
private:
	RSendAs *iSendas;
	RSendAsMessage *iMessage;

	TInt iCurrentCommand;
	enum TCommands
	{
		ECmdSendPlaylist_Connect=200,
		ECmdSendPlaylist_Version,
		ECmdSendPlaylist_SendFile,
		ECmdSendPlaylist_DoneSending,
		ECmdSendPlaylist_Done
		//ECmdCheck4Updates,
		//ECmdCheck4Updates_Done
	};
	enum TSenderStatus
	{
		ESendAsAddAttachment=1,
		EBTConnect
	};

	//for sending the playlist:
	CPlaylist *iFilesInPlaylist;
	TInt iI; //an index to the above array
};

class CCommReceiver : public CCommCommon
{
public:
	static CCommReceiver* GetInstanceL(); //singleton pattern
	static void DeleteInstance();
	virtual ~CCommReceiver();
	virtual void Clean(TBool aKeepListening);

	//TInt RegisterBTService();
	void ListenL(MMLauncherTransferNotifier *aObserver);
	void AcceptL();
	void CancelTransfer(TBool aAllFiles); //if aAllFiles is FALSE, we cancel only the current file
protected:
	virtual void RunL();
	virtual void DoCancel();
private:
	CCommReceiver();
	static CCommReceiver* instance;

	void ReceiveApplicationL();//TODO should this leave??
	virtual void CommCommandComplete(TInt aErr=0);
private:
	//void HandleConnectCompleteL(TInt /*aErr*/) {};
    void HandleAcceptCompleteL(TInt aErr);
    //void HandleShutdownCompleteL(TInt aErr);
    //void HandleSendCompleteL(TInt aErr) {};
    //void HandleReceiveCompleteL(TInt aErr) {};
    //void HandleIoctlCompleteL(TInt aErr) {};
    //void HandleActivateBasebandEventNotifierCompleteL(TInt aErr, TBTBasebandEventNotification& aEventNotification) {};
private:

	TInt iCurrentCommand;
	enum TCommands
	{
		ECmdDoneSending=300,
		ECmdSendDone
	};
};
#endif /*MLAUNCHERBT_H_*/
