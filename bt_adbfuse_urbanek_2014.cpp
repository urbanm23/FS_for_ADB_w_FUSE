#define FUSE_USE_VERSION 26
#include "bt_adbfuse_urbanek_2014.h"

static struct fuse_operations adbfuse_oper;

static int adbfuse_getattr( const char *path, struct stat *stbuf );
static int adbfuse_readdir( const char *path, void *buf, fuse_fill_dir_t filler, off_t offset, struct fuse_file_info *fi );
static int adbfuse_open( const char *path, struct fuse_file_info *fi );
static int adbfuse_read( const char *path, char *buf, size_t size, off_t offset, struct fuse_file_info *fi );
static int adbfuse_flush( const char *path, struct fuse_file_info *fi );
static int adbfuse_release( const char *path, struct fuse_file_info *fi );
static int adbfuse_write( const char *path, const char *buf, size_t size, off_t offset, struct fuse_file_info *fi );
static int adbfuse_utimens( const char *path, const struct timespec tv[2] );
static int adbfuse_truncate( const char *path, off_t size );
static int adbfuse_mknod( const char *path, mode_t mode, dev_t rdev );
static int adbfuse_mkdir( const char *path, mode_t mode );
static int adbfuse_rename( const char *from, const char *to );
static int adbfuse_rmdir( const char *path );
static int adbfuse_unlink( const char *path );
static int adbfuse_readlink( const char *path, char *buf, size_t size );


static int adbfuse_getattr( const char *path, struct stat *stbuf ){
    
	int res = 0;
    	memset( stbuf, 0, sizeof( struct stat ) );
    	queue<string> output;
   	string path_string;
	struct passwd * foruid;
    	struct group * forgid;
	string command;
	vector<string> output_chunk;
	int Date;
	struct tm ftime;
	
	// pretypovani, cesta k souboru ulozena do promenne string
	path_string.assign( path );
	
	// pokud data v cache nejsou nebo pokud jsou data starsi 30ti sekund pak se nactou znovu jinak se pouziji data z cache
	if( globalCache.find( path_string ) ==  globalCache.end() || ( globalCache[path_string].timestamp + 30 ) < time( NULL ) ){
		// sestaveni prikazu pro ADB
		command.assign( "ls -l -a -d '" );
		command.append( path_string );
        	command.append( "'" );
        	output = run_adb_shell_command( command );
		// pokud zarizeni nevrati zadny retezec pak neni zadne pripojeno ( chyba )
        	if( output.empty() ) return -EAGAIN;
        	output_chunk = parse_string_to_array( output.front(), " " );
		// zaroven se zjistene informace ulozi do cache a nastavi se casova znamka
        	globalCache[path_string].fileInfo = output.front();
        	globalCache[path_string].timestamp = time( NULL );
    	}else{
		// ziskani dat z cache
        	output_chunk = parse_string_to_array( globalCache[path_string].fileInfo, " " );
	}

	//pokud vysup zacina cestou jde o chybu. Napr soubor neexistuje.
	if( output_chunk[0][0] == '/') return -ENOENT;
	// cislo inode = 1
    	stbuf->st_ino = 1;
	// prava k souboru a urceni typu souboru prevedeno pomoci funkce strmode_to_rawmode
    	stbuf->st_mode = strmode_to_rawmode( output_chunk[0] );
	// pocet hardlinku
	stbuf->st_nlink = 1;
	// zjisteni ID uzivatele
	foruid = getpwnam( output_chunk[1].c_str() );
    	if( foruid ) // pokud lze ziskat ID uzivatele pak se pouzije jinak se dosadi cislo 95 ( aby nekolidovalo )
		stbuf->st_uid = foruid->pw_uid;
	else
		stbuf->st_uid = 95;
	// zjisteni ID skupiny uzivatele
	forgid = getgrnam( output_chunk[2].c_str() );
	if ( forgid ) // pokud lze ziskat ID uzivatele pak se pouzije jinak se dosadi cislo 95 ( aby nekolidovalo )
		stbuf->st_gid = forgid->gr_gid;
	else
		stbuf->st_gid = 95;
	
	// pokud se jedna o soubor S_IFMT maska pro 0170000
	switch ( stbuf->st_mode & S_IFMT ){
		// blokove zarizeni
    		case S_IFBLK:
		// znakove zarizeni
		case S_IFCHR:
			stbuf->st_rdev = atoi( output_chunk[3].c_str() ) * 256 + atoi( output_chunk[4].c_str() );
			stbuf->st_size = 0;
			Date = 5;
		break;
		break;
		
		// pokud jde o normalni soubor priradi se velikost
		case S_IFREG:
			stbuf->st_size = atoi( output_chunk[3].c_str() );
			Date = 4;
		break;

		default:
		case S_IFSOCK: // soket
		case S_IFIFO:  // FIFO
		case S_IFLNK:  // symbolicky odkaz
		// pokud jde o adresar velikost je nastavena na nulu		
		case S_IFDIR:
			stbuf->st_size = 0;
			Date = 3;
		break;
	}
	// velikost bloku pro soyborovy system I/O
	stbuf->st_blksize = 0;
	// pocet alokovanych bloku ( nepodstatne pro virtualni souborovy system )
	stbuf->st_blocks = 1;

	// vyparsovani datumu a casu z informaci o souboru/adresari ( pozici urcuje Date )
	vector<string> ymd = parse_string_to_array( output_chunk[Date], "-" );
	vector<string> hm = parse_string_to_array( output_chunk[Date + 1], ":" );
	
    	ftime.tm_year = atoi(ymd[0].c_str()) - 1900;
	ftime.tm_mon  = atoi(ymd[1].c_str()) - 1;
	ftime.tm_mday = atoi(ymd[2].c_str());
	ftime.tm_hour = atoi(hm[0].c_str());
	ftime.tm_min  = atoi(hm[1].c_str());
	ftime.tm_sec  = 0;
	ftime.tm_isdst = -1;

	// vytvoreni casu v sekundach
	time_t now = mktime( &ftime );
	// cas prirazen do vsech tri casu ( vytvoreni, posledni modifikace, posledniho pristupu )
	stbuf->st_atime = now;
    	stbuf->st_mtime = now;
	stbuf->st_ctime = now;
	return res;
}

static int adbfuse_readdir( const char *path, void *buf, fuse_fill_dir_t filler, off_t offset, struct fuse_file_info *fi ){

	(void) offset;
	(void) fi;
	string path_string;
	queue<string> output;

    	path_string.assign( path );

	string command = "ls -l -a '";
	command.append( path_string );
	command.append( "'" );
	output = run_adb_shell_command( command );
	// neni pripojen telefon nebo cesta souboru neexistuje ?
    	if( !output.size() ) return 0;

    	vector<string> output_chunk = parse_string_to_array( output.front(), " " );
    	if( output.front().length() < 3 ) return -ENOENT;
	// overeni zda zarizeni nevraci chybove hlaseni
    	if( output.front().c_str()[1] != 'r' && output.front().c_str()[1] != '-' ) return -ENOENT;

	while( output.size() > 0 ) {
		// ziskani jmena z informaci o datech
		const string & fname_w_ws = output.front().substr( 54 ); 
		const string & fname_w_link = fname_w_ws.substr( find_nth( 1, " ", fname_w_ws ) );
		const string & fname = fname_w_link.substr( 0, fname_w_link.find( " -> " ) );
		filler( buf, fname.c_str(), NULL, 0 );
		// vytvoreni zaznamu do cache
		const string & path_string_c = path_string + ( path_string == "/" ? "" : "/" ) + fname;

		globalCache[path_string_c].fileInfo = output.front();
        	globalCache[path_string_c].timestamp = time( NULL );
		output.pop();
	}
    return 0;
}


static int adbfuse_open( const char *path, struct fuse_file_info *fi ){
	
	string path_string;
	string local_path_string;
	queue<string> output;
	string command;
	vector<string> output_parsed;
    
	path_string.assign( path );
	local_path_string.assign( local );
	change_str_in_str( path_string, "/", "=" );
	local_path_string.append( path_string );

	path_string.assign( path );
	// pokud se jiz soubor do lokalniho adresare dostal, nebude se stahova znovu
	if( !fileDownloaded[path_string] ){	
		command = "ls -l -a -d '";
		command.append( path_string );
		command.append( "'" );
		output = run_adb_shell_command( command );
		output_parsed = parse_string_to_array( output.front(), " " );
		// pokud mobil vrati hlasku o spatnem souboru/adresari pak skonci
		if( output_parsed[0][0] == '/' ) return -ENOENT;
		// zkopirovani souboru ze zarizeni do lolaniho adresare
		adb_pull( path_string, local_path_string );
	}else{
		fileDownloaded[path_string] = false;
	}

	fi->fh = open( local_path_string.c_str(), fi->flags );

	return 0;
}

static int adbfuse_read( const char *path, char *buf, size_t size, off_t offset, struct fuse_file_info *fi ){
    
	int file_handle;
	int result;
	
	file_handle = fi->fh;
	if( file_handle == -1 ) return -errno;
	
    	result = pread( file_handle, buf, size, offset );
    	if( result == -1 ) return -errno;

	return size;
}

static int adbfuse_write( const char *path, const char *buf, size_t size, off_t offset, struct fuse_file_info *fi ){

	int result;
	int file_handle;

	file_handle = fi->fh;
	
	fileOpenedToWrite[file_handle] = true;

	result = pwrite( file_handle, buf, size, offset );
    	if( result == -1 ) return -errno;
	
	return size;
}


static int adbfuse_flush( const char *path, struct fuse_file_info *fi ){
	
	string path_string;
	string local_path_string;
	string sync_command;
	int file_handle = fi->fh;

	path_string.assign( path );
	local_path_string.assign( local );
	change_str_in_str( path_string, "/", "=" );
	local_path_string.append( path_string );
	path_string.assign( path );

	delete_from_gCache( path_string );
	if( fileOpenedToWrite[file_handle] ){
		fileOpenedToWrite[file_handle] = false;
        	adb_push( local_path_string, path_string );
		sync_command.assign( "sync" );
		run_adb_shell_command( sync_command );
	}
    
	return 0;
}

static int adbfuse_release( const char *path, struct fuse_file_info *fi ){
	
	int file_handle;

	file_handle = fi->fh;
	fileOpenedToWrite.erase( fileOpenedToWrite.find( file_handle ) );
    	close( file_handle );
	
	return 0;
}

static int adbfuse_utimens( const char *path, const struct timespec tv[2] ){
	
	string path_string;
	string command;

	globalCache[path_string].timestamp = globalCache[path_string].timestamp + 50;
	command = "touch '";
    	command.append( path_string );
    	command.append( "'" );
    	run_adb_shell_command( command );

	return 0;
}

static int adbfuse_truncate( const char *path, off_t size ){
    
	int result = 0;
	string path_string;
	string local_path_string;

	path_string.assign( path );
	local_path_string.assign( local );
	change_str_in_str( path_string, "/", "=" );
	local_path_string.append( path_string );
	path_string.assign( path );
    
	adb_pull( path_string,local_path_string );
    	
    	fileDownloaded[path_string] = true;
    	delete_from_gCache(path_string);
	
	result = truncate( local_path_string.c_str(), size );

    	return result;
}

static int adbfuse_mknod( const char *path, mode_t mode, dev_t rdev ){
	
	string path_string;
	string local_path_string;
	string sync_command;
    
	path_string.assign( path );
	local_path_string.assign( local );
    	change_str_in_str( path_string, "/", "=" );
	local_path_string.append( path_string );
	path_string.assign( path );
	
    	mknod( local_path_string.c_str(), mode, rdev );

	adb_push( local_path_string, path_string );
	sync_command.assign( "sync" );
	run_adb_shell_command( sync_command );
	delete_from_gCache( path_string );

	return 0;
}

static int adbfuse_mkdir( const char *path, mode_t mode ){

	string path_string;
	string command;
	
	path_string.assign( path );

	
    	command.assign( "mkdir '" );
    	command.append( path_string );
    	command.append( "'" );
    	
	run_adb_shell_command( command );
    	delete_from_gCache( path_string );
    
	return 0;
}

static int adbfuse_rename( const char *from, const char *to ){

	string path_from;
	string path_to;
	string command;

	path_from.assign( from );
	path_to.assign( to );

	command.assign( "mv '" );
	command.append( path_from );
	command.append( "' '" );
	command.append( path_to );
	command.append( "'" );

	run_adb_shell_command( command );
	delete_from_gCache( path_from );
	delete_from_gCache( path_to );

	return 0;
}

static int adbfuse_rmdir( const char *path ){
    
	string path_string;
	string command;

	path_string.assign( path );

    	command.assign( "rmdir '" );
    	command.append( path_string );
    	command.append( "'" );

    	run_adb_shell_command( command );
    	delete_from_gCache(path_string);

	return 0;
}

static int adbfuse_unlink( const char *path ){

	string path_string;
	string command;
	
	path_string.assign( path );

    	command = "rm '";
    	command.append( path_string );
	command.append( "'" );
	run_adb_shell_command( command );
	delete_from_gCache( path_string );

	return 0;
}

static int adbfuse_readlink( const char *path, char *buf, size_t size ){

	string path_string;
	queue<string> output;
	string result;
	string command;
	unsigned int num_slashes, ii;
	size_t pos, my_size;

	path_string.assign( path );

    
    	for(num_slashes = ii = 0; ii < strlen(path); ii++)
        	if (path[ii] == '/') num_slashes++;
    	
	if (num_slashes >= 1) num_slashes--;
	
	// pokud je info o linku v cache pak se pouzije jinak se ziska z mob. zarizeni
	if( globalCache.find( path_string ) ==  globalCache.end() || ( globalCache[path_string].timestamp + 30 ) < time( NULL ) ){
		command = "ls -l -a -d '";
		command.append( path_string );
		command.append( "'" );
        	output = run_adb_shell_command( command );
        	if( output.empty() ) return -EINVAL; 
        	result = output.front();
        	globalCache[path_string].fileInfo = output.front();
        	globalCache[path_string].timestamp = time( NULL );
	}else{
        	result = globalCache[path_string].fileInfo;	
	}
	// pokud vrati chybovou hlasku napr. o nenalezenem souboru zacinajici lomitkem vrati chybu
	if( result[0] == '/' ) return -ENOENT;

    	pos = result.find( " -> " );
	if( pos == string::npos ) return -EINVAL;
    	pos += 4;
    	my_size = result.size();
    	buf[0] = 0;
    	if( result[pos] == '/' ){
		while( result[pos] == '/' ) ++pos;
	    	my_size += 3 * num_slashes - pos;
	    	if( my_size >= size ) return -ENOSYS;
	    	while( num_slashes ) {
			strncat( buf, "../", size );
			num_slashes--;
	    	}
    	}
    	if( my_size >= size ) return -ENOSYS;
    	strncat( buf, result.c_str() + pos,size );
	return 0;
}



int main( int argc, char *argv[] ){

	int devices = 0; 
	string device_prefix = "-s ";
   	
	create_local_tmp();
	devices = number_of_devices_connected();
	if ( devices > 1 ) selected_device = device_prefix.append( chose_device( devices ) );

    	memset( &adbfuse_oper, sizeof( adbfuse_oper ), 0 );
    
    	adbfuse_oper.readdir	= adbfuse_readdir;
    	adbfuse_oper.getattr	= adbfuse_getattr;
    	adbfuse_oper.open	= adbfuse_open;	
	adbfuse_oper.read	= adbfuse_read;
    	adbfuse_oper.flush 	= adbfuse_flush;
    	adbfuse_oper.release 	= adbfuse_release;
    	adbfuse_oper.write	= adbfuse_write;
    	adbfuse_oper.utimens 	= adbfuse_utimens;
    	adbfuse_oper.truncate 	= adbfuse_truncate;
	adbfuse_oper.mknod 	= adbfuse_mknod;
    	adbfuse_oper.mkdir 	= adbfuse_mkdir;
    	adbfuse_oper.rename 	= adbfuse_rename;
    	adbfuse_oper.rmdir 	= adbfuse_rmdir;
    	adbfuse_oper.unlink 	= adbfuse_unlink;
    	adbfuse_oper.readlink 	= adbfuse_readlink;

    return fuse_main( argc, argv, &adbfuse_oper, NULL );
}
