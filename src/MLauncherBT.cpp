/*
============================================================================
 Name        : MLauncherBT.cpp
 Author      : Florin Lohan
 Copyright   : Florin Lohan
 Description : CBTSender and CBTReceiver implementation
============================================================================
*/
#include <senxmlutils.h>
//
#include <eikenv.h>
#include <stringloader.h>
#include <MLauncher.rsg>
//send as (BT)
#include <rsendas.h>
#include <csendasmessagetypes.h>
#include <rsendasmessage.h>
#include <btmsgtypeuid.h>
#include <senduiconsts.h>
#include <in_sock.h> //Internet sockets

#include <sys/types.h> //for the types used in in.h
#include <netinet/in.h> //ntohl
//#include <libc/stdlib.h> //atoi

#include "MLauncherBT.h"
#include "MLauncherPreferences.h"
#include "XMLparsing.h"
#include "TrackInfo.h"
#include "log.h"
#include "defs.h"
#include "urls.h"
#include "MLauncher.pan"

_LIT(KApplicationExtension,"application");
const TInt KMLauncherUUID=171717;
const TInt KMLauncherPort=1717;
const TInt KDefaultMTU=500; //only used if we can not get the value of the MTU
const TInt KDefaultMaxMTU=65536;
const TInt KIPPort=1424;
//const TInt KNrChunks=30;
const TInt KBTBufferSize=40960; //40KB

const TInt URL_SIZE=256; // TODO: check this
const TInt KMB=1048576;
//TInt KMaxTransmitMtu;
//Res = iSocket.GetOpt(KL2CAPGetMaxOutboundMTU, KSolBtL2CAP, KMaxTransmitMtu);


///////////////////////////// CMLauncherBTSocket

CMLauncherBTSocket* CMLauncherBTSocket::NewL(CCommCommon* aNotifier)
{
	//instantiate
	CMLauncherBTSocket *instance=new(ELeave)CMLauncherBTSocket(aNotifier);

	//ConstructL
	//connect to socket server
	User::LeaveIfError(instance->iSocksrv.Connect());
	/*
	if(err)
	{
		//can not connect to ESOCK server
		iNotifier->ShowErrorL(R_ERRMSG_CONNECTESOCK,CCommCommon::EMsgError,err);
		return err;
	};*/
	return instance;
};

CMLauncherBTSocket::CMLauncherBTSocket(CCommCommon* aNotifier) : MMLauncherSocket(aNotifier), iListening(EFalse)
{};

CMLauncherBTSocket::~CMLauncherBTSocket()
{
	LOG(ELogBT,1,"CMLauncherBTSocket::~CMLauncherBTSocket() start");

	Clean(EFalse);
	__ASSERT_ALWAYS(!iListener,Panic(EBTListeningSocketNotNull));

	iSocksrv.Close();
	LOG(ELogBT,-1,"CMLauncherBTSocket::~CMLauncherBTSocket() end");
}

void CMLauncherBTSocket::Clean(TBool aKeepListening)
{
	LOG(ELogBT,1,"CMLauncherBTSocket::Clean() start (aKeepListening=%d)",aKeepListening);

	if(!aKeepListening && iListening)
	{
		__ASSERT_ALWAYS(iListener,Panic(EBTListeningSocketNull));
		delete iListener;
		iListener=NULL;
		//unregister from SDP database
#ifndef _FLL_SDK_VERSION_50_
		RSdp dbsrv;
		RSdpDatabase db;
		TInt err;
		err=dbsrv.Connect();
		if(err)return;
		err=db.Open(dbsrv);
		if(err)return;
		TRAP(err,db.DeleteRecordL(iRech)); //we do not care if it leaves (e.g. non-existant record)
		//close SDP database
		db.Close();
		dbsrv.Close();
#endif
		iListening=EFalse;
	};

	if(iSocket)
	{
		iSocket->CancelAll();
		delete iSocket;
		iSocket=NULL;
	};

	LOG(ELogBT,-1,"CMLauncherBTSocket::Clean() end");
};

TInt CMLauncherBTSocket::Listen()
{
	//first, register the BT service
	// TODO: address leave and error issues
	RSdp dbsrv;
	RSdpDatabase db;
	TInt err;
	LOG(ELogBT,1,"CMLauncherBTSocket::Listen: start");
	err=dbsrv.Connect();
	if(err)
	{
		//can not connect to BT services database
		iNotifier->ShowErrorL(R_ERRMSG_CONNECTBTDBSERVER,EMsgError,err);
		LOG(ELogBT,-1,"Can not connect to BT services DB");
		return -10001;
	};

	err=db.Open(dbsrv);
	if(err)
	{
		//can not open the BT services database
		iNotifier->ShowErrorL(R_ERRMSG_OPENBTDB,EMsgError,err);
		LOG(ELogBT,-1,"Can not open the BT services DB");
		return -10002;
	};

	//register our server
	TUUID MLauncherUUID(KMLauncherUUID);
	db.CreateServiceRecordL(MLauncherUUID,iRech);

	//close
	db.Close();
	dbsrv.Close();
	LOG(ELogBT,0,"Service registering ended.");
	iListening=ETrue;

	//now start listening
	LOG(ELogBT,0,"Listen: start");
	//addr.SetPort(KL2CAPPassiveAutoBind);

	TBTSockAddr btAddr;
	TBTServiceSecurity btSec;
	btSec.SetAuthentication(ETrue);
	btSec.SetAuthorisation(ETrue);
	btSec.SetEncryption(EFalse);
	btSec.SetDenied(EFalse);
	//btAddr.SetBTAddr(addr);
	btAddr.SetPort(KMLauncherPort);
	//btAddr.SetPort(KL2CAPPassiveAutoBind);
	btAddr.SetSecurity(btSec);

#ifdef __USE_L2CAP
	TRAP(err,{iListener=CBluetoothSocket::NewL(*iNotifier, iSocksrv, KSockSeqPacket, KL2CAP);});
#else
	TRAP(err,{iListener=CBluetoothSocket::NewL(*iNotifier, iSocksrv, KSockStream, KRFCOMM);});
#endif
	if(err)
	{
		//can not create a BT socket
	    LOG(ELogBT,-1,"Can not create the BT socket");
		iNotifier->ShowErrorL(R_ERRMSG_CREATEBTSOCKET,EMsgError,err);
		return err;
	};
	/* commented because it does not work
#ifdef __USE_L2CAP
	TInt res;
	res=iListener->SetOpt(KL2CAPGetOutboundMTU, KSolBtL2CAP, 600);
	LOG("Receiver: SetOpt outbound returned %d",res);
	iListener->SetOpt(KL2CAPInboundMTU, KSolBtL2CAP, 600);
	LOG("Receiver: SetOpt inbound returned %d",res);
#endif
    */

	// Configure the listener port.
	err=iListener->Bind(btAddr);
	if(err)
	{
		//can not bind the BT socket
	    LOG(ELogBT,0,"Can not bind the BT socket: %d",err);
	    if(err==KErrInUse)
	    {
	        //BT is in use exclusively. Do not attempt to listen again.
	        __ASSERT_ALWAYS(iNotifier->iObserver,Panic(EBTNoObserver));
	        LOG(ELogBT,-1,"BT used exclusively, do not attempt to listen again");
	        iNotifier->iObserver->ShowErrorCleanCommL(EMsgNote,R_ERRMSG_BINDBTSOCKET_ALREADYINUSE,err,ETrue,EFalse);
	        return err;
	    }
	    else
	    {
	    	iNotifier->ShowErrorL(R_ERRMSG_BINDBTSOCKET,EMsgError,err);
	    	return err;
	    }
	};
	err=iListener->Listen(1);
	if(err)
	{
		//can not start listening (BT socket)
	    LOG(ELogBT,-1,"Can not listen the BT socket: %d",err);
		iNotifier->ShowErrorL(R_ERRMSG_LISTENBTSOCKET,EMsgError,err);
		return err;
	};

	err=Accept();
	LOG(ELogBT,-1,"CMLauncherBTSocket::Listen: end");
	return err;
};

TInt CMLauncherBTSocket::Accept()
{
    LOG(ELogBT,0,"CMLauncherBTSocket::Accept: start");
	// TODO: should not leave
	if(!iListening)
	{
		//not listening. This may happen if listening failed
		return KErrNotReady;
	};
	//__ASSERT_ALWAYS(iListening,Panic(EBTNotListening));
	__ASSERT_ALWAYS(iListener,Panic(EBTListeningSocketNull));
	TInt err;
	TRAP(err,{iSocket=CBluetoothSocket::NewL(*iNotifier, iSocksrv);});
	if(err)
	{
		//can not create a BT socket
		iNotifier->ShowErrorL(R_ERRMSG_CREATEBTSOCKET,EMsgError,err);
		return err;
	};
	return iListener->Accept(*iSocket);
	LOG(ELogBT,0,"CMLauncherBTSocket::Accept: end");
};

TInt CMLauncherBTSocket::Connect(const TDesC8 */*addr*/)
{
	//should not leave
	TInt err;
    TBTDevAddr addr;
    TBTDeviceName name;
    TBTDeviceClass deviceClass;
    TRAP(err,GetDeviceAddressL(KMLauncherUUID,addr,name,deviceClass));
    if(err)return err;


#ifdef __USE_L2CAP
    TRAP(err,
	iSocket=CBluetoothSocket::NewL(*iNotifier,iSocksrv,KSockSeqPacket,KL2CAP);
	  /* commented because it does not work
	TInt xx=1000,res;
	res=iSocket->SetOpt(KL2CAPGetOutboundMTU, KSolBtL2CAP, xx);
	LOG("SetOpt: res=%d",res);
	res=iSocket->SetOpt(KL2CAPInboundMTU, KSolBtL2CAP, xx);
	LOG("SetOpt: res=%d",res);
    */
    ); //TRAP
#else //RFCOMM
    TRAP(err,
	iSocket=CBluetoothSocket::NewL(*this,iSocksrv,KSockStream,KRFCOMM);
    ); //TRAP
#endif
    if(err)return err; //probably KErrNoMemory

	TBTSockAddr btAddr;
	TBTServiceSecurity btSec;
	btSec.SetAuthentication(EFalse);
	btSec.SetAuthorisation(EFalse);
	btSec.SetEncryption(EFalse);
	btSec.SetDenied(EFalse);
	btAddr.SetBTAddr(addr);
	btAddr.SetPort(KMLauncherPort);
	btAddr.SetSecurity(btSec);

	/* commented because it does not work
#ifdef __USE_L2CAP
	res=iSocket->SetOpt(KL2CAPGetOutboundMTU, KSolBtL2CAP, xx);
	LOG("SetOpt: res=%d",res);
	res=iSocket->SetOpt(KL2CAPInboundMTU, KSolBtL2CAP, xx);
	LOG("SetOpt: res=%d",res);
#endif
	*/
	return iSocket->Connect(btAddr);
};

TInt CMLauncherBTSocket::Send(const TDesC8 &aDesc, TUint someFlags)
{
	return iSocket->Send(aDesc,someFlags);
};

TInt CMLauncherBTSocket::Read(TDes8 &aDesc)
{
#ifdef __USE_L2CAP
	return iSocket->Read(aDesc);
#else
	return iSocket->RecvOneOrMore(aDesc,0,iXfr);
#endif
};

TInt CMLauncherBTSocket::Shutdown(RSocket::TShutdown aHow)
{
	return iSocket->Shutdown(aHow);
};

void CMLauncherBTSocket::CancelAll()
{
	if(iListener)
		iListener->CancelAll();
	if(iSocket)
		iSocket->CancelAll();
};

TInt CMLauncherBTSocket::GetMTU(TInt &aSMtu, TInt &aRMtu)
{
	TInt res1,res2;
#ifdef __USE_L2CAP
	res1 = iSocket->GetOpt(KL2CAPInboundMTU,KSolBtL2CAP,aRMtu);
	LOG(ELogBT,0,"GetOpt: res=%d, InboundMtu=%d",res1,aRMtu);
	res2 = iSocket->GetOpt(KL2CAPGetOutboundMTU,KSolBtL2CAP,aSMtu);
	LOG(ELogBT,0,"GetOpt: res=%d, OutboundMtu=%d",res2,aSMtu);
#else
	//res1 = iSocket->GetOpt(KRFCOMMMaximumSupportedMTU,KSolBtRFCOMM,mtuRecv);
	//LOG("GetOpt: res=%d, InboundMtu=%d",res1,mtuRecv);
	res1=res2=KErrNone;
	mtuSend=mtuRecv=KDefaultMaxMTU;
#endif

	if(res1)
		aRMtu=KDefaultMaxMTU;
	if(res2)
		aSMtu=KDefaultMaxMTU;
	
	if(res1+res2)
	{
		LOG(ELogBT,0,"Could not get MTU");
		return KErrNotFound;
	};
	return KErrNone;
};

void CMLauncherBTSocket::GetDeviceAddressL(TInt aService, TBTDevAddr& aAddr,TBTDeviceName& aName,TBTDeviceClass& aClass)
{
	RNotifier btNotifier;
	User::LeaveIfError(btNotifier.Connect());

	TBTDeviceSelectionParamsPckg selectionFilter;
	TBTDeviceResponseParamsPckg selectionResponse;

	selectionFilter().SetUUID(aService);

	TRequestStatus status;
	btNotifier.StartNotifierAndGetResponse(status, KDeviceSelectionNotifierUid, selectionFilter, selectionResponse);
	User::WaitForRequest(status);
	//LOG("GetDeviceAddressL: status=%d",status.Int());

	btNotifier.Close();

	User::LeaveIfError(status.Int());

	// use the response data ....
	aName=selectionResponse().DeviceName();
	aAddr=selectionResponse().BDAddr();
	aClass=selectionResponse().DeviceClass();
}


///////////////////////////// CMLauncherIPSocket

CMLauncherIPSocket* CMLauncherIPSocket::NewL(CCommCommon* aNotifier)
{
	//instantiate
	CMLauncherIPSocket *instance=new(ELeave)CMLauncherIPSocket(aNotifier);

	//ConstructL
	CActiveScheduler::Add(instance);
	//connect to socket server
	User::LeaveIfError(instance->iSocksrv.Connect());
	/*
	if(err)
	{
		//can not connect to ESOCK server
		iNotifier->ShowErrorL(R_ERRMSG_CONNECTESOCK,CCommCommon::EMsgError,err);
		return err;
	};*/
	return instance;
};

CMLauncherIPSocket::CMLauncherIPSocket(CCommCommon* aNotifier) : CActive(CActive::EPriorityStandard), MMLauncherSocket(aNotifier)
    {
	iState=ENotConnected;
	};

CMLauncherIPSocket::~CMLauncherIPSocket()
{
	LOG(ELogBT,1,"CMLauncherIPSocket::~CMLauncherIPSocket() start");
    Clean(EFalse);
	iSocksrv.Close();
	LOG(ELogBT,-1,"CMLauncherIPSocket::~CMLauncherIPSocket() end");
};

void CMLauncherIPSocket::Clean(TBool aKeepListening)
{
	LOG(ELogBT,1,"CMLauncherIPSocket::Clean() start (aKeepListening=%d)",aKeepListening);
	Cancel();
	iSocket.Close();
	if(!aKeepListening)
	{
		iListener.Close();
	};
	LOG(ELogBT,-1,"CMLauncherIPSocket::Clean() end");
};

TInt CMLauncherIPSocket::Listen()
{
	return KErrNotSupported;
};

TInt CMLauncherIPSocket::Accept()
{
	return KErrNotSupported;
};

TInt CMLauncherIPSocket::Connect(const TDesC8 *aAddress)
{
	LOG(ELogBT,-1,"CMLauncherIPSocket::ConnectL: Start");
	TInt err;
	if(iState==ENotConnected)
	{
		//opening the socket can also be done only once
		err = iSocket.Open(iSocksrv, KAfInet, KSockStream, KProtocolInetTcp);
		if(err)
		{
			//can not open socket
	    	iNotifier->ShowErrorL(R_ERRMSG_CREATEIPSOCKET,EMsgError,err);
	    	LOG(ELogBT,-1,"ConnectL: Client sock opened, err=%d",err);
			return KErrNone; //this avoids creating a second error message
		};
		LOG(ELogBT,0,"ConnectL: Client sock opened, err=%d",err);
	}

	//announce to the user that there will be a lengthly operation
	// TODO:

	//get the ip:port
	__ASSERT_ALWAYS(aAddress,Panic(EIPAddressIsNULL));

	TSockAddr addr;
	err=GetIP(*aAddress,addr);

	//connecting
	iState = EConnecting;
	iSocket.Connect(addr, iStatus);
	SetActive();
	LOG(ELogBT,-1,"CMLauncherIPSocket::ConnectL: end");
	return KErrNone;
};

TInt CMLauncherIPSocket::Send(const TDesC8 &aDesc, TUint someFlags)
{
	iState=ESendingRequest;
	/*
	TInt sz=aDesc.Size();
	if(sz==0)
		LOG("Sending zero size!!");
		*/
	iSocket.Send(aDesc,someFlags,iStatus);
	if(!IsActive())
		SetActive();
	return KErrNone;
};

TInt CMLauncherIPSocket::Read(TDes8 &aDesc)
{
	iState=EReceivingResponse;
	//iSocket.Read(aDesc,iStatus);
	iSocket.RecvOneOrMore(aDesc,0,iStatus,iXfr);
	if(!IsActive())
		SetActive();
	return KErrNone;
};

TInt CMLauncherIPSocket::Shutdown(RSocket::TShutdown aHow)
{
	iState=EDisconnecting;
	iSocket.Shutdown(aHow,iStatus);
	SetActive();
	return KErrNone;
};

void CMLauncherIPSocket::CancelAll()
{
	iSocket.CancelAll();
	iListener.CancelAll();
};

TInt CMLauncherIPSocket::GetMTU(TInt &aSMtu, TInt &aRMtu)
{
	LOG(ELogBT,0,"CMLauncherIPSocket::GetMTU: NOT IMPLEMENTED");
	// TODO: Implement this function
	aSMtu=aRMtu=KDefaultMTU;
	return KErrNotFound;
};

void CMLauncherIPSocket::RunL()
{
	TInt err=iStatus.Int();
	if(err)
	    LOG(ELogBT,0,"CMLauncherIPSocket::RunL, err=%d",err);
	//Do something about the error?

	switch(iState)
	{
	case EConnecting:iNotifier->HandleConnectCompleteL(err);break;
	case ESendingRequest:iNotifier->HandleSendCompleteL(err);break;
	case EReceivingResponse:iNotifier->HandleReceiveCompleteL(err);break;
	case EDisconnecting:iNotifier->HandleShutdownCompleteL(err);break;
	default:LOG(ELogBT,0,"CMLauncherIPSocket::RunL: switch(iState): %d not handled",iState);Panic(ESwitchValueNotHandled);
	};
};

void CMLauncherIPSocket::DoCancel()
{

};

TInt CMLauncherIPSocket::RunError(TInt /*aError*/)
{
	return KErrNone;
};

TInt CMLauncherIPSocket::GetIP(const TDesC8& aAddress,TSockAddr& aAddr)
{
	//parse the aAddress, which has the format IP:port or just IP
	//the URL is iURL. The target IP:port is *addr
	//we must keep the iURL constant
	TUint32 ip=0;
	TUint port=KIPPort;//the default port

	TInt end,i,val;
	//TInt length,start;
	TBuf8<URL_SIZE> url;

	url.Copy(aAddress);

	/*
	//assume the http:// in front of the string
	ASSERT(iURL.Find(_L8("http://"))==0);
	start=7;
	//get the length of the url
	length=iURL.Length();

	//get the url without the http://
	url=iURL.Right(length-start);
	*/

	/*
	length = url.Find(_L8(":"));
	// get the ip addr from the url
	if ( length == KErrNotFound )
		{
		iIp = url;
		}
	else
		{
		iIp = url.Left(length);
		}*/

	if ( url[0] >= 48 && url[0] <=57)
		{
		//the IP starts with a number, so we have a numeric IP
		for( i = 3; i>=0; i-- )
			{
			//ip = ip*256+(unsigned)atoi((const char *)url.PtrZ());
			TLex8 lex(url);
			lex.Val(val);
			ip=ip*256+val;

	        LOG(ELogBT,0,"GetIP: ip: %d",val);
			//get the next dot/:
			if ( i )
				{
				end=url.Locate('.');
				url.Delete(0,end+1);
				}
			}
		}
	else
		{
		//the IP does not start with a number!
		ASSERT(0);// TODO: Implement
		};
	//get the port, if any
	end=url.Locate(':');
	if ( end != KErrNotFound )
		{
		//we found a port!
		url.Delete(0,end+1);
		TLex8 lex(url);
		lex.Val(val);
		port=val;
		//port = (unsigned)atoi((const char *)url.PtrZ());
		}

	/*
	//get the iPath, this is what remains
	end = url.Locate('/');
	if ( end != KErrNotFound )
		{
		//we have a path
		url.Delete(0,end);//we do not delete the /
		iPath.Copy(url);
		}
	else
		{
		//there is no path! we make it /
		iPath.Copy(_L8("/"));
		}
	*/

	aAddr =TInetAddr(ip,port);

	return KErrNone;
};

///////////////////////////// CComm*
CCommSender* CCommSender::instance=NULL;
CCommReceiver* CCommReceiver::instance=NULL;

_LIT8(KSendFileMessageTemplate,"<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n<file name=\"%S\" size=\"%d\"/>");

//_LIT8(KSendFileMessagePlusTotalsTemplate,"<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n<file name=\"%S\" size=\"%d\" totalnrfiles=\"%d\" totalsize=\"%d\"/>");
_LIT8(KSendFileMessagePlusTotalsTemplate_start,"<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n<file name=\"%S\" size=\"%d\" totalnrfiles=\"%d\" totalsize=\"%d\">\n");
_LIT8(KSendFileMessagePlusTotalsTemplate_fs,"<fs size=\"%d\"/>\n");
_LIT8(KSendFileMessagePlusTotalsTemplate_end,"</file>");

const TInt KSendFileMessageExtraSize=300; //256 for the filename and the rest for the size
const TInt KSendFsMessageExtraSize=10; //10 chars should be enough for a file length

///////////////////////////// CCommCommon
CEikonEnv* CCommCommon::iee=NULL;

CCommCommon::CCommCommon() : CActive(CActive::EPriorityStandard), //the low priority is to allow the view server to run
                         iMyStatus(0),
                         iDataBT(NULL), iDataFile(NULL),
                         iPtrDataBT(iDataBT,0,0),iPtrDataFile(iDataFile,0,0),
                         iFile2SendRecv(NULL)
{
	__ASSERT_DEBUG(iee,Panic(EieeIsNull));
}

CCommCommon::~CCommCommon()
{
	LOG(ELogBT,1,"CCommCommon::~CCommCommon() start");
	Clean(EFalse);

	__ASSERT_ALWAYS(!iSocket,Panic(EBTSocketNotNull));
	__ASSERT_ALWAYS(!iDataBT,Panic(EBTDataBTNotNull));
	__ASSERT_ALWAYS(!iDataFile,Panic(EBTDataFileNotNull));
	__ASSERT_ALWAYS(!iFile2SendRecv,Panic(EBTFile2SendRecvNotNull));
	LOG(ELogBT,-1,"CCommCommon::~CCommCommon() end");
}

void CCommCommon::Clean(TBool aKeepListening)
{
	LOG(ELogBT,1,"CCommCommon::Clean() start (aKeepListening=%d)",aKeepListening);
	Cancel();
	//delete stuff
	if(iFile2SendRecv)
	{
		iFile2SendRecv->Close();
		delete iFile2SendRecv;
		iFile2SendRecv=NULL;
	};
	delete iDataBT;
	delete iDataFile;
	iDataBT=NULL;
	iDataFile=NULL;

	if(aKeepListening)
	    iSocket->Clean(aKeepListening);
	else
	{
		delete iSocket;
		iSocket=NULL;
	};

	iMyStatus=0;
	LOG(ELogBT,-1,"CCommCommon::Clean() end");
}

void CCommCommon::ShowErrorL(TInt aErrorStringResource,TMsgType aErrorType,TInt aErr)
{
	//this is a wrapper on top of the ShowErrorClearCommL() function
	TBool clean=EFalse;
	if(aErrorType==EMsgError || aErrorType==EMsgErrorPersistent)
		clean=ETrue;
	__ASSERT_ALWAYS(iObserver,Panic(EBTNoObserver));
	iObserver->ShowErrorCleanCommL(aErrorType,aErrorStringResource,aErr,clean);
};

void CCommCommon::AllocateBTDataL()
{
	iSocket->GetMTU(iSMTU,iRMTU);

	iDataBT=new(ELeave)TUint8[KBTBufferSize];
	iPtrDataBT.Set(iDataBT,0,KBTBufferSize);
}

void CCommCommon::PackAndSendMessageL(const TInt aCommand,const TDesC8& aMessage,const TInt aStatus)
{
	TInt msgLength=aMessage.Size();
	LOG(ELogBT,1,"PackAndSendMessageL: start (MsgLength: %d)",msgLength);
	//format: command(1 byte), message length (4bytes), message

	TInt allocatedLength=msgLength+5;
	if(allocatedLength>iSMTU)
		allocatedLength=iSMTU; //the message is bigger than what we can send, we need to split it
	HBufC8 *message=HBufC8::NewLC(allocatedLength);
	message->Des().SetLength(5);
	message->Des()[0]=aCommand;

	TUint8 len;
	len=msgLength%256;
	message->Des()[4]=len;
	msgLength>>=8;
	len=msgLength%256;
	message->Des()[3]=len;
	msgLength>>=8;
	len=msgLength%256;
	message->Des()[2]=len;
	msgLength>>=8;
	len=msgLength%256;
	message->Des()[1]=len;

	msgLength=aMessage.Size();
	if(msgLength+5>iSMTU)
	{
		message->Des().Append(aMessage.Left(allocatedLength-5));
		//copy the rest of the message into iDataBT and send later
		msgLength-=(allocatedLength-5);
		if(KBTBufferSize<msgLength)
			Panic(EBTBufferTooSmall);
		
		iPtrDataBT.Set(iDataBT,0,KBTBufferSize);
		iPtrDataBT.Copy(aMessage.Right(msgLength));
		//
		iTargetStatus=aStatus;
		iMyStatus=EStatusSendingMessageMultiplePackets;
		
		//some logging:
		TBuf<1000> m1;
		TBuf<5000> m2;
		LOG(ELogBT,0,"m1 length: %d",message->Length());
		LOG(ELogBT,0,"m2 length: %d",iPtrDataBT.Length());
		m1.Copy(*message);
		m2.Copy(iPtrDataBT);
		LOG(ELogBT,0,"msg2send:\n%S",&m1);
		LOG(ELogBT,0,"second msg2send:\n%S",&m2);
	}
	else
	{
		message->Des().Append(aMessage);
		iMyStatus=aStatus;
	};
	//message is constructed, send it
	iSocket->Send(*message,0);
	CleanupStack::PopAndDestroy(message);
	LOG(ELogBT,-1,"PackAndSendMessageL: end");
}

void CCommCommon::SendNextPacketL()
{
	TInt msgLength=iPtrDataBT.Size();
	LOG(ELogBT,1,"SendNextPacketL: start (remaining MsgLength: %d)",msgLength);
	TInt sendLength=msgLength;
	if(sendLength>iSMTU)
		sendLength=iSMTU;

	iPtrDataBT.Set(iDataBT,sendLength,sendLength);
	//message is constructed, send it
	iSocket->Send(iPtrDataBT,0);

	//take care if iPtrDataBT and status
	if(sendLength!=msgLength)
	{
		//we still need to send data next time
		iPtrDataBT.Set(iDataBT,msgLength,msgLength);
		iPtrDataBT.Delete(0,sendLength);
	}
	else
	{
		//we sent all the data
		iMyStatus=iTargetStatus;
		iTargetStatus=0;
	};
	LOG(ELogBT,-1,"SendNextPacketL: end");
}

void CCommCommon::SendVersionL(TInt aCommand)
{
	LOG(ELogBT,1,"SendVersionL: start");
	//the socket is connected, send the command!

	TUint msgLength=iObserver->Preferences()->GetVersionDescriptionSize();

	HBufC8 *tmpmsg=HBufC8::NewLC(msgLength);
	TPtr8 ptr(tmpmsg->Des());
	iObserver->Preferences()->CopyVersionDescription(ptr);

	__ASSERT_ALWAYS(aCommand==ECmdVersionFollows||aCommand==ECmdGetVersion,Panic(EBTCurrentCommandNotVersion));
	if(aCommand==ECmdGetVersion)PackAndSendMessageL(aCommand,*tmpmsg,EStatusGetVersionSent);
	else PackAndSendMessageL(aCommand,*tmpmsg,EStatusReceiveCommand);
	CleanupStack::PopAndDestroy(tmpmsg);
	LOG(ELogBT,-1,"SendVersionL: end");
}

void CCommCommon::RecvDataL(TInt aNextStatus)
{
	LOG(ELogBT,1,"RecvDataL: start");
	__ASSERT_ALWAYS(iDataBT,Panic(EBTiDataBTNULL));
	//we receive data from the peer
	iPtrDataBT.Set(iDataBT,0,iRMTU);
	//#LOG("MaxSize: %d",iPtrDataBT.MaxSize());
	iMyStatus=aNextStatus;
	iDataFilledLength=0;

	iSocket->Read(iPtrDataBT);
	LOG(ELogBT,-1,"RecvDataL: end");
}

void CCommCommon::RecvMoreDataL()
{
	//we need to append some data to existing data
	LOG(ELogBT,1,"RecvMoreDataL: start (already filled=%d)",iPtrDataBT.Length());
	iDataFilledLength=iPtrDataBT.Length();
	iPtrDataBT.Set(iDataBT+iDataFilledLength,0,iPtrDataBT.MaxLength()-iDataFilledLength);
	iMyStatus=EStatusGotMoreData;

	iSocket->Read(iPtrDataBT);
	LOG(ELogBT,-1,"RecvMoreDataL: end");
}

void CCommCommon::ParseCommandL(TInt aExpectedCommand)
{
	//we have the data in iData and iPtrData
	LOG(ELogBT,1,"ParseCommandL: start (data size: %d)",iPtrDataBT.Length());
	if(iPtrDataBT.Length()<5)
	{
		LOG(ELogBT,-1,"ParseCommandL: end (asked for more data)");
		RecvMoreDataL();
		return;
	};
	iCommand=iPtrDataBT[0];
	LOG(ELogBT,0,"Received command: %d",iCommand);
	TInt dataConsumed=1;

	if(aExpectedCommand>0 && iCommand!=aExpectedCommand)
	{
		//bad command response
		// TODO: handle somehow
		LOG(ELogBT,0,"ParseCommandL: wrong command, expected: %d, received %d",aExpectedCommand,iCommand);
	};

	if(iCommand==ECmdDoneSending)
	{
		//the remaining 4 bytes are CRC sum
		//TODO
		dataConsumed+=4;
		if(iFile2SendRecv)
		{
			iFile2SendRecv->Close();
			delete iFile2SendRecv;
			iFile2SendRecv=NULL;
			LOG(ELogBT,0,"Recv file successfully closed.");
		};
		SendZeroTerminatedCommandL(ECmdReceivedOK);
		//TODO: SendZeroTerminatedCommandL(ECmdSendAgain);
	}
	else if(iCommand==ECmdDone)
	{
		//the peer has finished its job
		dataConsumed+=4;
		iPtrDataBT.Delete(0,dataConsumed);
		dataConsumed=0;
		CommCommandComplete();
	}
	else
	{
		__ASSERT_ALWAYS(iPtrDataBT.Length()-dataConsumed>=4,Panic(EBTDataDissapeared));
		//the next 4 bytes are command length
		TInt msgLength=iPtrDataBT[1];
		msgLength<<=8;
		msgLength+=iPtrDataBT[2];
		msgLength<<=8;
		msgLength+=iPtrDataBT[3];
		msgLength<<=8;
		msgLength+=iPtrDataBT[4];
		dataConsumed+=4;
		LOG(ELogBT,0,"ParseCommandL: Message to receive: %d bytes.",msgLength);

		if(iPtrDataBT.Length()-dataConsumed<msgLength)
		{
			//we need more data, we can not interpret this.
			RecvMoreDataL();
			LOG(ELogBT,-1,"ParseCommandL: end (asked for more data)");
			return;
		};

		//interpret this command
		switch(iCommand)
		{
		case ECmdGetVersion:
		{
			LOG(ELogBT,0,"ReceivedCommand: GetVersion");
			TPtr8 versionXML(iDataBT+dataConsumed,msgLength,msgLength);
			InterpretVersionL(versionXML);
			dataConsumed+=msgLength;
			iPtrDataBT.Delete(0,dataConsumed);
			SendVersionL(ECmdVersionFollows);
		};break;
		case ECmdVersionFollows:
		{
			LOG(ELogBT,0,"ReceivedCommand: VersionFollows");
			TPtr8 versionXML(iDataBT+dataConsumed,msgLength,msgLength);
			InterpretVersionL(versionXML);
			dataConsumed+=msgLength;
			iPtrDataBT.Delete(0,dataConsumed);
			//we do nothing more here
		};break;
		case ECmdFileFollows:
		{
			LOG(ELogBT,0,"ReceivedCommand: File Follows");
			//we need to prepare to receive the file
			TPtr8 recvFileData(iDataBT+dataConsumed,msgLength,msgLength);
			TRAPD(err,OpenReceivingFileL(recvFileData));
			if(!err)
			{
				dataConsumed+=msgLength;
				iPtrDataBT.Delete(0,dataConsumed);
				RecvFileL();
			}
			else if(err!=KErrDiskFull)User::Leave(err);
		};break;
		case ECmdReceivedOK:
		{
			//we have sent the file OK
			if(iFile2SendRecv)
			{
				iFile2SendRecv->Close();
				delete iFile2SendRecv;
				iFile2SendRecv=NULL;
				LOG(ELogBT,0,"Sent file successfully closed.");
			};
			CommCommandComplete();
		};break;
		case ECmdSendAgain:
		{
			//sending the file failed! We send again
			if(iFile2SendRecv)
			{
				iFile2SendRecv->Close();
				delete iFile2SendRecv;
				iFile2SendRecv=NULL;
				LOG(ELogBT,0,"Sent file successfully closed.");
			};
			CommCommandComplete(-1);
		};break;
		case ECmdCancelFile:
		{
			//
			LOG(ELogBT,0,"ReceivedCommand: Cancel File");
		};break;
		case ECmdCancelAll:
		{
			//
			LOG(ELogBT,0,"ReceivedCommand: Cancel All");
		};break;
		default:
		{
			LOG(ELogBT,0,"Unhandled command: %d",iCommand);
			Panic(EBTNotImplemented);
		};
		};
	};

	LOG(ELogBT,-1,"ParseCommandL: end");
}

/*
void CCommCommon::RecvFileL()
{
	LOG("RecvFileL: start");
	TInt dataLength=iPtrDataBT.Length();
	if(iObserver)
		iObserver->MoreDataTransfered(dataLength);
	//we have data in iPtrData, write it to the file
	if(dataLength+iFile2SendRecvBytesReceived <= iFile2SendRecvLength)
	{
		//not going past the end of the file
		if(iFile2SendRecv && dataLength>0)iFile2SendRecv->Write(iPtrDataBT);
		iFile2SendRecvBytesReceived+=dataLength;
		iPtrDataBT.Zero();
		LOG("Dumped %d bytes into the file (total=%d)",dataLength,iFile2SendRecvBytesReceived);
		RecvDataL(EStatusReceivedFileData);
	}
	else
	{
		//we go past the end of the file. We should only write the file into the
		TInt data2write=iFile2SendRecvLength-iFile2SendRecvBytesReceived;
		if(iFile2SendRecv)iFile2SendRecv->Write(iPtrDataBT,data2write);
		iFile2SendRecvBytesReceived=iFile2SendRecvLength;
		LOG("Dumped last %d bytes into the file",data2write);
		iPtrDataBT.Delete(0,data2write);
		dataLength=iPtrDataBT.Length();
		LOG("Remaining bytes in buffer: %d",dataLength);
		if(dataLength>0)ParseCommandL(ECmdDoneSending);
		else RecvDataL(EStatusReceiveDoneSending);
	};
	LOG("RecvFileL: end");
}
*/


void CCommCommon::RecvFileL()
{
	LOG(ELogBT,1,"RecvFileL: start");
	__ASSERT_ALWAYS(iDataBT && iDataFile,Panic(EBTiDataFileNotAllocated));

	TInt dataLength=iPtrDataBT.Length();
	iSRDataInBuffer+=dataLength;

	LOG(ELogBT,0,"dataLength: %d totalDataLength: %d",dataLength,iSRDataInBuffer);

	if(iSRDataInBuffer+iFile2SendRecvBytesReceived >= iFile2SendRecvLength || iSRDataInBuffer+iRMTU>KBTBufferSize)
	{
		//we are running out of receiving data. We should wait for the previous write operation to end
		if(iReadingWriting)
		{
			//previous write operation did not end
			iReadingWriting=EFalse; //this will make the RunL function to call us

			LOG(ELogBT,-1,"RecvFileL: end (waiting for write to file to end)");
			return;
		};
		//if we are here, it means that the previous write is complete

		//switch the iDatas if needed
		//LOG0("We are here, switching buffers (iReadingWriting=%d)",iReadingWriting);
		__ASSERT_ALWAYS(!iReadingWriting,Panic(EBTiReadingWritingNotFalse));
		//if dataLength
		TUint8 *tmp;
		//we suppose there is no data into iDataFile
		tmp=iDataBT;
		iDataBT=iDataFile;
		iDataFile=tmp;
		//iPtrDataBT.Set(iDataBT,0,iMTU);
		iPtrDataFile.Set(iDataFile,iSRDataInBuffer,KBTBufferSize);
		//#LOG("iFile2SendRecvBytesReceived: %d, iFile2SendRecvLength: %d",iFile2SendRecvBytesReceived,iFile2SendRecvLength);
		//start writing next data
		if(iSRDataInBuffer+iFile2SendRecvBytesReceived <= iFile2SendRecvLength)
		{
			//not going past the end of the file
			if(iFile2SendRecv && iSRDataInBuffer>0)
			{
				iReadingWriting=ETrue;
				iFile2SendRecv->Write(iPtrDataFile, iStatus);
			    SetActive();
			};
			iFile2SendRecvBytesReceived+=iSRDataInBuffer;
			LOG(ELogBT,0,"Dumping %d bytes into the file (total=%d)",iSRDataInBuffer,iFile2SendRecvBytesReceived);
			iSRDataInBuffer=0;
		}
		else
		{
			//we go past the end of the file. We should only write part of the data into the file
			TInt data2write=iFile2SendRecvLength-iFile2SendRecvBytesReceived;
			//copy the remaining data to iDataBT
			iPtrDataBT.Copy(iPtrDataFile.Right(iSRDataInBuffer-data2write));

			//we have a sync write, otherwise it would be too complicated
			if(iFile2SendRecv)iFile2SendRecv->Write(iPtrDataFile,data2write);//synchronous
			iFile2SendRecvBytesReceived=iFile2SendRecvLength;
			iSRDataInBuffer=0; //needed?
			//#LOG("Dumped last %d bytes into the file",data2write);

			dataLength=iPtrDataBT.Length();
			//#LOG("Remaining bytes in buffer: %d",dataLength);

			if(iObserver)
				iObserver->MoreDataTransfered(data2write);

			ParseCommandL(ECmdDoneSending);
			LOG(ELogBT,-1,"RecvFileL: end (recv file ended)");
			return;
		};
	};
	if(iObserver)
		iObserver->MoreDataTransfered(dataLength);

   
	iPtrDataBT.Set(iDataBT+iSRDataInBuffer,0,iRMTU);

	iMyStatus=EStatusReceivedFileData;
	iSocket->Read(iPtrDataBT);

	LOG(ELogBT,-1,"RecvFileL: end");
}

void CCommCommon::SendFileL(RFile* aFile,const TDesC8& aMessage)
{
	//we send the file
	LOG(ELogBT,1,"SendFileL: start");
	__ASSERT_ALWAYS(iFile2SendRecv==NULL,Panic(EBTFileNotNULL));
	iFile2SendRecv=aFile;
	PackAndSendMessageL(ECmdFileFollows,aMessage,EStatusStartSendFile);
	LOG(ELogBT,-1,"SendFileL: end");
}

/*
void CCommCommon::SendFile2L()
{
	LOG("SendFile2L: start");
	//read data and send it
	if(iMyStatus==EStatusStartSendFile)
	{
		//prepare iPtrData
		iPtrDataBT.Set(iDataBT,0,iMTU);
		iCrc=0;
	};


	//read
	iFile2SendRecv->Read(iPtrDataBT);
	if(iPtrDataBT.Size())
	{
		TUint32 crc;
		Mem::Crc32(crc,iPtrDataBT.Ptr(),iPtrDataBT.Size());
		iCrc+=crc;
		//write
		LOG("sending %d bytes ...",iPtrDataBT.Size());
		iMyStatus=EStatusSendingFile;
		iSocket->Send(iPtrDataBT,0);
		//announce the observer
		if(iObserver)
			iObserver->MoreDataTransfered(iPtrDataBT.Size());
	}
	else
	{
		//this is the end of the file.
		SendingFileDoneL();
	};
	LOG("SendFile2L: end");
}
*/

void CCommCommon::SendFile2L()
{
	LOG(ELogBT,1,"SendFile2L: start");
	__ASSERT_ALWAYS(iDataBT && iDataFile,Panic(EBTiDataFileNotAllocated));

	//read data and send it
	if(iMyStatus==EStatusStartSendFile)
	{
		iMyStatus=EStatusSendingFile;
		//read data synchronously into iPtrDataFile
		iPtrDataFile.Set(iDataFile,0,KBTBufferSize);
		iCrc=0;
		iReadingWriting=EFalse;
		iFile2SendRecv->Read(iPtrDataFile);//synchronous
		iSRDataInBuffer=iPtrDataFile.Length();
		//LOG("Data was read synchronously");
	};

	//only switch the iDatas if iCurrentChunk==KNrChunks
	if(iSRDataInBuffer==KBTBufferSize)
	{
		LOG0("SendFile2L: before panic?");
		__ASSERT_ALWAYS(!iReadingWriting,Panic(EBTiReadingWritingNotFalse));
		iDataFilledLength=iPtrDataFile.Length();
		TUint8 *tmp;
		//we suppose there is no data into iDataBT
		tmp=iDataBT;
		iDataBT=iDataFile;
		iDataFile=tmp;
		iPtrDataFile.Set(iDataFile,0,KBTBufferSize);
		iSRDataInBuffer=0;

		//start reading next data
		iReadingWriting=ETrue;
		iFile2SendRecv->Read(iPtrDataFile, iStatus);
		SetActive();
		LOG(ELogBT,0,"Pointers were switched and data is being read");
	};

	TInt dataLength=iDataFilledLength-iSRDataInBuffer;
	LOG(ELogBT,0,"iDataFilledLength: %d, dataLength: %d",iDataFilledLength,dataLength);

	if(dataLength>0)
	{
		//TUint32 crc;
		//Mem::Crc32(crc,iPtrDataBT.Ptr(),iPtrDataBT.Size());
		//iCrc+=crc;
		//write
		//#LOG("sending %d bytes ...",dataLength);

		//set the iPtrData
		if(dataLength>iSMTU)
			dataLength=iSMTU;
		LOG(ELogBT,0,"Data length here: %d",dataLength);

		iPtrDataBT.Set(iDataBT+iSRDataInBuffer,dataLength,KBTBufferSize);
		//... and send the current data
		iSocket->Send(iPtrDataBT,0);
		iSRDataInBuffer+=dataLength;
		LOG(ELogBT,0,"Data was sent, sent now from this buffer: %d",iSRDataInBuffer);
		//announce the observer
		if(iObserver)
			iObserver->MoreDataTransfered(dataLength);
	}
	else
	{
		//this is the end of the file.
		SendingFileDoneL();
	};
	LOG(ELogBT,-1,"SendFile2L: end");
}

void CCommCommon::SendingFileDoneL()
{
	//we send "DoneSending" command: ECmdDoneSending + 4 CRC bytes
	LOG(ELogBT,1,"SendingFileDoneL: start");

	HBufC8 *message=HBufC8::NewLC(5);
	message->Des().SetLength(5);
	message->Des()[0]=ECmdDoneSending;

	TUint8 crc;
	TUint tmpCrc=iCrc;
	crc=tmpCrc%256;
	message->Des()[4]=crc;
	tmpCrc>>=8;
	crc=tmpCrc%256;
	message->Des()[3]=crc;
	tmpCrc>>=8;
	crc=tmpCrc%256;
	message->Des()[2]=crc;
	tmpCrc>>=8;
	crc=tmpCrc%256;
	message->Des()[1]=crc;

	//message is constructed, send it
	iMyStatus=EStatusReceiveOKorSendAgain;
	iSocket->Send(*message,0);
	CleanupStack::PopAndDestroy(message);
	LOG(ELogBT,-1,"SendingFileDoneL: end");

}

void CCommCommon::SendZeroTerminatedCommandL(TInt aCommand)
{
	//we send "Done" command: ECmdDone + 4 zero bytes
	LOG(ELogBT,1,"SendZeroTerminatedCommandL: start (command=%d)",aCommand);
	__ASSERT_ALWAYS(aCommand==ECmdDone||
			        aCommand==ECmdReceivedOK||
			        aCommand==ECmdSendAgain,Panic(EBTCurrentCommandNotZeroTerminated));

	HBufC8 *message=HBufC8::NewLC(5);
	message->Des().SetLength(5);
	message->Des().FillZ();
	message->Des()[0]=aCommand;

	//message is constructed, send it
	switch(aCommand)
	{
	case ECmdDone:iMyStatus=EStatusDone;break;
	case ECmdReceivedOK:iMyStatus=EStatusFileReceivedOK;break;
	case ECmdSendAgain:iMyStatus=EStatusSendFileAgain;break;
	};

	iSocket->Send(*message,0);
	CleanupStack::PopAndDestroy(message);
	LOG(ELogBT,-1,"SendZeroTerminatedCommandL: end");
}

void CCommCommon::InterpretVersionL(TPtr8& aVersionXML)
{
	__ASSERT_ALWAYS(iCommand==ECmdVersionFollows||iCommand==ECmdGetVersion,Panic(EBTCurrentCommandNotVersion));
	//TODO: implement
}

void CCommCommon::OpenReceivingFileL(TPtr8& aFileData)
{
	LOG(ELogBT,1,"OpenReceivingFileL: start (XML has %d bytes)",aFileData.Size());
	//here we interpret the XML data from aFileData and allocate & open iFile2SendRecv
	Xml::CFileDetailsCH* fd=Xml::CFileDetailsCH::NewLC();
	fd->ParseFileDetailsL(aFileData);
	LOG(ELogBT,0,"File (%d bytes): %S",fd->iFileName.Length(),&(fd->iFileName));
	TFileName path;
	//TODO: select one of the receiving paths
	iObserver->Preferences()->GetRecvFilesDirFullPath(path);
	//create the target dir (if it does not exist already
	path.Append(fd->iFileName);
	iee->FsSession().MkDirAll(path);
	iFile2SendRecvLength=fd->iFileSize;
	iFile2SendRecvBytesReceived=0;
	LOG(ELogBT,0,"File2Open (%d bytes): %S",path.Length(),&path);
	LOG(ELogBT,0,"File2Open has %d bytes",iFile2SendRecvLength);
	LOG(ELogBT,0,"Total nr files: %d total bytes: %d",fd->iTotalNrFiles,fd->iTotalSize);
	if(fd->iTotalNrFiles>0 && fd->iTotalSize>0)
	{
		iTotalNrFiles=fd->iTotalNrFiles;
		iTotalSize=fd->iTotalSize;
		LOG(ELogBT,0,"Total nr files %d",iTotalNrFiles);
		LOG(ELogBT,0,"Total bytes %d",iTotalSize);
		// check if there is enough space
		TVolumeInfo info;
		TInt drive;
		TParse parse;
		parse.Set(path,NULL,NULL);
		iee->FsSession().CharToDrive(parse.Drive()[0],drive);
		iee->FsSession().Volume(info,drive);
		if(info.iFree<iTotalSize)
		{
			LOG(ELogBT,0,"TOO LITTLE SPACE: available:%d, needed:%d",info.iFree,iTotalSize);
			SendCancelL(ECmdCancelNoSpace,info.iFree,iTotalSize);
			//leave. It will be catched
			User::Leave(KErrDiskFull);
		};
		//
		if(iObserver)
			iObserver->NewTransferL(iTotalNrFiles,iTotalSize,fd->iFileSizes);
	};

	//open the file
	iFile2SendRecv=new(ELeave)RFile;

	TInt err=iFile2SendRecv->Replace(iee->FsSession(),path,EFileWrite|EFileShareAny);
	if(err)
	{
		iFile2SendRecv->Close();
		delete iFile2SendRecv;
		iFile2SendRecv=NULL;
		LOG(ELogBT,0,"FAILED to open the file (err=%d)!!",err);
	}
	else
	{
		//we allocate the file buffer, if needed
		if(!iDataFile)
		{
			iDataFile=new(ELeave)TUint8[KBTBufferSize];
			iPtrDataFile.Set(iDataFile,0,KBTBufferSize);
			iSRDataInBuffer=0;
		};
	};

	//announce the observer that we have a new file, so that it can be listed
	if(iObserver && !err)
		iObserver->TransferingNewFileL(MMLauncherTransferNotifier::EDirectionReceiving,fd->iFileName,iFile2SendRecvLength);
	CleanupStack::PopAndDestroy(fd);

	LOG(ELogBT,-1,"OpenReceivingFileL: end");
}

void CCommCommon::SendCancelL(TUint8 aError,TInt aBytesFree,TInt aBytesNeeded)
{
	//we send 4 bytes
	LOG(ELogBT,1,"SendCancelL: start (error=%d)",aError);

	HBufC8 *message=HBufC8::NewLC(4);
	message->Des().SetLength(4);
	message->Des().FillZ();
	message->Des()[0]=aError;
	iCancelStatus=aError;

	if(iCancelStatus==ECmdCancelNoSpace)
	{
		//we have to add the difference in space (the sender knows how many it wants to send, it can compute how much we have
		iErrorMBytesNeeded=aBytesNeeded/KMB;
		iErrorMBytesFree=aBytesFree/KMB;
		LOG(ELogBT,0,"MB needed: %d MB available: %d",iErrorMBytesNeeded,iErrorMBytesFree);
		TInt diff=iErrorMBytesNeeded-iErrorMBytesFree;
		message->Des()[1]=diff>>8; //most significant byte
		message->Des()[2]=diff%256;
	};

	//TUint l4b=*(TUint*)(message->Des().Right(4).Ptr());
	//LOG("Message as last4bytes: %x",l4b);

	//message is constructed, send it
	iMyStatus=EStatusSentErrorCode;
	__ASSERT_ALWAYS(iCancelStatus==ECmdCancelAll || iCancelStatus==ECmdCancelNoSpace,Panic(EBTInvalidCancelCommand));

	iSocket->Send(*message,0);
	CleanupStack::PopAndDestroy(message);
	LOG(ELogBT,-1,"SendCancelL: end");
}

TInt CCommCommon::InterpretCancel()
{
	//interpreting cancel means interpreting iLast4Bytes and providing values for
	//iCancelStatus, iErrorMBytesFree, iErrorMBytesNeeded
	//return 0 if cancel is valid, negative otherwise
	TInt err=KErrNotFound;
	iLast4Bytes=ntohl(iLast4Bytes); //ensure we have the right endianness
	TInt cancelStatus=iLast4Bytes>>24;

	LOG(ELogBT,1,"Interpret cancel: cancelStatus=%d, l4b=%x",cancelStatus,iLast4Bytes);
	switch(cancelStatus)
	{
	case ECmdCancelFile:
		if(iLast4Bytes<<8 ==0)
		{
			//valid cancel request
			iCancelStatus=EPeerCanceledFile;
			iErrorMBytesFree=iErrorMBytesNeeded=0;
			err=0;
		};
		break;
	case ECmdCancelAll:
		if(iLast4Bytes<<8 ==0)
		{
			//valid cancel request
			iCancelStatus=EPeerCanceledAll;
			iErrorMBytesFree=iErrorMBytesNeeded=0;
			err=0;
		};
		break;
	case ECmdCancelNoSpace:
		if(iLast4Bytes<<24 ==0)
		{
			//valid cancel request, so far
			iCancelStatus=EPeerCanceledNoSpace;
			TInt tmp=iLast4Bytes<<8;
			tmp>>=16;
			//now we build the difference (a bit more complex, but we do not need to care about the Endian)
			iErrorMBytesNeeded=256*(tmp>>8);
			iErrorMBytesNeeded+=tmp%256;
			LOG(ELogBT,0,"InterpretCancel: difference in MB: %d",iErrorMBytesNeeded);
			//put the true values
			TInt totalSizeMB=iTotalSize/KMB;
			iErrorMBytesFree=totalSizeMB-iErrorMBytesNeeded;
			iErrorMBytesNeeded=totalSizeMB;
			LOG(ELogBT,0,"InterpretCancel: needed on peer: %d, available on peer: %d" ,iErrorMBytesNeeded,iErrorMBytesFree);
			if(iErrorMBytesFree<=iErrorMBytesNeeded)
			    err=0;
			/*
			else
				iErrorMBytesFree=iErrorMBytesNeeded=0;*/
		};
		break;
	};
	LOG(ELogBT,-1,"Interpret cancel: err=%d",err);
	if(iErrorMBytesFree || iErrorMBytesNeeded)
		LOG(ELogBT,0,"Free MB: %d, Needed MB: %d",iErrorMBytesFree,iErrorMBytesNeeded);
	return err;
}

void CCommCommon::DoCancelTransfer()
{
	LOG(ELogBT,1,"DoCancelTransfer: start (iCancelStatus=%d)",iCancelStatus);
	/* here we have 6 cases:
	 * 1. We are receiver and there is no space to receive data (iCancelStatus==ECmdCancelNoSpace)
	 * 2. We are receiver and the user canceled the transfer (iCancelStatus==ECmdCancelAll)
	 * 3. We are receiving and the peer canceled the transfer (iCancelStatus==EPeerCanceledAll)
	 * 4. We are sending and the peer canceled the transfer because it has no space (iCancelStatus==EPeerCanceledNoSpace)
	 * 5. We are sending and the peer canceled the transfer (iCancelStatus==EPeerCanceledAll)
	 * 6. We are sending and the user canceled the transfer (iCancelStatus==ECmdCancelAll)
	 */

	//show error message
	CArrayFixFlat<TInt> *space(NULL);

	if(iCancelStatus==ECmdCancelNoSpace || iCancelStatus==EPeerCanceledNoSpace)
	{
		space=new(ELeave)CArrayFixFlat<TInt>(3);
	    CleanupStack::PushL(space);

	    if(iErrorMBytesFree==iErrorMBytesNeeded)
			iErrorMBytesNeeded++;
		space->AppendL(iErrorMBytesFree);
		space->AppendL(iErrorMBytesNeeded);
		TInt diff=iErrorMBytesNeeded-iErrorMBytesFree;
		space->AppendL(diff);
	};

	HBufC* noteText(NULL);
	TMsgType msgType(EMsgErrorPersistent);

	switch(iCancelStatus)
	{
	case ECmdCancelNoSpace:
		noteText=StringLoader::LoadL(R_ERRMSG_NOTENOUGHSPACE,*space);
		msgType=EMsgErrorPersistent;
		break;
	case EPeerCanceledNoSpace:
		noteText=StringLoader::LoadL(R_ERRMSG_PEERNOTENOUGHSPACE,*space);
		msgType=EMsgWarningPersistent;
		break;
	case ECmdCancelAll:
		noteText=StringLoader::LoadL(R_ERRMSG_CANCELSUCCESSFUL);
		msgType=EMsgNotePersistent;
		break;
	case EPeerCanceledAll:
		noteText=StringLoader::LoadL(R_ERRMSG_PEERCANCELEDTRANSFER);
		msgType=EMsgWarningPersistent;
		break;
	};
	if(space)
	    CleanupStack::PopAndDestroy(space);

	iObserver->ShowErrorCleanCommL(msgType,noteText,0,ETrue);

	LOG(ELogBT,-1,"DoCancelTransfer: end");
}

void CCommCommon::CancelTransferTimeout()
{
	LOG(ELogBT,1,"CancelTransferTimeout: start");
	if(iSocket)
	{
		iSocket->CancelAll();
		RecvDataL(EStatusRecvErrorCode);
	};
	LOG(ELogBT,-1,"CancelTransferTimeout: end");
}

void CCommCommon::HandleSendCompleteL(TInt aErr)
{
	//LOG("HandleSendCompleteL: start");
	if(aErr)
	{
		LOG(ELogBT,0,"HandleSendCompleteL: error %d",aErr);
		if(aErr==KErrEof)
		{
			LOG(ELogBT,0,"HandleSendCompleteL: eror KErrEof. Peer probably canceled the transfer.");
			RecvDataL(EStatusRecvErrorCode);
		}
		else
		{
		    ShowErrorL(R_ERRMSG_CANNOTSEND2BTDEVICE,EMsgErrorPersistent,aErr);
		    iMyStatus=0;
		};
		return;
	};
	//#LOG("We sent data successfully!");
	if(iCancelStatus==ECmdCancelAll && iMyStatus!=EStatusSentErrorCode)
	{
		LOG(ELogBT,0,"HandleSendCompleteL: CancelAll detected!");
		SendCancelL(iCancelStatus);
		//when sending & socket shutdown is over, the Clean() is called
		return;
	};

	//depending on the iMyStatus, we do something!
	switch(iMyStatus)
	{
	case EStatusSendingMessageMultiplePackets:SendNextPacketL();break;
	case EStatusGetVersionSent:RecvDataL(EStatusReceiveVersionFollows);break;
	case EStatusStartSendFile:SendFile2L();break;
	case EStatusSendingFile:
	{
		if(iSRDataInBuffer==KBTBufferSize)
		{
			if(iReadingWriting)
			{
				//#LOG("Done sending");
				iReadingWriting=EFalse;
			}
			else
			{
				//#LOG("Done sending & reading");
				SendFile2L();
			};
		}
		else SendFile2L();
	};break;
	case EStatusDone:CommCommandComplete();break;
	case EStatusReceiveCommand:RecvDataL(EStatusReceiveCommand);break;
	case EStatusFileReceivedOK:RecvDataL(EStatusReceiveCommand);break;
	case EStatusSendFileAgain:RecvDataL(EStatusReceiveCommand);break;
	case EStatusReceiveOKorSendAgain:RecvDataL(EStatusReceiveCommand);break;
	//case EStatusReceiveOKorSendAgain:iMyStatus=EStatusReceiveCommand;break;
	case EStatusSentErrorCode:/*DoCancelTransfer();*/iSocket->Shutdown(RSocket::EImmediate);break;
	default:iSocket->Shutdown(RSocket::ENormal);
	};
	//#LOG("HandleSendCompleteL: end");
}

void CCommCommon::HandleReceiveCompleteL(TInt aErr)
{
	LOG(ELogBT,1,"HandleReceiveCompleteL: start");
	if(aErr)
	{
		LOG(ELogBT,0,"There was an error receiving data (err=%d). We received %d bytes",aErr,iPtrDataBT.Length());
		if(iPtrDataBT.Length()>=4)
		{
			iLast4Bytes=*(TUint*)(iPtrDataBT.Right(4).Ptr());
			//attention, iLast4Bytes will have the System's endianness (Little Endian, but it may change)
			LOG(ELogBT,0,"Last4Bytes: %x",iLast4Bytes)
		};

		//check if this was a cancel
		//we have received our error code, try to interpret it
		TInt err=InterpretCancel();
		if(!err)
		{
		    DoCancelTransfer();
		    LOG(ELogBT,-1,"HandleReceiveCompleteL: end");
			return;
		};

		ShowErrorL(R_ERRMSG_CANNOTRECVFROMBTDEVICE,EMsgErrorPersistent,aErr);
		iMyStatus=0;
		LOG(ELogBT,-1,"HandleReceiveCompleteL: end (error canceling: %d)",err);
		return;
	};
	if(iPtrDataBT.Length()>=4)
	{
		//attention, iLast4Bytes will have the System's endianness (Little Endian, but it may change)
		iLast4Bytes=*(TUint*)(iPtrDataBT.Right(4).Ptr());
	}

	if(iCancelStatus==ECmdCancelAll)
	{
		LOG(ELogBT,0,"HandleReceiveCompleteL: CancelAll detected!");
		SendCancelL(iCancelStatus);
		//when sending & socket shutdown is over, the Clean() is called
		LOG(ELogBT,-1,"HandleReceiveCompleteL: end (cmd: cancel all)");
		return;
	};
	LOG(ELogBT,0,"We received data successfully! (%d bytes) last4Bytes=0x%x",iPtrDataBT.Length(),iLast4Bytes);

	//depending on the iMyStatus, we do something!
	switch(iMyStatus)
	{
	case EStatusReceiveGetVersion:ParseCommandL(ECmdGetVersion);break;
	case EStatusReceiveVersionFollows:ParseCommandL(ECmdVersionFollows);CommCommandComplete();break;
	case EStatusReceiveCommand:ParseCommandL();break;
	case EStatusGotMoreData:
	{
		LOG(ELogBT,0,"Received more data, old length was %d (now received %d bytes)",iDataFilledLength,iPtrDataBT.Length());
		iDataFilledLength+=iPtrDataBT.Length();
		iPtrDataBT.Set(iDataBT,iDataFilledLength,KBTBufferSize);
		ParseCommandL();
	};break;
	case EStatusReceivedFileData:
	{
		if(iSRDataInBuffer+iRMTU>KBTBufferSize)
		{
			if(iReadingWriting)
			{
				LOG(ELogBT,0,"Done receiving");
				iReadingWriting=EFalse;
			}
			else
			{
				LOG(ELogBT,0,"Done receiving & writing");
				RecvFileL();
			};
		}
		else RecvFileL();
	};break;
	case EStatusReceiveDoneSending:ParseCommandL(ECmdDoneSending);break;
	case EStatusSendingFile:ParseCommandL(ECmdDoneSending);break; //this is either done, or a cancel command
	case EStatusRecvErrorCode:RecvDataL(EStatusRecvErrorCode);
	default:iSocket->Shutdown(RSocket::ENormal);
	};
	LOG(ELogBT,-1,"HandleReceiveCompleteL: end");
}

void CCommCommon::HandleShutdownCompleteL(TInt aErr)
{
	LOG(ELogBT,1,"HandleShutdownCompleteL: start (err=%d)",aErr);
	if(iMyStatus==EStatusSentErrorCode)
		DoCancelTransfer();
	iMyStatus=0;
	LOG(ELogBT,-1,"HandleShutdownCompleteL: end");
}
///////////////////////////// CCommSender

CCommSender* CCommSender::GetInstanceL()
{
	if(!instance)
		{
		instance=new(ELeave)CCommSender;
		CActiveScheduler::Add(instance);
		}
	return instance;
}

CCommSender::CCommSender() : iSendas(NULL), iMessage(NULL)
{
	//
}

void CCommSender::SendUrlSMSL(MMLauncherTransferNotifier *aObserver)
{
	LOG(ELogBT,1,"SendUrlSMSL: start");
	TInt err;

	__ASSERT_ALWAYS(aObserver,Panic(EBTNoObserver));
    iObserver=aObserver;

	if(iSendas || iMessage)
	{
		//SendAs server is busy with a previous request
		ShowErrorL(R_ERRMSG_SENDASBUSY,EMsgNote,0);
		LOG(ELogBT,-1,"SendUrlSMSL: end (sendas busy with previous message)");
		return;
	};
	iActiveRequest=EActiveRequestSendURL;

	//all checkings done, send the sms
	iSendas=new RSendAs;
	iMessage=new RSendAsMessage;

	// Connecting to the SendAs server
	err = iSendas->Connect();
	if(err)
	{
		//can not connect to SendAs server
		ShowErrorL(R_ERRMSG_CONNECTSENDAS,EMsgError,err);
		LOG(ELogBT,-1,"SendUrlSMSL: end (connect sendas err=%d)",err);
		return;
	};
	LOG(ELogBT,0,"SendUrlSMSL: connected");

	// Selecting the appropriate SMS MTM UID
	TRAP(err,iMessage->CreateL(*iSendas, KSenduiMtmSmsUid));
	if(err)
	{
		//can not connect to SendAs server
		ShowErrorL(R_ERRMSG_CREATESMSMESSAGE,EMsgError,err);
		LOG(ELogBT,-1,"SendUrlSMSL: end (create msg err=%d)",err);
		return;
	};
	LOG(ELogBT,0,"SendUrlSMSL: SMS MTM created");

	//add the text
	HBufC *text=StringLoader::LoadLC(R_SMS_TEXT,KUrlInSMS);
	LOG(ELogBT,0,"SendUrlSMSL: body text loaded");
	iMessage->SetBodyTextL(*text);
	LOG(ELogBT,0,"SendUrlSMSL: body text set");
	CleanupStack::PopAndDestroy(text);
	LOG(ELogBT,0,"SendUrlSMSL: text destroyed");

	//send
	iMessage->LaunchEditorAndCloseL();
	LOG(ELogBT,0,"SendUrlSMSL: editor launched");

	//clean
	delete iMessage;
	iMessage=NULL;
	iSendas->Close();
	delete iSendas;
	iSendas=NULL;
	LOG(ELogBT,-1,"SendUrlSMSL: end");
}

void CCommSender::SendApplicationL(MMLauncherTransferNotifier *aObserver)
{
	LOG(ELogBT,1,"SendApplicationL: start");
	TInt err;

	__ASSERT_ALWAYS(aObserver,Panic(EBTNoObserver));
    iObserver=aObserver;

	//check for the proper state
	if(iMyStatus)
	{
		ShowErrorL(R_ERRMSG_BTBUSY,EMsgNote,0);
		LOG(ELogBT,-1,"SendApplicationL: end (BT busy)");
		return;
	};
	iActiveRequest=EActiveRequestTransferApplication;

	//check if the sis file exists
	TEntry entry;
	err=iee->FsSession().Entry(iObserver->Preferences()->PrivatePathPlus(KSISFileName),entry);
	if(err!=KErrNone)
	{
		//file does not exist
		ShowErrorL(R_ERRMSG_SISNOTFOUND,EMsgError,err);
		LOG(ELogBT,-1,"SendApplicationL: end (sis not found err=%d)",err);
		return;
	};

	//all checkings done, send the sis file
	iSendas=new RSendAs;
	iMessage=new RSendAsMessage;

	// Connecting to the SendAs server
	err = iSendas->Connect();
	if(err)
	{
		//can not connect to SendAs server
		ShowErrorL(R_ERRMSG_CONNECTSENDAS,EMsgError,err);
		LOG(ELogBT,-1,"SendApplicationL: end (connect to sendas err=%d)",err);
		return;
	};

	// Selecting the appropriate Bluetooth MTM UID
	TRAP(err,iMessage->CreateL(*iSendas, KSenduiMtmBtUid));
	if(err)
	{
		//can not connect to SendAs server
		ShowErrorL(R_ERRMSG_CREATEBTMESSAGE,EMsgError,err);
		LOG(ELogBT,-1,"SendApplicationL: end (create bt message err=%d)",err);
		return;
	};

	// Constructing the message
	iMessage->AddAttachment(iObserver->Preferences()->PrivatePathPlus(KSISFileName), iStatus);
	iMyStatus=ESendAsAddAttachment;
	SetActive();
	LOG(ELogBT,-1,"SendApplicationL: end");
}

void CCommSender::SendApplication2L()
{
	LOG(ELogBT,0,"SendApplication2L: start");
	iMyStatus=0; //in case we return with error!
	//we finished adding the attachment
    TInt err;
    TBTDevAddr addr;
    TBTDeviceName name;
    TBTDeviceClass deviceClass;

    TRAP(err,CMLauncherBTSocket::GetDeviceAddressL(KSerialPortUUID,addr,name,deviceClass));
    if(err==KErrCancel)
    {
    	//no need to display anything
    	__ASSERT_ALWAYS(iObserver,Panic(EBTNoObserver));
    	iObserver->Clean(ETrue);
    }
    else if(err)
    	ShowErrorL(R_ERRMSG_CANNOTCONNECT2BTDEVICE,EMsgWarningPersistent,err);

    TBuf<3> btaddr;
    btaddr.Copy((TUint16*)addr.Des().Ptr(),3);
    //addr.GetReadable(btaddr,_L(""),_L(":"),_L(""));
    LOG(ELogBT,0,"SendApplication2L: before adding the address, device name=%S",&name);
    TRAP(err,iMessage->AddRecipientL(btaddr,RSendAsMessage::ESendAsRecipientTo););
    if(err)
    {
    	ShowErrorL(R_ERRMSG_BADRECIPIENT,EMsgError,err);
		return;
    }
    LOG(ELogBT,0,"SendApplication2L: sending to remote device");
    TRAP(err,iMessage->SendMessageAndCloseL());
	if(err)iMessage->Close();
	LOG(ELogBT,0,"SendApplication2L: after sending the message");
	delete iMessage;
	iMessage=NULL;
	iSendas->Close();
	delete iSendas;
	iSendas=NULL;
	LOG(ELogBT,0,"SendApplication2L: after deleting stuff");
	if(err)
	{
		ShowErrorL(R_ERRMSG_CANNOTSEND,EMsgError,err);
		return;
	};
	ShowErrorL(R_ERRMSG_SENDAPPPROCEEDING,EMsgNote,0);
	iMyStatus=0; //we are available again!
}




void CCommSender::SendPlaylistL(TConnectionType aConnectionType, MMLauncherTransferNotifier *aObserver, CPlaylist *aFilesInPlaylist, const TDesC8 *aConnectionData)
{
	//first, we get the BT Address & connect
    LOG(ELogBT,1,"SendPlaylistL: start");
    __ASSERT_ALWAYS(aObserver,Panic(EBTNoObserver));
    iObserver=aObserver;

	//check for the proper state
	if(iMyStatus)
	{
		ShowErrorL(R_ERRMSG_BTBUSY,EMsgNote,0);
		User::Leave(KErrInUse);
	};
	iActiveRequest=EActiveRequestTransferPlaylist;

	//get the list of files
	iFilesInPlaylist=aFilesInPlaylist;

	//connect to peer and send files
	iMyStatus=EBTConnect;
	iCurrentCommand=ECmdSendPlaylist_Connect;
	iI=0;
	iObserver->SetDirection(MMLauncherTransferNotifier::EDirectionSending);

	switch(aConnectionType)
	{
	case EBTConnection:ConnectToBTDeviceL();break;
	case EIPConnection:ConnectToIpL(aConnectionData);break;
	default:Panic(EConnectionTypeUnknown);
	};

	LOG(ELogBT,-1,"SendPlaylistL: end");
}

void CCommSender::SendPlaylist2L()
{
	LOG(ELogBT,1,"SendPlaylist2L: start");
	//we are connected, send the GetVersion command
	iCurrentCommand=ECmdSendPlaylist_Version;
	SendVersionL(ECmdGetVersion);
	LOG(ELogBT,-1,"SendPlaylist2L: end");
}

void CCommSender::SendPlaylist3L()
{
	LOG(ELogBT,1,"SendPlaylist3L: start");
	//we are connected, we send files, one by one
	if(iI>=iFilesInPlaylist->Count())
	{
		//we sent all the files
		//send done
		iCurrentCommand=ECmdSendPlaylist_DoneSending;
		SendZeroTerminatedCommandL(ECmdDone);
	}
	else
	{
		//we still have files to send
		//but first, we need to create the message
		//... and for that, we need to get the current file's size
		TInt fileSize;
		TFileDirEntry *plsEntry=iFilesInPlaylist->GetEntry(iI,ETrue);
		TFileName filename;
		plsEntry->GetFullPath(filename,EFalse);
		
		RFile *file=new(ELeave)RFile;
		CleanupClosePushL(*file);
		//CnvUtfConverter::ConvertToUnicodeFromUtf8(filename,*(*iFilesInPlaylist)[iI]);
		
		TInt err=file->Open(iee->FsSession(),filename,EFileRead|EFileShareReadersOnly);
		if(!err)
		{
			file->Size(fileSize);
			HBufC8 *message;

			if(iI)
			{
				//create the message for second or next files
				message=HBufC8::NewLC(KSendFileMessageTemplate().Size()+KSendFileMessageExtraSize);
				TFileName relPath;
				plsEntry->GetPathWithoutSource(relPath);
				HBufC8 *relPath8=SenXmlUtils::ToUtf8LC(relPath);
				//TPtrC8 relPath=(*iFilesInPlaylist)[iI]->Right((*iFilesInPlaylist)[iI]->Length()-iPrefixLength);
				message->Des().Format(KSendFileMessageTemplate,relPath8,fileSize);
				CleanupStack::PopAndDestroy(relPath8);
			}
			else
			{
				//create the message for the first file (has totals as well)
				TInt nrFiles=iFilesInPlaylist->Count();
				iTotalSize=fileSize;
				TInt totalStringsSize(KSendFileMessagePlusTotalsTemplate_start().Size()+KSendFileMessagePlusTotalsTemplate_end().Size()+KSendFileMessageExtraSize);
				TInt i,err;
				TEntry entry;
				RArray<TInt> sizes;
				for(i=1;i<nrFiles;i++)
				{
					err=iee->FsSession().Entry(filename,entry);
					if(err==KErrNone)
					{
						iTotalSize+=entry.iSize;
						sizes.Append(entry.iSize);
					}
					else
					{
						LOG(ELogBT,0,"FAILED TO GET THE SIZE FOR A FILE: %S",&filename)
					};
				};
				totalStringsSize+=(nrFiles-1)*(KSendFileMessagePlusTotalsTemplate_fs().Size()+KSendFsMessageExtraSize);

				//create the beginning
				message=HBufC8::NewLC(totalStringsSize);
				TFileName relPath;
				plsEntry->GetPathWithoutSource(relPath);
				HBufC8 *relPath8=SenXmlUtils::ToUtf8LC(relPath);
				//HBufC8 *relPath=SenXmlUtils::ToUtf8LC(filename.Right(filename.Length()-iPrefixLength));
				message->Des().Format(KSendFileMessagePlusTotalsTemplate_start,relPath8,fileSize,nrFiles,iTotalSize);
				CleanupStack::PopAndDestroy(relPath8);

				//create the middle
				HBufC8 *fsMessage=HBufC8::NewLC(KSendFileMessagePlusTotalsTemplate_fs().Size()+KSendFsMessageExtraSize);
				for(i=1;i<nrFiles;i++)
				{
					fsMessage->Des().Format(KSendFileMessagePlusTotalsTemplate_fs,sizes[i-1]);
					message->Des().Append(*fsMessage);
				};
				CleanupStack::PopAndDestroy(fsMessage);


				//create the end
				message->Des().Append(KSendFileMessagePlusTotalsTemplate_end);

				//put something on the screen
				sizes.Insert(fileSize,0);
				if(iObserver)
					iObserver->NewTransferL(nrFiles,iTotalSize,sizes);
			};

			//announce the observer we have a new file
			if(iObserver)
				iObserver->TransferingNewFileL(MMLauncherTransferNotifier::EDirectionSending,filename,fileSize);

			//we allocate the file buffer, if needed
			if(!iDataFile)
			{
				iDataFile=new(ELeave)TUint8[KBTBufferSize];
				iPtrDataFile.Set(iDataFile,0,KBTBufferSize);
			};
			//now send the file
			iCurrentCommand=ECmdSendPlaylist_SendFile;
			SendFileL(file,*message); //transfers ownership of file
			//clean
			CleanupStack::PopAndDestroy(message);
			CleanupStack::Pop();  //TCleanupItem for file
		}
		else
		{
			//most probably the file does not exist!
			LOG(ELogBT,0,"SendPlaylist3L: file open error. Does file exist?");
			//TODO: error message
		};

		//next
		iI++;
	};
	LOG(ELogBT,-1,"SendPlaylist3L: end");
}

void CCommSender::CancelTransfer(TBool aAllFiles) //if aAllFiles is FALSE, we cancel only the current file
{
	if(aAllFiles)
		iCancelStatus=ECmdCancelAll;
	else
	{
		iCancelStatus=ECmdCancelFile;
	    ShowErrorL(R_ERRMSG_NOTIMPLEMENTED,EMsgNote,0);
	};
}

void CCommSender::CommCommandComplete(TInt aErr)
{
	LOG(ELogBT,1,"BTCommandComplete: start");
	switch(iCurrentCommand)
	{
	case ECmdSendPlaylist_Version:SendPlaylist3L();break;
	case ECmdSendPlaylist_SendFile:
	{
		if(aErr)
			iI--;//we send the same file again
		SendPlaylist3L();
	};break;
	case ECmdSendPlaylist_DoneSending:
	{
		iCurrentCommand=ECmdSendPlaylist_Done;
		RecvDataL(EStatusReceiveCommand);
	};break;
	case ECmdSendPlaylist_Done:
	{
		//we are done! Close the communication
		iSocket->Shutdown(RSocket::ENormal);
		//command below calls PlaylistTransferCompleteL() as well
		if(iObserver)
		    iObserver->ShowErrorCleanCommL(EMsgNote,R_ERRMSG_PLAYLISTSENTSUCCESSFULLY,0,ETrue);
	};break;
	default: __ASSERT_ALWAYS(0,Panic(EBTCurrentCommandWrongValue));
	};
	LOG(ELogBT,-1,"BTCommandComplete: end");
}

void CCommSender::ConnectToBTDeviceL()
{
	LOG(ELogBT,1,"ConnectToBTDeviceL: start");
	if(!iSocket)
		iSocket=CMLauncherBTSocket::NewL(this);
	iConnectionType=EBTConnection;
	TInt err=iSocket->Connect();
	if(err==KErrCancel)
	{
		//no need to display anything
		__ASSERT_ALWAYS(iObserver,Panic(EBTNoObserver));
		iObserver->Clean(ETrue);
	}
	else if(err)
		ShowErrorL(R_ERRMSG_CANNOTCONNECT2BTDEVICE,EMsgWarningPersistent,err);

	//we have the connection
	LOG(ELogBT,-1,"ConnectToBTDevice: end");
}

void CCommSender::ConnectToIpL(const TDesC8 *aConnectionData)
{
	LOG(ELogBT,1,"ConnectToIpL: start");
	if(!iSocket)
		iSocket=CMLauncherIPSocket::NewL(this);
	iConnectionType=EIPConnection;
	TInt err=iSocket->Connect(aConnectionData);
	if(err)
		ShowErrorL(R_ERRMSG_CANNOTCONNECT2IPDEVICE,EMsgWarningPersistent,err);

	//we have the connection
	LOG(ELogBT,-1,"ConnectToIpL: end");
};

void CCommSender::HandleConnectCompleteL(TInt aErr)
{
	LOG(ELogBT,0,"HandleConnectCompleteL: start");
	if(aErr)
	{
		switch(iConnectionType)
		{
		case EBTConnection:iObserver->ShowErrorCleanCommL(EMsgWarningPersistent,R_ERRMSG_CANNOTCONNECT2BTDEVICE,aErr,ETrue);break;
		case EIPConnection:iObserver->ShowErrorCleanCommL(EMsgWarningPersistent,R_ERRMSG_CANNOTCONNECT2IPDEVICE,aErr,ETrue);break;
		};
		return;
	};
	LOG(ELogBT,0,"We are connected!");
	__ASSERT_ALWAYS(iMyStatus==EBTConnect,Panic(EBTConnectWrongValue));

	//allocate the buffer for sending/receiving data
	AllocateBTDataL();


	/*
	if(res==KErrNone)
	{
		TInt xx=1000;
		do
		{
			res=iSocket->SetOpt(KL2CAPGetOutboundMTU, KSolBtL2CAP, xx);
			LOG("SetOpt: res=%d xx=%d",res,xx);
			res=iSocket->SetOpt(KL2CAPInboundMTU, KSolBtL2CAP, xx);
			LOG("SetOpt: res=%d xx=%d",res,xx);
			xx-=10;
		}
		while(res!=KErrNone && xx>0);

	};
	*/

	//depending on the iMyStatus, we do something!
	switch(iCurrentCommand)
	{
	//case ECmdCheck4Updates:CheckForUpdates2L();break;
	case ECmdSendPlaylist_Connect:SendPlaylist2L();break;
	default:iSocket->Shutdown(RSocket::ENormal);
	};
	LOG(ELogBT,01,"HandleConnectCompleteL: end");
}


void CCommSender::RunL()
{
	//#LOG("CCommSender::RunL, err=%d",iStatus.Int());
	User::LeaveIfError(iStatus.Int());

	switch(iMyStatus)
	{
	case ESendAsAddAttachment:SendApplication2L();break;
	case EStatusSendingFile:
	{
		if(iReadingWriting)
		{
			//LOG("Done reading from file");
			iReadingWriting=EFalse;
		}
		else
		{
			//LOG("Done reading from file & sending to socket");
			SendFile2L();
		};
	};break;
	case EStatusReceivedFileData:
		{
			if(iReadingWriting)
			{
				//#LOG("Done writing");
				iReadingWriting=EFalse;
			}
			else
			{
				//#LOG("Done writing & receiving");
				RecvFileL();
			};
		};break;
	};
}

void CCommSender::DoCancel()
{
	if(iMessage)iMessage->Cancel();
	if(iSocket)
		iSocket->CancelAll();
	if(iFile2SendRecv)
	{
		iFile2SendRecv->ReadCancel();
	};

}

TInt CCommSender::RunError(TInt aError)
{
	LOG(ELogBT,0,"CCommSender::RunError, err=%d, state=%d",aError,iMyStatus);
	TRAPD(err,
	//display a message
	switch(iMyStatus)
	{
	case ESendAsAddAttachment:ShowErrorL(R_ERRMSG_CANNOTATTACHMSG,EMsgError,aError);break;
	case EStatusSendingFile:ShowErrorL(R_ERRMSG_CANNOTREAD,EMsgError,aError);break;
	case EStatusReceivedFileData:ShowErrorL(R_ERRMSG_CANNOTWRITE,EMsgError,aError);break;
	default: ShowErrorL(R_ERRMSG_UNKNOWN,EMsgError,aError);
	};
	);
	return KErrNone;
}


CCommSender::~CCommSender()
{
	LOG(ELogBT,1,"CCommSender::~CCommSender() start");
	Clean(EFalse);
	__ASSERT_ALWAYS(!iSendas,Panic(EBTSendAsNotNull));
	__ASSERT_ALWAYS(!iMessage,Panic(EBTSendAsMessageNotNull));
	__ASSERT_ALWAYS(!iFilesInPlaylist,Panic(EBTFilesInPlaylistNotNull));
	LOG(ELogBT,-1,"CCommSender::~CCommSender() end");
}

void CCommSender::Clean(TBool aKeepListening)
{
	LOG(ELogBT,1,"CCommSender::Clean() start (aKeepListening=%d)",aKeepListening);
	if(iMessage)
	{
		iMessage->Close();
		delete iMessage;
		iMessage=NULL;
	};
	if(iSendas)
	{
		iSendas->Close();
		delete iSendas;
		iSendas=NULL;
	};

	//destroy iFilesInPlaylist
	if(iFilesInPlaylist)
	{
		//iFilesInPlaylist->ResetAndDestroy();
	    //iFilesInPlaylist->Close();
	    delete iFilesInPlaylist;
	    iFilesInPlaylist=NULL;
	};

	CCommCommon::Clean(aKeepListening);
	LOG(ELogBT,-1,"CCommSender::Clean() end");
}

void CCommSender::DeleteInstance()
{
	LOG(ELogBT,1,"CCommSender::DeleteInstance start");
	if(instance)
	{
		delete instance;
		instance=NULL;
	};
	LOG(ELogBT,-1,"CCommSender::DeleteInstance end");
}





/*
TRAP(err,
{
//just try something
RSendAs sendAs;
User::LeaveIfError(sendAs.Connect());
CleanupClosePushL(sendAs);

RSendAsMessage sendAsMessage;
sendAsMessage.CreateL(sendAs, KUidMsgTypeBt);
CleanupClosePushL(sendAsMessage);

//discover the peer
// 1. Create a notifier
RNotifier notifier;
User::LeaveIfError(notifier.Connect());

// 2. Start the device selection plug-in
TBTDeviceSelectionParams selectionFilter;
TUUID targetServiceClass(KObexUUID );//from btsdp.h
selectionFilter.SetUUID(targetServiceClass);
TBTDeviceSelectionParamsPckg pckg(selectionFilter);
TBTDeviceResponseParams result;
TBTDeviceResponseParamsPckg resultPckg(result);
TRequestStatus status;
notifier.StartNotifierAndGetResponse(status, KDeviceSelectionNotifierUid, pckg, resultPckg);
User::After(2000000);

WriteToLogL(_L("BT2: notifier started"));

// 3. Extract device name if it was returned
User::WaitForRequest(status);
WriteToLogL(_L("BT3: notifier done. Status: %d"),status.Int());
TBuf<256> btaddr;
resultPckg().BDAddr().GetReadable(btaddr);
//btaddr.Copy(resultPckg().BDAddr().Des());
WriteToLogL(_L("BT3: Device addr: %S"),&btaddr);
if (status.Int() != KErrNone)
{
    //handle the error
    WriteToLogL(_L("BT3: ERROR statur or device name. Device name: %S"),&btaddr);
    WriteToLogL(_L("BT3: IsValidDeviceName=%d (len=%d)"),result.IsValidDeviceName(),result.DeviceName().Length());
    WriteToLogL(_L("BT3: IsValidDeviceAddress=%d (len=%d"),result.IsValidBDAddr(),btaddr.Length());
    WriteToLogL(_L("BT3: IsValidDeviceClass=%d (class=%d"),result.IsValidDeviceClass(),result.DeviceClass().DeviceClass());
    WriteToLogL(_L("BT3: resultPckg len=%d (%S)"),resultPckg.Length(),&resultPckg);
};

// 4. Clean up
notifier.CancelNotifier(KDeviceSelectionNotifierUid);
notifier.Close();
WriteToLogL(_L("BT4: clean done"));
//CBTDevice btDev;
//btDev.SetDeviceAddress(resultPckg().BDAddr());
//btDev.SetDeviceNameL(BTDeviceNameConverter::ToUTF8L(resultPckg().DeviceName()));

// 5. prepare the message
WriteToLogL(_L("BT5: adding the recipient"));

//sendAsMessage.AddRecipientL(btDev.DeviceName(), RSendAsMessage::ESendAsRecipientTo);
//sendAsMessage.AddRecipientL(resultPckg().DeviceName(), RSendAsMessage::ESendAsRecipientTo);
//sendAsMessage.AddRecipientL(btaddr, RSendAsMessage::ESendAsRecipientTo);
WriteToLogL(_L("BT5: set the message"));
sendAsMessage.SetBodyTextL(_L("message"));
WriteToLogL(_L("BT5: done"));

// 6. send the message
sendAsMessage.SendMessageAndCloseL();
WriteToLogL(_L("BT6: sending the message"));

// 7. Clean
CleanupStack::Pop();//sendAsMessage (already closed)
CleanupStack::PopAndDestroy();// sendAs
WriteToLogL(_L("BT7: clean done"));
});

if(err)
	LOG("BT: ERROR trapped: %d",err);
	*/

///////////////////////////// CCommReceiver

CCommReceiver* CCommReceiver::GetInstanceL()
{
	if(!instance)
		{
		instance=new(ELeave)CCommReceiver;
		CActiveScheduler::Add(instance);
		}
	return instance;
}

CCommReceiver::CCommReceiver()
{}

void CCommReceiver::ListenL(MMLauncherTransferNotifier *aObserver)
{
	__ASSERT_ALWAYS(aObserver,Panic(EBTNoObserver));
    iObserver=aObserver;
	iSocket=CMLauncherBTSocket::NewL(this);
	iSocket->Listen();
}

void CCommReceiver::AcceptL()
{
	iSocket->Accept();
}

void CCommReceiver::CancelTransfer(TBool aAllFiles) //if aAllFiles is FALSE, we cancel only the current file
{
	if(aAllFiles)
		iCancelStatus=ECmdCancelAll;
	else
	{
		iCancelStatus=ECmdCancelFile;
	    ShowErrorL(R_ERRMSG_NOTIMPLEMENTED,EMsgNote,0);
	};
}

void CCommReceiver::CommCommandComplete(TInt aErr)
{
	LOG(ELogBT,1,"BTCommandComplete: start (iCurrentCommand: %d, iCommand=%d)",iCurrentCommand,iCommand);
	switch(iCurrentCommand)
	{
	case ECmdDoneSending:
	{
		//check for done command
		__ASSERT_ALWAYS(iCommand==ECmdDone,Panic(EBTCurrentCommandWrongValue));
		//we can do our own stuff now!
		iCurrentCommand=ECmdSendDone;
		SendZeroTerminatedCommandL(ECmdDone);
	};break;
	case ECmdSendDone:
	{
		//we are done! Close the communication
		iSocket->Shutdown(RSocket::ENormal);
		//command below calls PlaylistTransferCompleteL() as well
		if(iObserver)
		    iObserver->ShowErrorCleanCommL(EMsgNote,R_ERRMSG_PLAYLISTRECVSUCCESSFULLY,0,ETrue);
	};break;
	default: __ASSERT_ALWAYS(0,Panic(EBTCurrentCommandWrongValue));
	};
	LOG(ELogBT,-1,"BTCommandComplete: end");
}

void CCommReceiver::HandleAcceptCompleteL(TInt aErr)
{
	LOG(ELogBT,1,"HandleAcceptCompleteL: start");
	if(aErr)
	{
		ShowErrorL(R_ERRMSG_CANNOTACCEPTCONNECTION,EMsgError,aErr);
		LOG(ELogBT,-1,"HandleAcceptCompleteL: end (cannot accept connection err=%d)",aErr);
		return;
	};
	LOG(ELogBT,0,"We are connected!");
	iActiveRequest=EActiveRequestTransferPlaylist;

	//allocate the buffer for sending/receiving data
	AllocateBTDataL();

	iCurrentCommand=ECmdDoneSending;
	RecvDataL(EStatusReceiveGetVersion);
	LOG(ELogBT,-1,"HandleAcceptCompleteL: end");
};


void CCommReceiver::RunL()
{
	//#LOG("CCommReceiver::RunL, err=%d",iStatus.Int());
	User::LeaveIfError(iStatus.Int());

	switch(iMyStatus)
	{
	//case 1:ReceiveApplicationL();break;
	//case EStatusSendingFile:
	//case EStatusReceivedFileData:iReadingWriting=EFalse;LOG("Done reading/writing");break;

	case EStatusSendingFile:
	{
		if(iReadingWriting)
		{
			//#LOG("Done reading");
			iReadingWriting=EFalse;
		}
		else
		{
			//#LOG("Done reading & sending");
			SendFile2L();
		};
	};break;
	case EStatusReceivedFileData:
		{
			if(iReadingWriting)
			{
				//#LOG("Done writing to file");
				iReadingWriting=EFalse;
			}
			else
			{
				//#LOG("Done writing to file & receiving from socket");
				RecvFileL();
			};
		};break;
	};
}

void CCommReceiver::DoCancel()
{
	iSocket->CancelAll();
	if(iFile2SendRecv)
	{
		iFile2SendRecv->ReadCancel();
	};
}



void CCommReceiver::ReceiveApplicationL()
{
	//
	/*
	iSock->Close();
	iListener->Accept(*iSock);
	iMyStatus=1;
	SetActive();
	*/
}

CCommReceiver::~CCommReceiver()
{
	LOG(ELogBT,1,"CCommReceiver::~CCommReceiver() start");
	Clean(EFalse);
	LOG(ELogBT,-1,"CCommReceiver::~CCommReceiver() end");
}

void CCommReceiver::Clean(TBool aKeepListening)
{
	LOG(ELogBT,1,"CCommReceiver::Clean() start (aKeepListening=%d)",aKeepListening);

	if(iFile2SendRecv)
	{
		//we need to delete this file, as it is incomplete
		TFileName filename;
		iFile2SendRecv->FullName(filename);
		//now close/delete the file
		iFile2SendRecv->Close();
		delete iFile2SendRecv;
		iFile2SendRecv=NULL;
		if(iee)
			iee->FsSession().Delete(filename);
		if(iObserver && aKeepListening) //if we do not keep listening, we shutdown the application
			iObserver->UpdateView();
	};
	CCommCommon::Clean(aKeepListening);
	if(aKeepListening)
	{
		LOG(ELogBT,0,"Calling AcceptL()");
		AcceptL();
	};
	LOG(ELogBT,-1,"CCommReceiver::Clean() end");
}


void CCommReceiver::DeleteInstance()
{
	LOG(ELogBT,1,"CCommReceiver::DeleteInstance start");
	if(instance)
	{
		delete instance;
		instance=NULL;
	};
	LOG(ELogBT,-1,"CCommReceiver::DeleteInstance end");
}
