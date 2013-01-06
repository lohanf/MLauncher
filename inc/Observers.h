/*
 * Observers.h
 *
 *  Created on: Jun 28, 2010
 *      Author: Florin
 */

#ifndef OBSERVERS_H_
#define OBSERVERS_H_

class CMLauncherPreferences;
class MHTTPclientObserver
{
public:
	virtual void CheckForUpgradeEnded(TInt aStatus)=0;
	virtual void CrashLogfileUploadEnded(TInt aStatus)=0;
	virtual CMLauncherPreferences* Preferences()=0;
	enum TMyStatusIds
	{
		EUpgradeFailed=1,
		ESisDownloaded,
		ESisDownloadedInstall,

		ELogUploadedSuccessfully=0,
		ELogUploadFileCouldNotBeMoved=-1001,
		ELogUploadInternalError=-1002,
		ELogUploadWrongFilename=-1003,
		ELogUploadTransactionFailed=-1010,
		ELogUploadParsingFailed=-1011
	};
};

#endif /* OBSERVERS_H_ */
