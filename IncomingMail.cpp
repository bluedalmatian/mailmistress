/*IncomingMail.cpp
 *
 *IncomingMail objects represent an incoming email for a list
 *initialised from a BFile the object is owned by a MailingList obj
 *
 *
 *Copyright 2013-2014 Andrew Wood awood@comms.org.uk
 *All rights reserved
 */
 
#include "IncomingMail.h"
#include "MailMistressApplication.h"
#include "AppWideBMessageDefs.h"
#include <storage/Directory.h>
#include <storage/File.h>
#include <storage/Path.h>
#include <storage/NodeMonitor.h>
#include <storage/Entry.h>
#include <app/Application.h>
#include <image.h>
#include <OS.h>
#include <stdlib.h>
#include <sstream>
#include <iostream>
#include <vector>
#include <string>
#include <algorithm>
#include <cctype>
#include <mail/MailMessage.h>

extern BApplication* be_app;

IncomingMail::IncomingMail(BFile* icmsg, MailingList* list)
{
	//store a pointer to our owning list & app
	fList=list;
	fApp=(MailMistressApplication*) be_app;
	
	fICFileSize=0; //safety init. This will be set to size of IC file later so MailingList obj can ask for it
	
	
	fInitOK=false; //flag to indicate if we read file & initialised fHeaderlines & fBodylines OK
	
	off_t ICFileSize = 0;
	std::vector<std::string>* targetvector; //points to one of the  2 vectors fBodylines or fHeaderlines
	targetvector=&fHeaderlines; //we process header first
	bool onHeader=true; //so blank lines wont trigger a vector switch once we've finished the header
	std::string headerbodydivider;
	
		if (icmsg->GetSize(&ICFileSize) == B_NO_ERROR)
		{
			fICFileSize=ICFileSize; //store ICFileSize so MailingList obj can ask for it later on
			
			char* buff = new char[ICFileSize];
			//as we've already created some BMailMessage objects from this file we need to reset
			//the file pointer to the start of the file before we read it
			icmsg->Seek(0,SEEK_SET);
			off_t bytesread=icmsg->Read(buff,ICFileSize);
			if (bytesread > 3)  //technically this would be 0 but 3 allows us to look back for \n or \r witout overrunning front of array and no meaningful email will be < 3 bytes!
			{
				//file read ok
				std::string line="";
	
				for (int x=0; x < bytesread; x++)
				{
					
					line=line+buff[x];
					//printf("%c",buff[x]);
					
						if ( (buff[x]=='\n') && (onHeader) && (buff[x-1]=='\n') )
						{
						  //blank line indicating end of headers, next line will be body
						  targetvector=&fBodylines; //swap target vector
						  onHeader=false; //so we ignore anymore blank lines (in the body)
						   headerbodydivider=line;
						  line="";	//reset line buffer
						  //printf("EOH nn\n");
					
						}
						else if ((buff[x]=='\n') && (onHeader) && (buff[x-1]=='\r') && (buff[x-2]=='\n') )
						{
							//blank line indicating end of headers, next line will be body
						  targetvector=&fBodylines; //swap target vector
						  onHeader=false; //so we ignore anymore blank lines (in the body)
						  headerbodydivider=line;
						  line="";	//reset line buffer
						  // printf("EOH nrn \n");
						}
						else if (buff[x]=='\n')
						{
							targetvector->push_back(line); //add line to vector
							line="";	//reset line buffer
						}
					
				}	
				delete buff;
			}
			else
			{
				//read 0 bytes, either error or file empty
				fList->LogError("WARNING: Read 0 bytes from incoming email file, either error or file empty");
				delete buff;
				fInitOK=false;	
			}
		}
		else
		{
			fList->LogError("ERROR: Could not determine size of incoming email file");
			fInitOK=false;	
		}
			
		//we now have header in headerlines & body in bodylines vectors
		fInitOK=true;
	
}

bool IncomingMail::InitCheck()
{
	return fInitOK;	
}

off_t IncomingMail::GetICFileSize()
{
	return fICFileSize;
}

std::string IncomingMail::GetFromField()
{
	//return From: field header (whole line)  or null string if not found
	//if header is folded over multiple lines run them together as one string with spaces where line breaks were
	

		int start =0;
		int end=0;
		std::string fromheader = std::string("");
		
		
		if (FindHeaderStartAndFinish("FROM:",&start, &end))  //case insensitive search
		{
				//extract all existing From header lines and ram together as one with a space between
				
				
				
				if (start==end)
				{
					//header is on one line
					return fHeaderlines[start];	
				}
				//header spans multiple lines
				
				int diff=(end-start)+1;	//get num lines
				
				for (int x=0; x<diff; x++)
				{
					
					fromheader=fromheader+fHeaderlines[start+x]+" ";
				}
				
				return fromheader;
		}
		else
		{
				
			//from line not found
			return std::string("");
		}

		
}

std::string IncomingMail::ExtractEmailAddressFromFromField(std::string sendersAddrField)
{
	//extract email address from From: field and return in format "user@domain"
	//we assume we are passed a From: field on a single line as obtained from GetFromField()
	if (sendersAddrField=="")
	{
		return std::string("");	
	}
	int indexAT=sendersAddrField.rfind("@"); //get index of @ searching from end of str as the name portion may contain an @ as well
	int index=0;
	int numchars=0;
	for (int x=indexAT+1; x < sendersAddrField.length(); x++)
	{
		index=x;
		char ele = sendersAddrField[x];
		if ( (ele == ' ') || (ele == '>') ) 
		{
			//end of domain portion of addr
			index=x-1;
			break;
		}
	
	}
	
	std::string sendersAddrDomain=sendersAddrField.substr(indexAT+1,(index-indexAT));
	
	index=0;
	for (int x=indexAT-1; x >= 0; x--)
	{
		index=x;
		char ele = sendersAddrField[x];
		if ( (ele == ' ') || (ele == '<') ) 
		{
			//start of username portion of addr
			index=x+1;
			break;
		}
	
	}
	
	numchars=0;
	if (index==0)
	{
		numchars=indexAT;
	}
	else
	{
		numchars=indexAT-index;
	}

	std::string sendersAddrUsername=sendersAddrField.substr(index,numchars);
	
	std::string sendersAddr=sendersAddrUsername+"@"+sendersAddrDomain;
	
	return sendersAddr;
}




std::string IncomingMail::GetSendersAddr()
{
	std::string fromheader=GetFromField();
	return ExtractEmailAddressFromFromField(fromheader);
}

bool IncomingMail::FindHeaderStartAndFinish(std::string headerhandle, int* startidx, int* endidx)
{
	/*		
		Given a headerhandle which is the start of a headerline upto and including the : do
		a case insensitive search for it in the fHeaderlines vector and if found return true filling out startidx
		& endidx with the vector index of the 1st and last line of that header. If not found return false
	*/

	int x=0;
	std::vector<std::string>::iterator pos;
	for (pos=fHeaderlines.begin(); pos < fHeaderlines.end(); pos++)
	{
		std::string line=(*pos); //extract line from headerlines vector
		std::transform(line.begin(), line.end(),line.begin(),tolower); //make it lowercase
		std::transform(headerhandle.begin(), headerhandle.end(),headerhandle.begin(),tolower); //make search token lowercase
		std::string::size_type ctpos=line.find(headerhandle);
		
		
		if (ctpos==0)
		{
				//found
				(*startidx)=x; //record line where this headerstarts
				bool headercontinues=true; //until we find the end of this header and start of next one
				int y=x; //current line index
				while (headercontinues)
				{
					y++; //look at next line
					
					if (y==fHeaderlines.size())
					{
						//prevent overrun if we hit end of vector
						(*endidx)=(y-1); //record line where this header ends
						return true;
						
					}
					
					std::string continuingline=fHeaderlines[y];
					if ( (continuingline[0]==9) || (continuingline[0]==32) )
					{
						//white space char (tab==9 space==32) at start of line indicating header continues, so step to next line
						
					}
					else
					{
						//start of next header found
						headercontinues=false;
						(*endidx)=(y-1); //record line where this header ends
						return true;
					}
				}
		}
		x++;
	}
	return false; //not found
}

void IncomingMail::RemoveHeader(int startidx, int endidx)
{
	/*
		Given a start and end index. Remove all lines in fHeaderlines from
		startidx to endidx inclusive and replace fHeaderlines with the new vector with the lines removed
	*/
	
	std::vector<std::string> newheaderlines;
	for (int x=0; x < startidx; x++)
	{
		
		newheaderlines.push_back(fHeaderlines[x]);
	}
	//we now have a new vector with everything upto and excluding the 1st line we want to get rid of
	//so now copy over everything after and excluding the last line we want to get rid of
	for (int x=(endidx+1); x < ((fHeaderlines.size())); x++)
	{
		
		newheaderlines.push_back(fHeaderlines[x]);
	}
	
	fHeaderlines=newheaderlines;
}

void IncomingMail::AddSubjectPrefix(std::string prefix)
{
			
		for (int x=0; x < fHeaderlines.size(); x++)
		{
			
			//Processing header line x
			std::string keyword;
			
			
			
			//find Subject:field and append prefix if needed
			
				//if subject prefix wasnt defined in conf file prefix will be null str
				keyword=std::string((fHeaderlines[x]).substr(0,7)); //start at 0 and extract 7 chars
				//due to unknown capitalisation, convert to lowercase to do comparison
				std::transform(keyword.begin(), keyword.end(),keyword.begin(),tolower);
				if (keyword=="subject")
				{
						std::string existingsubject=std::string((fHeaderlines[x]).substr(8));  //extract everything after Subject:
						//existingsubject may or may not start with a space
						//existingsubject may contain Re: at start (in any case) and there may or may not be a space after the Re:
						
						std::string::size_type prefixidx;
						prefixidx=existingsubject.find(prefix);
						if ( (prefixidx==std::string::npos) || (prefixidx>6)  )
						{
								//prefix needs pre-pending	as it wasnt found within 1st 5 indexes
								//ensure that any existing Re: (with optional surrounding spaces) in any case is preserved
								std::string tempexistingsubject=std::string(existingsubject);
								std::transform(tempexistingsubject.begin(), tempexistingsubject.end(),tempexistingsubject.begin(),tolower);
								prefixidx=tempexistingsubject.find("re:");
								if (prefixidx==std::string::npos)
								{
									//re: not present	
								
									std::string newsubject="Subject: "+prefix+" "+existingsubject;	//do not append \n as previous subject line already has it
									fHeaderlines[x]=newsubject.c_str();
								}
								else
								{
									//re: needs preserving
									
									existingsubject=existingsubject.substr(prefixidx+3); //remove existing re:
									std::string newsubject="Subject: Re:"+prefix+" "+existingsubject;	//do not append \n as previous subject line already has it
									fHeaderlines[x]=newsubject.c_str();	
								}
								
								
						}
						
						
				}
					
			}
		
}

void IncomingMail::CleanHeaders()
{
		//remove any CC or Bcc fields to prevent infinite loops where a message is CCd to the mailing list
		bool allUnwantedHeadersRemoved=false; //loop control until all multiples of an unwanted header have been removed
		
		int start =0;
		int end=0;
		while (!allUnwantedHeadersRemoved)
		{
			
			//loop in case multiple CC fields present
			if (FindHeaderStartAndFinish("CC:",&start, &end))  //case insensitive search
			{
				
				RemoveHeader(start,end);
			}
			else
			{
				
				allUnwantedHeadersRemoved=true;	
			}
		}
		
		allUnwantedHeadersRemoved=false;
		while (!allUnwantedHeadersRemoved)
		{
			//loop in case multiple BCC fields present
			if (FindHeaderStartAndFinish("BCC:",&start, &end))  //case insensitive search
			{
			
				RemoveHeader(start,end);
			}
			else
			{
					allUnwantedHeadersRemoved=true;	
			}
		}
		
		//remove precedence headers
		allUnwantedHeadersRemoved=false;
		while (!allUnwantedHeadersRemoved)
		{
			//loop in case multiple fields present
			if (FindHeaderStartAndFinish("PRECEDENCE:",&start, &end))  //case insensitive search
			{
			
				RemoveHeader(start,end);
			}
			else
			{
					allUnwantedHeadersRemoved=true;	
			}
		}
		
		//remove reply-to headers
		allUnwantedHeadersRemoved=false;
		while (!allUnwantedHeadersRemoved)
		{
			//loop in case multiple fields present
			if (FindHeaderStartAndFinish("REPLY-TO:",&start, &end))  //case insensitive search
			{
			
				RemoveHeader(start,end);
			}
			else
			{
					allUnwantedHeadersRemoved=true;	
			}
		}
		
		//remove to headers (setting the to on the OG msg should overwrite the existing ones anyway..but just to be safe remove them anyway
		allUnwantedHeadersRemoved=false;
		while (!allUnwantedHeadersRemoved)
		{
			//loop in case multiple fields present
			if (FindHeaderStartAndFinish("TO:",&start, &end))  //case insensitive search
			{
			
				RemoveHeader(start,end);
			}
			else
			{
					allUnwantedHeadersRemoved=true;	
			}
		}
		
		//strip From: field down to its bare essentials otherwise MailKit will choke on it
		
			
				//extract all existing From header lines and ram together as one with a space between
				std::string origfrom=GetFromField();
				
				//remove the old From field from fHeaderlines
				if (FindHeaderStartAndFinish("FROM:",&start, &end))  //case insensitive search
				{
					RemoveHeader(start,end);
				}
				
				//extract the email address itself and put it back as a new From header
				
				std::string newfrom=std::string("From: ");
	
				std::string::size_type idxstartanglebracket;
				idxstartanglebracket=origfrom.find("<");
				std::string::size_type idxendanglebracket;
				idxendanglebracket=origfrom.find(">");
		
				std::string anglebrackets=origfrom.substr(idxstartanglebracket, (idxendanglebracket-idxstartanglebracket)+1);
				newfrom=newfrom+anglebrackets+"\n";
				fHeaderlines.push_back(newfrom);
				
					
		
		//unfold Content-Type header into 1 line as MailKit corrupts folded Content-Type headers
		
		if (FindHeaderStartAndFinish("CONTENT-TYPE:",&start, &end))  //case insensitive search
		{
				//extract all existing Content-Type header lines and ram together as one with a space between
				std::string ctheader = std::string("");
				std::string ctheader2=  std::string("");
			
				if (start==end)
				{
					//header is on one line
					
				}
				else
				{
					//header spans multiple lines
					
					int diff=(end-start)+1;	//get num lines
				
					for (int x=0; x<diff; x++)
					{
					
						ctheader=ctheader+fHeaderlines[start+x];
						if (x!=(diff-1))
						{
							ctheader=ctheader+" ";	
						}
					}
					RemoveHeader(start,end);
					//remove folds
					for (int x=0; x < ctheader.length(); x++)
					{
							if ( (ctheader[x]=='\n') || (ctheader[x]=='\r') || (ctheader[x]==9) )  //newline char or tab
							{
								
							}
							else
							{
								ctheader2=ctheader2+ctheader[x];
							}
					}
					ctheader2=ctheader2+"\r\n";
					fHeaderlines.push_back(ctheader2);
				}
		}
		
		//remove list-post headers
		allUnwantedHeadersRemoved=false;
		while (!allUnwantedHeadersRemoved)
		{
			//loop in case multiple fields present
			if (FindHeaderStartAndFinish("LIST-POST:",&start, &end))  //case insensitive search
			{
			
				RemoveHeader(start,end);
			}
			else
			{
					allUnwantedHeadersRemoved=true;	
			}
		}
		//remove list-help headers
		allUnwantedHeadersRemoved=false;
		while (!allUnwantedHeadersRemoved)
		{
			//loop in case multiple fields present
			if (FindHeaderStartAndFinish("LIST-HELP:",&start, &end))  //case insensitive search
			{
			
				RemoveHeader(start,end);
			}
			else
			{
					allUnwantedHeadersRemoved=true;	
			}
		}
		//remove list-owner headers
		allUnwantedHeadersRemoved=false;
		while (!allUnwantedHeadersRemoved)
		{
			//loop in case multiple fields present
			if (FindHeaderStartAndFinish("LIST-OWNER:",&start, &end))  //case insensitive search
			{
			
				RemoveHeader(start,end);
			}
			else
			{
					allUnwantedHeadersRemoved=true;	
			}
		}
		
		//remove list-subscribe headers
		allUnwantedHeadersRemoved=false;
		while (!allUnwantedHeadersRemoved)
		{
			//loop in case multiple fields present
			if (FindHeaderStartAndFinish("LIST-SUBSCRIBE:",&start, &end))  //case insensitive search
			{
			
				RemoveHeader(start,end);
			}
			else
			{
					allUnwantedHeadersRemoved=true;	
			}
		}
		//remove list-unsubscribe headers
		allUnwantedHeadersRemoved=false;
		while (!allUnwantedHeadersRemoved)
		{
			//loop in case multiple fields present
			if (FindHeaderStartAndFinish("LIST-UNSUBSCRIBE:",&start, &end))  //case insensitive search
			{
			
				RemoveHeader(start,end);
			}
			else
			{
					allUnwantedHeadersRemoved=true;	
			}
		}
		//remove list-archive headers
		allUnwantedHeadersRemoved=false;
		while (!allUnwantedHeadersRemoved)
		{
			//loop in case multiple fields present
			if (FindHeaderStartAndFinish("LIST-ARCHIVE:",&start, &end))  //case insensitive search
			{
			
				RemoveHeader(start,end);
			}
			else
			{
					allUnwantedHeadersRemoved=true;	
			}
		}
		//add headers we need
			
		fHeaderlines.push_back(std::string("precedence: list\r\n"));	
		
		std::string lp="List-Post: <mailto:";
		lp=lp+fList->GetICAddress()+">\r\n";
		fHeaderlines.push_back(lp);	  //see RFC 2369
		

		
		
		std::string rxfield="Received: by "   +fApp->GetHostname()+  " (" +  fApp->GetAppName()+  " "  +fApp->GetAppVersionString()+  ") for "  +fList->GetName()+" "+fApp->GetDateTime()+"\r\n";
		fHeaderlines.push_back(rxfield);
}

void IncomingMail::AdjustReplyTo(bool forcereplytolist)
{
	
		//remove any existing reply-to headers
		int start;
		int end;
		bool allUnwantedHeadersRemoved=false;
		while (!allUnwantedHeadersRemoved)
		{
			//loop in case multiple fields present
			if (FindHeaderStartAndFinish("REPLY-TO:",&start, &end))  //case insensitive search
			{
			
				RemoveHeader(start,end);
			}
			else
			{
					allUnwantedHeadersRemoved=true;	
			}
		}
	
	    if (forcereplytolist)
	    {
	    	//use list IC address for Reply-To
			std::string txt="Reply-To: ";
			txt=txt+fList->GetICAddress()+"\r\n";
			fHeaderlines.push_back(txt);
	    }
	    else
	    {
	    	//use senders address for Reply-To
	    	
	    	//extract all existing From header lines and ram together as one with a space between
			std::string origfrom=GetFromField();
				
	    	//extract the email address itself 
				
			std::string::size_type idxstartanglebracket;
			idxstartanglebracket=origfrom.find("<");
			std::string::size_type idxendanglebracket;
			idxendanglebracket=origfrom.find(">");
		
			std::string inanglebrackets=origfrom.substr(idxstartanglebracket+1, (idxendanglebracket-(idxstartanglebracket+1)));
		
	    	std::string txt="Reply-To: ";
			txt=txt+inanglebrackets+"\r\n";
			fHeaderlines.push_back(txt);
	    }
}

void IncomingMail::SetListOwnerHeader(std::string listowner)
{
		//remove any existing list-owner headers
		int start;
		int end;
		bool allUnwantedHeadersRemoved=false;
		while (!allUnwantedHeadersRemoved)
		{
			//loop in case multiple fields present
			if (FindHeaderStartAndFinish("LIST-OWNER:",&start, &end))  //case insensitive search
			{
			
				RemoveHeader(start,end);
			}
			else
			{
					allUnwantedHeadersRemoved=true;	
			}
		}
		std::string txt="List-Owner: <";
		txt=txt+listowner+">\r\n";
		fHeaderlines.push_back(txt);
}
void IncomingMail::SetListHelpHeader(std::string listhelp)
{
		//remove any existing list-help headers
		int start;
		int end;
		bool allUnwantedHeadersRemoved=false;
		while (!allUnwantedHeadersRemoved)
		{
			//loop in case multiple fields present
			if (FindHeaderStartAndFinish("LIST-HELP:",&start, &end))  //case insensitive search
			{
			
				RemoveHeader(start,end);
			}
			else
			{
					allUnwantedHeadersRemoved=true;	
			}
		}
		std::string txt="List-Help: <";
		txt=txt+listhelp+">\r\n";
		fHeaderlines.push_back(txt);
}

void IncomingMail::SetListArchiveHeader(std::string listarchive)
{
		//remove any existing list-archive headers
		int start;
		int end;
		bool allUnwantedHeadersRemoved=false;
		while (!allUnwantedHeadersRemoved)
		{
			//loop in case multiple fields present
			if (FindHeaderStartAndFinish("LIST-ARCHIVE:",&start, &end))  //case insensitive search
			{
			
				RemoveHeader(start,end);
			}
			else
			{
					allUnwantedHeadersRemoved=true;	
			}
		}
		std::string txt="List-Archive: <";
		txt=txt+listarchive+">\r\n";
		fHeaderlines.push_back(txt);
}
void IncomingMail::SetListSubscribeHeader(std::string listsubscribe)
{
		//remove any existing list-subscribe headers
		int start;
		int end;
		bool allUnwantedHeadersRemoved=false;
		while (!allUnwantedHeadersRemoved)
		{
			//loop in case multiple fields present
			if (FindHeaderStartAndFinish("LIST-SUBSCRIBE:",&start, &end))  //case insensitive search
			{
			
				RemoveHeader(start,end);
			}
			else
			{
					allUnwantedHeadersRemoved=true;	
			}
		}
		std::string txt="List-Subscribe: <";
		txt=txt+listsubscribe+">\r\n";
		fHeaderlines.push_back(txt);
}
void IncomingMail::SetListUnSubscribeHeader(std::string listunsubscribe)
{
		//remove any existing list-unsubscribe headers
		int start;
		int end;
		bool allUnwantedHeadersRemoved=false;
		while (!allUnwantedHeadersRemoved)
		{
			//loop in case multiple fields present
			if (FindHeaderStartAndFinish("LIST-UNSUBSCRIBE:",&start, &end))  //case insensitive search
			{
			
				RemoveHeader(start,end);
			}
			else
			{
					allUnwantedHeadersRemoved=true;	
			}
		}
		std::string txt="List-Unsubscribe: <";
		txt=txt+listunsubscribe+">\r\n";
		fHeaderlines.push_back(txt);
}

void IncomingMail::WriteToFile(BFile* tempFileBFile)
{
	

	//write headerlines into tempFileBFile
	for (int x=0; x < fHeaderlines.size(); x++)
	{
		tempFileBFile->Write(fHeaderlines[x].c_str(),strlen(fHeaderlines[x].c_str()));
		//printf("%d:::",x);
		//printf(headerlines[x].c_str());
	}
	
	//write blank line into tempFileBFile
	char nl=13;
	tempFileBFile->Write(&nl,1);
	nl=10;
	tempFileBFile->Write(&nl,1);
	
	
	//printf("There are %d bodylines lines\n",bodylines.size());
	//write bodylines into tempFileBFile
	for (int x=0; x < fBodylines.size(); x++)
	{
		tempFileBFile->Write(fBodylines[x].c_str(),strlen(fBodylines[x].c_str()));
		//printf("%d::",x);
		//printf(bodylines[x].c_str());
		
	}
}

bool IncomingMail::CheckIfPlainTextCriteriaPassed(char plainTextOnly)
{

	std::string contenttypefield="";  //will be converted to lowercase for consistent searching for content-type 
	std::string contenttypefield_unmodified=""; //will NOT be converted so as to preserve boundary delimeter if present
	std::string delimeter=""; //will hold multipart boundary delimeter IF its present
	
	for (int x=0; x < fHeaderlines.size(); x++)
	{

			std::string keyword=std::string((fHeaderlines[x]).substr(0,12)); //start at 0 and extract 12 chars
			//due to unknown capitalisation, convert to lowercase to do comparison
			std::transform(keyword.begin(), keyword.end(),keyword.begin(),tolower);
			if (keyword=="content-type") //look for content-type at start of line
			{
				//save it so it can be checked later
				contenttypefield=std::string(fHeaderlines[x]);
				contenttypefield_unmodified=std::string(fHeaderlines[x]);
				//due to unknown capitalisation, convert to lowercase to do comparison
				std::transform(contenttypefield.begin(), contenttypefield.end(),contenttypefield.begin(),tolower);
				//the delimeter between body parts needs to be located if present either on this line or next
				std::string::size_type boundarypos=contenttypefield.find("boundary=");
				
					
				if  (boundarypos==std::string::npos)  
				{
					// not found, try next line
					boundarypos=fHeaderlines[x+1].find("boundary=");
					if  (boundarypos==std::string::npos)  
					{
						//still not found so leave delimeter set to null str
					}
					else
					{
						std::string boundaryattr=std::string((fHeaderlines[x+1]).substr(boundarypos));   //start at boundary= and extract upto end
						delimeter=ExtractMIMEBoundaryDelimeter(boundaryattr);	
					}
						
				}
				else
				{
					std::string boundaryattr=std::string(contenttypefield_unmodified.substr(boundarypos)); //start at boundary= and extract upto end
					delimeter=ExtractMIMEBoundaryDelimeter(boundaryattr);	
					
				}
			}
	}


	if (plainTextOnly=='N')
	{
		//permission set to N so allowing
		return true;
	}
	else if (plainTextOnly=='Y')
	{
		//messages must only contain plain text
		if (contenttypefield=="")
		{
				//content-type header could not be found
				//bounce to sender
			    //permission set to Y and no Content-Type found so disallowing
				return false;
		}
		else
		{	
			std::string::size_type pos=contenttypefield.find("text/plain;"); //valid index pos for start of 'text/plain' substring is 13,14 or 15 depending on spaces either side of :
			if ( (pos<16) && (pos>12) )
			{
				//found in right place so it is text/plain type
				//permission set to Y and Content-Type is text/plain so allowing
				return true;
			}
			else
			{
				//not text/plain so reject
				//permission set to Y and Content-Type is not text/plain so disallowing
				return false;
			}
		}
	
	}
	else
	{
		//plainTextOnly must be 'H'
		if (contenttypefield=="")
		{
			//content-type header could not be found
			//bounce to sender
			//permission set to H and no Content-Type found so disallowing
			return false;
		}
			std::string::size_type pos=contenttypefield.find("text/"); //valid index pos for start of 'text/' substring is 13,14 or 15 depending on spaces either side of :
			if ( (pos<16) && (pos>12) )
			{
				//found in right place so it is text/ type
				//permission set to H and Content-Type  is text/ so allowing
				return true;
			}
			else
			{
				//not text/ but may be multipart/alternative type (used for HTML text) so carry on checking	reject multipart/mixed as this contains attached files
				//permission set to H and Content-Type not text/ checking for multipart...
				std::string::size_type pos=contenttypefield.find("multipart/alternative"); //valid index pos for start of 'multipart/alternative' substring is 13,14 or 15 depending on spaces either side of :
				if ( (pos<16) && (pos>12) )
				{
					//found in right place so it is multipart, see what the parts consist of
					
					if (delimeter!="")
					{
						return (CheckMultiPartBodyAgainstPlainTextCriteria(delimeter));
					}
					else
					{
						//no boundary delimeter found so disallowing
						return false; //no delimeter so cant process any further	
					}	
					
				}
				else
				{
					//not multipart/alternative so reject
					//its not multipart so disallowing
					return false;
				}
			}
		
	}
	
}
bool IncomingMail::CheckMultiPartBodyAgainstPlainTextCriteria(std::string delimeter)
{
	//check each line of body to identify sections in multipart message and check each ones content-type is text/
	
	
	bool delimeterfound=false;
	bool anydelimeterfound=false; //flag in case no delimeters are present at all
	for (int x=0; x < fBodylines.size(); x++)
	{
		
		std::string::size_type delpos=fBodylines[x].find(delimeter);
		
		//make duplicate of line and convert tolower case to search for content-type
		std::string lowercaseline=std::string(fBodylines[x]);
		std::transform(lowercaseline.begin(), lowercaseline.end(),lowercaseline.begin(),tolower);
		
		if (delimeterfound)
		{
			//inside a part looking for Content-Type
			std::string::size_type ctpos=lowercaseline.find("content-type:");
			anydelimeterfound=true;
			std::string::size_type seconddelpos=fBodylines[x].find(delimeter);
			if (seconddelpos==0)
			{
				//we have reached the next part without finding a content-type header
				//got to end of a part with no Content-Type so disallowing
				return false;
			}
			if (ctpos==0)
			{
				//content-type subheader found, check its attribute value
				std::string::size_type ctattrpos=lowercaseline.find("content-type: text/");
				if (ctattrpos==std::string::npos)
				{
					//found a part with a Content-Type which is not text/ so disallowing
					return false; //something other than text/ found which is no allowed	
				}
				else
				{
					//text/ found so keep going
					delimeterfound=false; //reset flag as there should be another delimeter before next content-type
					continue;
						
				}
			}
		}
		
			//must use npos (ie check it was found somewhere)
			// rather than position of 0 as its legal for delimeter to be sandwiched amoung other text on same line.
			//a lot of mail clients add extra hypens to start or end of delimeter in msg body
		
		if (delpos!=std::string::npos)  
		{
			if ((fBodylines.size()-x)<2)
			{
				//this is the delimeter at end of message so there wont be another content-type to find
				//got to end of message delimeter so allowing
				return true;
			}
			delimeterfound=true;
			continue; //step to next line
		}
	}
	//got to end of body
	if (anydelimeterfound)
	{
		return true;
	}
	else
	{
		//got to end of message with no delimeter found so disallowing
		return false;	
	}
}
std::string IncomingMail::ExtractMIMEBoundaryDelimeter(std::string boundaryattr)
{
	//boundaryattr is "boundary=......" upto EOL, attribute may or may not be quoted
	//if quoted end is indicated by end quote
	//else end is indicated by a space
	//extract the delimeter itself and return it eg 
	//given boundary="abcde" ;other stuff on same line OR 
	//boundary=abcde ;other stuff on same line
	//return abcde
	//we assume no spaces either side of the = ie boundary="xyz" or boundary=xyz

	
	
	if (boundaryattr[9]==34) //ascii value for "
	{
		//delimeter is quoted
		//find index of last " and extract from char 10 to indexoflast" -1
		bool found=false;
		int idx=10; //1st char after boundary="
		while (found==false)
		{
			if(idx>((boundaryattr.length())-1))
			{
				found=true;
				idx=-1;
			}
				if (boundaryattr[idx]==34) //ascii value for "
				{
					found=true;	
				}
				else
				{
					idx++;	
				}
		}
		//if idx > 10 then we found last ending " at that position
		if (idx>10)
		{
			std::string delimeter=boundaryattr.substr(10,(idx-10));
			
			return delimeter;
		}
		else
		{
			//return empty delimeter
			std::string delimeter("");	
			return delimeter;
		}
			
		
			
	}
	else
	{
		//delimeter is not quoted	
		//find index of end space and extract from char 9 to indexofendspace-1
		//unless we get to end of line in which case just extract to end of line
		bool found=false;
		int idx=9; //1st char after boundary=
		while (found==false)
		{
			if(idx>((boundaryattr.length())-1))
			{
				//got to end of line
				found=true;
				idx=(boundaryattr.length())-2;
			}
				if (boundaryattr[idx]==' ')
				{
					found=true;	
				}
				else
				{
					idx++;	
				}
		}
		//if idx > 9 then we found end of delimeter space at that position
		if (idx>9)
		{
			std::string delimeter=boundaryattr.substr(9,(idx-9));
			return delimeter;
		}
		else
		{
			//return empty delimeter
			std::string delimeter("");	
			return delimeter;
		}
		
	}
}

void IncomingMail::PrintfHeaders()
{
	//for debugging only
	//use printf to output each header line and its index
	for (int x=0; x < fHeaderlines.size(); x++)
	{
		printf("HEADERLINE %d:%s",x,fHeaderlines[x].c_str());
	}
		
}
