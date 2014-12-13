/*MailMistressApplication.h
 *
 *The main BApplication subclass for the server app
 *
 *Copyright 2013 Andrew Wood awood@comms.org.uk
 *All rights reserved
 */

#ifndef CLASS_MAILMISTRESSAPPLICATION
 #define CLASS_MAILMISTRESSAPPLICATION
 
#include <string>
#include <map>
#include <vector>
#include <mail/E-mail.h>
#include <app/Application.h>
#include <kernel/OS.h>

#include "MailingList.h"
#include "MailWorker.h"

class MailingList;
class MailWorker;

class MailMistressApplication : public BApplication
{
	
	public:
		MailMistressApplication(std::string build, std::string platform);
		void Pulse();
		void MessageReceived(BMessage* message);
		virtual bool QuitRequested();
		MailingList* FindListByDir(std::string path);
		MailingList* FindListByICAddress(std::string addr);
		MailingList* FindListByOGAddress(std::string addr);
		std::string GetTempDirPath();
		std::string GetAppName();
		std::string GetAppVersionString();
		float GetMajorMinorVersion();
		int GetSubMinorVersion();
		std::string GetBuildString();
		std::string GetPlatformString();
		std::string GetHostname();
		bool ParseConfigFile();
		bool ParseConfigFileGlobalSection(std::vector<std::string> lines,std::vector<int> originallinenos);
		bool ParseConfigFileListSection(std::vector<std::string> lines,std::vector<int> originallinenos);
		void LogError(std::string msg, bool critical=false, bool waitforack=false);
		unsigned long int GetMaxMsgBytes();
		char GetPlainTextOnly();
		std::string GetDateTime();
	
		
		
	protected:
	
		std::string _prependLeadingZeroIfNeeded(int num);
		MailWorker* _GetNextWorker();
	
		float fMajorMinorVer;
		int fSubMinorVer;
		std::string fVersionString;
		std::string fAppNameString;
		std::string fBuildString;
		std::string fPlatformString;
		int fPulseSeconds;
		std::string fTempDirPath;
		std::string fConfFileName;
		std::string fLogFilePath;
		sem_id fLogFileSem;
		int fNumThreads;
		unsigned long int fMaxBytes;
		bool fForceLogToScreen;
		char fPlainTextOnly;
		unsigned long fUpMinutes;
		unsigned long fTotalICMsgs;
		int fCheckDirectoryMins;
		
		
		
		std::vector<MailWorker*> fWorkers;
		int fLastWorkerUsed; //allows fWorkers to be cycled through for workload distribution


		//These maps contain all MailingList objs indexed by various handles
		//Once app is running & MailWorkers have been started these must NOT be altered
		//as MailWorker searches them without a lock
		std::map<std::string,MailingList*> fListsByDir;
		std::map<std::string,MailingList*> fListsByICAddress;
		std::map<std::string,MailingList*> fListsByOGAddress;
		
	
};
#endif
