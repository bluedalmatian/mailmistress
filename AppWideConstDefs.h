#ifndef MAILMISTRESS_CONSTDEFS
 #define MAILMISTRESS_CONSTDEFS


//App wide constants a.k.a 'magic numbers'
//DO NOT PUT BMESSAGE COMMAND CONSTS IN HERE

//Secs to wait for auth prog plug in to execute before assuming its jammed in an infinite loop or blocked forever, this prevents MailWorker from also blocking forever
#define kAUTHPROG_TIMEOUT_MINS 3



//Number of MailWorkers, (sane limits & default for directives read from config file)
#define kTHREADS_MIN 1
#define kTHREADS_MAX 128
#define kTHREADS_DEFAULT 8

//Secs to wait before trying to reprocess files which failed 1st time, (sane limits & default for directives read from config file)
#define kFAILEDMSGRETRY_MIN 1 //1min
#define kFAILEDMSGRETRY_MAX 1440 //24hrs
#define kFAILEDMSGRETRY_DEFAULT 60 //1hr

//Absolute max number of bytes an email can be , (sane limits & default for directives read from config file)
#define kMAXMSGBYTES_MIN 1 //1 byte
#define kMAXMSGBYTES_MAX 100000000 //100MB
#define kMAXMSGBYTES_DEFAULT 100000000 //100MB

//Absolute max number of bytes a bounce msg file can be, (sane limits)
#define kMAXBOUNCEMSGBYTES_MIN 1 //1 byte
#define kMAXBOUNCEMSGBYTES_MAX 50000 //50KB

#endif