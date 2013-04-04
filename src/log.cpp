#include "defs.h"
#if RELEASE_LOG
#include <eikenv.h>
#include <stringloader.h>
#include <MLauncher.rsg>
#include "MLauncher.pan"
#include "log.h"

_LIT(KMLauncherLog,"MLauncher.txt");
_LIT(KMLauncherLogCrashed,"MLauncher_crashed.txt");

_LIT(KMLauncherDebugFolder, "e:\\Documents\\MLauncherDebug\\");
_LIT(KMLauncherDebugFolder2,"c:\\Data\\MLauncherDebug\\");





#ifdef __WINS__
const char *KLogClosing="LOG Closing!";
const char *KLogClosingPanic="LOG Closing, but application ended with a Panic()!";
#else
_LIT(KLogClosing,"LOG Closing!");
_LIT(KLogClosingPanic,"LOG Closing, but application ended with a Panic()!");
const TInt KLogClosingLength=20;
#endif

FLLog* FLLog::instance=NULL;
RFs* FLLog::iFs=NULL;
TUint FLLog::iDefaultFlags=0xFFFFFFFF;
const TInt KLogLineLength=2048;

TBool FLLog::CreateInstanceL(RFs *aFs, TInt aFlags)
{
	__ASSERT_ALWAYS(!instance,Panic(ELogInstanceNotNull));
	iFs=aFs;
	iDefaultFlags=aFlags;
	
	//check if the debug folder exists
	TEntry entry;
	TInt err;
	TBool use2(EFalse);
	err=iFs->Entry(KMLauncherDebugFolder,entry);
	if(err!=KErrNone || !entry.IsDir())
	{
		//try the second folder
		err=iFs->Entry(KMLauncherDebugFolder2,entry);
		if(err!=KErrNone || !entry.IsDir())
			return EFalse;
		else
			use2=ETrue;
	}
	
	//if we are here, it means that we have the folder and we can create the log
	iFs->ShareAuto(); //so we can access the log from other threads
	
	//check for crashed log
	/*
	CDir *dir=NULL;
	err=iFs->GetDir(KMLauncherDebugFolder,KEntryAttNormal,ESortByName,dir);
	if(err!=KErrNone)return EFalse;
	    CleanupDeletePushL(dir); //this is needed because the thread where this function is called may be canceled
	*/
	
	TFileName oldPath;
	if(use2)oldPath.Copy(KMLauncherDebugFolder2);
	else oldPath.Copy(KMLauncherDebugFolder);
	TFileName newPath(oldPath);
	oldPath.Append(KMLauncherLog);
	newPath.Append(KMLauncherLogCrashed);

	RFile logFile;
	TBool crashDetected(EFalse);
	err=logFile.Open(*iFs,oldPath,EFileRead);
	/*
	while(err==KErrInUse)
	{
		//we do this in case the user starts MLauncher very soon after a crash.
		//RFileLogger keeps the file in use for about 2 seconds after it gets closed
		User::After(100000); //0.1 s
		err=logFile.Open(iee->FsSession(),oldPath,EFileRead);
	};*/
	if(!err)
	{
		//file exists and it can be opened
#ifdef __WINS__
		TPtrC8 logClosing8((const unsigned char*)KLogClosing);
#else
		TBuf8<KLogClosingLength> logClosing8;
		logClosing8.Copy(KLogClosing);
#endif
		TInt length=logClosing8.Length()+KEOL().Length();
		TInt pos=-length;
		err=logFile.Seek(ESeekEnd,pos);

		HBufC8 *lastRow8=HBufC8::NewLC(length);
		TPtr8 ptrLastRow8(lastRow8->Des());
		logFile.Read(ptrLastRow8);
		logFile.Close();
		pos=lastRow8->Length()-KEOL().Length();
		if(pos>=0)lastRow8->Des().Delete(pos,KEOL().Length());
		err=lastRow8->Des().Compare(logClosing8);
		if(err)
		{
			//the log file was not closed properly.
			err=iFs->Delete(newPath);
			err=iFs->Rename(oldPath,newPath);
			crashDetected=ETrue;
		};
		CleanupStack::PopAndDestroy(lastRow8);
	};

	//open the log
	instance=new FLLog;
	User::LeaveIfNull(instance);
	err=instance->iLog.Replace(*iFs,oldPath,EFileWrite);
	instance->iLogStartTime.UniversalTime();
	instance->iFlags=iDefaultFlags;
	
	//write version
	CArrayFixFlat<TInt> *version=new(ELeave)CArrayFixFlat<TInt>(3);
	CleanupStack::PushL(version);
	TInt ver=Vmajor;
	version->AppendL(ver);
	ver=Vminor;
	version->AppendL(ver);
#ifdef Vdevelopment
	ver=Vbuild;
	version->AppendL(ver);
	HBufC* versionStr=StringLoader::LoadLC(R_MLAUNCHER_DEVELOPMENT,*version);
#else
	HBufC* versionStr=StringLoader::LoadLC(R_ABOUT_HEADER,*version);
#endif
	
#ifdef __WINS__
	HBufC8 *versionStr8=HBufC8::NewLC(versionStr->Length()+1);
	versionStr8->Des().Copy(*versionStr);
	instance->Write(0,versionStr8->Des().PtrZ());
	CleanupStack::PopAndDestroy(versionStr8);
#else
	instance->Write(0,*versionStr);
#endif
	CleanupStack::PopAndDestroy(2,version);
	return crashDetected;
}


#ifndef __WINS__
void FLLog::Write(TInt aIndent, TRefByValue<const TDesC> aFmt, ...)
{
	VA_LIST args;
	VA_START(args,aFmt);
	Write(aIndent,aFmt,args);
	VA_END(args);
}
#endif

#ifdef __WINS__
void FLLog::Write(TInt aIndent, ...)
#else
void FLLog::Write(TInt aIndent, TRefByValue<const TDesC> aFmt, VA_LIST aList)
#endif
{	
	TUint bufferLogLength(BUFFER_LOG);
	bufferLogLength<<=10; //kB
	if(!iLogBuf)
	{
		//allocate iLogBuf
#if BUFFER_LOG >0
		iLogBuf=HBufC8::New(bufferLogLength);
#else
		iLogBuf=HBufC8::New(KLogLineLength); //a single line
#endif
	};
	if(!iLogBuf)return; //buffer empty, can not log
	
	TBuf<KLogLineLength> buf,format;
	TInt i;
	
	//indentation
/*
	TInt pos;
	if((pos=(aFmt).Find(_L("++")))>=0)
		iIndentation++;
	if((pos=(&aFmt).Find(_L("--")))>=0)
		iIndentation--;*/

	if(aIndent<0)
		iIndentation+=aIndent;
	for(i=0;i<iIndentation;i++)
		buf.Append(_L("  "));
	if(aIndent>0)
		iIndentation+=aIndent;
#ifdef __WINS__
	VA_LIST args;
	VA_START(args,aIndent);
	const TUint8 *fmt=VA_ARG(args,const TUint8*);
	format.Copy(TPtrC8(fmt));
	buf.AppendFormatList(format,args);
	VA_END(args);
#else
	buf.AppendFormatList(aFmt,aList);
#endif
	
	//timing stuff
	TTime now;
	now.UniversalTime();
	TTimeIntervalMicroSeconds tDelta(now.MicroSecondsFrom(iLogStartTime));
	TInt64 td(tDelta.Int64());
	TInt min,sec,msec;
	msec=td%1000000;
	td/=1000000;
	sec=td%60;
	td/=60;
	min=(TInt)td;
#if BUFFER_LOG >0
	if(iLogBuf->Length()+KLogLineLength>bufferLogLength)
	{
		//dump the buffer into the file
		iLog.Write(*iLogBuf);
		iLogBuf->Des().SetLength(0);
	};
	iLogBuf->Des().AppendFormat(_L8("%4d:%02d.%06d "),min,sec,msec);
#else
	iLogBuf->Des().Format(_L8("%4d:%02d.%06d "),min,sec,msec);
#endif
	iLogBuf->Des().Append(buf);
	iLogBuf->Des().Append(KEOL8);
#if BUFFER_LOG == 0
	iLog.Write(*iLogBuf);
#ifdef _DEBUG
	TBuf<KLogLineLength> b16;
	b16.Copy(*iLogBuf);
	RDebug::RawPrint(b16);
#endif
	iLogBuf->Des().SetLength(0);
#else
#ifdef _DEBUG
	RDebug::Print(buf);
#endif
#endif
	
	if(iIndentation<0)iIndentation=0; //we can "legally" have indentation less than zero if the log was enabled in the middle of the program
	//FL_ASSERT(iIndentation>=0);
}

FLLog::~FLLog()
{
#if BUFFER_LOG >0
	iLog.Write(*iLogBuf);
#endif
	iLog.Close();//must be called
	delete iLogBuf;
	instance=NULL;
}

FLLog::FLLog() : iLogBuf(NULL), iIndentation(0)
{
	//no implementation necessary
}

void FLLog::EnableL(TBool aEnable)
{
	//if log is enabled, then we create KMLauncherDebugFolder
	//if log is disabled, then we delete KMLauncherDebugFolder
	//in both cases we delete and recreate the log instance
	TFileName path(KMLauncherDebugFolder);
	
	if(aEnable)
	{
		iFs->MkDirAll(path);
		delete instance;
		CreateInstanceL(iFs,iDefaultFlags);
	}
	else if(instance)
	{
		instance->End(EFalse);
		TInt err;
		CFileMan *fm=CFileMan::NewL(*iFs);
		if(fm)
		{
			err=fm->RmDir(path);
			delete fm;
		};
		delete instance;
		instance=NULL;
	};
}
/*
TInt FLLog::IdleWorker(TAny *aInstance)
{
	//do the work
	TInt err(0);
	TFileName path(KLogsPath);
	path.Append(KMLauncherFolder);
	path.Append(KDirBackslash);

	CFileMan *fm=CFileMan::NewL(FLLog::iee->FsSession());
	if(fm)
	{
		err=fm->RmDir(path);
	    delete fm;
	};

	if(!err || err==KErrPathNotFound)
	{
		//we deleted the folder (or there is no folder)
		//nothing to do anymore

		delete iIdle;
		iIdle=NULL;
		CreateInstanceL(iee,iFlags);
		return 0; //no more work to do
	};
	return 1; //still something to do
}
*/

void FLLog::End(TBool aPanic)
{
	if(aPanic)Write(0, KLogClosingPanic);
	else Write(0, KLogClosing);

	delete instance;
	instance=NULL;
/*
#ifdef Vdevelopment
    //move files to E:\Debug
    RFs fs;
    fs.Connect(5);
    CFileMan *fMan=CFileMan::NewL(fs);
    fMan->Copy(KOurPrivateFolder,KDebugFolder,CFileMan::EOverWrite|CFileMan::ERecurse);
    //fMan->Copy(_L("C:\\logs\\MLauncher\\MLauncher.log"),KDebugFolder,CFileMan::EOverWrite);
    delete fMan;
    fs.Close();
#endif
*/
}

/*
TBool FLLog::Enabled()
{
	return ETrue; //if we have an instance, the log is valid
}
*/

void FLLog::GetCrashLogFilename(TFileName &aFilename)
{
	aFilename.Copy(KMLauncherDebugFolder);
	aFilename.Append(KMLauncherLogCrashed);
}

/*
TRefByValue<const TDesC16> FLLog::GetusFormat(TRefByValue<const TDesC16> aFormat)
{
	TTime now;
	now.UniversalTime();
	TTimeIntervalMicroSeconds tius(now.MicroSecondsFrom(iStartTime));
	TInt us(tius.Int64());
	iFormat.Format(_L("[%9d us]: "),us);
	iFormat.Append(aFormat);
	return TRefByValue<const TDesC16>(iFormat);
}
*/
#endif //RELEASE_LOG
