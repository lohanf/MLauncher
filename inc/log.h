#ifndef LOG_H_
#define LOG_H_

#include <f32file.h>
#include "defs.h"


#if RELEASE_LOG
class CEikonEnv;


enum TFlags
{
	ELogGeneral=1,
	ELogMusicPlayerDraw=2,
	ELogBT=4,
	ELogNewSources=8,
};

#define NR_LOGS 2 //we do not need more logs, at the moment

_LIT(applicationName,"MLauncher");

class FLLog
{
public:
	static TBool CreateInstanceL(RFs *aFs, TInt aFlags); //returns ETrue if a crash log file was detected
	~FLLog();
	
	static void EnableL(TBool aEnable);
	//TBool Enabled();
	
	void End(TBool aPanic);
	
	void GetCrashLogFilename(TFileName &aFilename);
#ifdef __WINS__
	void Write(TInt aIndent, ...);
#else
	void Write(TInt aIndent, TRefByValue<const TDesC> aFmt, VA_LIST aList);
	void Write(TInt aIndent, TRefByValue<const TDesC> aFmt, ...);
#endif

public:
	static FLLog* instance;
	TUint iFlags;
	static TUint iDefaultFlags;
private:
	FLLog();
	
private:
	RFile iLog;
	TFileName iLogFilename;
	//static TInt IdleWorker(TAny *aInstance);
	static RFs *iFs;
	//static CIdle *iIdle;
	//static RFile *iDebugLog;
	
	TTime iLogStartTime;
	HBufC8 *iLogBuf;
	TInt iIndentation;
};

#define WIDEN2(x) L ## x
#define WIDEN(x) WIDEN2(x)
#define __DBG_FILE__ WIDEN(__FILE__)

#define FL_ASSERT(c) {if(FLLog::instance && !(c)){ \
	FLLog::instance->Write(0,_L("Assertion failed in %s at line %d."), __DBG_FILE__ , __LINE__ ); \
	FLLog::instance->End(ETrue); \
	User::Panic(applicationName, 1000);} }

#define LOG_START(x,f)    FLLog::CreateInstanceL(x,f)
#define LOG_ENABLED       (FLLog::instance)
#define LOG_ENABLED_FLAG(f)    (FLLog::instance&&(f&FLLog::instance->iFlags))
#define LOG_ENABLE(x)     {FLLog::EnableL(x); }

#ifdef __WINS__
#define LOG(f,i,x...)        {if(FLLog::instance && (f&FLLog::instance->iFlags))FLLog::instance->Write(i,x); }
#define LOG0(x...)      {if(FLLog::instance && (ELogGeneral&FLLog::instance->iFlags))FLLog::instance->Write(0,x); }
#define LOGret(r,x...)      {if(FLLog::instance && (ELogGeneral&FLLog::instance->iFlags))FLLog::instance->Write(-1,##x); return r;}
#define LOGleave(r,x...)      {if(FLLog::instance && (ELogGeneral&FLLog::instance->iFlags))FLLog::instance->Write(-1,##x); User::Leave(r);}

#define FL_ASSERT_TXT(c,x...) {if(FLLog::instance && !(c)){ \
	FLLog::instance->Write(0,_L("Assertion failed in %s at line %d:"), __DBG_FILE__ , __LINE__ ); \
	FLLog::instance->Write(0,##x ); \
	FLLog::instance->End(ETrue); \
	User::Panic(applicationName, 1000);} }

#else
#define LOG(f,i,x, ...)        {if(FLLog::instance && (f&FLLog::instance->iFlags))FLLog::instance->Write(i,_L(x),##__VA_ARGS__ ); }
#define LOG0(x, ...)      {if(FLLog::instance && (ELogGeneral&FLLog::instance->iFlags))FLLog::instance->Write(0,_L(x),##__VA_ARGS__ ); }
#define LOGret(r,x, ...)      {if(FLLog::instance && (ELogGeneral&FLLog::instance->iFlags))FLLog::instance->Write(-1,_L(x),##__VA_ARGS__ ); return r;}
#define LOGleave(r,x, ...)      {if(FLLog::instance && (ELogGeneral&FLLog::instance->iFlags))FLLog::instance->Write(-1,_L(x),##__VA_ARGS__ ); User::Leave(r);}

#define FL_ASSERT_TXT(c,x, ...) {if(FLLog::instance && !(c)){ \
	FLLog::instance->Write(0,_L("Assertion failed in %s at line %d:"), __DBG_FILE__ , __LINE__ ); \
	FLLog::instance->Write(0,_L(x),##__VA_ARGS__ ); \
	FLLog::instance->End(ETrue); \
	User::Panic(applicationName, 1000);} }

#endif 

#define LOG_END           {if(FLLog::instance)FLLog::instance->End(EFalse); }
#define LOG_END_PANIC     {if(FLLog::instance)FLLog::instance->End(ETrue); }

#else //RELEASE_LOG

#define LOG_START(x,f)     EFalse
#define LOG_ENABLED      EFalse
#define LOG_ENABLE(x)
#define LOG(f,i,x)

#define LOG_END
#define LOG_END_PANIC

#endif //RELEASE_LOG


#endif /*LOG_H_*/
