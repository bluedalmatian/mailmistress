
/*MailWorker.cpp
 *MailWorkers are BLoopers which process incoming mail, freeing up the BApp
 *These MailWorkers do not belong to any trade unions, do not go on strike
 *or demand ever higher wages for increasingly shoddy work.
 *Nor do they steal items of mail and hardly ever loose any
 *
 *Copyright 2013 Andrew Wood awood@comms.org.uk
 *All rights reserved
 */

#include "MailWorker.h"
#include "AppWideBMessageDefs.h"
#include <storage/Directory.h>
#include <storage/File.h>
#include <storage/Path.h>
#include <storage/NodeMonitor.h>
#include <storage/Entry.h>

MailWorker::MailWorker(int id) : BLooper("MailWorker")
{
	fID=id;
	fApp=(MailMistressApplication*) be_app;
}
void MailWorker::MessageReceived(BMessage* message)
{
	
	if (message->what==ORG_SIMPLE_MAILMISTRESS_CHECKINDIR)
	{	
			std::string pathstr=std::string(message->FindString("ICDirPathStr"));
			
			MailingList* ml=NULL;
			ml=fApp->FindListByDir(pathstr); //safe without locking as maps in BApp are read-only once app is running
			if (ml==NULL)
			{
					//dont need to Lock() the BApp to call fApp->LogError as this method locks the actual file itself
					fApp->LogError("ERROR: MailWorker::MessageReceived() could not find MailingList obj from str path to IC dir");
					return;
			}
			else
			{
				ml->CheckInFolder();
			}
	}
	else if (message->what==ORG_SIMPLE_MAILMISTRESS_PROCESSFILE)
	{
		
		std::string pathstr=std::string(message->FindString("FilePathStr"));
		//convert pathstr to BEntry and call this->ProcessEmailEntryRef()
		BEntry ent(pathstr.c_str());
		this->ProcessEmailEntryRef(ent);
	}
	else
	{
		BLooper::MessageReceived(message);	
	}
}
bool MailWorker::QuitRequested()
{
	return true;	
}

void MailWorker::ProcessEmailEntryRef(BEntry bentry)
{
	//lock file to prevent race cond between Node Monitor messages and the loop through of exisiting files at startup
	BPath bpath;
	bentry.GetPath(&bpath);
	entry_ref eref;
	bentry.GetRef(&eref);
	snooze(1000000); //TODO: This is a temp kludge because Haiku mail daemon doesnt lock file so we have to wait until its finished processing it otherwise we read an empty file!!
	std::string filepathstr(bpath.Path());
	//check which MailingList it belongs to, hand off for processing,check return value
	MailingList* ml=NULL;
	BPath bpathDir;
	bpath.GetParent(&bpathDir);
	ml=fApp->FindListByDir(std::string(bpathDir.Path())); //safe without locking as maps in BApp are read-only once app is running
	if (ml==NULL)
	{
			return;
	}
	bool msgprocessstatus=ml->ProcessEmailFile(filepathstr); 
	//delete file if processed ok
	if (msgprocessstatus)
	{
		bentry.Remove(); 
	}
}
