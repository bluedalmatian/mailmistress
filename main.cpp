/*MailMistress main.cpp
 *
 *MailMistress is a mailing list server which works in conjunction with
 *the Haiku mail daemon
 *
 *Copyright 2013 Andrew Wood awood@comms.org.uk
 *All rights reserved
 */

#include "MailMistressApplication.h"


void main()
{
	std::string buildstring = std::string("");//leave set to null string, will be set automatically by compiler to date&time
	std::string platform = std::string("UnknownCPU"); //leave set to unknown, will be set by compiler to indicate 32/64 bit & CPU type

	std::string exe_invocation_path; //path to the folder containing the program exe file

	//get compiler to set buildstring
	std::string date=std::string(__DATE__); //compiler macro, gives 'Jan 1 2011'
	std::string time=std::string(__TIME__);//compiler macro, gives HH:MM:SS
	
	std::string year=std::string("");
	year += date[date.length()-4];
	year += date[date.length()-3];
	year += date[date.length()-2];
	year += date[date.length()-1]; //gives 2011 etc

	std::string month=std::string("");
	month += date[0]; 
	month += date[1];
	month += date[2]; //gives 'Jan' 'Feb' etc
	
	std::string day = std::string("");
		
		if (date[4] == ' ')
		{
			//1 figure day
			day+="0";
			day += date[5];
		}
		else
		{
			//2 figure day
			day += date[4];
			day += date[5];
		}

	//convert month to number
	if (month=="Jan")
	{
		month="01";
	}
	else if (month=="Feb")
	{
		month="02";
	}
	else if (month=="Mar")
	{
		month="03";
	}
	else if (month=="Apr")
	{
		month="04";
	}
	else if (month=="May")
	{
		month="05";
	}
	else if (month=="Jun")
	{
		month="06";
	}
	else if (month=="Jul")
	{
		month="07";
	}
	else if (month=="Aug")
	{
		month="08";
	}
	else if (month=="Sep")
	{
		month="09";
	}
	else if (month=="Oct")
	{
		month="10";
	}
	else if (month=="Nov")
	{
		month="11";
	}
	else
	{
		month="12";
	}
	
	buildstring += year;
	buildstring += month;
	buildstring +=day;

	std::string hour=std::string("");
	hour+=time[0];
	hour+=time[1];
	std::string minutes=std::string("");
	minutes+=time[3];
	minutes+=time[4];
	std::string seconds=std::string("");
	seconds+=time[6];
	seconds+=time[7];

	buildstring += hour;
	buildstring += minutes;
	buildstring +=seconds;


	#if __i386__
		/* x86 */
		platform=std::string("x86/32");

		#if __x86_64__
			/* 64-bit x86 */
			platform=std::string("x86/64");
		#endif
	#endif


	MailMistressApplication* app = new MailMistressApplication(buildstring,platform);
	app->Run();
}
