/* ---------------------------------------------------------------------- *
 * D&D Treasure Generator CGI shell (version 2.0)
 * 
 * by Jamis Buck (minam@rpgplanet.com)
 * online version at http://dynamic.gamespy.com/~generators/treasure2.cgi
 *
 * This file is protected under the Artistic License (see LICENSE file).
 * ---------------------------------------------------------------------- */

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>
#include <errno.h>

#include "writetem.h"
#include "wtstream.h"

#include "bskdb.h"
#include "bskstream.h"
#include "bskthing.h"
#include "bskidtbl.h"
#include "bskutil.h"
#include "bskexec.h"
#include "bskvalue.h"

#include "bskcallbacks.h"

#define TEMPATH "tem/"
#define CGINAME "treasure2.cgi"

/* the qDecoder routines are available from http://www.qDecoder.org
 * they are used for CGI environment parsing and management */

#include "qDecoder.h"

/* replace these with the appropriate values */

time_t seed;                /* random seed */
char   url[1024];           /* url used to generate current page */
char*  look;                /* the look and feel of the page */
BSKFLOAT minVal;            /* minimum treasure value */
BSKFLOAT maxVal;            /* maximum treasure value */
BSKBOOL  fillCoins;         /* fill difference with coins */

typedef struct __lf__ LOOKANDFEEL;
struct __lf__ {
  char* lfName;
  char* temName;
  char* temFile;
};


#define lfSTANDARD       "standard"

#define tnPRINTABLE      "printable"
#define tnRANDOMBYEL     "randomByEL"
#define tnRANDOMBYITEM   "randomByItem"

LOOKANDFEEL lookAndFeel[] = {
  { lfSTANDARD, tnPRINTABLE,    TEMPATH "printable-2.0.tem" },
  { lfSTANDARD, tnRANDOMBYEL,   TEMPATH "randomByEL-2.0.tem" },
  { lfSTANDARD, tnRANDOMBYITEM, TEMPATH "randomByItem-2.0.tem" },
  { 0,          0,              0 }
};


#define BLURB_COUNT ( 8 )

/* "blurbs" -- tantalizing snippets to tempt the imagination of the user! */
char* blurbs[] = {
  "Gems sparkle, coins glitter, and riches await!  Deep beneath the surface lay hoards of powerful artifacts, priceless jewels, and ancient weapons.  <I>Find them, here!</I>",
  "The wizard's workshop is quiet.  Across the room is a shelf full of potions of all different sizes, shapes and hues.  Nothing appears to guard them . . . the potions await . . .",
  "The fiend lays dead upon the ground, its mammoth body pierced by a myriad bloody wounds.  Beyond, your eye catches the glitter of gold . . .",
  "Wealth and riches pour through your fingers like water.  The fabled trove lies before you in its ancient splendor -- <I>all yours!</I>",
  "Your thief lies dying, his finger pricked by a poison needle as he attempted -- poorly -- to unlock the sturdy chest.  You hardly notice his feeble cries . . . he is forgotten as you bury your arms in the cache of ancient gold . . .",
  "Gold glitters, silver shines, platinum pulses . . . and atop the breathtaking pile lies an ancient red dragon, its eyes glinting at you dangerously.  Do you dare plunder this spectacular hoard?  <I>Is it worth your life?</I>",
  "You've heard the legends, the rumors.  People <I>talk</I> about this place.  Somewhere, hidden, guarded, lies the great-grandmother of all treasure hoards, just waiting for a seeker with enough ambition . . . and enough strength . . .",
  "<I>This</I> is what sets you apart from the rabble.  The <I>common</I> folk never behold the wonders you have seen, the treasures you have beheld . . . and the treasures you <I>will</I> behold.  Your ambition buoys you up . . . the treasure pulls you onward!",
  0    
};

/* logs access to the CGI */
void logAccess( char *mode, char *msg ) {
#ifdef USE_LOG
  FILE *f;
  char buffer[512];
  time_t t;
  char *host;

  f = fopen( LOGLOCATION, "at" );
  if( f == NULL ) {
    printf( "content-type: text/html\r\n\r\n" );
    printf( "could not open log file: %d (%s)\n", errno, strerror( errno ) );
    return;
  }

  t = time( NULL );
  strftime( buffer, sizeof( buffer ), "[%d %b %Y %H:%M:%S]", localtime( &t ) );

  host = getenv( "REMOTE_ADDR" );
  fprintf( f, "%s %s - %s (%s)\n", buffer, host, mode, msg );
  fclose( f );
#endif
}


void commify( char* buf, int num ) {
	char temp[60];
	char temp2[60];
	char* t;

	sprintf( temp, "%d", num );
	buf[0] = 0;

	t = temp + strlen( temp );
	t -= 3;
	while( t > temp ) {
    sprintf( temp2, ",%s%s", t, buf );
		*t = 0;
		t -= 3;
		strcpy( buf, temp2 );
	}
	sprintf( temp2, "%s%s", temp, buf );
	strcpy( buf, temp2 );
}


int getCounter() {
#ifdef USE_COUNTER
  FILE *f;
  char  buffer[32];
  int   count;

  f = fopen( CTRLOCATION, "r+t" );
  if( f == NULL ) {
    f = fopen( CTRLOCATION, "w+t" );
    if( f == NULL ) {
      return 0;
    }
  }

  fgets( buffer, sizeof( buffer ), f );
  count = atoi( buffer ) + 1;
  rewind( f );
  fprintf( f, "%d", count );
  fclose( f );

  return count;
#else
  return 0;
#endif
}


char* getLookAndFeel( char* lf, char* tem ) {
  int i;

  for( i = 0; lookAndFeel[ i ].lfName != 0; i++ ) {
    if( ( strcmp( lookAndFeel[ i ].lfName, lf ) == 0 ) &&
        ( strcmp( lookAndFeel[ i ].temName, tem ) == 0 ) )
    { 
      return lookAndFeel[ i ].temFile;
    }    
  }

  return "";
}


char* getBlurb() {
  int i;

  i = rand() % BLURB_COUNT;
  return blurbs[ i ];
}


void strrepl( char* buf, char* srch, char* repl ) {
  char* p;
  int   slen;
  int   rlen;

  slen = strlen( srch );
  rlen = strlen( repl );

  p = strstr( buf, srch );
  while( p != NULL ) {
    memmove( p+rlen, p+slen, strlen( p + slen ) + 1 );
    strncpy( p, repl, rlen );
    p = strstr( p+rlen, srch );
  }
}


BSKI32 htmlConsole( BSKCHAR* msg, BSKExecutionEnvironment* env, BSKNOTYPE data ) {
	BSKCHAR buf[1024];

	BSKStrCpy( buf, msg );

	while( BSKStringReplace( buf, "~B", "<B>", 0 ) != 0 ) ;
	while( BSKStringReplace( buf, "~b", "</B>", 0 ) != 0 ) ;
	while( BSKStringReplace( buf, "~I", "<I>", 0 ) != 0 ) ;
	while( BSKStringReplace( buf, "~i", "</I>", 0 ) != 0 ) ;
	while( BSKStringReplace( buf, " ", "&nbsp;&nbsp;", 0 ) != 0 ) ;
	while( BSKStringReplace( buf, "~n", "<BR>\n", 0 ) != 0 ) ;

	fprintf( stdout, "%s", buf );

	return 0;
}


int sourceIsSelected( char* src ) {
	char* v;

	v = qValueFirst( "sources" );
	while( v != 0 ) {
		if( strcmp( v, src ) == 0 ) {
			return 1;
		}
		v = qValueNext();
	}

	return 0;
}


int displaySourceList( wtSTREAM_t* stream, wtTAG_t** tags, wtGENERIC_t data, char* other ) {
  BSKUI32 id;
	BSKDatabase* db;
	BSKCHAR ident[128];

	db = (BSKDatabase*)data;
	id = BSKFindIdentifier( db->idTable, "groupAllSources" );
	if( id > 0 ) {
		BSKCategory* c;

		c = BSKFindCategory( db, id );
		if( c != 0 ) {
			BSKCategoryEntry* e;

			id = BSKFindIdentifier( db->idTable, "name" );
			for( e = BSKCategoryGetFirstMember( c ); e != 0; e = e->next ) {
				BSKAttribute* attr;
				BSKThing* thing;
				BSKCHAR* name;

				if( e->member == 0 ) {
					continue;
				}

				thing = (BSKThing*)BSKCategoryEntryGetMember( e );
				BSKGetIdentifier( db->idTable, BSKThingGetID( thing ), ident, sizeof( ident ) );

				attr = BSKGetAttributeOf( thing, id );
				if( attr == 0 ) {
					continue;
				}

				name = BSKValueGetString( &(attr->value) );

				printf( "<option value=\"%s\"%s>%s</option>\n",
						    ident,
								( sourceIsSelected( ident ) ? " selected" : "" ),
								name );
			}
		}
	}

	return 0;
}


int displayTreasureHandler( wtSTREAM_t* stream, wtTAG_t** tags, wtGENERIC_t data, char* other ) {
	BSKExecOpts* opts;

	opts = (BSKExecOpts*)data;

	if( opts != 0 ) {
		BSKValue retVal;
		BSKBOOL  halt;

		if( !qiValue( "printable" ) ) {
			printf( "<A HREF=\"%s&printable=1\" TARGET=\"PRINTABLE\">Open Printable Version</A>\n<P>\n", url );
		}

		halt = BSKFALSE;
		opts->rval = &retVal;
		opts->errorHandler = BSKDefaultRuntimeErrorHandler;
		opts->console = htmlConsole;
		opts->halt = &halt;

		BSKExec( opts );

		BSKCleanupReturnValue( &retVal );
	}

  return 0;
}


void setAttributeNum( BSKThing* thing, BSKUI32 id, BSKFLOAT number, BSKUI32 unit ) {
	BSKAttribute* attr;

	attr = BSKGetAttributeOf( thing, id );
	if( attr == 0 ) {
		BSKValue val;

		BSKSetValueNumberU( &val, number, unit );
		BSKAddAttributeTo( thing, id, &val );
	} else {
		BSKValue* pval;

		pval = BSKThingAttributeGetValue( attr );
		BSKInvalidateValue( pval );
		BSKSetValueNumberU( pval, number, unit );
	}
}


void initializeOptions( BSKDatabase* db, BSKThing* options ) {
	BSKExecOpts opts;
	BSKValue* parameters[1];
	BSKValue  parm;
	BSKValue  retVal;
	BSKBOOL   halt;
	BSKUI32   id;
  BSKBOOL   intelligent;
	BSKCHAR*  exclude;
	BSKCHAR*  source;

	halt = BSKFALSE;

	BSKValueSetThing( &parm, options );
	parameters[0] = &parm;

	opts.db = db;
	opts.ruleId = BSKFindIdentifier( db->idTable, "rInitializeOptions" );
	opts.parameterCount = 1;
	opts.parameters = parameters;
	opts.rval = &retVal;
	opts.errorHandler = BSKDefaultRuntimeErrorHandler;
	opts.console = BSKDefaultConsole;
	opts.userData = 0;
	opts.halt = &halt;

	BSKExec( &opts );

	BSKInvalidateValue( &parm );
	BSKCleanupReturnValue( &retVal );

	id = BSKFindIdentifier( db->idTable, "gp" );
	if( minVal > 0 ) {
	  setAttributeNum( options, 
				             BSKFindIdentifier( db->idTable, "optMinTreasureValue" ),
										 minVal,
										 id );
	}
	if( maxVal > 0 ) {
	  setAttributeNum( options, 
				             BSKFindIdentifier( db->idTable, "optMaxTreasureValue" ),
										 maxVal,
										 id );
	}
	if( minVal > 0 || maxVal > 0 ) {
		setAttributeNum( options,
										 BSKFindIdentifier( db->idTable, "optFillDifferenceWithCoins" ),
										 fillCoins,
										 0 );
	}

	intelligent = ( *qValueDefault( "", "intelligent" ) == 'Y' );
	setAttributeNum( options,
									 BSKFindIdentifier( db->idTable, "optAlwaysIntelligent" ),
									 intelligent,
									 0 );

	exclude = qValueDefault( "standard", "exclude" );
	if( strcmp( exclude, "art" ) == 0 ) {
		setAttributeNum( options,
				             BSKFindIdentifier( db->idTable, "optExcludeArt" ),
										 1,
										 0 );
	} else if( strcmp( exclude, "gems" ) == 0 ) {
		setAttributeNum( options,
				             BSKFindIdentifier( db->idTable, "optExcludeGems" ),
										 1,
										 0 );
	}

	source = qValueFirst( "sources" );
	if( source != 0 ) {
		BSKCategory* optUseSources;
		BSKValue val;

		optUseSources = BSKNewCategory( 0 );
		while( source != 0 ) {
			id = BSKFindIdentifier( db->idTable, source );
			if( id > 0 ) {
				BSKAddToCategory( optUseSources, 1, BSKFindThing( db, id ) );
			}
			source = qValueNext();
		}

		id = BSKFindIdentifier( db->idTable, "optUseSources" );
		BSKValueSetCategory( &val, optUseSources );
		BSKAddAttributeTo( options, id, &val );
	}
}

#define SETCONDITION( cond, ift, data, n, txt )    (ift).value=(data); \
                                                   (ift).neg=(n); \
																									 (ift).text=(txt); \
																									 (cond).handler = wtConditionalHandler; \
																									 (cond).userData = &(ift)


void randomTreasure( BSKDatabase* db ) {
	int level;
  char *plevel;
  char buffer[256];
  wtTAG_t* tags[16];
  wtIF_t ifeq;
	wtIF_t iffill;
	wtIF_t ifexclude;
  wtDELEGATE_t lf;
  wtDELEGATE_t tf;
  wtDELEGATE_t ff;
	wtDELEGATE_t ef;
	wtDELEGATE_t sources;
  BSKCHAR* tem;
	BSKCHAR  minValBuf[32];
	BSKCHAR  maxValBuf[32];
	BSKCHAR* fill;
	BSKCHAR* coins;
	BSKCHAR* goods;
	BSKCHAR* items;
	BSKCHAR* exclude;
	BSKExecOpts opts;
	BSKValue* parameters[6];
	BSKValue  parms[6];
	BSKThing* options;
	BSKUI8 i;

  plevel = qValueDefault( "", "level" );
	level = atoi( plevel );
	fill = qValueDefault( "", "fillcoins" );
	minValBuf[0] = maxValBuf[0] = 0;
	if( minVal > 0 ) {
		sprintf( minValBuf, "%g", minVal );
  }
	if( maxVal > 0 ) {
		sprintf( maxValBuf, "%g", maxVal ); 
  }
  sprintf( buffer, "%d", level );
  sprintf( url, "%s?%s", CGINAME, getenv( "QUERY_STRING" ) );
	if( strstr( url, "&seed=" ) == 0 ) {
		sprintf( &(url[strlen(url)]), "&seed=%ld", seed );
	} else if( strstr( url, "&seed=&" ) != 0 ) {
		sprintf( buffer, "&seed=%ld&", seed );
		BSKStringReplace( url, "&seed=&", buffer, 0 );
	}

	coins = qValueDefault( "100", "coins" );
	goods = qValueDefault( "100", "goods" );
	items = qValueDefault( "100", "items" );
	exclude = qValueDefault( "standard", "exclude" );

	SETCONDITION( lf, ifeq, plevel, 0, "SELECTED" );
	SETCONDITION( ff, iffill, fill, 0, "CHECKED" );
	SETCONDITION( ef, ifexclude, exclude, 0, "CHECKED" );

  tf.handler = displayTreasureHandler;
	if( level > 0 ) {
		tf.userData = &opts;
	} else {
		tf.userData = 0;
	}

	sources.handler = displaySourceList;
	sources.userData = db;

  commify( buffer, getCounter() );  
  tags[0] = wtTagReplace( "CGINAME", CGINAME );
  tags[1] = wtTagReplace( "COUNTER", buffer );
  tags[2] = wtTagDelegate( "LEVEL", &lf );
  tags[3] = wtTagDelegate( "TREASURE", &tf );
  tags[4] = wtTagReplaceI( "SEED", seed );
  tags[5] = wtTagReplace( "BLURB", getBlurb() );
  tags[6] = wtTagReplace( "MINIMUMVALUE", minValBuf );
  tags[7] = wtTagReplace( "MAXIMUMVALUE", maxValBuf );
  tags[8] = wtTagDelegate( "FILL", &ff );
	tags[9] = wtTagReplace( "COINS", coins );
	tags[10] = wtTagReplace( "GOODS", goods );
	tags[11] = wtTagReplace( "ITEMS", items );
	tags[12] = wtTagInclude( "COMMON", 0 );
	tags[13] = wtTagDelegate( "EXCLUDE", &ef );
	tags[14] = wtTagDelegate( "SOURCES", &sources );
	tags[15] = 0;

	if( level > 0 ) {
		BSKUI32 id;

		opts.db = db;
		opts.ruleId = BSKFindIdentifier( db->idTable, "rGenerateByELAndDisplay" );
		opts.parameterCount = 6;
		opts.parameters = parameters;

		id = BSKFindIdentifier( db->idTable, "rDisplayTreasureHoard" );

		BSKSetValueNumber( &parms[0], level );
		BSKSetValueNumber( &parms[1], atof( coins ) / 100.0 );
		BSKSetValueNumber( &parms[2], atof( goods ) / 100.0 );
		BSKSetValueNumber( &parms[3], atof( items ) / 100.0 );
		BSKValueSetRule( &parms[4], BSKFindRule( db->rules, id ) );

		options = BSKNewThing( 0 );
		initializeOptions( db, options );
		BSKValueSetThing( &parms[5], options );

		for( i = 0; i < opts.parameterCount; i++ ) {
			parameters[i] = &parms[i];
		}
  }

  if( qiValue( "printable" ) ) {
    tem = getLookAndFeel( look, tnPRINTABLE );
  } else {
    tem = getLookAndFeel( look, tnRANDOMBYEL );
  }

  wtWriteTemplateToFile( stdout, tem, tags );
  wtFreeTagList( tags );

	if( level > 0 ) {
		for( i = 0; i < opts.parameterCount; i++ ) {
			BSKInvalidateValue( &parms[1] );
		}
		BSKDestroyThing( options );
	}
}


void addStringToArray( BSKArray* array, BSKCHAR* string ) {
	BSKValue temp;

	if( string == 0 ) {
		return;
	}

	BSKSetValueString( &temp, string );
	BSKPutElement( array, BSKArrayGetLength( array ), &temp );
}


void buildSpecificType( BSKDatabase* db ) {
  int   count;
  int   i;
  int   showTreasure;
  char  buffer[1024];
  wtTAG_t* tags[25];

	char* minor;
	char* medium;
	char* major;

	char* armor;
	char* potion;
	char* ring;
	char* rod;
	char* scroll;
	char* staff;
	char* wand;
	char* weapon;
	char* wondrous;

	wtDELEGATE_t minorf;
	wtDELEGATE_t mediumf;
	wtDELEGATE_t majorf;
	wtDELEGATE_t sources;

	wtDELEGATE_t armorf;
	wtDELEGATE_t potionf;
	wtDELEGATE_t ringf;
	wtDELEGATE_t rodf;
	wtDELEGATE_t scrollf;
	wtDELEGATE_t stafff;
	wtDELEGATE_t wandf;
	wtDELEGATE_t weaponf;
	wtDELEGATE_t wondrousf;

  wtDELEGATE_t treasuref;
	wtDELEGATE_t fillf;
	wtDELEGATE_t intf;

	wtIF_t ifMinor;
	wtIF_t ifMedium;
	wtIF_t ifMajor;

	wtIF_t ifArmor;
	wtIF_t ifPotion;
	wtIF_t ifRing;
	wtIF_t ifRod;
	wtIF_t ifScroll;
	wtIF_t ifStaff;
	wtIF_t ifWand;
	wtIF_t ifWeapon;
	wtIF_t ifWondrous;

	wtIF_t ifFill;
	wtIF_t ifIntelligent;

  char* tem;
	char  minValBuf[32];
	char  maxValBuf[32];
	char* fill;
	char* intelligent;

	BSKExecOpts  opts;
	BSKValue*    parameters[6];
	BSKValue     parms[6];
	BSKThing*    options;
	BSKArray*    types;
	BSKArray*    magnitudes;

  showTreasure = 1;

	intelligent = qValueDefault( "", "intelligent" );

	fill = qValueDefault( "", "fillcoins" );
	minValBuf[0] = maxValBuf[0] = 0;
	if( minVal > 0 ) {
		sprintf( minValBuf, "%g", minVal );
	}
	if( maxVal > 0 ) {
		sprintf( maxValBuf, "%g", maxVal );
	}

	minor = qValueDefault( "N", "minor" );
	medium = qValueDefault( "N", "medium" );
	major = qValueDefault( "N", "major" );

	armor = qValueDefault( "N", "armor" );
	potion = qValueDefault( "N", "potion" );
	ring = qValueDefault( "N", "ring" );
	rod = qValueDefault( "N", "rod" );
	scroll = qValueDefault( "N", "scroll" );
	staff = qValueDefault( "N", "staff" );
	wand = qValueDefault( "N", "wand" );
	weapon = qValueDefault( "N", "weapon" );
	wondrous = qValueDefault( "N", "wondrous" );

	if( *minor == 'N' && *medium == 'N' && *major == 'N' ) {
		minor = "Y";
		showTreasure = 0;
	}

	if( *armor == 'N' && *potion == 'N' && *ring == 'N' &&
			*rod == 'N' && *scroll == 'N' && *staff == 'N' &&
			*wand == 'N' && *weapon == 'N' && *wondrous == 'N' )
	{
		armor = "Y";
		showTreasure = 0;
	}

  count = qiValue( "count" );
  if( count < 1 ) {
    count = 1;
  }
  if( count > 100 ) {
    count = 100;
  }

  sprintf( url, "%s?%s",
           CGINAME, getenv( "QUERY_STRING" ) );
	if( strstr( url, "&seed=" ) == 0 ) {
		sprintf( &(url[strlen(url)]), "&seed=%ld", seed );
	} else if( strstr( url, "&seed=&" ) != 0 ) {
		sprintf( buffer, "&seed=%ld&", seed );
		BSKStringReplace( url, "&seed=&", buffer, 0 );
	}

  commify( buffer, getCounter() );
  tags[0] = wtTagReplace( "CGINAME", CGINAME );
  tags[1] = wtTagReplace( "COUNTER", buffer );
  tags[2] = wtTagReplaceI( "COUNT", count );

  SETCONDITION( minorf, ifMinor, minor, 0, "CHECKED" );
  SETCONDITION( mediumf, ifMedium, medium, 0, "CHECKED" );
  SETCONDITION( majorf, ifMajor, major, 0, "CHECKED" );

	SETCONDITION( armorf, ifArmor, armor, 0, "CHECKED" );
	SETCONDITION( potionf, ifPotion, potion, 0, "CHECKED" );
	SETCONDITION( ringf, ifRing, ring, 0, "CHECKED" );
	SETCONDITION( rodf, ifRod, rod, 0, "CHECKED" );
	SETCONDITION( scrollf, ifScroll, scroll, 0, "CHECKED" );
	SETCONDITION( stafff, ifStaff, staff, 0, "CHECKED" );
	SETCONDITION( wandf, ifWand, wand, 0, "CHECKED" );
	SETCONDITION( weaponf, ifWeapon, weapon, 0, "CHECKED" );
	SETCONDITION( wondrousf, ifWondrous, wondrous, 0, "CHECKED" );

	SETCONDITION( fillf, ifFill, fill, 0, "CHECKED" );
	SETCONDITION( intf, ifIntelligent, intelligent, 0, "CHECKED" );

  treasuref.handler = displayTreasureHandler;
	if( showTreasure ) {
		treasuref.userData = &opts;
	} else {
		treasuref.userData = 0;
	}

	sources.handler = displaySourceList;
	sources.userData = db;

  tags[3] = wtTagDelegate( "MINOR", &minorf );
	tags[4] = wtTagDelegate( "MEDIUM", &mediumf );
	tags[5] = wtTagDelegate( "MAJOR", &majorf );
	tags[6] = wtTagDelegate( "ARMOR", &armorf );
	tags[7] = wtTagDelegate( "POTION", &potionf );
	tags[8] = wtTagDelegate( "RING", &ringf );
	tags[9] = wtTagDelegate( "ROD", &rodf );
	tags[10] = wtTagDelegate( "SCROLL", &scrollf );
	tags[11] = wtTagDelegate( "STAFF", &stafff );
	tags[12] = wtTagDelegate( "WAND", &wandf );
	tags[13] = wtTagDelegate( "WEAPON", &weaponf );
	tags[14] = wtTagDelegate( "WONDROUS", &wondrousf );
  tags[15] = wtTagDelegate( "TREASURE", &treasuref );
  tags[16] = wtTagReplaceI( "SEED", seed );
  tags[17] = wtTagReplace( "BLURB", getBlurb() );
	tags[18] = wtTagInclude( "COMMON", 0 );
  tags[19] = wtTagReplace( "MINIMUMVALUE", minValBuf );
  tags[20] = wtTagReplace( "MAXIMUMVALUE", maxValBuf );
  tags[21] = wtTagDelegate( "FILL", &fillf );
	tags[22] = wtTagDelegate( "INT", &intf );
	tags[23] = wtTagDelegate( "SOURCES", &sources );
	tags[24] = 0;

  if( qiValue( "printable" ) ) {
    tem = getLookAndFeel( look, tnPRINTABLE );
  } else {
    tem = getLookAndFeel( look, tnRANDOMBYITEM );
  }

	if( showTreasure ) {
		BSKUI32 id;

		opts.db = db;
		opts.ruleId = BSKFindIdentifier( db->idTable, "rGenerateByTypeAndDisplay" );
		opts.parameterCount = 5;
		opts.parameters = parameters;

		id = BSKFindIdentifier( db->idTable, "rDisplayTreasureHoard" );

		magnitudes = BSKNewArray( 0 );
		types = BSKNewArray( 0 );

		addStringToArray( magnitudes, ( *minor == 'Y' ? "minor" : 0 ) );
		addStringToArray( magnitudes, ( *medium == 'Y' ? "medium" : 0 ) );
		addStringToArray( magnitudes, ( *major == 'Y' ? "major" : 0 ) );

		addStringToArray( types, ( *armor == 'Y' ? "armor" : 0 ) );
		addStringToArray( types, ( *potion == 'Y' ? "potion" : 0 ) );
		addStringToArray( types, ( *ring == 'Y' ? "ring" : 0 ) );
		addStringToArray( types, ( *rod == 'Y' ? "rod" : 0 ) );
		addStringToArray( types, ( *scroll == 'Y' ? "scroll" : 0 ) );
		addStringToArray( types, ( *staff == 'Y' ? "staff" : 0 ) );
		addStringToArray( types, ( *wand == 'Y' ? "wand" : 0 ) );
		addStringToArray( types, ( *weapon == 'Y' ? "weapon" : 0 ) );
		addStringToArray( types, ( *wondrous == 'Y' ? "wondrous item" : 0 ) );

		BSKSetValueNumber( &parms[0], count );
		BSKValueSetArray( &parms[1], magnitudes );
		BSKValueSetArray( &parms[2], types );
		BSKValueSetRule( &parms[3], BSKFindRule( db->rules, id ) );

		options = BSKNewThing( 0 );
		initializeOptions( db, options );
		BSKValueSetThing( &parms[4], options );

		for( i = 0; i < opts.parameterCount; i++ ) {
			parameters[i] = &parms[i];
		}
  }

  wtWriteTemplateToFile( stdout, tem, tags );
  wtFreeTagList( tags );

	if( showTreasure ) {
		for( i = 0; i < opts.parameterCount; i++ ) {
			BSKInvalidateValue( &parms[i] );
		}
		BSKDestroyArray( magnitudes );
		BSKDestroyArray( types );
		BSKDestroyThing( options );
	}
}


void removeNonDigits( BSKCHAR* buf ) {
	BSKUI32 i;
	BSKUI32 j;

	j = 0;
	i = 0;
	while( buf[i] != 0 ) {
		if( buf[i] == '.' || isdigit( buf[i] ) ) {
			buf[j] = buf[i];
			j++;
		}
		i++;
	}
	buf[j] = 0;
}


int main( int argc, char* argv[] ) {
  BSKCHAR* mode;
	BSKDatabase* db;
	BSKStream* stream;
	BSKCHAR minValBuf[32];
	BSKCHAR maxValBuf[32];

  qDecoder();

  printf( "content-type: text/html\r\n" );
  printf( "Pragma: no-cache\r\n" );
  printf( "Expires: Thu, 1 Jan 1970 00:00:01 GMT\r\n\r\n" );

	stream = BSKStreamOpenFile( "dat/index.bdb", "rt" );
	if( stream == 0 ) {
		printf( "could not open data file!\n" );
		return 0;
	}

	db = BSKSerializeDatabaseIn( stream );
	stream->close( stream );

	if( db == 0 ) {
		printf( "could not read database!\n" );
		return 0;
	}

  look = qValueDefault( lfSTANDARD, "look" );
  mode = qValueDefault( "random", "mode" );
  seed = qiValue( "seed" );
  if( seed < 1 ) {
    seed = time(NULL);
  }
  BSKSRand( seed );

	fillCoins = ( *qValueDefault( "", "fillcoins" ) == 'Y' );
	strcpy( minValBuf, qValueDefault( " ", "minval" ) );
	strcpy( maxValBuf, qValueDefault( " ", "maxval" ) );

	removeNonDigits( minValBuf );
	removeNonDigits( maxValBuf );

  minVal = BSKAtoF( minValBuf );
  maxVal = BSKAtoF( maxValBuf );

  if( strcmp( mode, "random" ) == 0 ) {
    randomTreasure( db );
  } else if( strcmp( mode, "specific" ) == 0 ) {
    buildSpecificType( db );
  }

	BSKDestroyDatabase( db );
  qFree();

  return 0;
}
