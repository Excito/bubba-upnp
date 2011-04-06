/*

   bubba-upnp - http://www.excito.com/

   Igd.h - this file is part of bubba-upnp.

   Copyright (C) 2009 Tor Krill <tor@excito.com>
   Copyright (C) 2011 Carl Fürstenberg <tor@excito.com>

   bubba-easyfind is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License version 2
   as published by the Free Software Foundation.

   bubba-easyfind is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   version 2 along with bubba-easyfind if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301, USA.
   */
#ifndef IGD_H_
#define IGD_H_

#include <map>
#include <string>
#include <vector>
#include <libgupnp/gupnp-control-point.h>
#include <boost/thread.hpp>


typedef struct{
	std::string wanip;
	std::string lanip;
	void* proxy;
} igdevice;

typedef std::map<std::string, igdevice> IgMap;


class IGD{
private:
	boost::thread m_Thread;
	std::string localhost;
	std::vector<int> ports;
	IgMap igmap;
	GMainLoop *main_loop;

	IGD(const IGD&);
	IGD& operator=(const IGD&);
	void removeAllPorts();
	void registerPortMappings(std::string udn);
	void unregisterPortMappings(std::string udn);
	void updateEasyfind();
	void run();

public:
	IGD(){}
	void start(std::string localhost,
			std::vector<int> ports);

	void join();

	void stop();

	void addDevice(
			std::string udn,
			std::string host
			);
	void addService(
			std::string udn,
			std::string host,
			GUPnPServiceProxy* proxy
			);

	void addPortMapping(
			std::string localhost,
			int publicport,
			int localport,
			std::string type
			);
	void removePortMapping(
			int publicport,
			std::string type
			);

	void removeDevice(std::string udn);
	void removeService(std::string udn);

	void dumpMap();

	virtual ~IGD(){}

};

#endif
