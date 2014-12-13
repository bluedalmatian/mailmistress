/*IncomingMail.h
 *
 *Copyright 2013-2014 Andrew Wood awood@comms.org.uk
 *All rights reserved
 */


#ifndef CLASS_INCOMINGMAIL
 #define CLASS_INCOMINGMAIL
 
#include <string>
#include <storage/File.h>
#include <kernel/OS.h>
#include "MailMistressApplication.h"

class MailingList;
class MailMistressApplication;

class IncomingMail
{
	public:
		IncomingMail(BFile* icmsg, MailingList* list);
		bool InitCheck();
		off_t GetICFileSize();
		std::string GetFromField();
		std::string ExtractEmailAddressFromFromField(std::string sendersAddrField);
		std::string GetSendersAddr();
		bool FindHeaderStartAndFinish(std::string headerhandle, int* startidx, int* endidx);
		void RemoveHeader(int startidx, int endidx);
		void AddSubjectPrefix(std::string prefix);
		void AdjustReplyTo(bool forcereplytolist);
		void CleanHeaders();
		void SetListOwnerHeader(std::string listowner);
		void SetListHelpHeader(std::string listhelp);
		void SetListArchiveHeader(std::string listarchive);
		void SetListSubscribeHeader(std::string listsubscribe);
		void SetListUnSubscribeHeader(std::string listunsubscribe);
		void WriteToFile(BFile* tempFileBFile);
		bool CheckIfPlainTextCriteriaPassed(char plainTextOnly);
		bool CheckMultiPartBodyAgainstPlainTextCriteria(std::string delimeter);
		std::string ExtractMIMEBoundaryDelimeter(std::string boundaryattr);
		void PrintfHeaders();
		
		
	protected:
		bool fInitOK;
		MailingList* fList;
		MailMistressApplication* fApp;
		std::vector<std::string> fHeaderlines;
		std::vector<std::string> fBodylines;
		off_t fICFileSize;
		
	
		
};

#endif
