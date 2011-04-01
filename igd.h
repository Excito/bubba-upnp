/*

    bubba-easyfind - http://www.excito.com/

    Igd.h - this file is part of bubba-easyfind.

    Copyright (C) 2009 Tor Krill <tor@excito.com>

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

    $Id$
*/

#ifndef IGD_H_
#define IGD_H_

#include <libeutils/Thread.h>
#include <map>
#include <string>

using namespace std;

typedef struct{
	string wanip;
	string lanip;
	void* proxy;
} igdevice;

typedef map<string, igdevice> IgMap;

class IGD: public EUtils::Thread{
private:
        /**
         * Hidden copy(constructor)
         */
        IGD(const IGD&);
        IGD& operator=(const IGD&);

        IgMap igmap;

protected:
	IGD();
	virtual void Run();
public:
	static IGD& Instance();

	void addDevice(string udn, string host);
	void addService(string udn, string host, void* proxy);

	void addPortMapping(string localhost, int publicport, int localport, string type);
	void removePortMapping(int publicport, string type);

	void removeDevice(string udn);
	void removeService(string udn);

    void dumpMap();

    virtual ~IGD();

};

#endif
