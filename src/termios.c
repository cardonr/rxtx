#ifdef TRENT_IS_HERE
#define TRACE
#define DEBUG
#endif /* TRENT_IS_HERE */
extern void report( char * );
extern void report_warning( char * );
extern void report_error( char * );
/*-------------------------------------------------------------------------
|   RXTX License v 2.1 - LGPL v 2.1 + Linking Over Controlled Interface.
|   RXTX is a native interface to serial ports in java.
|   Copyright 1998-2002 by Wayne roberts wroberts1@home.com
|   Copyright 1997-2009 by Trent Jarvi tjarvi@qbang.org and others who
|   actually wrote it.  See individual source files for more information.
|
|   A copy of the LGPL v 2.1 may be found at
|   http://www.gnu.org/licenses/lgpl.txt on March 4th 2007.  A copy is
|   here for your convenience.
|
|   This library is free software; you can redistribute it and/or
|   modify it under the terms of the GNU Lesser General Public
|   License as published by the Free Software Foundation; either
|   version 2.1 of the License, or (at your option) any later version.
|
|   This library is distributed in the hope that it will be useful,
|   but WITHOUT ANY WARRANTY; without even the implied warranty of
|   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
|   Lesser General Public License for more details.
|
|   An executable that contains no derivative of any portion of RXTX, but
|   is designed to work with RXTX by being dynamically linked with it,
|   is considered a "work that uses the Library" subject to the terms and
|   conditions of the GNU Lesser General Public License.
|
|   The following has been added to the RXTX License to remove
|   any confusion about linking to RXTX.   We want to allow in part what
|   section 5, paragraph 2 of the LGPL does not permit in the special
|   case of linking over a controlled interface.  The intent is to add a
|   Java Specification Request or standards body defined interface in the
|   future as another exception but one is not currently available.
|
|   http://www.fsf.org/licenses/gpl-faq.html#LinkingOverControlledInterface
|
|   As a special exception, the copyright holders of RXTX give you
|   permission to link RXTX with independent modules that communicate with
|   RXTX solely through the Sun Microsytems CommAPI interface version 2,
|   regardless of the license terms of these independent modules, and to copy
|   and distribute the resulting combined work under terms of your choice,
|   provided that every copy of the combined work is accompanied by a complete
|   copy of the source code of RXTX (the version of RXTX used to produce the
|   combined work), being distributed under the terms of the GNU Lesser General
|   Public License plus this exception.  An independent module is a
|   module which is not derived from or based on RXTX.
|
|   Note that people who make modified versions of RXTX are not obligated
|   to grant this special exception for their modified versions; it is
|   their choice whether to do so.  The GNU Lesser General Public License
|   gives permission to release a modified version without this exception; this
|   exception also makes it possible to release a modified version which
|   carries forward this exception.
|
|   You should have received a copy of the GNU Lesser General Public
|   License along with this library; if not, write to the Free
|   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
|   All trademarks belong to their respective owners.
--------------------------------------------------------------------------*/
#include <windows.h>
#include <stdio.h>
#include <errno.h>
#include <time.h>
#include "win32termios.h"

/*
 * odd malloc.h error with lcc compiler
 * winsock has FIONREAD with lcc
 */


#ifdef __LCC__
#	include <winsock.h>
#else
#	include <malloc.h>
#endif /* __LCC__ */

#define SIGIO 0

int my_errno;

//CARDON
//extern int errno;


//cardon
#define MAXLEN 255

#ifdef DEBUG_VERBOSE

char message[MAXLEN + 1];
#endif /* DEBUG_VERBOSE*/



struct termios_list
{
	char filename[80];
	int my_errno;
	int interrupt;
	int event_flag;
	int tx_happened;
	unsigned long *hComm;
	struct termios *ttyset;
	struct serial_struct *sstruct;
	/* for DTR DSR */
	unsigned char MSR;
	struct async_struct *astruct;
	struct serial_icounter_struct *sis;
	int open_flags;
	OVERLAPPED rol;
	OVERLAPPED wol;
	OVERLAPPED sol;
	HANDLE readInterruptEvent; //added by Cardon
	HANDLE monitorInterruptEvent; //added by Cardon
	int readOngoing; //added by Cardon. Is true when a read is busy (the event handle can't be freed).
	int waitingCommEvent; //added by Cardon. Is true when waiting for comm event (the event handle can't be freed).
	int is_closing; //added by Cardon. Is true when serial_close was called.

	int fd;
	struct termios_list *next;
	struct termios_list *prev;
	struct env_struct* pEnvStruct;
	

	
	/*


	//FREE
		//added by Cardon
	if (eis->readInterruptEvent) {
		CloseHandle(eis->readInterruptEvent);
	}
	if (eis->monitorInterruptEvent) {
		CloseHandle(eis->monitorInterruptEvent);
	}



	*/

};
struct termios_list *first_tl = NULL;

/*----------------------------------------------------------
serial_test

   accept: filename to test
   perform:
   return:      1 on success 0 on failure
   exceptions:
   win32api:    CreateFile CloseHandle
   comments:    if the file opens it should be ok.
----------------------------------------------------------*/
int serial_test(struct env_struct* pEnvStruct, char * filename )
{
	
	unsigned long *hcomm;
	int ret;
	
	SLEEP_AND_TRACE("serial_test:CreateFile");
	hcomm = CreateFile( filename, GENERIC_READ |GENERIC_WRITE, 0, 0, OPEN_EXISTING, 0, 0 );
	if ( hcomm == INVALID_HANDLE_VALUE )
	{
		if (GetLastError() == ERROR_ACCESS_DENIED)
		{
			ret = 1;
		}
		else
		{
			ret = 0;
		}
	}
	else
	{
		ret = 1;
	}

	SLEEP_AND_TRACE("serial_test:CloseHandle");
	CloseHandle( hcomm );
	return(ret);
}

void termios_setflags( int fd, int termios_flags[] )
{
	struct termios_list *index = find_port( fd );
	int i, result;
	//int windows_flags[11] = { 0, EV_RXCHAR, EV_TXEMPTY, EV_CTS, EV_DSR,
	//				EV_RING|0x2000, EV_RLSD, EV_ERR,
	//				EV_ERR, EV_ERR, EV_BREAK
	//			};

	//adapted by Cardon: no event of RX, ERR, BREAK
	int windows_flags[11] = { 0, 0, EV_TXEMPTY, EV_CTS, EV_DSR,
					EV_RING, EV_RLSD, 0,
					0, 0, 0	};

	if( !index )
	{
		LEAVE( "termios_setflags" );
		return;
	}
	index->event_flag = 0;
	for(i=0;i<11;i++)
		if( termios_flags[i] )
			index->event_flag |= windows_flags[i];
	
	
	//make sure the following event are not tracked
	index->event_flag &= ~EV_BREAK;
	index->event_flag &= ~EV_ERR;
	index->event_flag &= ~EV_RXCHAR;
	index->event_flag &= ~EV_RXFLAG;

	result = SetCommMask( index->hComm, index->event_flag );
	
}

/*----------------------------------------------------------
get_fd()

   accept:      filename
   perform:     find the file descriptor associated with the filename
   return:      fd
   exceptions:
   win32api:    None
   comments:    This is not currently called by anything
----------------------------------------------------------*/

int get_fd( char *filename )
{
	struct termios_list *index = first_tl;

	ENTER( "get_fd" );
	if( !index )
	{
		return -1;
	}

	while( strcmp( index->filename, filename ) )
	{
		index = index->next;
		if( !index->next )
			return( -1 );
	}
	LEAVE( "get_fd" );
	return( index->fd );
}

/*----------------------------------------------------------
get_filename()

   accept:      file descriptor
   perform:     find the filename associated with the file descriptor
   return:      the filename associated with the fd
   exceptions:  None
   win32api:    None
   comments:    This is not currently called by anything
----------------------------------------------------------*/

char *get_filename( int fd )
{
	struct termios_list *index = first_tl;

	ENTER( "get_filename" );
	if( !index )
		return( "bad" );
	while( index->fd != fd )
	{
		if( index->next == NULL )
			return( "bad" );
		index = index->next;
	}
	LEAVE( "get_filename" );
	return( index->filename );
}

/*----------------------------------------------------------
dump_termios_list()

   accept:      string to print out.
   perform:
   return:
   exceptions:
   win32api:    None
   comments:    used only for debugging eg serial_close()
----------------------------------------------------------*/

void dump_termios_list( char *foo )
{
#ifdef DEBUG
	struct termios_list *index = first_tl;
	printf( "============== %s start ===============\n", foo );
	if ( index )
	{
		printf( "%i filename | %s\n", index->fd, index->filename );
	}
/*
	if ( index->next )
	{
		printf( "%i filename | %s\n", index->fd, index->filename );
	}
*/
	printf( "============== %s end  ===============\n", foo );
#endif
}

/*----------------------------------------------------------
set_errno()

   accept:
   perform:
   return:
   exceptions:
   win32api:    None
   comments:   FIXME
----------------------------------------------------------*/

void set_errno( int error )
{
	my_errno = error;
}

/*----------------------------------------------------------
usleep()

   accept:
   perform:
   return:
   exceptions:
   win32api:    Sleep()
   comments:
----------------------------------------------------------*/

void usleep( unsigned long usec )
{
	Sleep( usec/1000 );
}

/*----------------------------------------------------------
CBR_toB()

   accept:
   perform:
   return:
   exceptions:
   win32api:     None
   comments:
----------------------------------------------------------*/

int CBR_to_B( int Baud )
{
	ENTER( "CBR_to_B" );
	switch ( Baud )
	{

		case 0:			return( B0 );
		case 50:		return( B50 );
		case 75:		return( B75 );
		case CBR_110:		return( B110 );
		case 134:		return( B134 );
		case 150:		return( B150 );
		case 200:		return( B200 );
		case CBR_300:		return( B300 );
		case CBR_600:		return( B600 );
		case CBR_1200:		return( B1200 );
		case 1800:		return( B1800 );
		case CBR_2400:		return( B2400 );
		case CBR_4800:		return( B4800 );
		case CBR_9600:		return( B9600 );
		case CBR_14400:		return( B14400 );
		case CBR_19200:		return( B19200 );
		case CBR_28800:		return( B28800 );
		case CBR_38400:		return( B38400 );
		case CBR_57600:		return( B57600 );
		case CBR_115200:	return( B115200 );
		case CBR_128000:	return( B128000 );
		case CBR_230400:	return( B230400 );
		case CBR_256000:	return( B256000 );
		case CBR_460800:	return( B460800 );
		case CBR_500000:	return( B500000 );
		case CBR_576000:	return( B576000 );
		case CBR_921600:	return( B921600 );
		case CBR_1000000:	return( B1000000 );
		case CBR_1152000:	return( B1152000 );
		case CBR_1500000:	return( B1500000 );
		case CBR_2000000:	return( B2000000 );
		case CBR_2500000:	return( B2500000 );
		case CBR_3000000:	return( B3000000 );
		case CBR_3500000:	return( B3500000 );
		case CBR_4000000:	return( B4000000 );
		default:
			/* assume custom baudrate */
			return( Baud );
	}
}

/*----------------------------------------------------------
B_to_CBR()

   accept:
   perform:
   return:
   exceptions:
   win32api:
   comments:      None
----------------------------------------------------------*/

int B_to_CBR( int Baud )
{
	int ret;
	ENTER( "B_to_CBR" );
	switch ( Baud )
	{
		case 0:		ret = 0;		break;
		case B50:	ret = 50;		break;
		case B75:	ret = 75;		break;
		case B110:	ret = CBR_110;		break;
		case B134:	ret = 134;		break;
		case B150:	ret = 150;		break;
		case B200:	ret = 200;		break;
		case B300:	ret = CBR_300;		break;
		case B600:	ret = CBR_600;		break;
		case B1200:	ret = CBR_1200;		break;
		case B1800:	ret = 1800;		break;
		case B2400:	ret = CBR_2400;		break;
		case B4800:	ret = CBR_4800;		break;
		case B9600:	ret = CBR_9600;		break;
		case B14400:	ret = CBR_14400;	break;
		case B19200:	ret = CBR_19200;	break;
		case B28800:	ret = CBR_28800;	break;
		case B38400:	ret = CBR_38400;	break;
		case B57600:	ret = CBR_57600;	break;
		case B115200:	ret = CBR_115200;	break;
		case B128000:	ret = CBR_128000;	break;
		case B230400:	ret = CBR_230400;	break;
		case B256000: 	ret = CBR_256000;	break;
		case B460800:	ret = CBR_460800;	break;
		case B500000:	ret = CBR_500000;	break;
		case B576000:	ret = CBR_576000;	break;
		case B921600:	ret = CBR_921600;	break;
		case B1000000:	ret = CBR_1000000;	break;
		case B1152000:	ret = CBR_1152000;	break;
		case B1500000:	ret = CBR_1500000;	break;
		case B2000000:	ret = CBR_2000000;	break;
		case B2500000:	ret = CBR_2500000;	break;
		case B3000000:	ret = CBR_3000000;	break;
		case B3500000:	ret = CBR_3500000;	break;
		case B4000000:	ret = CBR_4000000;	break;

		default:
			/* assume custom baudrate */
			return Baud;
	}
	LEAVE( "B_to_CBR" );
	return ret;
}

/*----------------------------------------------------------
bytesize_to_termios()

   accept:
   perform:
   return:
   exceptions:
   win32api:      None
   comments:
----------------------------------------------------------*/

int bytesize_to_termios( int ByteSize )
{
	ENTER( "bytesize_to_termios" );
	switch ( ByteSize )
	{
		case 5: return( CS5 );
		case 6: return( CS6 );
		case 7: return( CS7 );
		case 8:
		default: return( CS8 );
	}
}

/*----------------------------------------------------------
termios_to_bytesize()

   accept:
   perform:
   return:
   exceptions:
   win32api:     None
   comments:
----------------------------------------------------------*/

int termios_to_bytesize( int cflag )
{
	ENTER( "termios_to_bytesize" );
	switch ( cflag & CSIZE )
	{
		case CS5: return( 5 );
		case CS6: return( 6 );
		case CS7: return( 7 );
		case CS8:
		default: return( 8 );
	}
}

/*----------------------------------------------------------
get_dos_port()

   accept:
   perform:
   return:
   exceptions:
   win32api:     None
   comments:
----------------------------------------------------------*/

const char *get_dos_port( char const *name )
{
	ENTER( "get_dos_port" );
	if ( !strcmp( name, "/dev/cua0" ) ) return( "COM1" );
	if ( !strcmp( name, "/dev/cua1" ) ) return( "COM2" );
	if ( !strcmp( name, "/dev/cua2" ) ) return( "COM3" );
	if ( !strcmp( name, "/dev/cua3" ) ) return( "COM4" );
	LEAVE( "get_dos_port" );
	return( ( const char * ) name );
}

/*----------------------------------------------------------
ClearErrors()

   accept:
   perform:      keep track of errors for the eventLoop() (SerialImp.c)
   return:       the return value of ClearCommError()
   exceptions:
   win32api:     ClearCommError()
   comments:
----------------------------------------------------------*/

int ClearErrors( struct termios_list *index, COMSTAT *Stat , int *errNum)
{
	unsigned long ErrCode;
	int ret;

	ret = ClearCommError( index->hComm, &ErrCode, Stat );
	if ( ret == 0 )
	{
		YACK();
		return( ret );
	}
#ifdef DEBUG_ERRORS
	if ( ErrCode )
	{
		printf("%i frame %i %i overrun %i %i  parity %u %i brk %i %i\n",
			(int) ErrCode,
			(int) ErrCode & CE_FRAME,
			index->sis->frame,
			(int) (ErrCode & CE_OVERRUN) | ( ErrCode & CE_RXOVER ),
			index->sis->overrun,
			(int) ErrCode & CE_RXPARITY,
			index->sis->parity,
			(int) ErrCode & CE_BREAK,
			index->sis->brk
		);
	}
#endif /* DEBUG_ERRORS */
	if( ErrCode & CE_FRAME )
	{
		index->sis->frame++;
		*errNum = E_CUSTOM_SERIAL_FRAME;
		ErrCode &= ~CE_FRAME;
	}
#ifdef LIFE_IS_GOOD
  FIXME OVERRUN is spewing
	if( ErrCode & CE_OVERRUN )
	{
		index->sis->overrun++;
		ErrCode &= ~CE_OVERRUN;
	}
	/* should this be here? */
	else if( ErrCode & CE_RXOVER )
	{
		index->sis->overrun++;
		ErrCode &= ~CE_OVERRUN;
	}
#endif /* LIFE_IS_GOOD */
	if( ErrCode & CE_RXPARITY )
	{
		index->sis->parity++;
		*errNum = E_CUSTOM_SERIAL_RX_PARITY;
		ErrCode &= ~CE_RXPARITY;
	}
	if( ErrCode & CE_BREAK )
	{
		index->sis->brk++;
		*errNum = E_CUSTOM_SERIAL_BREAK;
		ErrCode &= ~CE_BREAK;
	}
	return( ret );
}
// CARDON note: FillDCB is not used anymore
//
///*----------------------------------------------------------
//FillDCB()
//
//   accept:
//   perform:
//   return:
//   exceptions:
//   win32api:     GetCommState(),  SetCommState(), SetCommTimeouts()
//   comments:
//----------------------------------------------------------*/
//
//BOOL FillDCB( DCB *dcb, unsigned long *hCommPort, COMMTIMEOUTS Timeout )
//{
//
//	ENTER( "FillDCB" );
//	dcb->DCBlength = sizeof( dcb );
//	if ( !GetCommState( hCommPort, dcb ) )
//	{
//		report( "GetCommState\n" );
//		return( -1 );
//	}
//	dcb->BaudRate        = CBR_9600 ;
//	dcb->ByteSize        = 8;
//	dcb->Parity          = NOPARITY;
//	dcb->StopBits        = ONESTOPBIT;
//	dcb->fDtrControl     = DTR_CONTROL_ENABLE;
//	dcb->fRtsControl     = RTS_CONTROL_ENABLE;
//	dcb->fOutxCtsFlow    = FALSE;
//	dcb->fOutxDsrFlow    = FALSE;
//	dcb->fDsrSensitivity = FALSE;
//	dcb->fOutX           = FALSE;
//	dcb->fInX            = FALSE;
//	dcb->fTXContinueOnXoff = FALSE;
//	dcb->XonChar         = 0x11;
//	dcb->XoffChar        = 0x13;
//	dcb->XonLim          = 0;
//	dcb->XoffLim         = 0;
//	dcb->fParity = TRUE;
//	if ( EV_BREAK|EV_CTS|EV_DSR|EV_ERR|EV_RING|( EV_RLSD & EV_RXFLAG ) )
//		dcb->EvtChar = '\n';
//	else dcb->EvtChar = '\0';
//	if ( !SetCommState( hCommPort, dcb ) )
//	{
//		report( "SetCommState\n" );
//		YACK();
//		return( -1 );
//	}
//	if ( !SetCommTimeouts( hCommPort, &Timeout ) )
//	{
//		YACK();
//		report( "SetCommTimeouts\n" );
//		return( -1 );
//	}
//	LEAVE( "FillDCB" );
//	return ( TRUE ) ;
//}

/*----------------------------------------------------------
serial_close()

   accept:
   perform:
   return:
   exceptions:
   win32api:      SetCommMask(), CloseHandle()
   comments:
----------------------------------------------------------*/

int serial_close(struct env_struct* pEnvStruct, int fd )
{
	struct termios_list *index;
	/* char message[80]; */

	ENTER2(pEnvStruct, "serial_close" );
	if( !first_tl || !first_tl->hComm )
	{
		report2(pEnvStruct, "gotit!" );
		return( 0 );
	}
	index = find_port( fd );
	if ( !index )
	{
		LEAVE2(pEnvStruct, "serial_close" );
		return -1;
	}
	if (index->readInterruptEvent && !SetEvent(index->readInterruptEvent))
	{
		sprintf(message, "SetEvent failed in serial_close: (%d)\n", GetLastError());
		report2(pEnvStruct,message);
	}

	//wait for any remaining read actions (yes, we use a loop here to keep it simple), so we can free the HANDLE
	int counter = 0;
	index->is_closing = 1;
	while (index->readOngoing && counter < 100) {
		usleep(10000); //10 ms
		counter++; //the counter is a wacthdog in case we forgot to set readOngoing=0 when exiting the read function
	}
	while (index->waitingCommEvent && counter < 100) {
		usleep(10000); //10 ms
		counter++; //the counter is a wacthdog in case we forgot to set readOngoing=0 when exiting the read function
	}

	/* WaitForSingleObject( index->wol.hEvent, INFINITE ); */
/*
	if ( index->hComm != INVALID_HANDLE_VALUE )
	{
		if ( !SetCommMask( index->hComm, EV_RXCHAR ) )
		{
			YACK();
			report( "eventLoop hung\n" );
		}
		CloseHandle( index->hComm );
	}
	else
	{
		sprintf( message, "serial_ close():  Invalid Port Reference for %s\n",
			index->filename );
		report( message );
	}
*/
	if ( index->next  && index->prev )
	{
		index->next->prev = index->prev;
		index->prev->next = index->next;
	}
	else if ( index->prev )
	{
		index->prev->next = NULL;
	}
	else if ( index->next )
	{
		index->next->prev = NULL;
		first_tl = index->next;
	}
	else
		first_tl = NULL;
	if ( index )
	{
		if ( index->rol.hEvent ) CloseHandle( index->rol.hEvent );
		if ( index->wol.hEvent ) CloseHandle( index->wol.hEvent );
		if ( index->sol.hEvent ) CloseHandle( index->sol.hEvent );
		if ( index->hComm ) CloseHandle( index->hComm );
		if ( index->ttyset )   free( index->ttyset );
		if ( index->astruct )  free( index->astruct );
		if ( index->sstruct )  free( index->sstruct );
		if ( index->sis )      free( index->sis );
		/* had problems with strdup
		if ( index->filename ) free( index->filename );
		*/
		//added by cardon
		if (index->readInterruptEvent) {
			CloseHandle(index->readInterruptEvent);
		}
		if (index->monitorInterruptEvent) {
			CloseHandle(index->monitorInterruptEvent);
		}


		free( index );
	}
	LEAVE2(pEnvStruct, "serial_close" );
	return 0;
}

/*----------------------------------------------------------
cfmakeraw()

   accept:
   perform:
   return:
   exceptions:
   win32api:     None
   comments:
----------------------------------------------------------*/

void cfmakeraw( struct termios *s_termios )
{
	ENTER( "cfmakeraw" );
	s_termios->c_iflag &= ~( IGNBRK|BRKINT|PARMRK|ISTRIP
		|INLCR|IGNCR|ICRNL|IXON );
	s_termios->c_oflag &= ~OPOST;
	s_termios->c_lflag &= ~( ECHO|ECHONL|ICANON|ISIG|IEXTEN );
	s_termios->c_cflag &= ~( CSIZE|PARENB );
	s_termios->c_cflag |= CS8;
	LEAVE( "cfmakeraw" );
}

/*----------------------------------------------------------
init_termios()

   accept:
   perform:
   return:
   exceptions:
   win32api:
   comments:
----------------------------------------------------------*/

BOOL init_serial_struct( struct serial_struct *sstruct )
{
	ENTER( "init_serial_struct" );

	/*
	use of custom_divisor and baud_base requires access to
	kernel space.  The kernel does try its best if you just
	toss a baud rate at it though.
	*/

	sstruct->custom_divisor = 0;
	sstruct->baud_base = 115200;

	/* not currently used check values before using */

	/* unsigned short */

	sstruct->close_delay = 0;
	sstruct->closing_wait = 0;
	sstruct->iomem_reg_shift = 0;

	/* int */

	sstruct->type = 0;
	sstruct->line = 0;
	sstruct->irq = 0;
	sstruct->flags = 0;
	sstruct->xmit_fifo_size = 0;
	sstruct->hub6 = 0;

	/* unsigned int */

	sstruct->port = 0;
	sstruct->port_high = 0;

	/* char */

	sstruct->io_type = 0;

	/* unsigned char * */

	sstruct->iomem_base = NULL;

	LEAVE( "init_serial_struct" );
	return TRUE;

}
/*----------------------------------------------------------
init_termios()

   accept:
   perform:
   return:
   exceptions:
   win32api:
   comments:
----------------------------------------------------------*/

BOOL init_termios(struct termios *ttyset )
{
ENTER( "init_termios" );
	if ( !ttyset )
		return FALSE;
	memset( ttyset, 0, sizeof( struct termios ) );
	cfsetospeed( ttyset, B9600 );
	cfmakeraw( ttyset );

	//FIX BY CARDON

	ttyset->c_cc[VINTR] = 0x03;	/* 0: C-c */
	ttyset->c_cc[VQUIT] = 0x1c;	/* 1: C-\ */
	ttyset->c_cc[VERASE] = 0x7f;	/* 2: <del> */
	ttyset->c_cc[VKILL] = 0x15;	/* 3: C-u */
	ttyset->c_cc[VEOF] = 0x04;	/* 4: C-d */
	ttyset->c_cc[VTIME] = 0;	/* 5: read timeout */
	ttyset->c_cc[VMIN] = 1;		/* 6: read returns after this
						many bytes */ 
	ttyset->c_cc[VSUSP] = 0x1a;	/* 10: C-z */
	ttyset->c_cc[VEOL] = '\r';	/* 11: */
	ttyset->c_cc[VREPRINT] = 0x12;	/* 12: C-r */
/*
	ttyset->c_cc[VDISCARD] = 0x;	   13: IEXTEN only
*/
	ttyset->c_cc[VWERASE] = 0x17;	/* 14: C-w */
	ttyset->c_cc[VLNEXT] = 0x16;	/* 15: C-w */
	ttyset->c_cc[VEOL2] = '\n';	/* 16: */
	
	//added by Cardon
	ttyset->c_cflag |= HARDWARE_FLOW_CONTROL;
	ttyset->c_iflag |= IXANY; // fTXContinueOnXoff
	ttyset->c_iflag &= ~IXOFF;
	ttyset->c_iflag &= ~IXON;


	LEAVE( "init_termios" );
	return TRUE;
	/* default VTIME = 0, VMIN = 1: read blocks forever until one byte */
}

/*----------------------------------------------------------
port_opened()

   accept:
   perform:
   return:
   exceptions:
   win32api:     None
   comments:
----------------------------------------------------------*/

int port_opened( const char *filename )
{
	struct termios_list *index = first_tl;

	ENTER( "port_opened" );
	if ( ! index )
		return 0;
	if( !strcmp( index->filename, filename ) )
		return index->fd;
	while ( index->next )
	{
		index = index->next;
		if( !strcmp( index->filename, filename ) )
			return index->fd;
	}
	LEAVE( "port_opened" );
	return 0;
}

/*----------------------------------------------------------
open_port()

   accept:
   perform:
   return:
   exceptions:
   win32api:   CreateFile(), SetupComm(), CreateEvent()
   comments:
	FILE_FLAG_OVERLAPPED allows one to break out the select()
	so RXTXPort.close() does not hang.

	The setDTR() and setDSR() are the functions that noticed
	to be blocked in the java close.  Basically ioctl(TIOCM[GS]ET)
	are where it hangs.

	FILE_FLAG_OVERLAPPED also means we need to create valid OVERLAPPED
	structure in Serial_select.
----------------------------------------------------------*/

int open_port( struct termios_list *port )
{
	struct env_struct* pEnvStruct = port->pEnvStruct;
	ENTER2(pEnvStruct, "open_port" );
	SLEEP_AND_TRACE("open_port:CreateFile");
	port->hComm = CreateFile( port->filename,
		GENERIC_READ | GENERIC_WRITE,
		0,
		0,
		OPEN_EXISTING,
		FILE_FLAG_OVERLAPPED,
		0
	);
	if ( port->hComm == INVALID_HANDLE_VALUE )
	{
		YACK();
		set_errno( EINVAL );
		report2(pEnvStruct, "open_port: INVALID_HANDLE_VALUE");
/*
		printf( "serial_open failed %s\n", port->filename );
*/
		return -1;
	}

	SLEEP_AND_TRACE("open_port:SetupComm");
	if( !SetupComm( port->hComm, 2048, 1024 ) )
	{
		YACK();
		report2(pEnvStruct, "open_port: SetupComm failed");
		return -1;
	}

	memset( &port->rol, 0, sizeof( OVERLAPPED ) );
	memset( &port->wol, 0, sizeof( OVERLAPPED ) );
	memset( &port->sol, 0, sizeof( OVERLAPPED ) );

	port->rol.hEvent = CreateEvent( NULL, TRUE, FALSE, NULL );

	if ( !port->rol.hEvent )
	{
		YACK();
		report2(pEnvStruct, "Could not create read overlapped\n" );
		goto fail;
	}

	port->sol.hEvent = CreateEvent( NULL, TRUE, FALSE, NULL );

	if ( !port->sol.hEvent )
	{
		YACK();
		report2(pEnvStruct, "Could not create select overlapped\n" );
		goto fail;
	}
	port->wol.hEvent = CreateEvent( NULL, TRUE, FALSE, NULL );

	if ( !port->wol.hEvent )
	{
		YACK();
		report2(pEnvStruct, "Could not create write overlapped\n" );
		goto fail;
	}

	//added by Cardon
	port->readInterruptEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
	if (!port->readInterruptEvent) {
		report2(pEnvStruct,"Could not create readInterruptEvent\n");
		goto fail;
	}
	port->monitorInterruptEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
	if (!port->monitorInterruptEvent) {
		report2(pEnvStruct,"Could not create monitorInterruptEvent\n");
		goto fail;
	}


	LEAVE2(pEnvStruct, "open_port" );
	return( 0 );
fail:
	return( -1 );
}

/*----------------------------------------------------------
termios_list()

   accept:       fd which is a fake # for the port assigned when the port
		 is opened
   perform:      walk through a double linked list to see if the given
		 fd is in any of the termios_list members.
   return:       the termios_list if it is found.
		 NULL if no matches are found.
   exceptions:   None
   win32api:     None
   comments:
----------------------------------------------------------*/

struct termios_list *find_port( int fd )
{

	char message[80];
	struct termios_list *index = first_tl;

	ENTER( "find_port" );
	if ( fd <= 0 || !first_tl ) goto fail;

	while( index->fd )
	{
		if ( index->fd == fd )
		{
			LEAVE( "find_port" );
			return index;
		}
		if ( !index->next )
			break;
		index = index->next;
	}
fail:
	sprintf( message, "No info known about the port. %i\n", fd );
	report( message );
	set_errno( EBADF );
	LEAVE( "find_port" );
	return NULL;
}

/*----------------------------------------------------------
get_free_fd()

   accept:
   perform:
   return:
   exceptions:
   win32api:       None
   comments:
----------------------------------------------------------*/

int get_free_fd(void)
{
	int next, last;
	struct termios_list *index = first_tl;

	ENTER( "get_free_fd" );
	if ( !index )
	{
		return( 1 );
	}
	if ( !index->fd )
	{
		report( "!index->fd\n" );
		return( 1 );
	}
	if ( index->fd > 1)
	{
		first_tl = index;
		return ( 1 );
	}

	last = index->fd;

	while( index->next )
	{
		next = index->next->fd;
		if ( next !=  last + 1 )
		{
			return( last + 1 );

		}
		index = index->next;
		last = next;
	}
	LEAVE( "get_free_fd" );
	return( index->fd + 1 );
}

/*----------------------------------------------------------
add_port()

   accept:
   perform:
   return:
   exceptions:
   win32api:      None
   comments:
----------------------------------------------------------*/

struct termios_list *add_port(struct env_struct* pEnvStruct, const char *filename )
{
	struct termios_list *index = first_tl;
	struct termios_list *port;

	ENTER2(pEnvStruct, "add_port" );

	port = malloc( sizeof( struct termios_list ) );
	if (!port)
		goto fail;
	memset(port, 0, sizeof(struct termios_list));

	port->pEnvStruct = pEnvStruct;
	
	port->ttyset = malloc( sizeof( struct termios ) );
	if( ! port->ttyset )
		goto fail;
	memset( port->ttyset, 0, sizeof( struct termios ) );

	port->sstruct = malloc( sizeof( struct serial_struct ) );
	if( ! port->sstruct )
		goto fail;
	memset( port->sstruct, 0, sizeof( struct serial_struct ) );
	port->sis = malloc( sizeof( struct serial_icounter_struct ) );
	if( ! port->sis )
		goto fail;
	memset( port->sis, 0, sizeof( struct serial_icounter_struct ) );

/*	FIXME  the async_struct is being defined by mingw32 headers?
	port->astruct = malloc( sizeof( struct async_struct ) );
	if( ! port->astruct )
		goto fail;
	memset( port->astruct, 0, sizeof( struct async_struct ) );
*/
	port->MSR = 0;

	strcpy(port->filename, filename );
	/* didnt free well? strdup( filename ); */
	if( ! port->filename )
		goto fail;

	port->fd = get_free_fd();


	if ( !first_tl )
	{
		port->prev = NULL;
		port->next = NULL;
		first_tl = port;
	}
	else
	{
		while ( ( index->fd < port->fd ) && index->next )
			index = index->next;
		if ( index->fd > port->fd )
		{
			/* inserting previously closed fd */
			if ( index->prev )
			{
				/* adding between list items */
				port->prev = index->prev;
				port->next = index;
				index->prev->next = port;
				index->prev = port;
			} 
			else
			{
				/* adding as first item in list */
				port->prev = NULL;
				port->next = index;
				index->prev = port;
				first_tl = port;
			}

		}
		else
		{
			/* adding to end of list */
			port->prev = index;
			port->next = NULL;
			index->next = port;
		}
	}
	LEAVE2(pEnvStruct, "add_port" );
	
	return port;

fail:
	report2(pEnvStruct, "add_port:  Out Of Memory\n");
	if ( port->ttyset )   free( port->ttyset );
	if ( port->astruct )  free( port->astruct );
	if ( port->sstruct )  free( port->sstruct );
	if ( port->sis )      free( port->sis );
	/* had problems with strdup
	if ( port->filename ) free( port->filename );
	*/
	if ( port ) free( port );
	return port;
}

/*----------------------------------------------------------
check_port_capabilities()

   accept:
   perform:
   return:
   exceptions:
   win32api:      GetCommProperties(), GetCommState()
   comments:
----------------------------------------------------------*/

int check_port_capabilities( struct termios_list *index )
{
	COMMPROP cp;
	DCB	dcb;
	char message[80];

	ENTER2(index->pEnvStruct, "check_port_capabilities" );
	/* check for capabilities */
	GetCommProperties( index->hComm, &cp );
	if ( !( cp.dwProvCapabilities & PCF_DTRDSR ) )
	{
		sprintf( message,
			"%s: no DTR & DSR support\n", index->filename );
		report2(index->pEnvStruct, message );
	}
	if ( !( cp.dwProvCapabilities & PCF_RLSD ) )
	{
		sprintf( message, "%s: no carrier detect (RLSD) support\n",
			index->filename );
		report2(index->pEnvStruct, message );
	}
	if ( !( cp.dwProvCapabilities & PCF_RTSCTS ) )
	{
		sprintf( message,
			"%s: no RTS & CTS support\n", index->filename );
		report2(index->pEnvStruct, message );
	}
	if ( !( cp.dwProvCapabilities & PCF_TOTALTIMEOUTS ) )
	{
		sprintf( message, "%s: no timeout support\n", index->filename );
		report2(index->pEnvStruct, message );
	}
	if ( !GetCommState( index->hComm, &dcb ) )
	{
		YACK();
		report2(index->pEnvStruct, "GetCommState failed\n" );
		return -1;
	}
	LEAVE2(index->pEnvStruct, "check_port_capabilities" );
	return 0;

}

/*----------------------------------------------------------
serial_open()

   accept:
   perform:
   return:
   exceptions:
   win32api:    None
   comments:
----------------------------------------------------------*/

int serial_open(struct env_struct* pEnvStruct, const char *filename, int flags, ... )
{
	struct termios_list *index;
	char message[80];
	wchar_t message2[80];

	ENTER2(pEnvStruct, "serial_open" );
	
	if ( port_opened( filename ) )
	{
		report2(pEnvStruct, "serial_open:Port is already opened." );
		return( -1 );
	}
	index = add_port(pEnvStruct, filename );
	if( !index )
	{
		report2(pEnvStruct, "serial_open: !index\n" );
		return( -1 );
	}

	index->interrupt = 0;
	index->tx_happened = 0;
	if ( open_port( index ) )
	{
		sprintf( message, "serial_open():  Invalid Port Reference for %s\n",
			filename );
		//report( message );
		report2(pEnvStruct, message);
		serial_close(pEnvStruct, index->fd );
		return -1;
	}

	if( check_port_capabilities( index ) )
	{
		//report( "check_port_capabilites!" );
		report2(pEnvStruct, "check_port_capabilites failed");
		serial_close(pEnvStruct, index->fd );
		return -1;
	}

	init_termios( index->ttyset );
	init_serial_struct( index->sstruct );

	/* set default condition */
	//FIX CARDON: done later during set port params
	//tcsetattr( index->fd, 0, index->ttyset );

	/* if opened with non-blocking, then operating non-blocking */
	if ( flags & O_NONBLOCK )
		index->open_flags = O_NONBLOCK;
	else
		index->open_flags = 0;


	if( !first_tl->hComm )
	{
		sprintf( message, "open():  Invalid Port Reference for %s\n",
			index->filename );
		//report( message );
		report2(pEnvStruct, message);
	}
	if (first_tl->hComm == INVALID_HANDLE_VALUE) {

		//report("serial_open: INVALID_HANDLE_VALUE\n");
		report2(pEnvStruct, "serial_open: INVALID_HANDLE_VALUE");

	};
	LEAVE2(pEnvStruct, "serial_open" );
	return( index->fd );
}


/*----------------------------------------------------------
serial_write()

   accept:
   perform:
   return:
   exceptions:
   win32api:     WriteFile(), GetLastError(),
                 WaitForSingleObject(),  GetOverlappedResult(),
                 FlushFileBuffers(), Sleep()
   comments:
----------------------------------------------------------*/

int serial_write(struct env_struct* pEnvStruct, int fd, char *Str, int length )
{
	unsigned long nBytes;
	struct termios_list *index;
	/* COMSTAT Stat; */
	int old_flag;

	ENTER2(pEnvStruct, "serial_write" );

	if ( fd <= 0 )
	{
		return 0;
	}
	index = find_port( fd );
	if ( !index )
	{
		LEAVE2(pEnvStruct, "serial_write");
		return -1;
	}
	old_flag = index->event_flag;
/*
	index->event_flag &= ~EV_TXEMPTY;
	SetCommMask( index->hComm, index->event_flag );
	index->tx_happened = 1;
*/
	index->wol.Offset = index->wol.OffsetHigh = 0;
	ResetEvent( index->wol.hEvent );
	if ( !WriteFile( index->hComm, Str, length, &nBytes, &index->wol ) )
	{
		WaitForSingleObject( index->wol.hEvent,100 );
		if ( GetLastError() != ERROR_IO_PENDING )
		{
			/* ClearErrors( index, &Stat ); */
			report2(pEnvStruct, "serial_write error\n" );
			/* report("Condition 1 Detected in write()\n"); */
			YACK();
			errno = EIO; //Input output error
			nBytes=-1;
			goto end;
		}
		/* This is breaking on Win2K, WinXP for some reason */
		else while( !GetOverlappedResult( index->hComm, &index->wol,
					&nBytes, TRUE ) )
		{
			if ( GetLastError() != ERROR_IO_INCOMPLETE )
			{
				/* report("Condition 2 Detected in write()\n"); */
				YACK();
				errno = EIO; //Input output error
				nBytes = -1;
				goto end;
				/* ClearErrors( index, &Stat ); */
			}
		}
	}
	else
	{
		/* Write finished synchronously.  That is ok!
		 * I have seen this with USB to Serial
		 * devices like TI's.
		 */
	}
end:
	/* FlushFileBuffers( index->hComm ); */
	index->event_flag |= EV_TXEMPTY;
	/* ClearErrors( index, &Stat ); */
	SetCommMask( index->hComm, index->event_flag );
	/* ClearErrors( index, &Stat ); */
	index->event_flag = old_flag;
	index->tx_happened = 1;
	LEAVE2(pEnvStruct, "serial_write" );
	return nBytes;
}

/*----------------------------------------------------------
serial_read()

   accept:
   perform:
   return:
   exceptions:
   win32api:      ReadFile(), GetLastError(), WaitForSingleObject()
                  GetOverLappedResult()
   comments:    If setting errno make sure not to use EWOULDBLOCK
                In that case use EAGAIN.  See SerialImp.c:testRead()

Comment of Cardon:
	return -1 on error, see https://man7.org/linux/man-pages/man2/read.2.html
----------------------------------------------------------*/

int serial_read(struct env_struct* pEnvStruct, int fd, void *vb, int size )
{
	long start, now;
	unsigned long nBytes = 0, total = 0, error;
	/* unsigned long waiting = 0; */
	int err, vmin;
	struct termios_list *index;
	char message[80];
	COMSTAT stat;
	clock_t c;
	unsigned char *dest = vb;

	//added by Cardon
	HANDLE ghEvents[2]; //array that contains both events 
	


	start = GetTickCount();
	ENTER2(pEnvStruct, "serial_read" );

	if ( fd <= 0 )
	{
		return 0;
	}
	index = find_port( fd );
	if ( !index )
	{
		LEAVE( "serial_read" );
		return -1;
	}

	/* FIXME: CREAD: without this, data cannot be read
	   FIXME: PARMRK: mark framing & parity errors
	   FIXME: IGNCR: ignore \r
	   FIXME: ICRNL: convert \r to \n
	   FIXME: INLCR: convert \n to \r
	*/


// COMMENTED BY CARDON
//	
//	if ( index->open_flags & O_NONBLOCK  )
//	{
//		int ret;
//		vmin = 0;
//		/* pull mucho-cpu here? */
//		do {
//#ifdef DEBUG_VERBOSE
//			report( "vmin=0\n" );
//#endif /* DEBUG_VERBOSE */
//			ret = ClearErrors( index, &stat);
//			/* we should use -1 instead of 0 for disabled timeout */
//			now = GetTickCount();
//			if ( index->ttyset->c_cc[VTIME] &&
//				now-start >= (index->ttyset->c_cc[VTIME]*100)) {
///*
//				sprintf( message, "now = %i start = %i time = %i total =%i\n", now, start, index->ttyset->c_cc[VTIME]*100, total);
//				report( message );
//*/
//				return total;	/* read timeout */
//			}
//		} while( stat.cbInQue < size && size > 1 );
//	}
//	else
//	{
//		/* VTIME is in units of 0.1 seconds */
//
//#ifdef DEBUG_VERBOSE
//		report( "vmin!=0\n" );
//#endif /* DEBUG_VERBOSE */
//		vmin = index->ttyset->c_cc[VMIN];
//
//		c = clock() + index->ttyset->c_cc[VTIME] * CLOCKS_PER_SEC / 10;
//		do {
//			error = ClearErrors( index, &stat);
//			usleep(1000);
//		} while ( c > clock() );
//
//	}


	//added by cardon
	//fill events to wait
	ghEvents[0] = index->rol.hEvent;
	ghEvents[1] = index->readInterruptEvent;
	index->readOngoing = 1;

	if (index->is_closing) {
		index->readOngoing = 0;
		//errno == EINTR;
		LEAVE("serial_read");
		return 0;
	}

	total = 0;
	while ( 1 )  //Cardon: loop with a high timeout set with SetCommTimeouts
	{
		nBytes = 0;
		index->rol.Offset = index->rol.OffsetHigh = 0;
		ResetEvent( index->rol.hEvent );
		
		if (ReadFile(index->hComm, dest + total, size, &nBytes, &index->rol)) {
			// read completed immediately
			size -= nBytes;
			total += nBytes;
			index->readOngoing = 0;
			LEAVE2(pEnvStruct,"serial_read");
			return total;
		} else {
			switch ( GetLastError() ){
				case ERROR_BROKEN_PIPE:
					report2(pEnvStruct, "ERROR_BROKEN_PIPE\n ");
					index->readOngoing = 0;
					LEAVE2(pEnvStruct,"serial_read");
					return -1;

				case ERROR_MORE_DATA:
					report2(pEnvStruct, "ERROR_MORE_DATA: More data can be read.\n" );
					//more data can be read. no worry.
					size -= nBytes;
					total += nBytes;
					index->readOngoing = 0;
					LEAVE2(pEnvStruct,"serial_read");
					return total;

				case ERROR_IO_PENDING:
					DWORD dwRes;
					dwRes = WaitForMultipleObjects(
						2, // number of objects in array 
						ghEvents, //array of objects 
						FALSE, //wait for any object
						INFINITE); //wait for ever
					switch (dwRes) {
							// ghEvents[0] was signaled (the read operation completed)
						case WAIT_OBJECT_0 + 0:
							if (!GetOverlappedResult(
								index->hComm,
								&index->rol,
								&nBytes,
								FALSE  // don't wait
							)) {
								// Error in communications; report it.
								DWORD lastError = GetLastError();
								if (lastError =
									ERROR_IO_INCOMPLETE) {
									//the operation is not completed. (timeout), loop
									break;
								} else {
									
									int errNum = 0;
									//clear errors
									ClearErrors(
										index,
										&stat, 
										&errNum);
									//FIX ME: get type of error 
									sprintf(message, "serial_read error. Last-error code: %d\n", lastError);
									report2(pEnvStruct,message);
									YACK();
									errno = EIO;
									if (errNum > 0) {
										errno = errNum;
									}
									index->readOngoing = 0;
									LEAVE2(pEnvStruct,"serial_read");
									return -1;
								}
							}
							size -= nBytes;
							total += nBytes;
							sprintf(message, "serial_read:number of bytes read=%d\n", nBytes);
							report2(pEnvStruct,message);
							index->readOngoing = 0;
							LEAVE("serial_read");
							return total;
	
						// ghEvents[1] was signaled (the read operation is interrupted // returns 0 byte - no error)
						case WAIT_OBJECT_0 + 1:
							sprintf(message, "serial_read:read was interrupted.\n");
							report2(pEnvStruct,message);
							index->readOngoing = 0;
							LEAVE2(pEnvStruct,"serial_read");
							//errno == EINTR;
							return 0;

						case WAIT_TIMEOUT:
							//should not happen because we set INFINITE. Ignore
							sprintf(message, "serial_read:unexpected wait timeout.\n");
							report2(pEnvStruct,message);
							index->readOngoing = 0;
							LEAVE2(pEnvStruct,"serial_read");
							return -1;

						default:
							// Error in the WaitForMultipleObjects; abort.
							sprintf(message, "serial_read:Error in the WaitForMultipleObjects;.\n");
							report2(pEnvStruct,message);
							index->readOngoing = 0;
							LEAVE2(pEnvStruct,"serial_read");
							return -1;
					}
			}
		}
	}



//backup of original code:

//	total = 0;
//	while (size > 0)
//	{
//		nBytes = 0;
//		/* ret = ClearErrors( index, &stat); */
//
//		index->rol.Offset = index->rol.OffsetHigh = 0;
//		ResetEvent(index->rol.hEvent);
//
//		err = ReadFile(index->hComm, dest + total, size, &nBytes, &index->rol);
//
//#ifdef DEBUG_VERBOSE
//		/* warning Roy Rogers! */
//		sprintf(message, " ========== ReadFile = %i %s\n",
//			(int)nBytes, (char*)dest + total);
//		report(message);
//#endif /* DEBUG_VERBOSE */
//		size -= nBytes;
//		total += nBytes;
//
//		if (!err)
//		{
//			switch (GetLastError())
//			{
//			case ERROR_BROKEN_PIPE:
//				report("ERROR_BROKEN_PIPE\n ");
//				nBytes = 0;
//				break;
//			case ERROR_MORE_DATA:
//				report("ERROR_MORE_DATA\n");
//				break;
//			case ERROR_IO_PENDING:
//				//CARDON: add here wait multiplie, 
//				while (!GetOverlappedResult(
//					index->hComm,
//					&index->rol,
//					&nBytes,
//					TRUE))
//				{
//					if (GetLastError() !=
//						ERROR_IO_INCOMPLETE)
//					{
//						ClearErrors(
//							index,
//							&stat);
//						return(total);
//					}
//				}
//				size -= nBytes;
//				total += nBytes;
//				sprintf(message, "serial_read:number of bytes read=%d\n", nBytes);
//				report(message);
//				if (size > 0) {
//					now = GetTickCount();
//					sprintf(message, "size > 0: spent=%ld have=%d\n", now - start, index->ttyset->c_cc[VTIME] * 100);
//					report(message);
//					/* we should use -1 for disabled
//					   timouts */
//					if (index->ttyset->c_cc[VTIME] && now - start >= (index->ttyset->c_cc[VTIME] * 100)) {
//						report("TO ");
//						/* read timeout */
//						return total;
//					}
//				}
//				sprintf(message, "end nBytes=%ld] ", nBytes);
//				report(message);
//				report("ERROR_IO_PENDING\n");
//				break;
//			default:
//				YACK();
//				return -1;
//			}
//		}
//		else
//		{
//			ClearErrors(index, &stat);
//			return(total);
//		}
//	}

}

#ifdef asdf
int serial_read( int fd, void *vb, int size )
{
	long start, now;
	unsigned long nBytes = 0, total = 0, error;
	/* unsigned long waiting = 0; */
	int err, vmin;
	struct termios_list *index;
	char message[80];
	COMSTAT Stat;
	clock_t c;
	unsigned char *dest = vb;

	start = GetTickCount();
	ENTER( "serial_read" );

	if ( fd <= 0 )
	{
		printf("1\n");
		return 0;
	}
	index = find_port( fd );
	if ( !index )
	{
		LEAVE( "serial_read 7" );
		errno = EIO;
		printf("2\n");
		return -1;
	}

	/* FIXME: CREAD: without this, data cannot be read
	   FIXME: PARMRK: mark framing & parity errors
	   FIXME: IGNCR: ignore \r
	   FIXME: ICRNL: convert \r to \n
	   FIXME: INLCR: convert \n to \r
	*/

	ClearErrors( index, &Stat );
	if ( index->open_flags & O_NONBLOCK  )
	{
		int ret;
		vmin = 0;
		/* pull mucho-cpu here? */
		do {
#ifdef DEBUG_VERBOSE
			report( "vmin=0\n" );
#endif /* DEBUG_VERBOSE */
			ret = ClearErrors( index, &Stat);
			/* we should use -1 instead of 0 for disabled timeout */
			now = GetTickCount();
			if ( index->ttyset->c_cc[VTIME] &&
				now-start >= (index->ttyset->c_cc[VTIME]*100)) {
/*
				sprintf( message, "now = %i start = %i time = %i total =%i\n", now, start, index->ttyset->c_cc[VTIME]*100, total);
				report( message );
*/
				errno = EAGAIN;
				printf("3\n");
				return -1;	/* read timeout */
			}
		} while( Stat.cbInQue < size && size > 1 );
	}
	else
	{
		/* VTIME is in units of 0.1 seconds */

#ifdef DEBUG_VERBOSE
		report( "vmin!=0\n" );
#endif /* DEBUG_VERBOSE */
		vmin = index->ttyset->c_cc[VMIN];

		c = clock() + index->ttyset->c_cc[VTIME] * CLOCKS_PER_SEC / 10;
		do {
			error = ClearErrors( index, &Stat);
			usleep(1000);
		} while ( c > clock() );

	}

	total = 0;
	while ( size > 0 )
	{
		nBytes = 0;
		/* ret = ClearErrors( index, &Stat); */

		index->rol.Offset = index->rol.OffsetHigh = 0;
		ResetEvent( index->rol.hEvent );

		err = ReadFile( index->hComm, dest + total, size, &nBytes, &index->rol );
#ifdef DEBUG_VERBOSE
	/* warning Roy Rogers! */
		sprintf(message, " ========== ReadFile = %i %s\n",
			( int ) nBytes, (char *) dest + total );
		report( message );
#endif /* DEBUG_VERBOSE */
		size -= nBytes;
		total += nBytes;

		if ( !err )
		{
			switch ( GetLastError() )
			{
				case ERROR_BROKEN_PIPE:
					report( "ERROR_BROKEN_PIPE\n ");
					nBytes = 0;
					break;
				case ERROR_MORE_DATA:
					report( "ERROR_MORE_DATA\n" );
					break;
				case ERROR_IO_PENDING:
					while( ! GetOverlappedResult(
							index->hComm,
							&index->rol,
							&nBytes,
							TRUE ) )
					{
						if( GetLastError() !=
							ERROR_IO_INCOMPLETE )
						{
							ClearErrors(
								index,
								&Stat);
							printf("4\n");
							return( total );
						}
					}
					size -= nBytes;
					total += nBytes;
					if (size > 0) {
						now = GetTickCount();
						sprintf(message, "size > 0: spent=%ld have=%d\n", now-start, index->ttyset->c_cc[VTIME]*100);
						report( message );
						/* we should use -1 for disabled
						   timouts */
						if ( index->ttyset->c_cc[VTIME] && now-start >= (index->ttyset->c_cc[VTIME]*100)) {
							report( "TO " );
							/* read timeout */
							printf("5\n");
							return total;
						}
					}
					sprintf(message, "end nBytes=%ld] ", nBytes);
					report( message );
					report( "ERROR_IO_PENDING\n" );
					break;
				default:
					YACK();
					errno = EIO;
					printf("6\n");
					return -1;
			}
		}
		else
		{
			ClearErrors( index, &Stat);
			printf("7\n");
			return( total );
		}
	}
	LEAVE( "serial_read" );
	ClearErrors( index, &Stat);
	return total;
}
#endif /* asdf */

/*----------------------------------------------------------
cfsetospeed()

   accept:
   perform:
   return:
   exceptions:
   win32api:     None
   comments:
----------------------------------------------------------*/

int cfsetospeed( struct termios *s_termios, speed_t speed )
{
	char message[80];
	ENTER( "cfsetospeed" );
	/* clear baudrate */
	s_termios->c_cflag &= ~CBAUD;
	if ( speed & ~CBAUD )
	{
		sprintf( message, "cfsetospeed: not speed: %#o\n", speed );
		report( message );
		/* continue assuming its a custom baudrate */
		s_termios->c_cflag |= B38400;  /* use 38400 during custom */
		s_termios->c_cflag |= CBAUDEX; /* use CBAUDEX for custom */
	}
	else if( speed )
	{
		s_termios->c_cflag |= speed;
	}
	else
	{
		/* PC blows up with speed 0 handled in Java */
		s_termios->c_cflag |= B9600;
	}
	s_termios->c_ispeed = s_termios->c_ospeed = speed;
	LEAVE( "cfsetospeed" );
	return 1;
}

/*----------------------------------------------------------
cfsetispeed()

   accept:
   perform:
   return:
   exceptions:
   win32api:     None
   comments:
----------------------------------------------------------*/

int cfsetispeed( struct termios *s_termios, speed_t speed )
{
	return cfsetospeed( s_termios, speed );
}

/*----------------------------------------------------------
cfsetspeed()

   accept:
   perform:
   return:
   exceptions:
   win32api:     None
   comments:
----------------------------------------------------------*/

int cfsetspeed( struct termios *s_termios, speed_t speed )
{
	return cfsetospeed( s_termios, speed );
}

/*----------------------------------------------------------
cfgetospeed()

   accept:
   perform:
   return:
   exceptions:
   win32api:     None
   comments:
----------------------------------------------------------*/

speed_t cfgetospeed( struct termios *s_termios )
{
	ENTER( "cfgetospeed");
	return s_termios->c_ospeed;
}

/*----------------------------------------------------------
cfgetispeed()

   accept:
   perform:
   return:
   exceptions:
   win32api:     None
   comments:
----------------------------------------------------------*/

speed_t cfgetispeed( struct termios *s_termios )
{
	ENTER("cfgetospeed" );
	return s_termios->c_ispeed;
}

/*----------------------------------------------------------
termios_to_DCB()

   accept:
   perform:
   return:
   exceptions:
   win32api:     None
   comments:
----------------------------------------------------------*/
int termios_to_DCB( struct termios *s_termios, DCB *dcb )
{
	ENTER( "termios_to_DCB" );
	if ( !(s_termios->c_cflag & CBAUDEX) )
		s_termios->c_ispeed = s_termios->c_ospeed = s_termios->c_cflag & CBAUD;
	dcb->BaudRate        = B_to_CBR( s_termios->c_ispeed );
	dcb->ByteSize = termios_to_bytesize( s_termios->c_cflag );

	if ( s_termios->c_cflag & PARENB )
	{
		if ( s_termios->c_cflag & PARODD
			&& s_termios->c_cflag & CMSPAR )
		{
			dcb->Parity = MARKPARITY;
		}
		else if ( s_termios->c_cflag & PARODD )
		{
			dcb->Parity = ODDPARITY;
		}
		else if ( s_termios->c_cflag & CMSPAR )
		{
			dcb->Parity = SPACEPARITY;
		}
		else
		{
			dcb->Parity = EVENPARITY;
		}
	}
	else
	{
		dcb->Parity = NOPARITY;
	}

	if ( s_termios->c_cflag & CSTOPB )
	{
		if (dcb->ByteSize == 5)
		{
			dcb->StopBits = ONE5STOPBITS;
		} else dcb->StopBits = TWOSTOPBITS;
	}
	else
	{
		dcb->StopBits = ONESTOPBIT;
	}

	if ( s_termios->c_cflag & HARDWARE_FLOW_CONTROL )
	{
#ifdef DEBUG_VERBOSE
		sprintf(message, "enable HARDWARE_FLOW_CONTROL\n");
		report(message);
#endif /* DEBUG_VERBOSE */
		dcb->fRtsControl = RTS_CONTROL_HANDSHAKE;
		dcb->fOutxCtsFlow = TRUE;
	}
	else
	{
		dcb->fRtsControl = RTS_CONTROL_DISABLE;
		dcb->fOutxCtsFlow = FALSE;
	}

	LEAVE( "termios_to_DCB" );
	return 0;
}

/*----------------------------------------------------------
DCB_to_serial_struct()

   accept:
   perform:
   return:
   exceptions:
   win32api:     None
   comments:
----------------------------------------------------------*/
int DCB_to_serial_struct( DCB *dcb, struct serial_struct *sstruct  )
{
	return( 0 );
}
/*----------------------------------------------------------
DCB_to_termios()

   accept:
   perform:
   return:
   exceptions:
   win32api:     None
   comments:
----------------------------------------------------------*/
void DCB_to_termios( DCB *dcb, struct termios *s_termios )
{
	ENTER( "DCB_to_termios" );
	s_termios->c_ispeed = CBR_to_B( dcb->BaudRate );
	s_termios->c_ospeed = s_termios->c_ispeed;
	s_termios->c_cflag |= s_termios->c_ispeed & CBAUD;
	LEAVE( "DCB_to_termios" );
}

/*----------------------------------------------------------
show_DCB()

   accept:
   perform:
   return:
   exceptions:
   win32api:     None
   comments:
----------------------------------------------------------*/
void show_DCB( DCB myDCB )
{

#ifdef DEBUG_HOSED
	char message[80];

	sprintf( message, "DCBlength: %ld\n", myDCB.DCBlength );
	report( message );
	sprintf( "BaudRate: %ld\n", myDCB.BaudRate );
	report( message );
	if ( myDCB.fBinary )
		report( "fBinary\n" );
	if ( myDCB.fParity )
	{
		report( "fParity: " );
		if ( myDCB.fErrorChar )
		{
			sprintf( message, "fErrorChar: %#x\n", myDCB.ErrorChar );
			report( message );
		}
		else
		{
			report( "fErrorChar == false\n" );
		}
	}
	if ( myDCB.fOutxCtsFlow )
		report( "fOutxCtsFlow\n" );
	if ( myDCB.fOutxDsrFlow )
		report( "fOutxDsrFlow\n" );
	if ( myDCB.fDtrControl & DTR_CONTROL_HANDSHAKE );
		report( "DTR_CONTROL_HANDSHAKE\n" );
	if ( myDCB.fDtrControl & DTR_CONTROL_ENABLE );
		report( "DTR_CONTROL_ENABLE\n" );
	if ( myDCB.fDtrControl & DTR_CONTROL_DISABLE );
		report( "DTR_CONTROL_DISABLE\n" );
	if ( myDCB.fDsrSensitivity )
		report( "fDsrSensitivity\n" );
	if ( myDCB.fTXContinueOnXoff )
		report( "fTXContinueOnXoff\n" );
	if ( myDCB.fOutX )
		report( "fOutX\n" );
	if ( myDCB.fInX )
		report( "fInX\n" );
	if ( myDCB.fNull )
		report( "fNull\n" );
	if ( myDCB.fRtsControl & RTS_CONTROL_TOGGLE )
		report( "RTS_CONTROL_TOGGLE\n" );
	if ( myDCB.fRtsControl == 0 )
		report( "RTS_CONTROL_HANDSHAKE ( fRtsControl==0 )\n" );
	if ( myDCB.fRtsControl & RTS_CONTROL_HANDSHAKE )
		report( "RTS_CONTROL_HANDSHAKE\n" );
	if ( myDCB.fRtsControl & RTS_CONTROL_ENABLE )
		report( "RTS_CONTROL_ENABLE\n" );
	if ( myDCB.fRtsControl & RTS_CONTROL_DISABLE )
		report( "RTS_CONTROL_DISABLE\n" );
	if ( myDCB.fAbortOnError )
		report( "fAbortOnError\n" );
	sprintf( message, "XonLim: %d\n", myDCB.XonLim );
	report( message );
	sprintf( message, "XoffLim: %d\n", myDCB.XoffLim );
	report( message );
	sprintf( message, "ByteSize: %d\n", myDCB.ByteSize );
	report( message );
	switch ( myDCB.Parity )
	{
		case EVENPARITY:
			report( "EVENPARITY" );
			break;
		case MARKPARITY:
			report( "MARKPARITY" );
			break;
		case NOPARITY:
			report( "NOPARITY" );
			break;
		case ODDPARITY:
			report( "ODDPARITY" );
			break;
		default:
			sprintf( message,
				"unknown Parity (%#x ):", myDCB.Parity );
			report( message );
			break;
	}
	report( "\n" );
	switch( myDCB.StopBits )
	{
		case ONESTOPBIT:
			report( "ONESTOPBIT" );
			break;
		case ONE5STOPBITS:
			report( "ONE5STOPBITS" );
			break;
		case TWOSTOPBITS:
			report( "TWOSTOPBITS" );
			break;
		default:
			report( "unknown StopBits (%#x ):", myDCB.StopBits );
			break;
	}
	report( "\n" );
	sprintf( message,  "XonChar: %#x\n", myDCB.XonChar );
	report( message );
	sprintf(  message, "XoffChar: %#x\n", myDCB.XoffChar );
	report( message );
	sprintf(  message, "EofChar: %#x\n", myDCB.EofChar );
	report( message );
	sprintf(  message, "EvtChar: %#x\n", myDCB.EvtChar );
	report( message );
	report( "\n" );
#endif /* DEBUG_HOSED */
}

/*----------------------------------------------------------
tcgetattr()

   accept:
   perform:
   return:
   exceptions:
   win32api:    GetCommState(), GetCommTimeouts()
   comments:
----------------------------------------------------------*/

int tcgetattr(struct env_struct* pEnvStruct, int fd, struct termios *s_termios )
{
	DCB myDCB;
	COMMTIMEOUTS timeouts;
	struct termios_list *index;
	char message[80];

	ENTER2(pEnvStruct, "tcgetattr" );
	if ( fd <= 0 )
		return 0;
	index = find_port( fd );
	if ( !index )
	{
		LEAVE2(pEnvStruct, "tcgetattr" );
		return -1;
	}
	
	SLEEP_AND_TRACE("tcgetattr:GetCommState");
	if ( !GetCommState( index->hComm, &myDCB ) )
	{
		sprintf( message, "GetCommState failed\n" );
		report2(pEnvStruct, message );
		return -1;
	}
	memcpy( s_termios, index->ttyset, sizeof( struct termios ) );

	show_DCB( myDCB );


	/***** input mode flags (c_iflag ) ****/
	/* parity check enable */
	if ( myDCB.fParity )
	{
		s_termios->c_iflag |= INPCK;
		s_termios->c_iflag &= ~IGNPAR;
	} else {
		s_termios->c_iflag &= ~INPCK;
		s_termios->c_iflag |= IGNPAR;
	}
	/* FIXME: IGNBRK: ignore break */
	/* FIXME: BRKINT: interrupt on break */

	if ( myDCB.fOutX )
	{
		s_termios->c_iflag |= IXON;
	}
	else
	{
		/* IXON: output start/stop control */
		s_termios->c_iflag &= ~IXON;
	}
	if ( myDCB.fInX )
	{
		s_termios->c_iflag |= IXOFF;
	}
	else
	{
		/* IXOFF: input start/stop control */
		s_termios->c_iflag &= ~IXOFF;
	}

	if ( myDCB.fTXContinueOnXoff )
	{
		s_termios->c_iflag |= IXANY;
	}
	else
	{
		/* IXANY: any char restarts output */
		s_termios->c_iflag &= ~IXANY;
	}
	/* FIXME: IMAXBEL: if input buffer full, send bell */

	/***** control mode flags (c_cflag ) *****/
	/* FIXME: CLOCAL: DONT send SIGHUP on modem disconnect */
	/* FIXME: HUPCL: generate modem disconnect when all has closed or
		exited */
	/* CSTOPB two stop bits ( otherwise one) */
	if ( myDCB.StopBits == TWOSTOPBITS )
	{
		s_termios->c_cflag |= CSTOPB;
	} else if ( myDCB.StopBits == ONE5STOPBITS )
	{
		s_termios->c_cflag |= CSTOPB;
		s_termios->c_cflag |= CS5;
	} else if ( myDCB.StopBits == ONESTOPBIT )
	{
		s_termios->c_cflag &= ~CSTOPB;
	}

	/* PARENB enable parity bit */
	s_termios->c_cflag &= ~( PARENB | PARODD | CMSPAR );
	myDCB.fParity = 1;
	if( myDCB.fParity )
	{
		report2(pEnvStruct, "tcgetattr getting parity\n" );
		s_termios->c_cflag |= PARENB;
		if ( myDCB.Parity == MARKPARITY )
		{
			s_termios->c_cflag |= ( PARODD | CMSPAR );
		}
		else if ( myDCB.Parity == SPACEPARITY )
		{
			s_termios->c_cflag |= CMSPAR;
		}
		else if ( myDCB.Parity == ODDPARITY )
		{
			report2(pEnvStruct, "ODDPARITY\n" );
			s_termios->c_cflag |= PARODD;
		}
		else if ( myDCB.Parity == EVENPARITY )
		{
			report2(pEnvStruct, "EVENPARITY\n" );
			s_termios->c_cflag &= ~PARODD;
		}
		else if ( myDCB.Parity == NOPARITY )
		{
			s_termios->c_cflag &= ~(PARODD | CMSPAR | PARENB);
		}
	} else
	{
		s_termios->c_cflag &= ~PARENB;
	}
	/* CSIZE */
	s_termios->c_cflag |= bytesize_to_termios( myDCB.ByteSize );
	/* HARDWARE_FLOW_CONTROL: hardware flow control */
	if (( myDCB.fOutxCtsFlow == TRUE ) ||
            ( myDCB.fRtsControl == RTS_CONTROL_HANDSHAKE))
	{
		s_termios->c_cflag |= HARDWARE_FLOW_CONTROL;
	}
        else
	{
		s_termios->c_cflag &= ~HARDWARE_FLOW_CONTROL;
	}
	/* MDMBUF: carrier based flow control of output */
	/* CIGNORE: tcsetattr will ignore control modes & baudrate */

	/***** NOT SUPPORTED: local mode flags (c_lflag) *****/
	/* ICANON: canonical (not raw) mode */
	/* ECHO: echo back to terminal */
	/* ECHOE: echo erase */
	/* ECHOPRT: hardcopy echo erase */
	/* ECHOK: show KILL char */
	/* ECHOKE: BSD ECHOK */
	/* ECHONL: ICANON only: echo newline even with no ECHO */
	/* ECHOCTL: if ECHO, then control-A are printed as '^A' */
	/* ISIG: recognize INTR, QUIT & SUSP */
	/* IEXTEN: implmentation defined */
	/* NOFLSH: dont clear i/o queues on INTR, QUIT or SUSP */
	/* TOSTOP: background process generate SIGTTOU */
	/* ALTWERASE: alt-w erase distance */
	/* FLUSHO: user DISCARD char */
	/* NOKERNINFO: disable STATUS char */
	/* PENDIN: input line needsd reprinting, set by REPRINT char */
	/***** END - NOT SUPPORTED *****/

	/***** control characters (c_cc[NCCS] ) *****/

	SLEEP_AND_TRACE("tcgetattr:GetCommTimeouts");
	if ( !GetCommTimeouts( index->hComm, &timeouts ) )
	{
		YACK();
		report2(pEnvStruct, "GetCommTimeouts failed\n" );
		return -1;
	}

	//ignore 
	//s_termios->c_cc[VTIME] = timeouts.ReadTotalTimeoutConstant/100;
/*
	handled in SerialImp.c?
	s_termios->c_cc[VMIN] = ?
*/

	s_termios->c_cc[VSTART] = myDCB.XonChar;
	s_termios->c_cc[VSTOP] = myDCB.XoffChar;
	s_termios->c_cc[VEOF] = myDCB.EofChar;

#ifdef DEBUG_VERBOSE
	sprintf( message,
		"tcgetattr: VTIME:%d, VMIN:%d\n", s_termios->c_cc[VTIME],
		s_termios->c_cc[VMIN] );
	report( message );
#endif /* DEBUG_VERBOSE */

	/***** line discipline ( c_line ) ( == c_cc[33] ) *****/

	DCB_to_termios( &myDCB, s_termios ); /* baudrate */
	LEAVE( "tcgetattr" );
	return 0;
}

/*
	`TCSANOW'
		Make the change immediately.

	`TCSADRAIN'
		Make the change after waiting until all queued output has
		been written.  You should usually use this option when
		changing parameters that affect output.

	`TCSAFLUSH'
		This is like `TCSADRAIN', but also discards any queued input.

	`TCSASOFT'
		This is a flag bit that you can add to any of the above
		alternatives.  Its meaning is to inhibit alteration of the
		state of the terminal hardware.  It is a BSD extension; it is
		only supported on BSD systems and the GNU system.

		Using `TCSASOFT' is exactly the same as setting the `CIGNORE'
		bit in the `c_cflag' member of the structure TERMIOS-P points
		to.  *Note Control Modes::, for a description of `CIGNORE'.
*/

/*----------------------------------------------------------
tcsetattr()

   accept:
   perform:
   return:
   exceptions:
   win32api:     GetCommState(), GetCommTimeouts(), SetCommState(),
                 SetCommTimeouts()
   comments:
----------------------------------------------------------*/
int tcsetattr(struct env_struct* pEnvStruct, int fd, int when, struct termios *s_termios )
{
	int vtime;
	DCB dcb;
	COMMTIMEOUTS timeouts;
	struct termios_list *index;

	ENTER2(pEnvStruct, "tcsetattr" );
	if ( fd <= 0 )
		return 0;
	index = find_port( fd );
	if ( !index )
	{
		LEAVE2(pEnvStruct, "tcsetattr" );
		return -1;
	}
	fflush( stdout );
	if ( s_termios->c_lflag & ICANON )
	{
		report2(pEnvStruct, "tcsetattr: no canonical mode support\n" );
		/* and all other c_lflags too */
		return -1;
	}

	SLEEP_AND_TRACE("tcsetattr:GetCommState");
	if ( !GetCommState( index->hComm, &dcb ) )
	{
		YACK();
		report2(pEnvStruct, "tcsetattr:GetCommState\n" );
		return -1;
	}

	//FIXME CARDON
	//force default values for hardware control from the beginning
/*	myDCB.fOutxCtsFlow = TRUE;
	myDCB.fOutxDsrFlow = FALSE;
	myDCB.fRtsControl == RTS_CONTROL_HANDSHAKE;
	myDCB.fTXContinueOnXoff = TRUE;
	myDCB.fDtrControl = DTR_CONTROL_DISABLE;
	myDCB.fDsrSensitivity = FALSE;
	myDCB.fOutX = FALSE;
	myDCB.fInX = FALSE;
*/
	
	SLEEP_AND_TRACE("tcsetattr:GetCommTimeouts");
	if ( !GetCommTimeouts( index->hComm, &timeouts ) )
	{
		YACK();
		report2(pEnvStruct, "tcsetattr:GetCommTimeouts failed" );
		return -1;
	}

	/*** control flags, c_cflag **/
	if ( !( s_termios->c_cflag & CIGNORE ) ) 
		//CIGNORE: If this bit is set, it says to ignore the control modes and line speed values entirely. 
	{
		dcb.fParity=1;
		/* CIGNORE: ignore control modes and baudrate */
		/* baudrate */
		if ( termios_to_DCB( s_termios, &dcb ) < 0 ) return -1;
	}
	else
	{
	}

	/*** input flags, c_iflag **/
/*  This is wrong.  It disables Parity  FIXME
	if( ( s_termios->c_iflag & INPCK ) && !( s_termios->c_iflag & IGNPAR ) )
	{
		dcb.fParity = TRUE;
	} else
	{
		dcb.fParity = FALSE;
	}
*/
	/* not in win95?
	   Some years later...
	   eww..  FIXME This is used for changing the Parity
	   error character

	   I think this code is hosed.  See VEOF below

	   Trent
	*/

	if ( s_termios->c_iflag & ISTRIP ) dcb.fBinary = FALSE;
	/* ISTRIP: strip to seven bits */
	else dcb.fBinary = TRUE;

	/* FIXME: IGNBRK: ignore break */
	/* FIXME: BRKINT: interrupt on break */
	if ( s_termios->c_iflag & IXON )
	{
		dcb.fOutX = TRUE;
	}
	else
	{
		dcb.fOutX = FALSE;
	}
	if ( s_termios->c_iflag & IXOFF )
	{
		dcb.fInX = TRUE;
	}
	else
	{
		dcb.fInX = FALSE;
	}
	
	//dcb.fTXContinueOnXoff = ( s_termios->c_iflag & IXANY ) ? TRUE : FALSE;
	dcb.fTXContinueOnXoff = FALSE;
	/* FIXME: IMAXBEL: if input buffer full, send bell */

	/* no DTR control in termios? */
	//FIX CARDON
	dcb.fParity = 0;

	//FIX BY CARDON: enable
	dcb.fDtrControl     =  DTR_CONTROL_ENABLE;
	/* no DSR control in termios? */
	dcb.fOutxDsrFlow    = FALSE;
	/* DONT ignore rx bytes when DSR is OFF */
	dcb.fDsrSensitivity = FALSE;
	//fix by Cardon: default value if NULL
	dcb.XonChar         = s_termios->c_cc[VSTART] ? s_termios->c_cc[VSTART]: 17;
	dcb.XoffChar        = s_termios->c_cc[VSTOP] ?  s_termios->c_cc[VSTOP]: 19;
	//FIX BY CARDON XonLim, XoffLim and eofChar
	dcb.XonLim          = 512;	/* instead of 0 */
	dcb.XoffLim         = 128;	/* instead of 0 */
	dcb.EofChar =	0; //s_termios->c_cc[VEOF];
	/* //Fix my Cardon: https://docs.microsoft.com/en-us/windows/win32/api/winbase/ns-winbase-dcb
	* //fBinary If this member is TRUE, binary mode is enabled. Windows does not support nonbinary mode transfers, so this member must be TRUE.
	if( dcb.EofChar != '\0' )
	{
		dcb.fBinary = 0;
	}
	else
	{
		dcb.fBinary = 1;
	}
	*/
	dcb.fBinary = 1;
	dcb.EvtChar = 0; // event char should be ignored by driver in binary mode
	
	SLEEP_AND_TRACE("tcsetattr:SetCommState");
	if ( !SetCommState( index->hComm, &dcb ) )
	{
		report2(pEnvStruct, "SetCommState failed\n" );
		YACK();
		return -1;
	}

#ifdef DEBUG_VERBOSE
	//sprintf( message, "VTIME:%d, VMIN:%d\n", s_termios->c_cc[VTIME],
	//	s_termios->c_cc[VMIN] );
	//report( message );
#endif /* DEBUG_VERBOSE */
	
	//CARDON: ignore VMIN and VTIME for read
	//
	//
	//vtime = s_termios->c_cc[VTIME] * 100;
	//timeouts.ReadTotalTimeoutConstant = vtime;
	//timeouts.ReadIntervalTimeout = 0;
	//timeouts.ReadTotalTimeoutMultiplier = 0;

	//timeouts.WriteTotalTimeoutConstant = vtime;
	//timeouts.WriteTotalTimeoutMultiplier = 0;
	///* max between bytes */
	//if ( s_termios->c_cc[VMIN] > 0 && vtime > 0 )
	//{
	//	/* read blocks forever on VMIN chars */
	//} else if ( s_termios->c_cc[VMIN] == 0 && vtime == 0 )
	//{
	//	/* read returns immediately */
	//	timeouts.ReadIntervalTimeout = MAXDWORD;
	//	timeouts.ReadTotalTimeoutConstant = 0;
	//	timeouts.ReadTotalTimeoutMultiplier = 0;
	//}


	//https://docs.microsoft.com/en-us/windows/win32/api/winbase/ns-winbase-commtimeouts
	/*If an application sets ReadIntervalTimeoutand ReadTotalTimeoutMultiplier to MAXDWORDand sets ReadTotalTimeoutConstant to a value greater than zeroand less than MAXDWORD, one of the following occurs when the ReadFile function is called :

	If there are any bytes in the input buffer, ReadFile returns immediately with the bytes in the buffer.
		If there are no bytes in the input buffer, ReadFile waits until a byte arrivesand then returns immediately.
		If no bytes arrive within the time specified by ReadTotalTimeoutConstant, ReadFile times out. */

	//timeouts.ReadTotalTimeoutConstant = 0xfffffffe; // less than MAXDWORD
	timeouts.ReadTotalTimeoutConstant = 0xfffffffe; // less than MAXDWORD, alt: 100000 (for 100 sec)
	timeouts.ReadIntervalTimeout = MAXDWORD; //0xffffffff
	timeouts.ReadTotalTimeoutMultiplier = MAXDWORD; //0xffffffff

	//A value of zero for both the WriteTotalTimeoutMultiplier and WriteTotalTimeoutConstant members indicates that total time-outs are not used for write operations.
	timeouts.WriteTotalTimeoutConstant = 0;
	timeouts.WriteTotalTimeoutMultiplier = 0;



#ifdef DEBUG_VERBOSE
	sprintf( message, "ReadIntervalTimeout=%d\n",
		timeouts.ReadIntervalTimeout );
	report( message );
	sprintf( message, "c_cc[VTIME] = %d, c_cc[VMIN] = %d\n",
		s_termios->c_cc[VTIME], s_termios->c_cc[VMIN] );
	report( message );
	sprintf( message, "ReadTotalTimeoutConstant: %ld\n",
			timeouts.ReadTotalTimeoutConstant );
	report( message );
	sprintf( message, "ReadIntervalTimeout : %ld\n",
		timeouts.ReadIntervalTimeout );
	report( message );
	sprintf( message, "ReadTotalTimeoutMultiplier: %ld\n",
		timeouts.ReadTotalTimeoutMultiplier );
	report( message );
#endif /* DEBUG_VERBOSE */


	SLEEP_AND_TRACE("tcsetattr:SetCommTimeouts");
	if ( !SetCommTimeouts( index->hComm, &timeouts ) )
	{
		YACK();
		report2(pEnvStruct, "SetCommTimeouts failed" );
		return -1;
	}
	memcpy( index->ttyset, s_termios, sizeof( struct termios ) );
	LEAVE2(pEnvStruct, "tcsetattr" );
	return 0;
}

/*----------------------------------------------------------
tcsendbreak()

   accept:
   perform:
   return:
   exceptions:
   win32api:     None
   comments:
		break for duration*0.25 seconds or
		0.25 seconds if duration = 0.
----------------------------------------------------------*/

int tcsendbreak( int fd, int duration )
{
	struct termios_list *index;
	COMSTAT Stat;

	ENTER( "tcsendbreak" );

	index = find_port( fd );

	if ( !index )
	{
		LEAVE( "tcdrain" );
		return -1;
	}

	if ( duration <= 0 ) duration = 1;
	int errNum=0;
	if( !SetCommBreak( index->hComm ) )
		ClearErrors( index, &Stat, &errNum);
	/* 0.25 seconds == 250000 usec */
	usleep( duration * 250000 );
	if( !ClearCommBreak( index->hComm ) )
		ClearErrors( index, &Stat, &errNum);
	LEAVE( "tcsendbreak" );
	return 1;
}

/*----------------------------------------------------------
tcdrain()

   accept:       file descriptor
   perform:      wait for ouput to be written.
   return:       0 on success, -1 otherwise
   exceptions:   None
   win32api:     FlushFileBuffers
   comments:
----------------------------------------------------------*/

int tcdrain ( int fd )
{
	struct termios_list *index;
	char message[80];
	int old_flag;

	ENTER( "tcdrain" );
	index = find_port( fd );

	if ( !index )
	{
		LEAVE( "tcdrain" );
		return -1;
	}
	old_flag = index->event_flag;
/*
	index->event_flag &= ~EV_TXEMPTY;
	SetCommMask( index->hComm, index->event_flag );
	index->tx_happened = 1;
*/
	if ( !FlushFileBuffers( index->hComm ) )
	{
		/* FIXME  Need to figure out what the various errors are in
		          windows.  YACK() should report them and we can
			  handle them as we find them


			  Something funky is happening on NT.  GetLastError =
			  0.
		*/
		
		//cardon
		sprintf( message,  "FlushFileBuffers() %i\n",
			(int) GetLastError() );
		report(message);
		if (GetLastError() == 50) {
			//flushing is not supported. ignore.
		} else {

			if (GetLastError() == 0)
			{
				set_errno(0);
				return(0);
			}
			set_errno(EAGAIN);
			YACK();
			LEAVE("tcdrain");
			return -1;
		}
	}
/*
	sprintf( message,  "FlushFileBuffers() %i\n",
		(int) GetLastError() );
	report( message );
*/
	LEAVE( "tcdrain success" );
	//index->event_flag |= EV_TXEMPTY;
	//SetCommMask( index->hComm, index->event_flag );
	//index->event_flag = old_flag;
/*
	index->tx_happened = 1;
*/
	return 0;
}

/*----------------------------------------------------------
tcflush()

   accept:       file descriptor, queue_selector
   perform:      discard data not transmitted or read
		 TCIFLUSH:  flush data not read
		 TCOFLUSH:  flush data not transmitted
		 TCIOFLUSH: flush both
   return:       0 on success, -1 on error
   exceptions:   none
   win32api:     PurgeComm
   comments:
----------------------------------------------------------*/

int tcflush( int fd, int queue_selector )
{
	struct termios_list *index;
	int old_flag;

	index = find_port( fd );
	if( !index)
	{
		LEAVE( "tclflush" );
		return(-1);
	}

	old_flag = index->event_flag;
/*
	index->event_flag &= ~EV_TXEMPTY;
	SetCommMask( index->hComm, index->event_flag );
	index->tx_happened = 1;
*/
	ENTER( "tcflush" );
	if ( !index )
	{
		LEAVE( "tcflush" );
		return -1;
	}

	index->tx_happened = 1;
	switch( queue_selector )
	{
		case TCIFLUSH:
			if ( !PurgeComm( index->hComm, PURGE_RXABORT ) )
			{
				goto fail;
			}
			break;
		case TCOFLUSH:
			if ( !PurgeComm( index->hComm, PURGE_TXABORT ) )
			{
				goto fail;
			}
			break;
		case TCIOFLUSH:
			if ( !PurgeComm( index->hComm, PURGE_TXABORT ) )
			{
				goto fail;
			}
			if ( !PurgeComm( index->hComm, PURGE_RXABORT ) )
			{
				goto fail;
			}
			break;
		default:
/*
			set_errno( ENOTSUP );
*/
			report( "tcflush: Unknown queue_selector\n" );
			LEAVE( "tcflush" );
			return -1;
	}
	//index->event_flag |= EV_TXEMPTY;
	//SetCommMask( index->hComm, index->event_flag );
	//index->event_flag = old_flag;
	index->tx_happened = 1;
	LEAVE( "tcflush" );
	return( 0 );

/* FIXME  Need to figure out what the various errors are in
          windows.  YACK() should report them and we can
	  handle them as we find them
*/

fail:
	LEAVE( "tcflush" );
	set_errno( EAGAIN );
	YACK();
	return -1;

}

/*----------------------------------------------------------
tcflow()

   accept:
   perform:
   return:
   exceptions:
   win32api:     None
   comments:   FIXME
----------------------------------------------------------*/

int tcflow( int fd, int action )
{
	ENTER( "tcflow" );
	switch ( action )
	{
		/* Suspend transmission of output */
		case TCOOFF: break;
		/* Restart transmission of output */
		case TCOON: break;
		/* Transmit a STOP character */
		case TCIOFF: break;
		/* Transmit a START character */
		case TCION: break;
		default: return -1;
	}
	LEAVE( "tcflow" );
	return 1;
}
/*----------------------------------------------------------
fstat()

   accept:
   perform:
   return:
   exceptions:
   win32api:
   comments:  this is just to keep the eventLoop happy.
----------------------------------------------------------*/

//#if ! defined( __LCC__ )
//int fstat( int fd, ... )
//{
//	return( 0 );
//}
//#endif



//added by cardon
/* return 1 on success, 0 if failed */
int get_comm_status(int fd, LPDWORD lpdwModemStatus) {
	struct termios_list* index;

	if (fd <= 0) {
		report("get_comm_status: Invalid file descriptor\n");
		return 0;
	}
	index = find_port(fd);
	if (!index){
		report("get_comm_status: Unknown file descriptor\n");
		return 0;
	}
	if (!GetCommModemStatus(index->hComm, lpdwModemStatus)){
		report("get_comm_status: call to Win32 GetCommModemStatus failed\n");
		return 0;
	}

	return 1;
}


/*----------------------------------------------------------
ioctl()

   accept:
   perform:
   return:
   exceptions:
   win32api:     GetCommError(), GetCommModemStatus, EscapeCommFunction()
   comments:  FIXME
	the DCB struct is:

	typedef struct _DCB
	{
		unsigned long DCBlength, BaudRate, fBinary:1, fParity:1;
		unsigned long fOutxCtsFlow:1, fOutxDsrFlow:1, fDtrControl:2;
		unsigned long fDsrSensitivity:1, fTXContinueOnXoff:1;
		unsigned long fOutX:1, fInX:1, fErrorChar:1, fNull:1;
		unsigned long fRtsControl:2, fAbortOnError:1, fDummy2:17;
		WORD wReserved, XonLim, XoffLim;
		BYTE ByteSize, Parity, StopBits;
		char XonChar, XoffChar, ErrorChar, EofChar, EvtChar;
		WORD wReserved1;
	} DCB;

----------------------------------------------------------*/

int ioctl( int fd, int request, ... )
{
	unsigned long dwStatus = 0;
	va_list ap;
	int *arg, ret, old_flag;
	char message[80];

#ifdef TIOCGSERIAL
	DCB	*dcb;
	struct serial_struct *sstruct;
#endif /* TIOCGSERIAL */
	COMSTAT Stat;

	struct termios_list *index;
	struct async_struct *astruct;
	struct serial_multiport_struct *mstruct;
#ifdef TIOCGICOUNT
	struct serial_icounter_struct *sistruct;
#endif  /* TIOCGICOUNT */

	ENTER( "ioctl" );
	if ( fd <= 0 )
		return 0;
	index = find_port( fd );
	if ( !index )
	{
		LEAVE( "ioctl" );
		return -1;
	}

	va_start( ap, request );
	// COMMENTED BY Cardon
	int errNum = 0;
	//ret = ClearErrors( index, &Stat, &errNum );
	//if (ret == 0)
	//{
	//	set_errno( EBADFD );
	//	YACK();
	//	report( "ClearError Failed! ernno EBADFD" );
	//	arg = va_arg( ap, int * );
	//	va_end( ap );
	//	return -1;
	//}
	switch( request )
	{
		case TCSBRK:
			arg = va_arg( ap, int * );
			va_end( ap );
			return -ENOIOCTLCMD;
		case TCSBRKP:
			arg = va_arg( ap, int * );
			va_end( ap );
			return -ENOIOCTLCMD;
		case TIOCGSOFTCAR:
			arg = va_arg( ap, int * );
			va_end( ap );
			return -ENOIOCTLCMD;
		case TIOCSSOFTCAR:
			arg = va_arg( ap, int * );
			va_end( ap );
			return -ENOIOCTLCMD;

		case TIOCMGET:
			arg = va_arg( ap, int * );
		/* DORITOS */
			if ( !GetCommModemStatus( index->hComm, &dwStatus ) )
				report_error( "GetCommMOdemStatus failed!\n" );
			if ( dwStatus & MS_RLSD_ON ) *arg |= TIOCM_CAR;
			else *arg &= ~TIOCM_CAR;
			if ( dwStatus & MS_RING_ON ) *arg |= TIOCM_RNG;
			else *arg &= ~TIOCM_RNG;
			if ( dwStatus & MS_DSR_ON ) *arg |= TIOCM_DSR;
			else *arg &= ~TIOCM_DSR;
			if ( dwStatus & MS_CTS_ON ) *arg |= TIOCM_CTS;
			else *arg &= ~TIOCM_CTS;
			/*  I'm not seeing a way to read the MSR directly
			    we store the state using TIOCM_*

			    Trent
			*/
			if ( index->MSR & TIOCM_DTR )
				*arg |= TIOCM_DTR;
			else *arg &= ~TIOCM_DTR;
			if ( index->MSR & TIOCM_RTS )
				*arg |= TIOCM_RTS;
			else *arg &= ~TIOCM_RTS;

/*

			TIOCM_LE
			TIOCM_ST
			TIOCM_SR
*/
			va_end( ap );
			return( 0 );
		/* TIOCMIS, TIOCMBIC and TIOCMSET all do the same thing... */
		case TIOCMBIS:
			arg = va_arg( ap, int * );
			va_end( ap );
			return -ENOIOCTLCMD;
		case TIOCMBIC:
			arg = va_arg( ap, int * );
			va_end( ap );
			return -ENOIOCTLCMD;
		case TIOCMSET:
			arg = va_arg( ap, int * );
			if (( *arg & TIOCM_DTR) == (index->MSR & TIOCM_DTR) )
			{
				report( "DTR is unchanged\n" );
			}
			sprintf(message, "DTR %i %i\n", *arg&TIOCM_DTR, index->MSR & TIOCM_DTR );
			report( message );
			if ( *arg & TIOCM_DTR )
			{
				index->MSR |= TIOCM_DTR;
			}
			else
			{
				index->MSR &= ~TIOCM_DTR;
			}
			if ( EscapeCommFunction( index->hComm,
				( *arg & TIOCM_DTR ) ? SETDTR :
				CLRDTR ) )
				report( "EscapeCommFunction: True\n" );
			else
				report( "EscapeCommFunction: False\n" );
			if ( (*arg & TIOCM_RTS) == ( index->MSR & TIOCM_RTS) )
			{
				report( "RTS is unchanged\n" );
			}
			sprintf( message, "RTS %i %i\n", *arg&TIOCM_RTS, index->MSR & TIOCM_RTS );
			report( message );
			if ( *arg & TIOCM_RTS )
			{
				index->MSR |= TIOCM_RTS;
			}
			else
			{
				index->MSR &= ~TIOCM_RTS;
			}
			if( EscapeCommFunction( index->hComm,
				( *arg & TIOCM_RTS ) ? SETRTS : CLRRTS ) )
				report( "EscapeCommFunction: True\n" );
			else
				report( "EscapeCommFunction: False\n" );
			break;

#ifdef TIOCGSERIAL
		case TIOCGSERIAL:
			report( "TIOCGSERIAL\n" );

			dcb = malloc( sizeof( DCB ) );
			if( !dcb )
			{
				va_end( ap );
				return -1;
			}
			memset( dcb, 0, sizeof( DCB ) );
			GetCommState( index->hComm, dcb );

			sstruct = va_arg( ap, struct serial_struct * );
			if ( DCB_to_serial_struct( dcb, sstruct ) < 0 )
			{
				va_end( ap );
				return -1;
			}
			index->sstruct = sstruct;

			report( "TIOCGSERIAL\n" );
			free(dcb);
			break;

#endif /* TIOCGSERIAL */
#ifdef TIOCSSERIAL
		case TIOCSSERIAL:
			report( "TIOCSSERIAL\n" );

			dcb = malloc( sizeof( DCB ) );
			if( !dcb )
			{
				va_end( ap );
				return -1;
			}
			memset( dcb, 0, sizeof( DCB ) );
			GetCommState( index->hComm, dcb );

			index->sstruct = va_arg( ap, struct serial_struct * );

			report( "TIOCSSERIAL\n" );
			free(dcb);
			break;

#endif /* TIOCSSERIAL */
		case TIOCSERCONFIG:
		case TIOCSERGETLSR:  //CARDON: Gets the value of this tty device's line status register (LSR).
			arg = va_arg( ap, int * );
			/*
			do {
				wait = WaitForSingleObject( index->sol.hEvent, 5000 );
			} while ( wait == WAIT_TIMEOUT );
			*/
			
			ret = ClearErrors( index, &Stat, &errNum );
			if ( ret == 0 )
			{
				/* FIXME ? */
				set_errno( EBADFD );
				YACK();
				report( "TIOCSERGETLSR EBADFD" );
				va_end( ap );
				return -1;
			}
			if ( (int ) Stat.cbOutQue == 0 ) //Cardon:The number of bytes of user data remaining to be transmitted for all write operations. This value will be zero for a nonoverlapped write.
			{
				/* output is empty */
				if( index->tx_happened == 1 )
				{
					old_flag = index->event_flag;
					index->event_flag &= ~EV_TXEMPTY; // Cardon: remove EV_TXEMPTY from the list. Why???
					SetCommMask( index->hComm,
						index->event_flag );
					index->event_flag = old_flag;
					*arg = 1;
					index->tx_happened = 0;
					report( "ioctl: ouput empty\n" );
				}
				else
				{
					*arg = 0;
				}
				ret = 0;
			}
			else
			{
				/* still data out there */
				*arg = 0;
				ret = 0;
			}
			va_end( ap );
			return(0);
			break;
		case TIOCSERGSTRUCT:
			astruct = va_arg( ap, struct async_struct * );
			va_end( ap );
			return -ENOIOCTLCMD;
		case TIOCSERGETMULTI:
			mstruct = va_arg( ap, struct serial_multiport_struct * );
			va_end( ap );
			return -ENOIOCTLCMD;
		case TIOCSERSETMULTI:
			mstruct = va_arg( ap, struct serial_multiport_struct * );
			va_end( ap );
			return -ENOIOCTLCMD;
		case TIOCMIWAIT:
			arg = va_arg( ap, int * );
			va_end( ap );
			return -ENOIOCTLCMD;
		/*
			On linux this fills a struct with all the line info
			(data available, bytes sent, ...
		*/
#ifdef TIOCGICOUNT
		case TIOCGICOUNT:
			//Cardon: the struct will be increased for errors counts
			sistruct= va_arg( ap, struct  serial_icounter_struct * );
			ret = ClearErrors( index, &Stat, &errNum ); //Cardon: this counts error
			if ( ret == 0 )
			{
				/* FIXME ? */
				report( "TIOCGICOUNT failed\n" );
				set_errno( EBADFD );
				va_end( ap );
				return -1;
			}
			if( sistruct->frame != index->sis->frame )
			{
				sistruct->frame = index->sis->frame;
/*
				printf( "---------------frame = %i\n", sistruct->frame++ );
*/
			}
			if( sistruct->overrun != index->sis->overrun )
			{
/*
				printf( "---------------overrun\n" );
*/
				sistruct->overrun = index->sis->overrun;
				/* ErrCode &= ~CE_OVERRUN; */
			}
			if( sistruct->parity != index->sis->parity )
			{
/*
				printf( "---------------parity\n" );
*/
				sistruct->parity = index->sis->parity;
			}
			if( sistruct->brk != index->sis->brk )
			{
/*
				printf( "---------------brk\n" );
*/
				sistruct->brk = index->sis->brk;
			}
			va_end( ap );
			return 0;
		/* abolete ioctls */
#endif /* TIOCGICOUNT */
		case TIOCSERGWILD:
		case TIOCSERSWILD:
			report( "TIOCSER[GS]WILD absolete\n" );
			va_end( ap );
			return 0;
		/*  number of bytes available for reading */
		case FIONREAD:
			arg = va_arg( ap, int * );
			ret = ClearErrors( index, &Stat, &errNum );
			if ( ret == 0 )
			{
				/* FIXME ? */
				report( "FIONREAD failed\n" );
				set_errno( EBADFD );
				va_end( ap );
				return -1;
			}
			*arg = ( int ) Stat.cbInQue;
#ifdef DEBUG_VERBOSE
			sprintf( message, "FIONREAD:  %i bytes available\n",
				(int) Stat.cbInQue );
			report( message );
			if( *arg )
			{
				sprintf( message, "FIONREAD: %i\n", *arg );
				report( message );
			}
#endif /* DEBUG_VERBOSE */
			ret = 0;
			break;

		/* pending bytes to be sent */
		case TIOCOUTQ:
			arg = va_arg( ap, int * );
			va_end( ap );
			return -ENOIOCTLCMD;
		default:
			sprintf( message,
				"FIXME:  ioctl: unknown request: %#x\n",
				request );
			report( message );
			va_end( ap );
			return -ENOIOCTLCMD;
	}
	va_end( ap );
	LEAVE( "ioctl" );
	return 0;
}

/*----------------------------------------------------------
fcntl()

   accept:
   perform:
   return:
   exceptions:
   win32api:    None
   comments:    FIXME
----------------------------------------------------------*/

int fcntl( int fd, int command, ... )
{
	int arg, ret = 0;
	va_list ap;
	struct termios_list *index;
	char message[80];

	ENTER( "fcntl" );
	if ( fd <= 0 )
		return 0;
	index = find_port( fd );
	if ( !index )
	{
		LEAVE( "fcntl" );
		return -1;
	}

	va_start( ap, command );

	arg = va_arg( ap, int );
	switch ( command )
	{
		case F_SETOWN:	/* set ownership of fd */
			break;
		case F_SETFL:	/* set operating flags */
#ifdef DEBUG
			sprintf( message, "F_SETFL fd=%d flags=%d\n", fd, arg );
			report( message );
#endif
			index->open_flags = arg;
			break;
		case F_GETFL:	/* get operating flags */
			ret = index->open_flags;
			break;
		default:
			sprintf( message, "unknown fcntl command %#x\n", command );
			report( message );
			break;
	}

	va_end( ap );
	LEAVE( "fcntl" );
	return ret;
}

/*----------------------------------------------------------
termios_interrupt_event_loop()

   accept:
   perform:
   return:	let Serial_select break out so the thread can die
   exceptions:
   win32api:
   comments:
----------------------------------------------------------*/
void termios_interrupt_event_loop( int fd, int flag )
{
	struct termios_list * index = find_port( fd );
	if ( !index )
	{
		LEAVE( "termios_interrupt_event_loop" );
		return;
	}
/*
	index->event_flag = 0;
	 TRENT SetCommMask( index->hComm, index->event_flag );
	usleep(2000);
	tcdrain( index->fd );
	SetEvent( index->sol.hEvent );
*/

	//added by cardon
	if (!SetEvent(index->monitorInterruptEvent))
	{
		sprintf(message, "termios_interrupt_event_loop: SetEvent failed (%d)\n", GetLastError());
		report(message);
	}

	index->interrupt = flag;
	return;
}

/*----------------------------------------------------------
Serial_select()

   accept:
   perform:
   return:      1 on success or 0 on error.
   exceptions:
   win32api:    SetCommMask(), GetCommEvent(), 
   comments:
----------------------------------------------------------*/
#ifndef __LCC__
int serial_wait_comm_event( int  fd, LPDWORD pdwCommEvent)
{

	//unsigned long  wait = WAIT_TIMEOUT;
	struct termios_list *index;
	char message[80];
	COMSTAT Stat;
	int ret;

	//added by Cardon
	HANDLE ghEvents[2];

	ENTER( "serial_wait_comm_event" );
	if (!fd)
	{
		/*  Baby did a bad baad thing */
		report("serial_wait_comm_event: bad fd value\n");
		goto fail;
	}

	index = find_port( fd );
	//if ( !index || !index->event_flag )
	if ( !index) 
		{
		/* still setting up the port? hold off for a Sec so
		   things can fire up

		   this does happen.  loops ~twice on a 350 Mzh with
		   usleep(1000000)
		*/
		usleep(10000);
		report("serial_wait_comm_event: bad index value\n");
		return(0);
	}


	ghEvents[0] = index->sol.hEvent;
	ghEvents[1] = index->monitorInterruptEvent;
	index->waitingCommEvent = 1;


	//ResetEvent( index->wol.hEvent );
	ResetEvent( index->sol.hEvent );
	//ResetEvent( index->rol.hEvent );
	//ret = ClearErrors( index, &Stat );
	//if (ret == 0) goto fail;
	
	//while ( wait == WAIT_TIMEOUT && index->sol.hEvent )
	//{
		if( index->interrupt == 1 || index->is_closing)
		{
			report("serial_wait_comm_event: is closing or interrupted\n");
			goto fail;
		}

		int event_flag = index->event_flag;
		//make sure the following event are not tracked
		event_flag &= ~EV_BREAK;
		event_flag &= ~EV_ERR;
		event_flag &= ~EV_RXCHAR;
		event_flag &= ~EV_RXFLAG;

		if (!SetCommMask(index->hComm, event_flag)) {
			report("serial_wait_comm_event: SetCommMask failed.\n");
			goto fail;
		}

		//ClearErrors( index, &Stat );
		if ( !WaitCommEvent( index->hComm, pdwCommEvent,
			&index->sol ) )
		{
			/* WaitCommEvent failed probably overlapped though */
			if ( GetLastError() != ERROR_IO_PENDING )
			{
				//ClearErrors( index, &Stat );
				report("serial_wait_comm_event: WaitCommEvent failed (no IO PENDING)\n");
				// may return ERROR_INVALID_PARAMETER
				goto fail;
			}
			/* thought so... */
		}
		else {
			//returned immediately
			//report status here


		}
		/*  could use the select timeout here but it should not
		    be needed
		*/
		//ClearErrors( index, &Stat );
		DWORD dwRes;
		dwRes = WaitForMultipleObjects(
			2, // number of objects in array 
			ghEvents, //array of objects 
			FALSE, //wait for any object
			INFINITE); //wait for ever

		DWORD dwOvRes; //number of bytes returned;

		switch (dwRes)
		{// ghEvents[0] was signaled (the wait operation completed)
			case WAIT_OBJECT_0 + 0:
				if (!GetOverlappedResult(index->hComm, &index->sol, &dwOvRes, FALSE)) {
					// An error occurred in the overlapped operation;
					// call GetLastError to find out what it was
					// and abort if it is fatal.
					DWORD lastError = GetLastError();
					if (lastError = ERROR_IO_INCOMPLETE) {
						//the operation is not completed. (timeout)
						goto timeout;
					}
					else {
						sprintf(message, "serial_read error. Last-error code: %d\n", lastError);
						report(message);
						goto fail;
					}
				} 
				// Status event is stored in the event flag
				// specified in the original WaitCommEvent call.
				// Deal with the status event as appropriate.
				//ReportStatusEvent(dwCommEvent); // See https://docs.microsoft.com/en-us/windows/win32/api/winbase/nf-winbase-waitcommevent
				goto end;
				
				// ghEvents[1] was signaled (the read operation is interrupted // returns 0 byte - no error)
			case WAIT_OBJECT_0 + 1:
				sprintf(message, "serial_select:wait for COM event was interrupted.\n");
				report(message);
				goto interrupt;
			case WAIT_TIMEOUT:
				//should not appear
				goto timeout;
			case WAIT_ABANDONED:
			default:
				goto fail;

		}
	//}
end:
	index->waitingCommEvent = 0;
	LEAVE( "serial_wait_comm_event" );
	return( 1 );
interrupt:
	index->waitingCommEvent = 0;
	LEAVE("serial_wait_comm_event");
	return( 1 );
timeout:
	index->waitingCommEvent = 0;
	LEAVE( "serial_wait_comm_event" );
	return( 0 );
fail:
	index->waitingCommEvent = 0;
	YACK();
	sprintf( message, "< select called error in file desriptor %i\n", fd );
	report( message );
	errno = EBADFD;
	LEAVE( "serial_wait_comm_event" );
	return( 0 );
}
#ifdef asdf
int  serial_select( int  n,  fd_set  *readfds,  fd_set  *writefds,
			fd_set *exceptfds, struct timeval *timeout )
{

	unsigned long nBytes, dwCommEvent, wait = WAIT_TIMEOUT;
	int fd = n-1;
	struct termios_list *index;
	char message[80];

	ENTER( "serial_select" );
	if ( fd <= 0 )
	{
		usleep(1000);
		return 1;
	}
	index = find_port( fd );
	if ( !index )
	{
		LEAVE( "serial_select" );
		return -1;
	}
	if( index->interrupt == 1 )
	{
		goto end;
	}
	while(!index->event_flag )
	{
		usleep(1000);
		return -1;
	}

	while ( wait == WAIT_TIMEOUT && index->sol.hEvent )
	{
		if( index->interrupt == 1 )
		{
			goto end;
		}
		if( !index->sol.hEvent )f
		{
			return 1;
		}
		if ( !WaitCommEvent( index->hComm, &dwCommEvent,
			&index->sol ) )
		{
			/* WaitCommEvent failed */
			if( index->interrupt == 1 )
			{
				goto end;
			}
			if ( GetLastError() != ERROR_IO_PENDING )
			{
				sprintf( message, "WaitCommEvent filename = %s\n", index->filename);
				report( message );
				return(1);
/*
				goto fail;
*/
			}
			return(1);
		}
		if( index->interrupt == 1 )
		{
			goto end;
		}
		wait = WaitForSingleObject( index->sol.hEvent, 1000 );
		switch ( wait )
		{
			case WAIT_OBJECT_0:
				if( index->interrupt == 1 )
				{
					goto end;
				}
				if( !index->sol.hEvent ) return(1);
				if (!GetOverlappedResult( index->hComm,
					&index->sol, &nBytes, TRUE ))
				{
					goto end;
				}
				else if( index->tx_happened == 1 )
				{
					goto end;
				}
				else
					goto end;
				break;
			case WAIT_TIMEOUT:
			default:
				return(1); /* WaitFor error */

		}
	}
end:
	LEAVE( "serial_select" );
	return( 1 );
#ifdef asdf
	/* FIXME this needs to be cleaned up... */
fail:
	sprintf( message, "< select called error %i\n", n );
	YACK();
	report( message );
	set_errno( EBADFD );
	LEAVE( "serial_select" );
	return( 1 );
#endif /* asdf */

}
#endif /* asdf */
#endif /* __LCC__ */

/*----------------------------------------------------------
termiosSetParityError()

   accept:      fd The device opened
   perform:     Get the Parity Error Char
   return:      the Parity Error Char
   exceptions:  none
   win32api:    GetCommState()
   comments:    No idea how to do this in Unix  (handle in read?)
----------------------------------------------------------*/

int termiosGetParityErrorChar( int fd )
{
	struct termios_list *index;
	DCB	dcb;

	ENTER( "termiosGetParityErrorChar" );
	index = find_port( fd );
	if( !index )
	{
		LEAVE( "termiosGetParityErrorChar" );
		return(-1);
	}
	GetCommState( index->hComm, &dcb );
	LEAVE( "termiosGetParityErrorChar" );
	return( dcb.ErrorChar );
}

/*----------------------------------------------------------
termiosSetParityError()

   accept:      fd The device opened, value the new Parity Error Char
   perform:     Set the Parity Error Char
   return:      void
   exceptions:  none
   win32api:    GetCommState(), SetCommState()
   comments:    No idea how to do this in Unix  (handle in read?)
----------------------------------------------------------*/

void termiosSetParityError( int fd, char value )
{
	DCB	dcb;
	struct termios_list *index;

	ENTER( "termiosGetParityErrorChar" );
	index = find_port( fd );
	if ( !index )
	{
		LEAVE( "termiosSetParityError" );
		return;
	}
	GetCommState( index->hComm, &dcb );
	dcb.ErrorChar = value;
	SetCommState( index->hComm, &dcb );
	LEAVE( "termiosGetParityErrorChar" );
}
/*----------------------- END OF LIBRARY -----------------*/
#ifdef PLAYING_AROUND
static inline int inportb( int index )
{
   unsigned char value;
  __asm__ volatile ( "inb %1,%0"
                    : "=a" (value)
                    : "d" ((unsigned short)index) );
   return value;
}

static inline void outportb(unsigned char val, unsigned short int index)
{
  __asm__ volatile (
                    "outb %0,%1\n"
                    :
                    : "a" (val), "d" (index)
                    );
}
#endif /* PLAYING_AROUND */



void printTime() {
	SYSTEMTIME st, lt;
	GetSystemTime(&st);
	fprintf(stderr, "%d-%d-%d  %d:%d:%d.%d\n", st.wYear,st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond, st.wMilliseconds);
}
