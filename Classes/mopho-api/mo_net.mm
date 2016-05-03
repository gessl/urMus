//-----------------------------------------------------------------------------
// name: mo_net.mm
// desc: MoPhO API for networking
//
// authors: Jorge Herrera
//          Nick Bryan
//          Jieun Oh
//          Ge Wang
//
//    date: Fall 2009
//    version: 0.2
//
// Stanford Mobile Phone Orchestra
//     http://mopho.stanford.edu/
//-----------------------------------------------------------------------------
#include "mo_net.h"
#include <stdarg.h>
#include <ifaddrs.h> 
#include <arpa/inet.h>

pthread_mutex_t monet_mutex = PTHREAD_MUTEX_INITIALIZER; // Network threat safety

//-----------------------------------------------------------------------------
// name: instance()
// desc: get instance of MoNet
//-----------------------------------------------------------------------------
MoThread MoNet::m_thread;
bool MoNet::m_thread_started = false;
long MoNet::m_listening_port = 6449;

#define IP_MTU_SIZE 1536

//unsigned int MoNet::m_output_buffer_size = 1024;
unsigned int MoNet::m_output_buffer_size = IP_MTU_SIZE;
std::map<std::string, MoNetCallback> MoNet::m_pattern_callbacks;


//-----------------------------------------------------------------------------
// name: startListening()
// desc: Start a new thread that will listen to incomming OSC messages
//-----------------------------------------------------------------------------
void MoNet::startListening()
{
    if( !m_thread_started ) {
        m_thread_started = true;
        m_thread.start( MoNet::cb_osc_listener );
    } else {
        std::cout << "Thread has already been started\n";
    }
}


//-----------------------------------------------------------------------------
// name: stopListening()
// desc: Will kill the listening thread
//-----------------------------------------------------------------------------
void MoNet::stopListening()
{
    // TODO: figure out how to stop it
    // gessl: Here is a way
    m_thread.stop();
}


//-----------------------------------------------------------------------------
// name: addAddressCallback()
// desc: Register a callback to get called with a particular OSC message address
//-----------------------------------------------------------------------------
void MoNet::addAddressCallback( const std::string pattern, 
                                const MoNetReceiveCallback & callback, 
                                void * data  )
{
    // m_pattern_callbacks[pattern] = callback;
    MoNetCallback cbk;
    cbk.callback = callback;
    cbk.data = data;
    m_pattern_callbacks[pattern] = cbk;
}


//-----------------------------------------------------------------------------
// name: removeAddressCallback()
// desc: Will unregister a specific callback
//-----------------------------------------------------------------------------
void MoNet::removeAddressCallback( const std::string pattern )
{
    // TODO: implement this
}


//-----------------------------------------------------------------------------
// name: setListeningPort()
// desc: Defines the port that listens to incomming OSC messages
//-----------------------------------------------------------------------------
void MoNet::setListeningPort( long port )
{
    m_listening_port = port;
}


//-----------------------------------------------------------------------------
// name: getListeningPort()
// desc: Get the currently registered listening port
//-----------------------------------------------------------------------------
long MoNet::getListeningPort() 
{
    return m_listening_port;
}


//-----------------------------------------------------------------------------
// name: ProcessMessage()
// desc: Private function that process an incomming message and calls the 
//       corresponding callback
//-----------------------------------------------------------------------------
void MoNetOSCPacketListener::ProcessMessage( const osc::ReceivedMessage & m, 
                                             const IpEndpointName & remoteEndpoint )
{
    pthread_mutex_lock( &monet_mutex );
    try
    {
        std::string pattern = m.AddressPattern();
        std::map<std::string, MoNetCallback>::iterator it = 
            MoNet::m_pattern_callbacks.find( pattern );
        
        if( it != MoNet::m_pattern_callbacks.end() ) {
            osc::ReceivedMessageArgumentStream args = m.ArgumentStream();
            osc::ReceivedMessage::const_iterator arg = m.ArgumentsBegin();

            MoNet::m_pattern_callbacks[pattern].callback( args, arg, MoNet::m_pattern_callbacks[pattern].data );
        } else {
            std::cerr << "[mopho]: no callback has been registerd for: '"
                      << pattern  << "' pattern address\n";
        }
    }
    catch( osc::Exception & e )
    {
        std::cerr << "[mopho]: error while parsing message: "
                  << m.AddressPattern() << ": " << e.what() << "\n";
    }
    pthread_mutex_unlock( &monet_mutex );
}


//-----------------------------------------------------------------------------
// name: cb_osc_listener()
// desc: Private method to starts the new thread that listens to OSC messages
//-----------------------------------------------------------------------------
void * MoNet::cb_osc_listener( void * )
{
    // instantiate listener
    MoNetOSCPacketListener listener;
    long port = MoNet::getListeningPort();
    
    UdpListeningReceiveSocket s( IpEndpointName( IpEndpointName::ANY_ADDRESS, 
                                                 port), &listener );
    
    // print
    std::cerr << "[mopho]: OSC listener started on port: ";
    std::cerr << port << "..." << std::endl;
    
    // set priority
    MoThread::setSelfPriority( 95 );
    
    // go!
    s.RunUntilSigInt();
    
    return NULL;
}


//-----------------------------------------------------------------------------
// name: sendMessage()
// desc: Sends an OSC message
//-----------------------------------------------------------------------------
void MoNet::sendMessage( const std::string &ip, 
                         uint port, 
                         const std::string &pattern_address, 
                         char types[], 
                         uint size, 
                         ... )
{
    pthread_mutex_lock( &monet_mutex );
    unsigned int ipHex = stringIPtoHEX( ip );
    
    // TODO: this could be opened only once somewhere else to improve 
    // performance.
    UdpTransmitSocket transmitSocket( IpEndpointName( ipHex, port ) );
    
    char buffer[MoNet::m_output_buffer_size];
    osc::OutboundPacketStream p( buffer, MoNet::m_output_buffer_size );
    
    // Macros to read the variable arguments
    va_list args;
    va_start( args, size ); 

    // TODO: do we want to send bundles or just messages?
//    p << osc::BeginBundleImmediate << osc::BeginMessage( pattern_address.c_str() );
    p << osc::BeginMessage( pattern_address.c_str() );
    for( int i = 0; i < size; i++ )
    {
        if( types[i] == 'i' ) { // OSC-int32 
            p << va_arg( args, int );
        }
        else if( types[i] == 'f' ) { // OSC-float32
            // va_args requires double instead float values
            p << ( float )va_arg( args, double ); 
        }
        else if( types[i] == 's' ) { // OSC-string
            p << va_arg( args, char* );
        }
        // TODO: handle OSC-blob type, defined in the OSC specs
    }
    va_end( args );
//    p << osc::EndMessage << osc::EndBundle;
    p << osc::EndMessage;

    transmitSocket.Send( p.Data(), p.Size() );
    pthread_mutex_unlock( &monet_mutex );

}

// Added by gessl 11/23/10 to allow easier var arg passing from lua
// Splitting sendMessage into three parts. One is start send, one is send one arg, one is close send. Repeated calls of send one arg
// allows easy chaining of args.

UdpTransmitSocket *mytransmitSocket;

void MoNet::startSendStream(const std::string &ip, 
							uint port)
{
    unsigned int ipHex = stringIPtoHEX( ip );
	mytransmitSocket = new UdpTransmitSocket(IpEndpointName(ipHex, port));
}

char buffer[IP_MTU_SIZE];
osc::OutboundPacketStream p( buffer, IP_MTU_SIZE );

void MoNet::startSendMessage(const std::string &pattern_address)
{
    // TODO: do we want to send bundles or just messages?
//    p << osc::BeginBundleImmediate << osc::BeginMessage( pattern_address.c_str() );
    p << osc::BeginMessage( pattern_address.c_str() );
}

void MoNet::addSendInt(int i)
{
	p << i;
}

void MoNet::addSendFloat(float f)
{
	p << f;
}

void MoNet::addSendString(const char* s)
{
	p << s;
}

void MoNet::endSendMessage()
{
//    p << osc::EndMessage << osc::EndBundle;
    p << osc::EndMessage;
    mytransmitSocket->Send( p.Data(), p.Size() );
	p.Clear();
}

void MoNet::closeSendStream()
{
	delete mytransmitSocket;
}

//-----------------------------------------------------------------------------
// name: stringIPtoHEX
// desc: Private utilitary method that given an IP string, returns the 
//       corresponding Hexadecimal number
//-----------------------------------------------------------------------------
unsigned int MoNet::stringIPtoHEX( const std::string & ip )
{
    std::string delimiters = ".";
    unsigned int ipHex = 0;
    unsigned int i = 0;
    
    std::string::size_type last = ip.find_first_not_of( delimiters, 0 );
    std::string::size_type pos  = ip.find_first_of( delimiters, last );
    
    while( std::string::npos != pos || std::string::npos != last )
    {
        ipHex += ( atoi( ip.substr( last, pos - last ).c_str() ) << ( 3-i++ ) * 8 );
        last = ip.find_first_not_of( delimiters, pos );
        pos = ip.find_first_of( delimiters, last );
    }

    return ipHex;
}


//-----------------------------------------------------------------------------
// name: getMyIPaddress()
// desc: static method to retrieve the wifi IP address of the iPhone (based on 
// http://zachwaugh.com/2009/03/programmatically-retrieving-ip-address-of-iphone/ )
//-----------------------------------------------------------------------------
std::string MoNet::getMyIPaddress()
{
    std::string address = "error"; 
    struct ifaddrs *interfaces = NULL; 
    struct ifaddrs *temp_addr = NULL; 
    int success = 0;
    
    // retrieve the current interfaces - returns 0 on success  
    success = getifaddrs(&interfaces); 
    if( success == 0 )  { 
        // Loop through linked list of interfaces  
        temp_addr = interfaces; 
        while( temp_addr != NULL ) { 
            if(temp_addr->ifa_addr->sa_family == AF_INET)  { 
                // Check if interface is en0 which is the wifi connection on the iPhone  
                if([[NSString stringWithUTF8String:temp_addr->ifa_name] isEqualToString:@"en0"])  { 
                    // Get NSString from C String  
                    address = inet_ntoa(((struct sockaddr_in *)temp_addr->ifa_addr)->sin_addr);
                } 
            } 
            temp_addr = temp_addr->ifa_next; 
        } 
    } 
    // Free memory  
    freeifaddrs(interfaces); 
    
    return address;
}
