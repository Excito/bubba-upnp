#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <iostream>
#include <fstream>
#include <string>

#include <boost/program_options.hpp>
#include <boost/bind.hpp>

#include <csignal>
#include <cstdlib>
#include <cstring>
#include <cerrno>
#include <syslog.h>
#include <unistd.h>

#include "igd.h"

namespace po = boost::program_options;

using namespace std;

IGD igd;

void shutdown(int) {
    static int counter = 0;
    if(++counter > 2) {
        // die!
        exit(1);
    }
    igd.stop();
    exit(0);
}

int main(int argc, char** argv){

    po::variables_map vm;        

    try {
        string config_file;

        po::options_description generic("Generic options");
        generic.add_options()
            ("version,v", "print version string")
            ("help,h", "show this help message")
            ("forground,f", "Do not daemonize")
            ("config,c", po::value<string>(&config_file)->default_value("/etc/bubba-upnp.conf"), "read this config file")
            ;

        po::options_description config("Configuration");
        config.add_options()
            ("interface", po::value<string>()->default_value("eth0"), "interface to query MAC-address from")
            ("enable-port-forward", "enable port forwarding")
            ("ip", po::value<string>(), "local IP address for port forwarding")
            ("port", po::value< vector<int> >()->composing(), "keep port forwarded")
            ;

        po::options_description cmdline_options;
        cmdline_options.add(generic).add(config);

        po::options_description config_file_options;
        config_file_options.add(config);

        po::options_description visible("");
        visible.add(generic).add(config);

        po::store(po::parse_command_line(argc, argv, cmdline_options), vm);
        po::notify(vm);    

        if (vm.count("help")) {
            cout << visible << endl;
            return 1;
        }
        if (vm.count("version")) {
            cout << "bubba-upnp, version 0.1" << endl;
            return 0;
        }

        ifstream ifs(config_file.c_str());
        if (ifs)
        {
            store(parse_config_file(ifs, config_file_options), vm);
            notify(vm);
        }

        if(vm.count("enable-port-forward")) {
            if (!vm.count("ip"))
            {
                cout << "No IP was given" << endl;
                return 1;
            }

            if (!vm.count("port"))
            {
                cout << "No ports specified" << endl;
                return 1;
            }
        }

    }
    catch(exception& e) {
        cerr << "error: " << e.what() << endl;
        return 1;
    }
    catch(...) {
        cerr << "Exception of unknown type!" << endl;
    }
    if(!vm.count("forground"))  {
        if(daemon(0,0) != 0) {
            cerr << "error: " << strerror(errno) << endl;
        }
    }

    openlog( "bubba-upnp", LOG_PERROR,LOG_DAEMON );

    syslog( LOG_NOTICE,"Application starting" );
    setlogmask(LOG_UPTO(LOG_DEBUG));


    igd.start(vm);

    signal(SIGTERM, shutdown);
    signal(SIGINT, shutdown);
    signal(SIGQUIT, shutdown);

    igd.join();

    return 0;
}
