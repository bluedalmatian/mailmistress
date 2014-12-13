#ifndef MAILMISTRESS_BMESSAGEDEFS
 #define MAILMISTRESS_BMESSAGEDEFS


//App wide BMessage constants

//CHECKINDIR is used to ask a MailWorker to manually cycle through a MailingLists in directory to check for files
//This message must contain a string dir path called 'ICDirPathStr'
#define ORG_SIMPLE_MAILMISTRESS_CHECKINDIR 'chkd'

//PROCESSFILE is used to ask a MailWorker to process a specific file path (incoming email)
//This message must contain a string file path called 'FilePathStr'
#define ORG_SIMPLE_MAILMISTRESS_PROCESSFILE 'file'

#endif