/*This is a skeleton example of an authentication
*program for MailMistress which uses simple string
*comparison to check if someone can post to the list
*and has the valid posters &recipients hardcoded in it.
*In practice you would extend this to consult external files
*or databases but this will give you an idea of the fundamental
*MailMistress API
*/

#include <stdlib.h>
#include <stdio.h>
#include <string>

int main(int argc, char* argv[])
{
	if (argc!=4)
	{
		printf("Wrong number of arguments\n");
		return 1;	
	}
	std::string sender=std::string(argv[1]);
	std::string filepath=std::string(argv[2]);
	std::string listICAddress=std::string(argv[3]);
	
	if ( (sender=="TEST") && (filepath=="TEST")  )
	{
		return 100;
	}
	
	printf("IC addr: %s\n",listICAddress.c_str());
	
	if ( (sender=="person1@example.com") || (sender=="person2@example.com") || (sender=="person1@example2.com") || (sender=="person1@example3.com"))
	//{
		FILE* file=fopen(filepath.c_str(),"w");
		if (file==NULL)
		{
			printf("Could not open temp file %s\n",filepath.c_str());
			return 1;	
		}
		fprintf(file,"person1@example.com\n");
		fprintf(file,"person2@example.com\n");
		fprintf(file,"person3@example.com\n");
		fclose(file);
		return 0;
	}
	else if (sender=="person4@example.com")
	{
		//bounce
		return 2;
	}
	else
	{
		//silently discard
		return 3;
	}
	
}
