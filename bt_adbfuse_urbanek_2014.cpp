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
	string output;
    	string path_string;
	string command;
    	
	path_string.assign( path );
	command = "stat -t \"";
	command.append( path_string );
        command.append( "\"" );
	
	output = run_adb_shell_command( command );
	if ( output.empty() ) return -EIO;

	vector<string> output_parsed = parse_string_to_array( output, " " );
	if ( output_parsed.size() < 13 ) return -ENOENT;

	while( output_parsed.size() > 14 ){ //odstraneni jmena souboru/adresare
	        output_parsed.erase( output_parsed.begin() );
	}
	
	stbuf->st_size		= atoi( output_parsed[0].c_str() );
	stbuf->st_blocks	= atoi( output_parsed[1].c_str() );
	int raw_mode = str_hex_to_int ( output_parsed[2] );
	stbuf->st_mode = raw_mode | 0700;
	stbuf->st_uid 		= atoi( output_parsed[3].c_str() );
	stbuf->st_gid 		= atoi( output_parsed[4].c_str() );
	int device_id = str_hex_to_int( output_parsed[5] );
	stbuf->st_rdev 		= device_id;
	stbuf->st_ino		= atoi( output_parsed[6].c_str() );
	stbuf->st_nlink		= atoi( output_parsed[7].c_str() );
	stbuf->st_atime 	= atol( output_parsed[10].c_str() );
    	stbuf->st_mtime 	= atol( output_parsed[11].c_str() ); 
    	stbuf->st_ctime		= atol( output_parsed[12].c_str() );
	
	return res;	 	
}

static int adbfuse_readdir( const char *path, void *buf, fuse_fill_dir_t filler, off_t offset, struct fuse_file_info *fi ){
	
	( void ) offset;
    	( void ) fi;
	string path_string;
	string command;
	string output;	
	vector<string> output_parsed;
	
	path_string.assign( path );
	command.assign( "ls -1a --color=none \"" );
	command.append( path_string );
	command.append( "\"" );

	output = run_adb_shell_command( command );
	if ( output.empty() ) return -EIO;

	output_parsed = parse_string_to_array( output, "\n" );

	while ( output_parsed.size() > 0 ){
		output_parsed.front().erase( output_parsed.front().size()-1 );
		filler( buf, output_parsed.front().c_str(), NULL, 0 );
		output_parsed.erase( output_parsed.begin() );
	}
	
	return 0;
}

static int adbfuse_open( const char *path, struct fuse_file_info *fi ){
	
	string path_string;
	string local_path_string;

	path_string.assign( path );
	local_path_string.assign( local );
	local_path_string.append( change_str_in_str( path_string, "/", "=" ) );

	adb_pull( path_string, local_path_string );

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

static int adbfuse_flush( const char *path, struct fuse_file_info *fi ){
	
	string path_string;
	string local_path_string;
	string sync_command;
	
	path_string.assign( path );
	local_path_string.assign( local );
	local_path_string.append( change_str_in_str( path_string, "/", "=" ) );

	adb_push( local_path_string, path_string );
	sync_command.assign( "sync" );
        run_adb_shell_command( sync_command );
	
	return 0;
}

static int adbfuse_release( const char *path, struct fuse_file_info *fi ){
	
	int file_handle;
	file_handle = fi->fh;
    	close( file_handle );	

	return 0;
}

static int adbfuse_write( const char *path, const char *buf, size_t size, off_t offset, struct fuse_file_info *fi ){
	
	int result;
	int file_handle;
	
    	file_handle = fi->fh;
	if( file_handle == -1 ) return -errno;

	result = pwrite( file_handle, buf, size, offset );
	if ( result == -1 )return -errno;
	
	return size;
}

static int adbfuse_utimens( const char *path, const struct timespec tv[2] ){
	
	string path_string;
	string command;	
	string output;	

	path_string.assign( path );	
	command.assign( "touch \"" );
	command.append( path_string );
	command.append( "\"" );	
	output = run_adb_shell_command( command );
	
	return 0;
}

static int adbfuse_truncate( const char *path, off_t size ){
	
	string path_string;
	string local_path_string;
	
	path_string.assign( path );
	local_path_string.assign( local );
	local_path_string.append( change_str_in_str( path_string, "/", "=" ) );	
	
	adb_pull( path_string, local_path_string );

	if ( truncate( local_path_string.c_str(), size ) != 0 ) return -errno;

	return 0;
}

static int adbfuse_mknod( const char *path, mode_t mode, dev_t rdev ){
	
	string path_string;
	string local_path_string;
	string sync_command;	

	path_string.assign( path );
	local_path_string.assign( local );
	local_path_string.append( change_str_in_str( path_string, "/", "=" ) );

	mknod( local_path_string.c_str(), mode, rdev );
	adb_push( local_path_string, path_string );
	sync_command.assign( "sync" );
        run_adb_shell_command( sync_command );

	return 0;
}

static int adbfuse_mkdir( const char *path, mode_t mode ){
	
	string path_string;
	string command;
	string output;
	
	path_string.assign( path );
	
	command.assign( "mkdir '" );
	command.append( path_string );
	command.append( "'" );

	output = run_adb_shell_command( command );

	return 0;
}

static int adbfuse_rename( const char *from, const char *to ){
	
	string path_from;
	string path_to;
	string command;
	string output;

	path_from.assign( from );
	path_to.assign( to );
	
	command.assign( "mv '" );
	command.append( path_from );
	command.append( "' '" );
	command.append( path_to );
	command.append( "'" );

	output = run_adb_shell_command( command );	

	return 0;
}

static int adbfuse_rmdir( const char *path ){
	
	string path_string;
	string command;
	string output;

	path_string.assign( path );

	command.assign( "rmdir '" );
	command.append( path_string );
	command.append( "'" );

	output = run_adb_shell_command( command );
	
	return 0;
}

static int adbfuse_unlink( const char *path ){
	
	string path_string;
	string command;
	string output;

	path_string.assign( path );

	command.assign( "rm '" );
	command.append( path_string );
	command.append( "'" );

	output = run_adb_shell_command( command );	
	
	return 0;
}

static int adbfuse_readlink( const char *path, char *buf, size_t size ){
		
	string path_string;
	string command;
	string adb_output;

	path_string.assign( path );

	command.assign( "readlink -n \"" );
	command.append( path_string );
	command.append	( "\"" );

	adb_output = run_adb_shell_command( command );
	
	if( adb_output.at( 0 ) == '/' ) adb_output.erase( adb_output.begin() );

	size_t local_size = adb_output.size();

	memcpy( buf, adb_output.c_str(), local_size );
	buf[local_size]='\0';
	
	return 0;
}


int main(int argc, char *argv[]){

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
