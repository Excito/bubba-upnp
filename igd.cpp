#include <libgupnp/gupnp-control-point.h>
#include <libsoup/soup.h>


#include "igd.h"

#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

#include <cstring>
#include <syslog.h>
#include <net/if.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <ifaddrs.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include <boost/regex.hpp>

#define EASYFIND_URL "https://easyfind.excito.org"

using namespace std;

static string _get_local_ip_address(const char *if_name) {
    string retval;
    struct ifaddrs *ifaddrs_struct = NULL;

    getifaddrs(&ifaddrs_struct);

    while( ifaddrs_struct != NULL ) {
        if( strcmp(ifaddrs_struct->ifa_name, if_name) == 0 ) {
            struct sockaddr* sock_addr = ifaddrs_struct->ifa_addr;
            if( sock_addr->sa_family==AF_INET ) {
                char buf[INET_ADDRSTRLEN];
                inet_ntop(
                        AF_INET,
                        &reinterpret_cast<struct sockaddr_in*>(sock_addr)->sin_addr,
                        buf,
                        INET_ADDRSTRLEN
                        );
                return string(buf);
            } else if( sock_addr->sa_family==AF_INET6 ) {
                char buf[INET6_ADDRSTRLEN];
                inet_ntop(
                        AF_INET6,
                        &reinterpret_cast<struct sockaddr_in*>(sock_addr)->sin_addr,
                        buf,
                        INET6_ADDRSTRLEN
                        );
                return string(buf);
            }
        }
        ifaddrs_struct=ifaddrs_struct->ifa_next;
    }

    return NULL;
}

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
		syslog(LOG_DEBUG, "Error: %s",error->message);
		g_error_free (error);
	}

	return ret;
}


static void device_proxy_available_cb (
		GUPnPControlPoint */*cp*/,
		GUPnPDeviceProxy *proxy,
		gpointer userdata
		){

	GUPnPDeviceInfo *info=GUPNP_DEVICE_INFO(proxy);
	string udn = gupnp_device_info_get_udn(info);
	string host = gupnp_device_info_get_url_base(info)->host;
	syslog(LOG_DEBUG, "Device available: %s at %s", udn.c_str(), host.c_str());

	IGD* i=static_cast<IGD*>(userdata);

	i->addDevice(udn,host);

}

static void device_proxy_unavailable_cb (
		GUPnPControlPoint */*cp*/,
		GUPnPDeviceProxy *proxy,
		gpointer userdata
		){


	IGD* i=static_cast<IGD*>(userdata);

	GUPnPDeviceInfo *info=GUPNP_DEVICE_INFO(proxy);
	string udn = gupnp_device_info_get_udn(info);
	syslog(LOG_DEBUG, "Device unavailable: %s", udn.c_str());

	i->removeDevice(udn);

}

static void
external_ip_address_changed (
		GUPnPServiceProxy *proxy,
		const char */*variable*/,
		GValue *value,
		gpointer userdata
		){

	IGD* i=static_cast<IGD*>(userdata);

	GUPnPServiceInfo *info=GUPNP_SERVICE_INFO(proxy);
	string udn = gupnp_service_info_get_udn(info);
	syslog(LOG_INFO, "External IP changed: %s at %s", udn.c_str(), g_value_get_string(value));

	i->addService(udn,g_value_get_string (value),proxy);

}

static void service_proxy_available_cb (
		GUPnPControlPoint */*cp*/,
		GUPnPServiceProxy *proxy,
		gpointer userdata
		){

	IGD* i = static_cast<IGD*>(userdata);

	GUPnPServiceInfo *info=GUPNP_SERVICE_INFO(proxy);
	string udn = gupnp_service_info_get_udn(info);

	string wanip = GetExternalIPAddress(proxy);

	syslog(LOG_DEBUG, "Service available: %s at %s", udn.c_str(), wanip.c_str());

	i->addService(udn,wanip, proxy);

	gupnp_service_proxy_set_subscribed(proxy, TRUE);
	if (
			!gupnp_service_proxy_add_notify(
				proxy,
				"ExternalIPAddress",
				G_TYPE_STRING,
				external_ip_address_changed,
				userdata
				)
	   ) {
		syslog(LOG_WARNING, "Failed to add ExternalIPAddress notification");
	}

}

static void service_proxy_unavailable_cb (
		GUPnPControlPoint */*cp*/,
		GUPnPServiceProxy *proxy,
		gpointer userdata
		){
	IGD* i=static_cast<IGD*>(userdata);

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

static bool openPort(
		GUPnPServiceProxy *proxy,
		const string& lhost,
		int pport,
		int lport,
		string type
		){
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
		syslog(LOG_DEBUG, "Error: %s",error->message);
		g_error_free (error);
		ret=false;
	}

	return ret;
}


void IGD::addPortMapping(
		string localhost,
		int publicport,
		int localport,
		string type
		){
	for(IgMap::iterator iIt=this->igmap.begin();iIt!=this->igmap.end();iIt++){
		if ((*iIt).second.proxy) {
			GUPnPServiceProxy *proxy = static_cast<GUPnPServiceProxy *>((*iIt).second.proxy);
			openPort(proxy,localhost,publicport,localport, type);
		}
	}
}

static bool closePort(
		GUPnPServiceProxy *proxy,
		int pport,
		string type
		){
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
		syslog(LOG_DEBUG, "Error: %s",error->message);
		g_error_free (error);
		ret=false;
	}

	return ret;
}

void IGD::removePortMapping(
		int publicport,
		string type
		){
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

void IGD::addService(
		string udn,
		string host,
		GUPnPServiceProxy* proxy
		){
	this->igmap[udn].wanip=host;
	this->igmap[udn].proxy=proxy;
	if( this->do_portforward ) {
		this->registerPortMappings(udn);
	}
    if( this->do_easyfind ) {
        this->updateEasyfind();
    }
}

void IGD::removeDevice(string udn){
	this->igmap[udn].lanip="";
}

void IGD::removeService(string udn){
	if( this->do_portforward ) {
		this->unregisterPortMappings(udn);
	}
	this->igmap[udn].wanip="";
	this->igmap[udn].proxy=NULL;
}


void IGD::run(void){
	g_thread_init (NULL);
	g_type_init ();

	GUPnPContext *context;
	GUPnPControlPoint *service_control_point,*device_control_point;

	context = gupnp_context_new(NULL, NULL, 0, NULL);
	service_control_point = gupnp_control_point_new(
			context,
			"urn:schemas-upnp-org:service:WANIPConnection:1"
			);
	g_signal_connect(
			service_control_point,
			"service-proxy-available",
			G_CALLBACK (service_proxy_available_cb),
			this
			);
	g_signal_connect(
			service_control_point,
			"service-proxy-unavailable",
			G_CALLBACK (service_proxy_unavailable_cb),
			this
			);

	device_control_point = gupnp_control_point_new(
			context,
			"urn:schemas-upnp-org:device:WANConnectionDevice:1"
			);
	g_signal_connect(
			device_control_point,
			"device-proxy-available",
			G_CALLBACK (device_proxy_available_cb),
			this
			);
	g_signal_connect(
			device_control_point,
			"device-proxy-unavailable",
			G_CALLBACK (device_proxy_unavailable_cb),
			this
			);

	/* Tell the Control Point to start searching */
	gssdp_resource_browser_set_active (GSSDP_RESOURCE_BROWSER (service_control_point), TRUE);
	gssdp_resource_browser_set_active (GSSDP_RESOURCE_BROWSER (device_control_point), TRUE);

	/* Enter the main loop. This will start the search and result in callbacks to
	   service_proxy_available_cb. */
	main_loop = g_main_loop_new (NULL, FALSE);
	g_main_loop_run (main_loop);

	/* Clean up */
	g_main_loop_unref (main_loop);

    if(service_control_point) {
        g_object_unref (service_control_point);
    }
    if(device_control_point) {
        g_object_unref (device_control_point);
    }
    if(context) {
        g_object_unref (context);
    }
}
void IGD::start(boost::program_options::variables_map vm) {
	this->interface = vm["interface"].as<string>();
	this->do_portforward = vm.count("enable-port-forward") > 0;
	this->do_easyfind = vm.count("enable-easyfind") > 0;
	if( this->do_portforward ) {
		this->localhost = _get_local_ip_address(this->interface.c_str());
		this->ports = vm["port"].as< vector<int> >();
	}
	syslog(LOG_NOTICE, "Starting IGD UPNP service");
	m_Thread = boost::thread( &IGD::run, this );
}

void IGD::join() {
	m_Thread.join();
}

void IGD::stop() {
	if( g_main_loop_is_running (main_loop) ) {
		syslog(LOG_DEBUG, "Stopping IGD UPNP service");
		this->removeAllPorts();
		g_main_loop_quit(main_loop);
	}
}

void IGD::removeAllPorts() {
	for(
			IgMap::iterator iIt=this->igmap.begin();
			iIt!=this->igmap.end();
			iIt++
	   ){
		this->unregisterPortMappings((*iIt).first);
	}
}

void IGD::registerPortMappings(string udn) {
	GUPnPServiceProxy *proxy = static_cast<GUPnPServiceProxy *>(this->igmap[udn].proxy);

	for(
			vector<int>::const_iterator iter = this->ports.begin();
			iter != this->ports.end();
			++iter
	   ) {
		openPort(proxy,this->localhost,*iter,*iter, "TCP");
		openPort(proxy,this->localhost,*iter,*iter, "UDP");
		syslog(
				LOG_DEBUG,
				"Opened port mapping %2$s:%1$d <-> %3$s:%1$d",
				*iter,
				this->localhost.c_str(),
				this->igmap[udn].wanip.c_str()
			  );
	}
}

void IGD::unregisterPortMappings(string udn) {
	GUPnPServiceProxy *proxy = static_cast<GUPnPServiceProxy *>(this->igmap[udn].proxy);

	for(
			vector<int>::const_iterator iter = this->ports.begin();
			iter != this->ports.end();
			++iter
	   ) {
		closePort(proxy,*iter, "TCP");
		closePort(proxy,*iter, "UDP");
		syslog(
				LOG_NOTICE,
				"Closed port mapping %2$s:%1$d <-> %3$s:%1$d",
				*iter,
				this->localhost.c_str(),
				this->igmap[udn].wanip.c_str()
			  );
	}
}


static string _get_mac(string interface) {

	unsigned char cMacAddr[8]; // Server's MAC address
	int nSD; // Socket descriptor
	struct ifreq sIfReq; // Interface request
	int ret;

	nSD = socket( PF_INET, SOCK_STREAM, 0 );
	if ( nSD < 0 )
	{
		syslog(
				LOG_ERR,
				"File %s: line %d: Socket failed",
				__FILE__,
				__LINE__
			  );
		return(0);
	}
	strncpy(sIfReq.ifr_name, interface.c_str(), IF_NAMESIZE);
	if ( ioctl(nSD, SIOCGIFHWADDR, &sIfReq) != 0 )
	{
		// We failed to get the MAC address for the interface
		syslog( 
				LOG_ERR,
				"File %s: line %d: Ioctl failed", 
				__FILE__, 
				__LINE__ 
			  );
		return(0);
	}

	memmove( (void *)&cMacAddr[0], (void *)&sIfReq.ifr_ifru.ifru_hwaddr.sa_data[0], 6 );

	char *c_hwaddr;

	ret = asprintf(&c_hwaddr, "%02X:%02X:%02X:%02X:%02X:%02X",
			cMacAddr[0], 
			cMacAddr[1], 
			cMacAddr[2],
			cMacAddr[3], 
			cMacAddr[4], 
			cMacAddr[5] 
			);

	if( ret < 0 ) {
		syslog(
				LOG_ERR,
				"File: %s line %d: Failed to allocate string",
				__FILE__, 
				__LINE__ 
				);
	}
	string hwaddr(c_hwaddr);

	free(c_hwaddr);
	return hwaddr;
}
static string slurp(ifstream& in) {
    stringstream sstr;
    sstr << in.rdbuf();
    return sstr.str();
}

static string _get_key() {
	boost::regex re("key=(\\S+)");
	boost::smatch matches;
	ifstream ifs("/proc/cmdline");
	string str(slurp(ifs));
	ifs.close();
	if(boost::regex_search(str, matches, re) != 0) {
		string res(matches[1].first, matches[1].second);
		return res;
	} else {
		return "zuwnerrb";
	}
}

void IGD::updateEasyfind() {

	SoupMultipart *multipart;
	SoupMessage *msg;
	SoupSession *session;
	session = soup_session_sync_new();

	multipart = soup_multipart_new (SOUP_FORM_MIME_TYPE_MULTIPART);
	soup_multipart_append_form_string (multipart, "key", _get_key().c_str());
	soup_multipart_append_form_string (multipart, "mac0", _get_mac(this->interface).c_str());
	msg = soup_form_request_new_from_multipart (EASYFIND_URL, multipart);
	soup_multipart_free (multipart);

	soup_session_send_message (session, msg);

	if (!SOUP_STATUS_IS_SUCCESSFUL (msg->status_code)) {
		syslog(
				LOG_ERR,
				"Error: Unexpected status %d, %s",
				msg->status_code,
				msg->reason_phrase
				);
	} else {
		syslog(
				LOG_DEBUG,
				"Easyfind: %s",
				msg->response_body->data
			  );
	}

	g_object_unref (msg);
	soup_session_abort (session);
	g_object_unref (session);
}
