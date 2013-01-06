/*
 * ThreadObserver_notused.h
 *
 *  Created on: Nov 19, 2011
 *      Author: Florin
 */

#ifndef THREADOBSERVER_NOTUSED_H_
#define THREADOBSERVER_NOTUSED_H_

#ifdef 0
class CThreadObserver : public CActive
{
public:
	enum TMessages
	{
		EMsgUpdateView=1,
		EMsgDone=2,
		EMsgSourcesDone=4,
	};
	CThreadObserver(CMLauncherAppUi *aAppUi);
	~CThreadObserver();
	TInt LaunchThread();
	
protected:
	virtual void RunL();
	virtual void DoCancel();
	//virtual TInt RunError(TInt aError);
private:
	static TInt ThreadFunction(TAny *aInstance);
	static TInt UpdateView(TAny *aInstance);
	void GetNewSourcesL();
	void AddToExistingSourcesL();
	void ParseMetadataL();
public:
	TBool iShouldExit;
	RThread iThread;
private:
	CMLauncherAppUi *iAppUi;//not owned
	RPointerArray<HBufC> iSources;
	RArray<TInt> iSourcesCount;
	CTrapCleanup *iCleanupStack;
	RMsgQueue<TInt> iQueue;
	RMutex iMutex;
};

#endif
#endif /* THREADOBSERVER_NOTUSED_H_ */
