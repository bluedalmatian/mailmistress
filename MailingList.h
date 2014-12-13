/*MailingList.h
 *
 *Copyright 2013-2014 Andrew Wood awood@comms.org.uk
 *All rights reserved
 */


#ifndef CLASS_MAILINGLIST
 #define CLASS_MAILINGLIST
 
#include <string>
#include <storage/File.h>
#include <kernel/OS.h>
#include "MailMistressApplication.h"
#include "IncomingMail.h"

class MailMistressApplication;
class IncomingMail;

class MailingList
{
	public:
		MailingList(std::string icemail, std::string ogenvemail,std::string listname,std::string subjectpre,std::string dirpath,std::string authprogpath, std::string logfilepath, 
						bool forcetolist, std::string listowner, std::string listhelp, std::string listarchive, std::string listsubscribe, std::string listunsubscribe);
		bool InitLogFileSemaphores();
		void SetBounceMsg(std::string path);
		void SetArchivePath(std::string path);
		void SetMaxMsgBytes(unsigned long int bytes);
		void SetPlainTextOnly(char pto);
		
		void Go();
		bool ProcessEmailFile(std::string emailfilepathstr);
		bool DistributeEmail(IncomingMail* ICMail, std::string ogaddrfilepath);
		void LogError(std::string msg);
		void CheckInFolder();
		std::string GetName();
		std::string GetICAddress();
		std::string GetSubjectPrefix();
		void DeleteLogFileSem();
	
		
	protected:
		MailMistressApplication* fApp;
		std::string fListICAddress;
		std::string fListOGEnvelopeAddressFromAddress;
		std::string	fListName;
		std::string fListSubjectPrefix;
		std::string fListInDirectoryPath;
		std::string fAuthProgPath;
		std::string fUnauthorisedBounceMsgContents;
		std::string fArchivePath;
		std::string fLogFilePath;
		sem_id fLogFileSem;
		uint32 fMaxContentBytes;
		bool fLogSuccesses;
		char fPlainTextOnly;
		bool fForceReplyToList;
		std::string fListOwner;
		std::string fListHelp;
		std::string fListArchive;
		std::string fListSubscribe;
		std::string fListUnSubscribe;
		
	
		
};

#endif
