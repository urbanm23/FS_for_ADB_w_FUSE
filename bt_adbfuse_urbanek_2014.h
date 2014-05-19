#include <fuse.h>
#include <stdio.h>
#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <cstdlib>
#include <fstream>
#include <sstream>
#include <string>
#include <iostream>
#include <queue>
#include <vector>
#include <map>
#include <unistd.h>
#include <execinfo.h>
#include <signal.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <pwd.h>
#include <grp.h>

using namespace std;

// struktura polozky cache obsahuje data i casovy udaj o porizeni
struct timeWdataCache{
	string fileInfo;
	time_t timestamp;
};

// promenna obsahujici cestu k lokalnimu adresari pro fuse
string local;
// id vybraneho zarizeni
string selected_device = "";
// globalni cache, v niz jsou jako klice cesty k souborum a hodnoty informace o souborech
map<string,timeWdataCache> globalCache;
// jednoducha mapa urcujici zda je soubor otevren pro zapis
map<int,bool> fileOpenedToWrite;
// mapa, ve ktere je info o jiz zmenenych souborech aby se pri operaci OPEN nestahovali znovu
map<string,bool> fileDownloaded;


void delete_from_gCache( const string & path );
size_t find_nth( int n, const string & substr, const string & corpus );
int strmode_to_rawmode( const string& strMode );
int number_of_devices_connected();
string chose_device( const int n_devices );
string run_local_shell_command(const string& command);
queue<string> run_adb_shell_command( const string & command );
void cleanupTmpDir( void );
void create_local_tmp( void );
void adb_pull( const string & path_string, const string & local_path_string );
void adb_push( const string & local_path_string, const string & path_string );
vector<string> parse_string_to_array( const string & input, const string & sep );
string & change_str_in_str( string & source, const string & find, const string & replace );
string localExec( const string command );
queue<string> exec( const string & command );


void delete_from_gCache( const string & path ){   
	// najdi pozici (klic) k ceste
    	map<string, timeWdataCache>::iterator to_delete = globalCache.find( path );
	// pokud to v mape je pak to smaz
    	if( to_delete != globalCache.end() ) globalCache.erase( to_delete );
}

size_t find_nth( int n, const string & substr, const string & corpus ){
    size_t p = 0;
    while( n-- ){
        if( ( ( p = corpus.find_first_of( substr, p ) ) ) == string::npos ) return string::npos;
        p = corpus.find_first_not_of( substr, p ); 
    }   
    return p;
}

int strmode_to_rawmode( const string& strMode ){

	//postupne vymaskovani deviti bitu pristupovych prav
	int intMode = 0;

	switch( strMode[0] ){
		case 's': intMode |= S_IFSOCK; break;
		case 'l': intMode |= S_IFLNK; break;
		case '-': intMode |= S_IFREG; break;
    		case 'd': intMode |= S_IFDIR; break;
    		case 'b': intMode |= S_IFBLK; break;
    		case 'c': intMode |= S_IFCHR; break;
    		case 'p': intMode |= S_IFIFO; break;
    	}
   
	if( strMode[1] == 'r' ) intMode |= S_IRUSR;
	if( strMode[2] == 'w' ) intMode |= S_IWUSR;
    
	switch( strMode[3] ){
    		case 'x': intMode |= S_IXUSR; break;
		case 's': intMode |= S_ISUID | S_IXUSR; break;
		case 'S': intMode |= S_ISUID; break;
	}

	if( strMode[4] == 'r') intMode |= S_IRGRP;
	if( strMode[5] == 'w') intMode |= S_IWGRP;

    	switch( strMode[6] ){
    		case 'x': intMode |= S_IXGRP; break;
    		case 's': intMode |= S_ISGID | S_IXGRP; break;
    		case 'S': intMode |= S_ISGID; break;
    	}
    
	if( strMode[7] == 'r' ) intMode |= S_IROTH;
    	if( strMode[8] == 'w' ) intMode |= S_IWOTH;

    	switch( strMode[9] ){
    		case 'x': intMode |= S_IXOTH; break;
    		case 't': intMode |= S_ISVTX | S_IXOTH; break;
    		case 'T': intMode |= S_ISVTX; break;
    	}

    return intMode;
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

string run_local_shell_command( const string & command ){
	
	string tmp_command;
	tmp_command.assign( command );
	
	return localExec( tmp_command );
}

queue<string> run_adb_shell_command( const string & command ){
   
	string tmp_command = "adb ";
	tmp_command.append( selected_device );
	tmp_command.append( " shell " );
	tmp_command.append( command );
	return exec( tmp_command );
}

void cleanupTmpDir( void ){
	string command = "rm -rf ";
    	command.append( local );
    	run_local_shell_command( command );
}

void create_local_tmp( void ){
	string command = "mkdir ";
    	local.assign( "/tmp/adbfuse/" );
	command.append( local );
	run_local_shell_command( command );	
	atexit( &cleanupTmpDir );
}

void adb_pull( const string & path_string, const string & local_path_string ){

	string command;
    	
	command.assign( "adb " );
	command.append( selected_device );
	command.append( " pull '" );
	command.append( path_string );
	command.append( "' '" );
	command.append( local_path_string );
	command.append( "'" );

	exec( command );
}

void adb_push( const string & local_path_string, const string & path_string ){

	string command;
    	
	command.assign( "adb " );
	command.append( selected_device );
	command.append( " push '" );
	command.append( local_path_string );
	command.append( "' '" );
	command.append( path_string );
	command.append( "'" );

	exec( command );
}

vector<string> parse_string_to_array( const string & input, const string & sep ){

	vector<string> result;
	string separator = sep;
	string::size_type lastPos =0, pos = 0;
	
    	if ( input.size() < 1) return result;
        lastPos = input.find_first_not_of( separator, 0 );
    	pos	= input.find_first_of( separator, lastPos );

	while( string::npos != pos || string::npos != lastPos ){
        	result.push_back( input.substr( lastPos, pos - lastPos ) );
        	lastPos = input.find_first_not_of( separator, pos );
            	pos = input.find_first_of( separator, lastPos );                
	}
	return result;
}

string & change_str_in_str( string & source, const string & find, const string & replace ){
	
	size_t pos = 0;
	
	while(  ( pos = source.find( find, pos ) ) != string::npos  ){
		source.replace( pos, find.length(), replace );
        	pos += replace.length();
    	}
    	return source;
}	

string localExec( const string command ){
	
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

queue<string> exec( const string & command ){

    	queue<string> output;
	char buffer[1024];
    	string tmp_string;	

    	FILE * pipe = popen( command.c_str(), "r" );

	while (  !feof( pipe ) ){
		if( fgets( buffer, sizeof buffer, pipe ) != NULL ){
        		tmp_string.assign( buffer );
        		tmp_string.erase( tmp_string.size()-2 );
        		output.push( tmp_string );
		}
    	}
	pclose( pipe );
    return output;
}

