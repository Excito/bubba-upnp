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

#include <libgupnp/gupnp-control-point.h>
#include <libsoup/soup.h>

#include <cstdio>
#include <syslog.h>

#include "igd.h"

#include <iostream>
#include <sstream>
#include <string>
using namespace std;

static string GetExternalIPAddress(GUPnPServiceProxy *proxy){
	GError *error = NULL;
	char *ip = NULL;
	string ret;

	gupnp_service_proxy_send_action (proxy,
		/* Action name and error location */
		"GetExternalIPAddress", &error,
		/* IN args */
		NULL,
		/* OUT args */
		"NewExternalIPAddress",
		G_TYPE_STRING, &ip,
		NULL);

	if (error == NULL) {
		ret=ip;
		g_free (ip);
	} else {
		syslog(LOG_NOTICE, "Error: %s",error->message);
		g_error_free (error);
	}

	return ret;
}


static void device_proxy_available_cb (GUPnPControlPoint *cp, GUPnPDeviceProxy *proxy, gpointer userdata){

	GUPnPDeviceInfo *info=GUPNP_DEVICE_INFO(proxy);
	string udn = gupnp_device_info_get_udn(info);
	string host = gupnp_device_info_get_url_base(info)->host;
	syslog(LOG_DEBUG, "Device available: %s at %s", udn.c_str(), host.c_str());

	IGD* i=static_cast<IGD*>((void*)userdata);

	i->addDevice(udn,host);

}

static void device_proxy_unavailable_cb (GUPnPControlPoint *cp, GUPnPDeviceProxy *proxy, gpointer userdata){


	IGD* i=static_cast<IGD*>((void*)userdata);

	GUPnPDeviceInfo *info=GUPNP_DEVICE_INFO(proxy);
	string udn = gupnp_device_info_get_udn(info);
	syslog(LOG_DEBUG, "Device unavailable: %s", udn.c_str());

	i->removeDevice(udn);

}

static void
external_ip_address_changed (GUPnPServiceProxy *proxy, const char *variable, GValue *value, gpointer userdata){

	IGD* i=static_cast<IGD*>((void*)userdata);

	GUPnPServiceInfo *info=GUPNP_SERVICE_INFO(proxy);
	string udn = gupnp_service_info_get_udn(info);
	syslog(LOG_DEBUG, "External IP changed: %s at %s", udn.c_str(), g_value_get_string(value));

	i->addService(udn,g_value_get_string (value),proxy);

}

static void service_proxy_available_cb (GUPnPControlPoint *cp, GUPnPServiceProxy *proxy, gpointer userdata){

	IGD* i = static_cast<IGD*>((void*)userdata);

	GUPnPServiceInfo *info=GUPNP_SERVICE_INFO(proxy);
	string udn = gupnp_service_info_get_udn(info);

	string wanip = GetExternalIPAddress(proxy);

	syslog(LOG_DEBUG, "Service available: %s at %s", udn.c_str(), wanip.c_str());

	i->addService(udn,wanip, proxy);

	gupnp_service_proxy_set_subscribed (proxy, TRUE);
	if (!gupnp_service_proxy_add_notify (proxy, "ExternalIPAddress",G_TYPE_STRING, external_ip_address_changed,	userdata)) {
		syslog(LOG_ERR, "Failed to add ExternalIPAddress notification");
	}

}

static void service_proxy_unavailable_cb (GUPnPControlPoint *cp, GUPnPServiceProxy *proxy, gpointer userdata){
	IGD* i=static_cast<IGD*>((void*)userdata);

	GUPnPServiceInfo *info=GUPNP_SERVICE_INFO(proxy);
	string udn = gupnp_service_info_get_udn(info);
	syslog(LOG_DEBUG, "Service unavailable: %s", udn.c_str());

	i->removeService(udn);
}

void IGD::dumpMap(){

	for(IgMap::iterator iIt=igmap.begin();iIt!=igmap.end();iIt++){
		cout << "UDN: ["<<(*iIt).first
				<< "] LAN: ["<<(*iIt).second.lanip
				<< "] WAN: ["<<(*iIt).second.wanip
				<<"]"<<endl;
	}

}

static bool openPort(GUPnPServiceProxy *proxy,const string& lhost, int pport,int lport, string type){
	GError *error = NULL;
	char rh[] = "";
	char desc[] = "Added by Bubba";
	bool ret;

	gupnp_service_proxy_send_action (proxy,
		/* Action name and error location */
		"AddPortMapping", &error,
		/* IN args */
		"NewRemoteHost", G_TYPE_STRING, &rh,
		"NewExternalPort", G_TYPE_UINT, pport,
		"NewProtocol", G_TYPE_STRING, type.c_str(),
		"NewInternalPort", G_TYPE_UINT, lport,
		"NewInternalClient",G_TYPE_STRING,lhost.c_str(),
		"NewEnabled", G_TYPE_BOOLEAN, true,
		"NewPortMappingDescription",G_TYPE_STRING,&desc,
		"NewLeaseDuration",G_TYPE_UINT,0,
		NULL,
		/* OUT args */
		NULL);

	if (error == NULL) {
		ret=true;
	} else {
		syslog(LOG_NOTICE, "Error: %s",error->message);
		g_error_free (error);
		ret=false;
	}

	return ret;
}


void IGD::addPortMapping(string localhost, int publicport, int localport, string type){
	for(IgMap::iterator iIt=this->igmap.begin();iIt!=this->igmap.end();iIt++){
		if ((*iIt).second.proxy) {
			GUPnPServiceProxy *proxy = static_cast<GUPnPServiceProxy *>((*iIt).second.proxy);
			openPort(proxy,localhost,publicport,localport, type);
		}
	}
}

static bool closePort(GUPnPServiceProxy *proxy, int pport, string type){
	GError *error = NULL;
	char rh[] = "";
	bool ret;

	gupnp_service_proxy_send_action (proxy,
		/* Action name and error location */
		"DeletePortMapping", &error,
		/* IN args */
		"NewRemoteHost", G_TYPE_STRING, &rh,
		"NewExternalPort", G_TYPE_UINT, pport,
		"NewProtocol", G_TYPE_STRING, type.c_str(),
		NULL,
		/* OUT args */
		NULL);

	if (error == NULL) {
		ret=true;
	} else {
		syslog(LOG_NOTICE, "Error: %s",error->message);
		g_error_free (error);
		ret=false;
	}

	return ret;
}

void IGD::removePortMapping(int publicport, string type){
	for(IgMap::iterator iIt=this->igmap.begin();iIt!=this->igmap.end();iIt++){
		if ((*iIt).second.proxy) {
			GUPnPServiceProxy *proxy = static_cast<GUPnPServiceProxy *>((*iIt).second.proxy);
			closePort(proxy, publicport, type);
		}
	}
}

void IGD::addDevice(string udn, string host){
	this->igmap[udn].lanip=host;
}

void IGD::addService(string udn, string host, void* proxy){
	this->igmap[udn].wanip=host;
	this->igmap[udn].proxy=proxy;
}

void IGD::removeDevice(string udn){
	this->igmap[udn].lanip="";
}

void IGD::removeService(string udn){
	this->igmap[udn].wanip="";
	this->igmap[udn].proxy=NULL;
}



IGD::IGD(){
	// Setup GLIB
	g_thread_init (NULL);
	g_type_init ();

	this->Start();

}


void IGD::Run(void){
	// Do nothing for now
	syslog(LOG_NOTICE,"IGD starting up");

	GUPnPContext *context;
	GUPnPControlPoint *cp,*dp;

	context = gupnp_context_new(NULL, NULL, 0, NULL);
	cp = gupnp_control_point_new(context, "urn:schemas-upnp-org:service:WANIPConnection:1");
	g_signal_connect (cp, "service-proxy-available", G_CALLBACK (service_proxy_available_cb), this);
	g_signal_connect (cp, "service-proxy-unavailable", G_CALLBACK (service_proxy_unavailable_cb), this);

	dp = gupnp_control_point_new(context,"urn:schemas-upnp-org:device:WANConnectionDevice:1");
	g_signal_connect(dp, "device-proxy-available", G_CALLBACK (device_proxy_available_cb), this);
	g_signal_connect(dp, "device-proxy-unavailable", G_CALLBACK (device_proxy_unavailable_cb), this);

	/* Tell the Control Point to start searching */
	gssdp_resource_browser_set_active (GSSDP_RESOURCE_BROWSER (cp), TRUE);
	gssdp_resource_browser_set_active (GSSDP_RESOURCE_BROWSER (dp), TRUE);

	/* Enter the main loop. This will start the search and result in callbacks to
	service_proxy_available_cb. */
	GMainLoop *main_loop = g_main_loop_new (NULL, FALSE);
	g_main_loop_run (main_loop);

	/* Clean up */
	g_main_loop_unref (main_loop);
	g_object_unref (cp);
	g_object_unref (dp);
	g_object_unref (context);
}

IGD& IGD::Instance(void){
	static IGD igd;

	return igd;
}

IGD::~IGD(){
}
