/*MailMistressApplication.cpp
 *
 *The main BApplication subclass for the server app
 *
 *Copyright 2013-2014 Andrew Wood awood@comms.org.uk
 *All rights reserved
 */

#include "MailMistressApplication.h"
#include "AppWideBMessageDefs.h"
#include "AppWideConstDefs.h"
#include "MailingList.h"
#include "MailWorker.h"
#include <storage/Directory.h>
#include <storage/File.h>
#include <storage/Path.h>
#include <storage/NodeMonitor.h>
#include <storage/Entry.h>
#include <storage/AppFileInfo.h>
#include <app/Roster.h> //needed for app_info struct
#include <sstream>
#include <string>
#include <vector>
#include <image.h> //to run external progs
#include <OS.h> //to run external progs
#include <interface/Alert.h>
#include <unistd.h>

#define kFORCELOGTOSCREENWITHGUI true  //set to false to use printf CLI rather than GUI dialog when fForceLogToScreen is true

MailMistressApplication::MailMistressApplication(std::string build, std::string platform) : BApplication("application/x-vnd.org.simple-MailMistress")
{
	//App version, set here only-- but also need to update rdef file
	fMajorMinorVer=0.8;
	fSubMinorVer=5;
	std::stringstream versionconverter;
	versionconverter << fMajorMinorVer << "." << fSubMinorVer;
	fVersionString = versionconverter.str();

	
	fBuildString=build;
	fPlatformString=platform;
	
	fAppNameString = std::string("MailMistress"); //this should always be the simple canonical name for the app e.g xyz not xyz mailing list server for platform x or version
	
	fUpMinutes=0; //will be incremented by Pulse() each minute.
	fTotalICMsgs=0; //incremented each time an IC msg is handled
	
	fLogFilePath=""; //do this before anything else as LogError will check it
	fForceLogToScreen=true; //during startup use printf for all logging so user can see its launched ok
	
	LogError(fAppNameString+" "+fVersionString+" "+fBuildString+" "+fPlatformString+" starting...");
	
	
	//Name of config file (always in same dir as app) set here only with no leading /
	fConfFileName=std::string("MailMistress.conf");
	
	//EVERYTHING BELOW HERE IS A SETTING WHICH WILL COME FROM CONFIG FILE
	
	//pulsing is used to do housekeeping
	//-process any files which have been 'missed' due to a temp fault when they were first processed
	//-kill jammed auth progs
	//quit time limited version of app
	
	//set up defaults for housekeeping frequencies FIRST
	fCheckDirectoryMins=kFAILEDMSGRETRY_DEFAULT; //1hr default
	//enable pulsing now
	fPulseSeconds=60; //Pulse() is *always* called once per min -- never change this
	SetPulseRate(fPulseSeconds*1000000);
	
	
	
	fMaxBytes=kMAXMSGBYTES_DEFAULT; //default server wide sanity limit
	
	fNumThreads=kTHREADS_DEFAULT;
	
	//temp dir used for temp files such as list of recipients from authentication programs
	//the dir path (without a trailing /) is given in conf file, this is whats pre-pended to the filename after the path
	fTempDirPath=std::string("MailMistress-");
	
	fPlainTextOnly='N';

	
	bool configstatus=ParseConfigFile();
	
	if (fLogFileSem = create_sem(1,"fLogFileSem") < B_NO_ERROR)
	{
			LogError("FATAL ERROR when initialising application log file semaphore. "+GetAppName()+" cannot start",true,true);
			be_app->PostMessage(B_QUIT_REQUESTED);
	}
	
	if (configstatus==false)
	{
		LogError("FATAL ERROR when processing configuration file. "+GetAppName()+" cannot start",true,true);
		be_app->PostMessage(B_QUIT_REQUESTED);
	}
	else
	{
		LogError("Configuration file processed OK, away we go...");	
		fForceLogToScreen=false; //switch to logging to file
	
		//fire (up) workers
		fLastWorkerUsed=-1;
		for (int x=0; x <fNumThreads; x++)
		{
			MailWorker* worker= new MailWorker(x+1);
			worker->Run();
			fWorkers.push_back(worker);
		}
		//fire up each list
		std::map<std::string,MailingList*>::iterator lit;
		int listcount=0;
		for (lit=fListsByICAddress.begin(); lit!=fListsByICAddress.end(); lit++)
		{
			bool listok=(lit->second)->InitLogFileSemaphores();
			if (listok==false)
			{
				std::string listaddr=(lit->second)->GetICAddress();
				//MailingList could not init its semaphores, this is a fatal error
				LogError("FATAL ERROR when initialising list log file semaphore for list "+listaddr+" . "+GetAppName()+" cannot start");
				be_app->PostMessage(B_QUIT_REQUESTED);	
			}
			printf("\nFiring up List...\n");
			(lit->second)->Go();	
			printf("List fired up\n\n");
			listcount++;
		}
	
		std::stringstream infolog;
		infolog << fNumThreads << " worker thread(s) and " << listcount << " list(s)...";
		LogError(fAppNameString+" "+fVersionString+" "+fBuildString+" "+fPlatformString+" starting with "+infolog.str()); //will goto log file not screen
	}
	
}

void MailMistressApplication::Pulse()
{
	fUpMinutes++;
	if (fUpMinutes==0)
	{
		//it just rolled over so set it to 1 not 0 (its a 64 bit int so this will only happen every 8171 years anyway!)
		//but we dont want a divide by zero so better to be safe
		fUpMinutes++;
	}
	
	
	//use modulus op to see if we need to perform a housekeeping task
	
	if (fUpMinutes%fCheckDirectoryMins==0)
	{
		//check all lists for email files which have not been processed because of an error the first time they were done
		//cycle through all MailingLists and for each one pick a worker and ask it to check its in dir
		std::map<std::string,MailingList*>::iterator lit;
		for (lit=fListsByDir.begin(); lit!=fListsByDir.end(); lit++)
		{
			MailWorker* mw=_GetNextWorker();
			BMessage msg(ORG_SIMPLE_MAILMISTRESS_CHECKINDIR);
			//(lit->second) gives pointer to MailingList obj
			status_t status=msg.AddString("ICDirPathStr",(lit->first).c_str());
			if (status==B_OK)
			{
				mw->PostMessage(&msg,NULL);
			}
			else
			{
				LogError("ERROR: MailMistressApplication::Pulse() could not add IC dir path string to BMessage");	
			}
		}
		
	}
	
	
	
	
	
}

void MailMistressApplication::MessageReceived(BMessage* message)
{
	if (message->what == B_NODE_MONITOR)
	{
		int32 opcode;
		if (message->FindInt32("opcode",&opcode) == B_OK)
		{
			if ( (opcode == B_ENTRY_CREATED) || (opcode=B_ENTRY_MOVED) )
			{
				
				//create an entry_ref pointing to the files containing dir and hand off every file in that dir for processing
				entry_ref eref;
				const char* name;
				message->FindInt32("device",&eref.device);
				if (opcode ==B_ENTRY_CREATED)
				{
					message->FindInt64("directory",&eref.directory);
				}
				else
				{
					message->FindInt64("to directory",&eref.directory);
				}
				message->FindString("name",&name);
				eref.set_name(name);
				//eref is the new file, we want the containing dir
				BEntry bentry(&eref);
				BPath bpath;
				bentry.GetPath(&bpath);
				BPath dirbpath;
				bpath.GetParent(&dirbpath);
				BDirectory indir(dirbpath.Path());
				//process each file in dir
				BEntry filebentry; //represents each & every file in dir
				while(indir.GetNextEntry(&filebentry) == B_NO_ERROR)
				{
					//for each file in dir, post a PROCESSFILE msg to a MailWorker
					BMessage msg(ORG_SIMPLE_MAILMISTRESS_PROCESSFILE);
					BPath filebpath;
					filebentry.GetPath(&filebpath);
					std::string pathstr(filebpath.Path());
					msg.AddString("FilePathStr",pathstr.c_str());
					MailWorker* mw= _GetNextWorker();
					mw->PostMessage(&msg);
					fTotalICMsgs++;
				}
				
			}
			
		}	
	}
	else if (message->what==ORG_SIMPLE_MAILMISTRESS_PROCESSFILE)
	{
		
		//forward to MailWorker
		MailWorker* mw= _GetNextWorker();
		mw->PostMessage(message);
		fTotalICMsgs++;
	}
	else
	{
		BApplication::MessageReceived(message);	
	}
}

bool MailMistressApplication::QuitRequested()
{
	LogError("QuitRequested, telling MailWorkers to knock off...");
	std::vector<MailWorker*>::iterator mwit;
	for (mwit=fWorkers.begin(); mwit!=fWorkers.end(); mwit++)
	{
		(*mwit)->Lock();
		(*mwit)->Quit(); //this deletes MailWorker obj after waiting for it to clear its queue
	}
	LogError("...workers gone home, cleaning up semaphores...");
	//because semaphores are OS wide they should be explicity deleted before app quits
	delete_sem(fLogFileSem);
	
	std::map<std::string,MailingList*>::iterator mlit;
	for (mlit=fListsByDir.begin(); mlit!=fListsByDir.end(); mlit++)
	{
		(mlit->second)->DeleteLogFileSem();
	}
	LogError("..application now quitting");
	return true;	
}

MailingList* MailMistressApplication::FindListByDir(std::string path)
{
			//Return pointer to the MailingList obj whose IN directory is at path or NULL if not found
			
			std::map<std::string,MailingList*>::iterator pos=fListsByDir.find(path);
			if (pos!=fListsByDir.end())
			{
				//ele found
				return pos->second;
					
			}
			else
			{
				return NULL;	
			}
}

MailingList* MailMistressApplication::FindListByICAddress(std::string addr)
{
			//Return pointer to the MailingList obj whose incoming email address is addr or NULL if not found
			
			std::map<std::string,MailingList*>::iterator pos=fListsByICAddress.find(addr);
			if (pos!=fListsByICAddress.end())
			{
				//ele found
				return pos->second;
					
			}
			else
			{
				return NULL;	
			}
}
MailingList* MailMistressApplication::FindListByOGAddress(std::string addr)
{
			//Return pointer to the MailingList obj whose outgoing email address is addr or NULL if not found
			
			std::map<std::string,MailingList*>::iterator pos=fListsByOGAddress.find(addr);
			if (pos!=fListsByOGAddress.end())
			{
				//ele found
				return pos->second;
					
			}
			else
			{
				return NULL;	
			}
}
std::string MailMistressApplication::GetTempDirPath()
{
	return fTempDirPath;	
}
std::string MailMistressApplication::GetAppName()
{
	return fAppNameString;
}
		
std::string MailMistressApplication::GetAppVersionString()
{
	return fVersionString;
}
float MailMistressApplication::GetMajorMinorVersion()
{
	return fMajorMinorVer;
}

int MailMistressApplication::GetSubMinorVersion()
{
	return fSubMinorVer;
}
std::string MailMistressApplication::GetBuildString()
{
	return fBuildString;
}
std::string MailMistressApplication::GetPlatformString()
{
	return fPlatformString;
}
std::string MailMistressApplication::GetHostname()
{
	std::string hostname=std::string("unknown host");	
	
	char chostname[256];
	if (gethostname(chostname,255)==0)
	{
			hostname=std::string(chostname);
			return hostname;
	}
	else
	{
		//hostname could not be determined
		return hostname;	
	}
}
bool MailMistressApplication::ParseConfigFile()
{
	app_info ai;
	be_app->GetAppInfo(&ai);
	BEntry appfilebentry(&ai.ref);
	BEntry appdirbentry;
	appfilebentry.GetParent(&appdirbentry);
	BPath appdirbpath;
	appdirbentry.GetPath(&appdirbpath);
	std::string conffilepath=std::string(appdirbpath.Path());
	conffilepath=conffilepath+"/"+fConfFileName;
	
	//open file
	BFile conffile;
	status_t status=conffile.SetTo(conffilepath.c_str(),B_READ_ONLY); //must specify read only so we fail if file doesnt exist
	//check if conf file exists
	if (status!=B_NO_ERROR)
	{
		if (status==B_ENTRY_NOT_FOUND)
		{
			LogError("Fatal error reading configuration file: Could not read file ("+conffilepath+") because it does not exist",true,true);
			return false; //conf file must exist as it has some mandatory directives in it, plus a server with no lists defined is useless!
		}
		if (status==B_PERMISSION_DENIED)
		{
			LogError("Fatal Error reading configuration file: Permission denied",true,true);
			return false;	
		}
		LogError("Fatal Error reading configuration file: Could not read file for an undetermined reason. This is either a bug or the OS is low on resources",true,true);
		return false;
	}
	
	LogError("Reading configuration file");
	std::vector<std::string> conffilelines;
	off_t bytes=0;
	if (conffile.GetSize(&bytes)!=B_NO_ERROR)
	{
		LogError("Fatal Error reading configuration file: Could not determine file size",true,true);
		return false; //conf file exists but we couldnt read it so return error
	}
	char* buf=new char[bytes];
	ssize_t bytesread=0;
	bytesread=conffile.Read(buf,bytes);
	if (bytesread >0)
	{
		std::string line="";
		for (int x=0; x < bytesread; x++)
		{
				//check if EOL first so we dont put \n on str
				if (buf[x]=='\n')
				{
					conffilelines.push_back(line);
					line="";
				}
				else if(x==(bytesread-1))
				{
					//in case last line of file isnt terminated with \n	
					conffilelines.push_back(line);
					line="";
				}
				else
				{
					line=line+buf[x];
				}
		}
	}
	else
	{
		LogError("Fatal Error reading configuration file: File is empty",true,true);	
		return false;
	}
	delete buf;
	//close file
	conffile.Unset();
	
	if (conffilelines.size() ==0)
	{
		LogError("Fatal Error reading configuration file: File is empty or not in the correct format",true,true);
		return false;	
	}
	
	//conf file read into vector so parse it
	LogError("Parsing configuration file");
	std::vector<std::string> sectionlines; //will hold each section of file as it is read
	std::vector<int> sectionlinenos; //will hold original line no of each sections directives so we can pass them through to sub parse methods
	bool inSection= false; //goes true when <SECTION> found and false when </SECTION> found
	bool inGlobalSection=false;// to enable correct Parse method to be called for each section
	bool globalSectionProcessed=false; //prevent > 1 <GLOBAL> section
	int listsProcessed=0; //number of <LIST> sections successfully processed
	bool inMultilineComment=false; //goes true when #/ found and false when /# found
	for (int x=0; x< conffilelines.size(); x++)
	{
		//for each line in file
		stringstream lineno;
		lineno << x+1;
		
	
		std::string line=conffilelines[x];
		if ( (line.substr(0,2)=="#/") )
		{
			inMultilineComment=true;
			continue;
		}
		if ( (line.substr(0,2)=="/#") )
		{
			inMultilineComment=false;
			continue;
		}
		//this must come after check for /# otherwise it will ignore everyline from #/ onwards
		if (inMultilineComment)
		{
			
			continue;
		}
		
		if (  (line[0]=='#') || (line[0]=='\n') || (line=="")  )
		{
			//blank line or comment so step over
			continue;
		}	
		if (line[0]=='<')
		{
			//start/end of section reached
			if (line=="<GLOBAL>")
			{
				if (globalSectionProcessed)
				{
					//already done a <GLOBAL> section so conf file is wrong
					LogError("Fatal Syntax Error in configuration file: <GLOBAL> section found on line "+lineno.str()+" having already processed a <GLOBAL> section",true,true);
					return false; 
					
				}
				if (inSection==true)
				{
					//previous section must have ended before starting a new one
					LogError("Fatal Syntax Error in configuration file: start of section found within existing section on line "+lineno.str(),true,true);
					return false;
				}	
				inSection=true;
				inGlobalSection=true;
				//dont add <GLOBAL> line to sectionlines vector
				continue;
				
			}
			else if (line=="</GLOBAL>")
			{
				if ( (inSection==false) || (inGlobalSection==false) )
				{
					LogError("Fatal Syntax Error in configuration file: end of GLOBAL section found outside a section on line "+lineno.str(),true,true);
					return false;
				}	
				inSection=false;
				inGlobalSection=false;
				if (ParseConfigFileGlobalSection(sectionlines,sectionlinenos))
				{
					globalSectionProcessed=true;
					//empty vector
					sectionlines.clear();
					sectionlinenos.clear();
					continue;	
				}
				else
				{
					//error processing <GLOBAL> section -- the ParseConfigFileGlobalSection method is reponsible for calling LogError
					return false;	
				}
				
			}
			else if (line=="<LIST>")
			{
				if (inSection==true)
				{
					//previous section must have ended before starting a new one
					LogError("Fatal Syntax Error in configuration file: start of section found within existing section on line "+lineno.str(),true,true);
					return false;
				}	
				inSection=true;	
				//dont add <LIST> line to sectionlines vector
				continue;
			}
			else if (line=="</LIST>")
			{
				if ( (inSection==false) || (inGlobalSection==true) )
				{
					LogError("Fatal Syntax Error in configuration file: end of LIST section found outside a section on line "+lineno.str(),true,true);
					return false;
				}	
				inSection=false;
				if (ParseConfigFileListSection(sectionlines,sectionlinenos))
				{
					//empty vector
					sectionlines.clear();
					sectionlinenos.clear();
					listsProcessed++;
					continue;	
				}
				else
				{
					//error processing <LIST> section -- the ParseConfigFileListSection method is reponsible for calling LogError
					return false;	
				}
			}
			else
			{
				//unknown section	
				LogError("Fatal Syntax Error in configuration file: Unknown section type ("+line+")found on line "+lineno.str(),true,true);
				return false;
			}
		}
		else
		{
			//we're in mid section
			//just add line to vector without any sanity check on it and let ParseConfigFileXYZSection method examine it
			sectionlines.push_back(line);
			sectionlinenos.push_back(x+1); //record its line no in the file so if ParseConfigXYZSection method wants to complain it can tell user which line is at fault
			
		}
	}
	//processed all lines
	if (inSection)
	{
		LogError("Syntax Error in configuration file: Unexpected end of file reached without end of section. Bang! You've hit the buffers!",true,true);
		return false;	
	}
	if (globalSectionProcessed==false)
	{
		LogError("Syntax Error in configuration file: No <GLOBAL> section found. A server with no core configuration is like a body with no soul",true,true);
		return false;	
	}
	if (listsProcessed ==0)
	{
		LogError("Syntax Error in configuration file: No <LIST> sections found. Mustard's no good without roast beef",true,true);
		return false;	
	}
	std::stringstream listsProcessedStr;
	listsProcessedStr << listsProcessed;
	LogError("Processed "+listsProcessedStr.str()+ " <LIST> section(s)");
	//all done OK
	return true;
}

bool MailMistressApplication::ParseConfigFileGlobalSection(std::vector<std::string> lines,std::vector<int>originallinenos)
{
	/*given a vector of strings which are directives for <GLOBAL> section without <GLOBAL> & </GLOBAL> lines
	  parse each line and set appropriate config setting if sane 
	  return true if all OK or false if line is invalid
	  Also call LogError() before returning false to log what was wrong
	*/
	
	//flags to indicate prescence of mandatory directives
	//it is the responsibility of the constructor to set default values for optional directives which it will already have done
	bool logfileProcessed = false;
	bool tempdirProcessed = false;
	
	for (int x =0; x < lines.size(); x++)
	{
		std::string line=lines[x];
		stringstream lineno;
		lineno << originallinenos[x];
		
		if ((line.substr(0,8))=="THREADS=")
		{
			std::string param=line.substr(8); //everything after=
			int numThreads=atoi(param.c_str());
			if ( (numThreads > (kTHREADS_MIN-1)) && (numThreads < (kTHREADS_MAX+1)) )
			{
				fNumThreads=numThreads;	
			}
			else
			{
				LogError("Non fatal Syntax Error in configuration file <GLOBAL> section on line "+lineno.str()+": THREADS is not a valid value. Using default",true,false);	
			}
		}
		else if( (line.substr(0,15))=="FAILEDMSGRETRY=")
		{
			std::string param=line.substr(15); //everything after=
			int mins=atoi(param.c_str());
			if ( (mins > (kFAILEDMSGRETRY_MIN-1)) && (mins < (kFAILEDMSGRETRY_MAX+1)) )
			{
				fCheckDirectoryMins=mins;
				
			}
			else
			{
				LogError("Non fatal Syntax Error in configuration file <GLOBAL> on line "+lineno.str()+": FAILEDMSGRETRY is not a valid value. Using default",true,false);	
			}
		}
		else if((line.substr(0,12))=="MAXMSGBYTES=")
		{
			std::string param=line.substr(12); //everything after=
			unsigned long int bytes=atoi(param.c_str());
			if ( (bytes >  (kMAXMSGBYTES_MIN-1)) && (bytes < ( kMAXMSGBYTES_MAX+1)) )
			{
				fMaxBytes=bytes;	
			}
			else
			{
				LogError("Non fatal Syntax Error in configuration file <GLOBAL> section on line "+lineno.str()+":MAXMSGBYTES is not a valid value. Using default",true,false);	
			}
		}
		else if ((line.substr(0,8))=="LOGFILE=")
		{
			std::string param=line.substr(8); //everything after=
			BFile logfile;
			status_t logfilestatus=logfile.SetTo(param.c_str(),B_READ_WRITE|B_CREATE_FILE|B_OPEN_AT_END);
			if (logfilestatus ==B_NO_ERROR)
			{
				logfile.Unset(); //close file
				fLogFilePath=param;
				logfileProcessed=true;
			}
			else
			{
					LogError("Fatal Error processing LOGFILE directive in configuration file <GLOBAL> section on line "+lineno.str()+". Check "+GetAppName()+" has write permission to the specified file ("+param+")",true,true);
					return false;
			}
			
			
		}
		else if ((line.substr(0,14))=="PLAINTEXTONLY=")
		{
			std::string param=line.substr(14); //everything after=
			if (param=="Y")
			{
				fPlainTextOnly='Y';
				
			}
			else if(param=="N")
			{
				fPlainTextOnly='N';
				
			}
			else if (param=="H")
			{
				fPlainTextOnly='H';
				
			}
			else
			{
				LogError("Non fatal Syntax Error in configuration file <GLOBAL> section on line "+lineno.str()+":PLAINTEXTONLY is not a valid value (Y, N or H). Using default (N)",true,false);
			}
		}
		else if ((line.substr(0,8))=="TEMPDIR=")
		{
			std::string param=line.substr(8); //everything after=
			//fTempDirPath already contains file name prefix so we just need to shove path on front
			fTempDirPath=param+"/"+fTempDirPath;
			//create a file to test it out
			std::stringstream epochsecs;
			epochsecs << real_time_clock(); //secs now since unix epoch
			int filenamecounterInt=0; //will be incremented until we get a unique filename string
			bool nonuniquefilename=true;
			BEntry tempFileBEntry;
			std::string tempFilePath;
			do
			{
				filenamecounterInt++;
				std::stringstream filenamecounter;
				filenamecounter << filenamecounterInt;
				tempFilePath=GetTempDirPath()+"-STARTUP-TEST-"+epochsecs.str()+filenamecounter.str();
				//test if tempFilePath already exists
				tempFileBEntry.SetTo(tempFilePath.c_str());
				BFile tempFileBFile;
				status_t tempFileResult=tempFileBFile.SetTo(&tempFileBEntry,B_READ_WRITE|B_FAIL_IF_EXISTS|B_CREATE_FILE); //fails if already exists
				if (tempFileResult==B_FILE_EXISTS)
				{
			  		//a file by that name already exists so try a different number on end
					nonuniquefilename=true;	
				}
				else if (tempFileResult==B_NO_ERROR)
				{
					//created the file OK
					nonuniquefilename=false;
					tempFileBFile.Unset(); //close file
					tempFileBEntry.Remove(); //delete file from disk
					tempdirProcessed=true;
				}
				else
				{
					//error creating the file for a reason other than file already exists
					LogError("Fatal Error processing TEMPDIR directive in configuration file <GLOBAL> section on line "+lineno.str()+".  The directory ("+param+") could not be accessed. Check permissions and check it exists",true,true);
					return false;
				}
		
			}while(nonuniquefilename);
		}
		else
		{
			//unknown directive	
			LogError("Non fatal Syntax Error in configuration file: Unknown directive in <GLOBAL> section at line "+lineno.str()+". Ignoring!",true,false);
		}
			
	} //end of for each line loop
	
	if ( (tempdirProcessed) && (logfileProcessed) )
		{
			LogError("Finished processing <GLOBAL> section OK");
			return true;	
		}
		else
		{
			LogError("Fatal Syntax Error in configuration file: Missing TEMPDIR and / or LOGFILE directive in <GLOBAL> section",true,true);
			return false;	
		}
	
}
bool MailMistressApplication::ParseConfigFileListSection(std::vector<std::string> lines,std::vector<int> originallinenos)
{
	/*given a vector of strings which are directives for <LIST> section without <LIST> & </LIST> lines
	 parse each line and set appropriate config setting if sane 
	  return true if all OK or false if line is invalid
	  Also call LogError() before returning false to log what was wrong
	 
	*/
	
	//flags to indicate if we've found mandatory directives
	bool nameProcessed=false;
	bool icProcessed=false;
    bool ogProcessed=false;
    bool indirProcessed=false;
    bool authProcessed=false;
    bool logfileProcessed=false;
    
    
    //config params used later to init MailingList obj
    std::string name="";
    std::string ic="";
    std::string og="";
    unsigned long int maxmsgbytes=0;
    std::string indir="";
    std::string auth="";
    std::string noauthpostmsg="";
    std::string logfile="";
    std::string archive="";
    std::string subjectprefix="";
    char plaintextonly='N';
    bool forcereplytolist=false;
    std::string listowner="";
    std::string listhelp="";
    std::string listarchive="";
    std::string listsubscribe="";
    std::string listunsubscribe="";
    
	for (int x =0; x < lines.size(); x++)
	{
		stringstream lineno;
		lineno << originallinenos[x];
		
		std::string line=lines[x];
		if ((line.substr(0,5))=="NAME=")
		{
			std::string param=line.substr(5);			
			//mandatory
			name=param;
			nameProcessed=true;
			
		}
		else if((line.substr(0,3))=="IC=")
		{
			std::string param=line.substr(3);
			//ic must be unique
			//mandatory
			MailingList* ml = FindListByICAddress(param);
			if (ml == NULL)
			{
				ic=param;
				icProcessed=true;	
			}
			else
			{
				LogError("Fatal Error processing configuration file <LIST> section on line "+lineno.str()+". Incoming address (IC) "+param+" is already in use by another list",true,true);
				return false;	
			}
		}
		else if((line.substr(0,3))=="OG=")
		{
			std::string param=line.substr(3);
			//og must be unique
			//mandatory
			MailingList* ml = FindListByOGAddress(param);
			if (ml == NULL)
			{
				og=param;	
				ogProcessed=true;
			}
			else
			{
				LogError("Fatal Error processing configuration file <LIST> section on line "+lineno.str()+". Outgoing address (OG) "+param+" is already in use by another list",true,true);
				return false;	
			}
		}
		else if((line.substr(0,12))=="MAXMSGBYTES=")
		{
			std::string param=line.substr(12);
			int bytes=atoi(param.c_str());
			if ( (bytes > ( kMAXMSGBYTES_MIN-1)) && (bytes < ( kMAXMSGBYTES_MAX+1)) )
			{
				maxmsgbytes=bytes;	
			} 
			else
			{
				maxmsgbytes= kMAXMSGBYTES_DEFAULT;
				LogError("Non fatal Syntax Error in configuration file <LIST> section on line "+lineno.str()+":MAXMSGBYTES in <LIST> section is not a valid value. Using default",true,false);	
			}
		}
		else if((line.substr(0,6))=="INDIR=")
		{
			std::string param=line.substr(6);
			BDirectory dir;
			if ( dir.SetTo(param.c_str()) !=B_OK )
			{
				LogError("Fatal Error processing INDIR directive in configuration file <LIST> section on line: "+lineno.str()+" Check folder ("+param+") exists and is readable.",true,true);
				return false;
			}
			else
			{
				MailingList* ml=FindListByDir(param);
				if (ml!=NULL)
				{
						LogError("Fatal Error processing configuration file <LIST> section on line "+lineno.str()+". Incoming folder (INDIR) "+param+" is already in use by another list",true,true);
						return false;
				}
				indir=param;
				indirProcessed=true;
			
			}
		}
		else if ((line.substr(0,14))=="PLAINTEXTONLY=")
		{
			std::string param=line.substr(14); //everything after=
			if (param=="Y")
			{
				plaintextonly='Y';
			}
			else if(param=="N")
			{
				plaintextonly='N';
			}
			else if(param=="H")
			{
				plaintextonly='H';
			}
			else
			{
				LogError("Non fatal Syntax Error in configuration file <LIST> section on line "+lineno.str()+":PLAINTEXTONLY is not a valid value (Y or N). Using default",true,false);
			}
		}
		else if((line.substr(0,5))=="AUTH=")
		{
			std::string param=line.substr(5);
			//try running auth prog with the args TEST TEST (the 3rd arg is also set to TEST but can be ignored by the auth plugin) and see it returns status code 100. If so its working.
			int32 authstatus=1;
			int32 arg_c = 4; //no of args passed in arg_v
			extern char **environ; //env variables
			char **arg_v;
			arg_v = (char **) malloc(sizeof(char*) * (arg_c+1)); //array must hold arg_c elements + a terminating null
	
			arg_v[0]=strdup(param.c_str()); //path to exe
			arg_v[1]=strdup("TEST"); //senders email addr
			arg_v[2]=strdup("TEST"); //path to temp file
			arg_v[3]=strdup("TEST"); //list IC address
			arg_v[4]=NULL; //terminating null
	
			thread_id authprog_team;
			authprog_team=load_image(arg_c,(const char**)arg_v,(const char**)environ);
			free(arg_v);
			wait_for_thread(authprog_team,&authstatus);
			if (authstatus==100)
			{
				//auth prog ran ok
				auth=param;
				authProcessed=true;
			}
			else
			{
				LogError("Fatal Error processing configuration file AUTH directive in <LIST> section on line "+lineno.str()+". Authentication program plug-in ("+param+") could not be run. Check it exists, is executable, and that it is written properly!",true,true);
				return false;
			}
		}
		else if((line.substr(0,15))=="NONAUTHPOSTMSG=")
		{
			std::string param=line.substr(15);
			//attempt to read the custom bounce message file
			BFile bouncemsgbfile;
			status_t bouncemsgfilestatus=bouncemsgbfile.SetTo(param.c_str(),B_READ_ONLY);
			if (bouncemsgfilestatus==B_NO_ERROR)
			{
				off_t bytes; //size of file
				if (bouncemsgbfile.GetSize(&bytes) == B_NO_ERROR)
				{
					if ( (bytes >( kMAXMSGBYTES_MIN-1)) && (bytes < ( kMAXMSGBYTES_MAX+1)) )
					{
						noauthpostmsg=param;	
					}
					else
					{
						LogError("Non Fatal Error processing NONAUTHPOSTMSG directive in configuration file <LIST> section on line: "+lineno.str()+". File is too big or empty. Ensure it's less than 50KB. Using default.",true,false);	
					}
			
				}
				else
				{
					LogError("Non Fatal Error processing NONAUTHPOSTMSG directive in configuration file <LIST> section on line: "+lineno.str()+". Could not determine file size. Using default.",true,false);
				}
				bouncemsgbfile.Unset(); //close file
			}
			else
			{
					LogError("Non Fatal Error processing NONAUTHPOSTMSG directive in configuration file <LIST> section on line: "+lineno.str()+". Could not read file. Using default.",true,false);	
			}
		}
		else if((line.substr(0,8))=="LOGFILE=")
		{
			std::string param=line.substr(8);
			//check we can open file ok
			BFile logbfile;
			status_t logfilestatus=logbfile.SetTo(param.c_str(),B_READ_WRITE|B_CREATE_FILE|B_OPEN_AT_END);
			if (logfilestatus ==B_NO_ERROR)
			{
				logbfile.Unset(); //close file
				logfile=param;
				logfileProcessed=true;
			}
			else
			{
					LogError("Fatal Error processing LOGFILE directive in configuration file <LIST> section on line "+lineno.str()+". Check "+GetAppName()+" has write permission to the specified file ("+param+")",true,true);
					return false;
			}
		}
		else if((line.substr(0,8))=="ARCHIVE=")
		{
			std::string param=line.substr(8);
			BDirectory dir;
			if ( dir.SetTo(param.c_str()) !=B_OK )
			{
				LogError("Non Fatal Error processing ARCHIVE directive in configuration file <LIST> section on line: "+lineno.str()+"Check archive folder exists and is writable. Ignoring! Messages for this list will NOT be archived",true,false);
			}
			else
			{
				archive=param;
			}
		}
		else if((line.substr(0,14))=="SUBJECTPREFIX=")
		{
			std::string param=line.substr(14);
			subjectprefix="["+param+"]";
		}
		else if ((line.substr(0,17))=="FORCEREPLYTOLIST=")
		{
			std::string param=line.substr(17); //everything after=
			if (param=="Y")
			{
				forcereplytolist=true;
			}
			else if(param=="N")
			{
				forcereplytolist=false;
			}
			else
			{
				LogError("Non fatal Syntax Error in configuration file <LIST> section on line "+lineno.str()+":FORCEREPLYTOLIST is not a valid value (Y or N). Using default",true,false);
			}
		}
		else if ((line.substr(0,10))=="LISTOWNER=")
		{
			std::string param=line.substr(10); //everything after=
			bool paramOK=false;
			std::string extracted="";
			int singlequote=0;
			int doublequote=0;
			bool endinganglefound=false;
			if (param[0]=='<')
			{
				//ok so far, need to test that we have an ending > and that between them quote marks pair up and there is no \r or \n
				for (int paramcharidx=1; paramcharidx < param.length(); paramcharidx++)
				{
						if (param[paramcharidx]=='>')
						{
							endinganglefound=true;
							break; //dont go past >	
						}
						else
						{
							extracted=extracted+param[paramcharidx];
						}
						if (param[paramcharidx]==39)
						{
							//singlequote
							if (singlequote==1)
							{
								singlequote--; //ending quote
							}
							else
							{
								singlequote++;
							}
						}
						if (param[paramcharidx]==34)
						{
							//doublequote
							if (doublequote==1)
							{
								doublequote--; //ending quote
							}
							else
							{
								doublequote++;
							}
						}
						
				}
				if ( (singlequote==0) && (doublequote==0) && (endinganglefound) )
				{
					paramOK=true;
				}
			}
			if (paramOK)
			{
				listowner=extracted;
			}
			else
			{
				LogError("Non fatal Syntax Error in configuration file <LIST> section on line "+lineno.str()+":LISTOWNER is not a valid value, please consult RFC2369. Ignoring, List-Owner header will not be set",true,false);
			}
		}
		else if ((line.substr(0,9))=="LISTHELP=")
		{
			std::string param=line.substr(9); //everything after=
			bool paramOK=false;
			std::string extracted="";
			int singlequote=0;
			int doublequote=0;
			bool endinganglefound=false;
			if (param[0]=='<')
			{
				//ok so far, need to test that we have an ending > and that between them quote marks pair up and there is no \r or \n
				for (int paramcharidx=1; paramcharidx < param.length(); paramcharidx++)
				{
						if (param[paramcharidx]=='>')
						{
							endinganglefound=true;
							break; //dont go past >	
						}
						else
						{
							extracted=extracted+param[paramcharidx];
						}
						if (param[paramcharidx]==39)
						{
							//singlequote
							if (singlequote==1)
							{
								singlequote--; //ending quote
							}
							else
							{
								singlequote++;
							}
						}
						if (param[paramcharidx]==34)
						{
							//doublequote
							if (doublequote==1)
							{
								doublequote--; //ending quote
							}
							else
							{
								doublequote++;
							}
						}
						
				}
				if ( (singlequote==0) && (doublequote==0) && (endinganglefound) )
				{
					paramOK=true;
				}
			}
			if (paramOK)
			{
				listhelp=extracted;
			}
			else
			{
				LogError("Non fatal Syntax Error in configuration file <LIST> section on line "+lineno.str()+":LISTHELP is not a valid value, please consult RFC2369. Ignoring, List-Help header will not be set",true,false);
			}
		}
		else if ((line.substr(0,12))=="LISTARCHIVE=")
		{
			std::string param=line.substr(12); //everything after=
			bool paramOK=false;
			std::string extracted="";
			int singlequote=0;
			int doublequote=0;
			bool endinganglefound=false;
			if (param[0]=='<')
			{
				//ok so far, need to test that we have an ending > and that between them quote marks pair up and there is no \r or \n
				for (int paramcharidx=1; paramcharidx < param.length(); paramcharidx++)
				{
						if (param[paramcharidx]=='>')
						{
							endinganglefound=true;
							break; //dont go past >	
						}
						else
						{
							extracted=extracted+param[paramcharidx];
						}
						if (param[paramcharidx]==39)
						{
							//singlequote
							if (singlequote==1)
							{
								singlequote--; //ending quote
							}
							else
							{
								singlequote++;
							}
						}
						if (param[paramcharidx]==34)
						{
							//doublequote
							if (doublequote==1)
							{
								doublequote--; //ending quote
							}
							else
							{
								doublequote++;
							}
						}
						
				}
				if ( (singlequote==0) && (doublequote==0) && (endinganglefound) )
				{
					paramOK=true;
				}
			}
			if (paramOK)
			{
				listarchive=extracted;
			}
			else
			{
				LogError("Non fatal Syntax Error in configuration file <LIST> section on line "+lineno.str()+":LISTARCHIVE is not a valid value, please consult RFC2369. Ignoring, List-Archive header will not be set",true,false);
			}
		}
		else if ((line.substr(0,14))=="LISTSUBSCRIBE=")
		{
			std::string param=line.substr(14); //everything after=
			bool paramOK=false;
			std::string extracted="";
			int singlequote=0;
			int doublequote=0;
			bool endinganglefound=false;
			if (param[0]=='<')
			{
				//ok so far, need to test that we have an ending > and that between them quote marks pair up and there is no \r or \n
				for (int paramcharidx=1; paramcharidx < param.length(); paramcharidx++)
				{
						if (param[paramcharidx]=='>')
						{
							endinganglefound=true;
							break; //dont go past >	
						}
						else
						{
							extracted=extracted+param[paramcharidx];
						}
						if (param[paramcharidx]==39)
						{
							//singlequote
							if (singlequote==1)
							{
								singlequote--; //ending quote
							}
							else
							{
								singlequote++;
							}
						}
						if (param[paramcharidx]==34)
						{
							//doublequote
							if (doublequote==1)
							{
								doublequote--; //ending quote
							}
							else
							{
								doublequote++;
							}
						}
						
				}
				if ( (singlequote==0) && (doublequote==0) && (endinganglefound) )
				{
					paramOK=true;
				}
			}
			if (paramOK)
			{
				listsubscribe=extracted;
			}
			else
			{
				LogError("Non fatal Syntax Error in configuration file <LIST> section on line "+lineno.str()+":LISTSUBSCRIBE is not a valid value, please consult RFC2369. Ignoring, List-Subscribe header will not be set",true,false);
			}
		}
		else if ((line.substr(0,16))=="LISTUNSUBSCRIBE=")
		{
			std::string param=line.substr(16); //everything after=
			bool paramOK=false;
			std::string extracted="";
			int singlequote=0;
			int doublequote=0;
			bool endinganglefound=false;
			if (param[0]=='<')
			{
				//ok so far, need to test that we have an ending > and that between them quote marks pair up and there is no \r or \n
				for (int paramcharidx=1; paramcharidx < param.length(); paramcharidx++)
				{
						if (param[paramcharidx]=='>')
						{
							endinganglefound=true;
							break; //dont go past >	
						}
						else
						{
							extracted=extracted+param[paramcharidx];
						}
						if (param[paramcharidx]==39)
						{
							//singlequote
							if (singlequote==1)
							{
								singlequote--; //ending quote
							}
							else
							{
								singlequote++;
							}
						}
						if (param[paramcharidx]==34)
						{
							//doublequote
							if (doublequote==1)
							{
								doublequote--; //ending quote
							}
							else
							{
								doublequote++;
							}
						}
						
				}
				if ( (singlequote==0) && (doublequote==0) && (endinganglefound) )
				{
					paramOK=true;
				}
			}
			if (paramOK)
			{
				listunsubscribe=extracted;
			}
			else
			{
				LogError("Non fatal Syntax Error in configuration file <LIST> section on line "+lineno.str()+":LISTUNSUBSCRIBE is not a valid value, please consult RFC2369. Ignoring, List-Unsubscribe header will not be set",true,false);
			}
		}
		else
		{
			//unknown directive	
			LogError("Non fatal Syntax Error in configuration file: Unknown directive in <LIST> section on line "+lineno.str()+". Ignoring!",true,false);
		}
			
	} //end of for each line
	if (  (nameProcessed)  && (icProcessed)  && (ogProcessed)  && (indirProcessed) &&  (authProcessed) && (logfileProcessed)  )
	{
			//all ok, so create MailingList obj
			MailingList* list = new MailingList(ic,og,name,subjectprefix,indir,auth,logfile,forcereplytolist, listowner,listhelp,listarchive,listsubscribe,listunsubscribe);
			list->SetMaxMsgBytes(maxmsgbytes);
			list->SetBounceMsg(noauthpostmsg); //safe to set to null str if it wasnt in conf file
			list->SetArchivePath(archive); //safe to set to null str if it wasnt in conf file
			list->SetPlainTextOnly(plaintextonly);
			fListsByDir.insert(std::make_pair(std::string(indir),list));
			fListsByICAddress.insert(std::make_pair(std::string(ic),list));
			fListsByOGAddress.insert(std::make_pair(std::string(og),list));
			
			return true;
	}
	else
	{
			LogError("Fatal Syntax Error in configuration file: Missing NAME and / or IC OG INDIR AUTH LOGFILE directive in <LIST> section",true,true);
			return false;	
	}
    
	
}

void MailMistressApplication::LogError(std::string msg,bool critical,bool waitforack)
{
	/*
		Log ,msg to app error log and append new line
		If log file cant be opened (eg because config file hasnt yet been parsed) then use printf() or GUI
		
		critical defaults to false if not specified. critical=true means pop up a BAlert if kFORCELOGTOSCREENWITHGUI is true and fForceLogToScreen is true
		this stops GUI BAlerts littering the screen for non critical info logs only
	*/	
			if ( (fLogFilePath=="") || (fForceLogToScreen)  )
			{
				//log file not yet set (as during startup conf file pasrsing) 
				//use printf instead or GUI and dont warn about log file
				if ( (critical) && (kFORCELOGTOSCREENWITHGUI) )
				{
					//BAlert needed	
					msg=fAppNameString+" - "+msg; //prepend name of app as BAlert doesnt have title bar so theres no indication what app it relates to!
					BAlert* alert=new BAlert("MailMistress Critical Problem",msg.c_str(),"OK",NULL,NULL,B_WIDTH_AS_USUAL,B_WARNING_ALERT);
					if (waitforack)
					{
						alert->Go();
					}
					else
					{
						alert->Go(NULL);
					}
					
				}
				else
				{
					printf(GetDateTime().c_str());
					printf(" ");
					printf(msg.c_str());
					printf("\n");
				}
				return;
			}	
			//check we can open file ok
			//lock sem to prevent other threads accessing file
			acquire_sem(fLogFileSem);
			BFile logbfile;
			//lock file as this method is called by multiple threads
			status_t logfilestatus=logbfile.SetTo(fLogFilePath.c_str(),B_READ_WRITE|B_CREATE_FILE|B_OPEN_AT_END);
			status_t lockstatus=B_BUSY;
			
			if (logfilestatus ==B_NO_ERROR)
			{
				std::string time=GetDateTime();
				time=time+" ";
				logbfile.Write(time.c_str(),strlen(time.c_str()));
				logbfile.Write(msg.c_str(),strlen(msg.c_str()));
				char nl='\n';
				logbfile.Write(&nl,1);
				logbfile.Unset(); //close file
				
			}
			else
			{
					//log file could not be opened use printf instead and do warn about log file
					printf(GetDateTime().c_str());
					printf(" ");
					printf(msg.c_str());
					printf(" ADDITIONALLY THIS MESSAGE SHOULD HAVE GONE TO THE LOG FILE BUT IT COULD NOT BE OPENED.");
					printf(" Check %s has write permission to the specified file %s",(GetAppName()).c_str(),fLogFilePath.c_str());
					printf("\n");
					
			}
			//unlock file sem
			release_sem(fLogFileSem);
}

unsigned long int MailMistressApplication::GetMaxMsgBytes()
{
	return fMaxBytes;
}
char MailMistressApplication::GetPlainTextOnly()
{
	return fPlainTextOnly;	
}
std::string MailMistressApplication::GetDateTime()
{
	time_t timenow;
	timenow=time(&timenow); //epoch timestamp
	
	if (timenow !=-1)
	{
			struct tm* humantime=NULL;
			humantime=gmtime(&timenow); //convert epoch timestamp to GMT
			if (humantime!=NULL)
			{
				std::stringstream timestr;
				int year=(humantime->tm_year)+1900; //add 1900 cos it returns num years since 1900
				int month=(humantime->tm_mon)+1; //add 1 cos it uses 0 for Jan etc, will need to prepend leading 0 if 1-9
				int day=humantime->tm_mday;// will need to prepend leading 0 if 1-0
				int hour=humantime->tm_hour; // will need to prepend leading 0 if 1-0
				int min=humantime->tm_min; // will need to prepend leading 0 if 1-0
				int sec=humantime->tm_sec; // will need to prepend leading 0 if 1-0
				timestr << year;
				timestr << "-";
				timestr << (_prependLeadingZeroIfNeeded(month));
				timestr << "-";
				timestr << (_prependLeadingZeroIfNeeded(day));
				timestr << " ";
				timestr << (_prependLeadingZeroIfNeeded(hour));
				timestr << ":";
				timestr << (_prependLeadingZeroIfNeeded(min));
				timestr << ":";
				timestr << (_prependLeadingZeroIfNeeded(sec));
				timestr << " GMT";
				return timestr.str();
			}
			else
			{
				return std::string("<TIME-NOT-OBTAINABLE-FROM-OS>");
			}
			
	}
	else
	{
		return std::string("<TIME-NOT-OBTAINABLE-FROM-OS>");	
	}
}

std::string MailMistressApplication::_prependLeadingZeroIfNeeded(int num)
{
	//return num as a std::string with a leading 0 if num was 0-9
	if ( (num < 10) && (num>=0) )
	{
		std::stringstream conv;
		conv << "0";
		conv << num;
		return conv.str();
	}
	else
	{
		std::stringstream conv;
		conv << num;
		return conv.str();	
	}
}
MailWorker* MailMistressApplication::_GetNextWorker()
{
	if (fLastWorkerUsed+1 < fNumThreads)
	{
		fLastWorkerUsed++;
	}
	else
	{
		//reset to 1st one
		fLastWorkerUsed=0;	
	}
	
	return fWorkers[fLastWorkerUsed];
}
