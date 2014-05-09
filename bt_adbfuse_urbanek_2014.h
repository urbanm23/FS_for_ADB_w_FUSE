#include <stdio.h>
#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <cstdlib>
#include <fstream>
#include <sstream>
#include <string>
#include <iostream>
#include <vector>
#include <queue>
#include <unistd.h>
#include <fuse.h>

using namespace std;

string local = "/tmp/adbfuse/";
string selected_device = "";

string run_adb_shell_command( const string command );
string run_local_shell_command( const string command );
string exec( const string command );
vector<string> parse_string_to_array( const string output );
int str_hex_to_int( const string hexValue );
void create_local_tmp();
void adb_pull( const string path_string, const string local_path_string );
void adb_push( const string local_path_string, const string path_string );
int number_of_devices_connected();
string chose_device( const int n_devices );
string change_str_in_str( string path_string, const string find, const string replace );


string run_adb_shell_command( const string command ){

	string tmp_command;

	tmp_command = "adb ";
	tmp_command.append( selected_device );
	tmp_command.append( " shell busybox " );
	tmp_command.append( command );
	
	return exec( tmp_command );
}

string run_local_shell_command( const string command ){
	
	return exec( command );
}

string exec( const string command ){
	
	char buffer[512];
    	string result;

	FILE *pipe = popen( command.c_str(), "r" );
	if ( !pipe ) return "";
	
	while ( !feof( pipe ) ){
		if ( fgets( buffer, sizeof buffer, pipe ) != NULL ){
			result += buffer;
		}
	}
	pclose( pipe );

	return result;
}

vector<string> parse_string_to_array( const string output, const string sep ){
	
	vector<string> result;
	string separator = sep;
	size_t lastPos = 0, pos = 0;

	lastPos = output.find_first_not_of( separator, 0 ); //pozice prvniho znaku stringu ktery neni mezera
	pos = output.find_first_of( separator, lastPos );   //pozice prvni mezery ve stringu

	while( string::npos != pos || string::npos != lastPos ){
		result.push_back( output.substr( lastPos, pos - lastPos ) );
		lastPos = output.find_first_not_of( separator, pos );
		pos = output.find_first_of( separator, lastPos );
	}

	return result;	
}

int str_hex_to_int( const string hexValue ){

	int result;
	stringstream ss;

	ss << hex << hexValue;
	ss >> result;

	return result;
}

void create_local_tmp(){

	string tmp_command;

	tmp_command.assign( "rm -rf " );
	tmp_command.append( local );
	run_local_shell_command( tmp_command );
	mkdir( local.c_str(), 0755 );
}

void adb_pull( const string path_string, const string local_path_string ){
	
	string command;
	
	command.assign( "adb pull '" );
	command.append( path_string );
	command.append( "' '" );
	command.append( local_path_string );
	command.append( "'" );
	exec( command );
}

void adb_push( const string local_path_string, const string path_string ){
	
	string command;

	command.assign( "adb push '" );
	command.append( local_path_string );
	command.append( "' '" );
	command.append( path_string );
	command.append( "'" );
	exec( command );
}

int number_of_devices_connected(){
	
	string output;
	int found = 0;
	size_t next = 1;
	
	output = run_local_shell_command( "adb devices" );
	while( 1 ){
		next = output.find( "device\n", found+next );
		if( next != string::npos ) found++;
		else break;
	}
	return found;
}

string chose_device( const int n_devices ){
	
	vector<string> devices;
	string output;
	int pos = 0, number = 0;

	output = run_local_shell_command( "adb devices" );	
	pos = output.find_first_of("\n");
	output.erase( 0, pos+1 );

	for( int i = 0; i < n_devices; i++ ){
		pos = output.find_first_of( "\t" );
		devices.push_back( output.substr( 0, pos ) );
		pos = output.find_first_of("\n");
		output.erase( 0, pos+1 );
	}

	cout << "Prosim vyberte zarizeni. Uvedte cislo a potvrdte entrem." << endl;
	for( int i = 0; i < n_devices; i++ ){
		cout << i+1 <<" - " << devices[i] << endl;
	}
	cin >> number;

	return devices[number-1];
}

string change_str_in_str( string path_string, const string find, const string replace ){
	
	size_t pos = 0;

	while( ( pos = path_string.find( find, pos ) ) != string::npos ){
		path_string.replace( pos, find.length(), replace );
		pos += replace.length();
	}

	return path_string;
}
