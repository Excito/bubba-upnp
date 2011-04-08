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
#include <libutil.h>

#include "igd.h"

namespace po = boost::program_options;

using namespace std;

IGD igd;

void shutdown(int) {
    igd.stop();
}

int main(int argc, char** argv){

    po::variables_map vm;        
    struct pidfh *pfh = 0;
    pid_t otherpid;

    try {
        string config_file;

        po::options_description generic("Generic options");
        generic.add_options()
            ("version,v", "print version string")
            ("help,h", "show this help message")
            ("forground,f", "Do not daemonize")
            ("pidfile", po::value<string>()->default_value("/var/run/bubba-igd.pid"), "Pidfile to use")
            ("config,c", po::value<string>(&config_file)->default_value("/etc/bubba-igd.conf"), "read this config file")
            ;

        po::options_description config("Configuration");
        config.add_options()
            ("interface", po::value<string>()->default_value("eth0"), "interface to query MAC-address from")
            ("enable-port-forward", "enable port forwarding")
            ("enable-easyfind", "enable easyfind update")
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
            exit(EXIT_FAILURE);
        }
        if (vm.count("version")) {
            cout << "bubba-igd, version " << VERSION << endl;
            exit(EXIT_SUCCESS);
        }

        ifstream ifs(config_file.c_str());
        if (ifs)
        {
            store(parse_config_file(ifs, config_file_options), vm);
            notify(vm);
        }

        if(vm.count("enable-port-forward")) {
            if (!vm.count("port"))
            {
                cerr << "No ports specified" << endl;
                exit(EXIT_FAILURE);
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

        pfh = pidfile_open(vm["pidfile"].as<string>().c_str(), 0600, &otherpid);
        if (pfh == NULL) {
            if (errno == EEXIST) {
                cerr << "Daemon already running, pid: " << (intmax_t)otherpid << endl;
                exit(EXIT_FAILURE);
            }
            /* If we cannot create pidfile from other reasons, only warn. */
            cerr << "Cannot open or create pidfile" << endl;
        }

        if(daemon(0,0) == -1) {
            cerr << "error: " << strerror(errno) << endl;
            pidfile_remove(pfh);
            exit(EXIT_FAILURE);
        }
        pidfile_write(pfh);

    }

    openlog( "bubba-igd", LOG_PERROR,LOG_DAEMON );

    syslog( LOG_NOTICE,"Application starting" );
    setlogmask(LOG_UPTO(LOG_DEBUG));


    igd.start(vm);

    signal(SIGTERM, shutdown);
    signal(SIGINT, shutdown);
    signal(SIGQUIT, shutdown);

    igd.join();

    pidfile_remove(pfh);

    closelog();

    exit(EXIT_SUCCESS);
}
