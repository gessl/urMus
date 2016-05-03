//-----------------------------------------------------------------------------
// name: mo_net.h
// desc: MoPhO API for networking
//
// authors: Jorge Herrera 
//          Jieun Oh
//          Nick Bryan
//          Ge Wang
//
//    date: Fall 2009
//
// Stanford Mobile Phone Orchestra
//     http://mopho.stanford.edu/
//    version: 0.2
//
// note: ( mo_net is pronounced "Monet" )
//       - You need to add MoPhO version of OSCpack to the project
//-----------------------------------------------------------------------------
#ifndef __MO_NET_H__
#define __MO_NET_H__

#include "mo_def.h"
#include "mo_thread.h"

#include "UdpSocket.h"
#include "OscReceivedElements.h"
#include "OscPacketListener.h"
#include "OscOutboundPacketStream.h"

#include <iostream>
#include <map>


//-----------------------------------------------------------------------------
// name: class MoNetOSCPacketListener
// desc: listen to incoming OSC messages
//-----------------------------------------------------------------------------
class MoNetOSCPacketListener : public osc::OscPacketListener
{   
protected:
    // Process the recieved messages
    virtual void ProcessMessage( const osc::ReceivedMessage & m, 
                                 const IpEndpointName & remoteEndpoint );
};

class MoNetSender 
{
    // TODO: implement me
};

// Type of callback to handle received OSC messages.
typedef void ( * MoNetReceiveCallback )( osc::ReceivedMessageArgumentStream 
& argument_stream, osc::ReceivedMessage::const_iterator & arg,
                                         void * data );


//-----------------------------------------------------------------------------
// name: struct MoNet
// desc: hold a OSC Receive Callback and its associated data
//-----------------------------------------------------------------------------
struct MoNetCallback {
    MoNetReceiveCallback callback;
    void * data;
};


//-----------------------------------------------------------------------------
// name: class MoNet
// desc: network stuff  ( singleton )
//-----------------------------------------------------------------------------
class MoNet
{
public:   
    
    // Register and Unregister callbacks to handle specific pattern address 
    // (messages)
    static void addAddressCallback(
        const std::string pattern, 
        const MoNetReceiveCallback & callback,
        void * data = NULL );
    static void removeAddressCallback( const std::string pattern );
    
    // Setter & getter for the listening port
    static void setListeningPort( long port );
    static long getListeningPort();
    
    // Start & stop the thread that listen to incoming OSC messages
    static void startListening();
    static void stopListening();
    
    // List of pattern addresses ( key ) and respective callbacks ( value )
    // FIXME: this should be private ( and static ), but then it was harder 
    // to get acces to it from MoNetOSCPacketListener
    static std::map<std::string, MoNetCallback> m_pattern_callbacks;
    
    // Sends an OSC message with variable number and type of arguments
    // 
    // 'ip' is a string with the destination ip ( i.e. "127.0.0.1" )
    // 'port' is the destination port ( i.e. 8888 )
    // 'pattern_address' is the pattern of the message ( i.e. /mopho/test  )
    // 'types' is and array of chars cointaining the types of the different OSC 
    // variables to be send. It can be defined as: 
    //              char types[num_vars] = {'i', 'f', 's'};
    // 'size' is the number of variables to be send ( must be the same length
    // as the types array.
    // After the 'size' argument, a comma separated list of varibles to be sent,
    // must be provided. The order must be the same as the order specified in 
    // the types array
    static void sendMessage(
        const std::string &ip, 
        uint port, 
        const std::string &pattern_address, 
        char types[], 
        uint size, 
        ... );
	static void startSendStream(const std::string &ip, 
									   uint port);
	static void startSendMessage(const std::string &pattern_address);
	static void addSendInt(int i);
	static void addSendFloat(float f);
	static void addSendString(const char* s);
	static void endSendMessage();
	static void closeSendStream();
	
public:
    static std::string getMyIPaddress();

private:
    // OSC listening thread
    static MoThread m_thread;
    static bool m_thread_started;
    // listening port
    static long m_listening_port;
    // Output buffer size
    static unsigned int m_output_buffer_size;

    // converter
    static unsigned int stringIPtoHEX( const std::string & ip );
    
private:
    // Launches the listening thread
    static void * cb_osc_listener( void * );    
};


#endif
