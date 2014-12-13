
/*MailWorkers are BLoopers which process incoming mail, freeing up the BApp
 *These MailWorkers do not belong to any trade unions, do not go on strike
 *or demand ever higher wages for increasingly shoddy work.
 *Nor do they steal items of mail and hardly ever loose any
 *
 *Copyright 2013 Andrew Wood awood@comms.org.uk
 *All rights reserved
 */

#ifndef CLASS_MAILWORKER
 #define CLASS_MAILWORKER
 
 
#include <string>
#include <map>
#include <mail/E-mail.h>
#include <app/Application.h>
#include <app/Looper.h>


#include "MailingList.h"
#include "MailMistressApplication.h"

class MailingList;
class MailMistressApplication;

class MailWorker : public BLooper
{
	
	public:
		MailWorker(int id);
		void MessageReceived(BMessage* message);
		bool QuitRequested();
		void ProcessEmailEntryRef(BEntry bentry);
		
	protected:
		MailMistressApplication* fApp;
		int fID; //numeric id of this worker for debugging identification - this is just the order they were spawned
	
};
#endif
