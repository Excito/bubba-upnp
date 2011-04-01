#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <iostream>

#include <syslog.h>
#include <stdlib.h>
#include <unistd.h>
#include <string>
#include <string.h>


#include "igd.h"


using namespace std;

int main(int argc, char** argv){

	if( argc < 4 || (strcmp(argv[1], "add") == 0 && argc < 6) ) {
		cout << "Usage: " << argv[0] << " add protocol local_ip local_port remote_port" << endl;
		cout << "       " << argv[0] << " del protocol remote_port" << endl;
		return 1;
	}

	openlog( "bubba-easyfind", LOG_PERROR,LOG_DAEMON );

	syslog( LOG_NOTICE,"Application starting" );
	setlogmask(LOG_UPTO(LOG_DEBUG));

	string cmd = argv[1];

	char *protocol = argv[2], *lip;
	int lport, rport;

	IGD& i=IGD::Instance();
	sleep(3);
	i.dumpMap();

	if( cmd == "add" ) {
		lip = argv[3];
		lport = atoi(argv[4]);
		rport = atoi(argv[5]);
		cout << "Adding mapping" << endl;
		i.addPortMapping(lip, lport, rport, protocol);

	} else if( cmd == "del" ) {
		rport = atoi(argv[3]);
		cout << "Removing mapping"<<endl;
		i.removePortMapping(rport,protocol);

	} else {
		cout << "Unknown command " << cmd << endl;
		return 2;
	}
	sleep(100);

	return 0;
}
