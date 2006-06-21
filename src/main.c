/*
 * main.c
 *
 */
#include "lib/framework/frame.h"

#include <physfs.h>

#include "lib/widget/widget.h"
#include "lib/script/script.h"
#include "init.h"
#include "loop.h"
#include "objects.h"
#include "display.h"
#include "lib/ivis_common/piestate.h"
#include "lib/gamelib/gtime.h"
#include "winmain.h"
#include "wrappers.h"
#include "scripttabs.h"
#include "deliverance.h"
#include "frontend.h"
#include "seqdisp.h"
#include "lib/sound/audio.h"
#include "console.h"
#include "lib/ivis_common/rendmode.h"
#include "lib/ivis_common/piemode.h"
#include "levels.h"
#include "research.h"
#include "warzoneconfig.h"
#include "clparse.h"
#include "cdspan.h"
#include "configuration.h"
#include "multiplay.h"
#include "lib/netplay/netplay.h"
#include "loadsave.h"
#include "game.h"
#include "lighting.h"
#include "lib/sound/mixer.h"
// FIXME Direct iVis implementation include!
#include "lib/ivis_opengl/screen.h"

#include "modding.h"

#ifndef DEFAULT_DATADIR
# define DEFAULT_DATADIR "/usr/share/warzone/"
#endif

#ifdef WIN32
# define WZ_WRITEDIR "Warzone-2.0"
#else
# define WZ_WRITEDIR ".warzone-2.0"
#endif

char datadir[MAX_PATH] = "\0"; // Global that src/clparse.c:ParseCommandLine can write to, so it can override the default datadir on runtime. Needs to be \0 on startup for ParseCommandLine to work!

char * global_mods[MAX_MODS] = { NULL };
char * campaign_mods[MAX_MODS] = { NULL };
char * multiplay_mods[MAX_MODS] = { NULL };


// Warzone 2100 . Pumpkin Studios

UDWORD	gameStatus = GS_TITLE_SCREEN;	// Start game in title mode.
UDWORD	lastStatus = GS_TITLE_SCREEN;
//flag to indicate when initialisation is complete
BOOL	videoInitialised = FALSE;
BOOL	gameInitialised = FALSE;
BOOL	frontendInitialised = FALSE;
BOOL	reInit = FALSE;
BOOL	bDisableLobby;
BOOL pQUEUE=TRUE;			//This is used to control our pQueue list. Always ON except for SP games! -Q
char	SaveGamePath[255];
char	ScreenDumpPath[255];
char	MultiForcesPath[255];
char	MultiCustomMapsPath[255];
char	MultiPlayersPath[255];
char	KeyMapPath[255];
char	UserMusicPath[255];
extern char RegFilePath[];

/*
BOOL checkDisableLobby(void)
{
	BOOL	disable;

	disable = FALSE;
	if(!openWarzoneKey())
	{
		return FALSE;
	}

	if (!getWarzoneKeyNumeric("DisableLobby",(DWORD*)&disable))
	{
		return FALSE;
	}

	if (!closeWarzoneKey())
	{
		return FALSE;
	}

	return disable;
}
*/

static BOOL inList( char * list[], const char * item )
{
	int i = 0;
	debug( LOG_NEVER, "Item: [%s]", item );
	while( list[i] != NULL )
	{
		debug( LOG_NEVER, "Checking for match with: [%s]", list[i] );
		if ( strcmp( list[i], item ) == 0 )
			return TRUE;
		i++;
	}
	return FALSE;
}

void addSubdirs( const char * basedir, const char * subdir, const BOOL appendToPath, char * checkList[] )
{
	char tmpstr[MAX_PATH];
	char ** subdirlist = PHYSFS_enumerateFiles( subdir );
	char ** i = subdirlist;
	while( *i != NULL )
	{
		debug( LOG_NEVER, "Examining subdir: [%s]", *i );
		if( !checkList || inList( checkList, *i ) )
		{
			strcpy( tmpstr, basedir );
			strcat( tmpstr, subdir );
			strcat( tmpstr, PHYSFS_getDirSeparator() );
			strcat( tmpstr, *i );
			debug( LOG_NEVER, "Adding [%s] to search path", tmpstr );
			PHYSFS_addToSearchPath( tmpstr, appendToPath );
		}
		i++;
	}
	PHYSFS_freeList( subdirlist );
}

void removeSubdirs( const char * basedir, const char * subdir, char * checkList[] )
{
	char tmpstr[MAX_PATH];
	char ** subdirlist = PHYSFS_enumerateFiles( subdir );
	char ** i = subdirlist;
	while( *i != NULL )
	{
		debug( LOG_NEVER, "Examining subdir: [%s]", *i );
		if( !checkList || inList( checkList, *i ) )
		{
			strcpy( tmpstr, basedir );
			strcat( tmpstr, subdir );
			strcat( tmpstr, PHYSFS_getDirSeparator() );
			strcat( tmpstr, *i );
			debug( LOG_NEVER, "Removing [%s] from search path", tmpstr );
			PHYSFS_removeFromSearchPath( tmpstr );
		}
		i++;
	}
	PHYSFS_freeList( subdirlist );
}

void printSearchPath( void )
{
	char ** i, ** searchPath;

	debug(LOG_WZ, "Search paths:");
	searchPath = PHYSFS_getSearchPath();
	for (i = searchPath; *i != NULL; i++) {
		debug(LOG_WZ, "    [%s]", *i);
	}
	PHYSFS_freeList( searchPath );
}

/***************************************************************************
	Initialize the PhysicsFS library.
***************************************************************************/
static void initialize_PhysicsFS(void)
{
	PHYSFS_Version compiled;
	PHYSFS_Version linked;
	char tmpstr[MAX_PATH];

	PHYSFS_VERSION(&compiled);
	PHYSFS_getLinkedVersion(&linked);

	debug(LOG_WZ, "Compiled against PhysFS version: %d.%d.%d",
	      compiled.major, compiled.minor, compiled.patch);
	debug(LOG_WZ, "Linked against PhysFS version: %d.%d.%d",
	      linked.major, linked.minor, linked.patch);

	strcpy( tmpstr, PHYSFS_getUserDir() );
	strcat( tmpstr, WZ_WRITEDIR );
	PHYSFS_setWriteDir( PHYSFS_getUserDir() ); // Ugly workaround for PhysFS not creating the writedir as expected.
	PHYSFS_mkDir( WZ_WRITEDIR ); // s.a.
	if ( PHYSFS_setWriteDir( tmpstr ) == 0 ) {
		debug( LOG_ERROR, "Error setting write directory to \"%s\": %s",
			tmpstr, PHYSFS_getLastError() );
		exit(1);
	}
	
	// User's home dir
	PHYSFS_addToSearchPath( PHYSFS_getWriteDir(), PHYSFS_APPEND );

	PHYSFS_permitSymbolicLinks(1);

	debug( LOG_WZ, "Write dir: %s", PHYSFS_getWriteDir() );
	debug( LOG_WZ, "Base dir: %s", PHYSFS_getBaseDir() );
}

// We need ParseCommandLine, before we can add any mods...
void scanDataDirs( void )
{
	char tmpstr[MAX_PATH], prefix[MAX_PATH] = { '\0' };

	// Command line supplied datadir
	PHYSFS_addToSearchPath( datadir, PHYSFS_PREPEND );

	// maps/mods subdirs of user's home dir
	addSubdirs( PHYSFS_getWriteDir(), "mods/global", PHYSFS_APPEND, global_mods );
	addSubdirs( PHYSFS_getWriteDir(), "maps", PHYSFS_APPEND, FALSE );

	// maps/mods subdirs of program dir
	PHYSFS_addToSearchPath( PHYSFS_getBaseDir(), PHYSFS_APPEND );
	addSubdirs( PHYSFS_getBaseDir(), "mods/global", PHYSFS_APPEND, global_mods );
	addSubdirs( PHYSFS_getBaseDir(), "maps", PHYSFS_APPEND, FALSE );
	PHYSFS_removeFromSearchPath( PHYSFS_getBaseDir() );

	// Program dir mp.wz patches
	strcpy( tmpstr, PHYSFS_getBaseDir() );
	strcat( tmpstr, "mp.wz" );
	PHYSFS_addToSearchPath( tmpstr, PHYSFS_APPEND );

	// Program dir warzone.wz
	strcpy( tmpstr, PHYSFS_getBaseDir() );
	strcat( tmpstr, "warzone.wz" );
	PHYSFS_addToSearchPath( tmpstr, PHYSFS_APPEND );


	// Plain program dir + patches
	strcpy( tmpstr, PHYSFS_getBaseDir() );
	strcat( tmpstr, "mp" );
	PHYSFS_addToSearchPath( tmpstr, PHYSFS_APPEND );
	PHYSFS_addToSearchPath( PHYSFS_getBaseDir(), PHYSFS_APPEND );


	// Plain default datadir on Unix
	strcpy( tmpstr, DEFAULT_DATADIR );
	strcat( tmpstr, "mp" );
	PHYSFS_addToSearchPath( tmpstr, PHYSFS_APPEND );
	PHYSFS_addToSearchPath( DEFAULT_DATADIR, PHYSFS_APPEND );

	// Default datadir with .wz files on Unix
	strcpy( tmpstr, DEFAULT_DATADIR );
	strcat( tmpstr, "mp.wz" );
	PHYSFS_addToSearchPath( tmpstr, PHYSFS_APPEND );
	strcpy( tmpstr, DEFAULT_DATADIR );
	strcat( tmpstr, "warzone.wz" );
	PHYSFS_addToSearchPath( tmpstr, PHYSFS_APPEND );


	// Find out which PREFIX we are in...
	strcpy( tmpstr, PHYSFS_getBaseDir() );
	*strrchr( tmpstr, *PHYSFS_getDirSeparator() ) = '\0'; // Trim ending '/', which getBaseDir always provides

	strncpy( prefix, PHYSFS_getBaseDir(), // Skip the last dir from base dir
		strrchr( tmpstr, *PHYSFS_getDirSeparator() ) - tmpstr );

	// Relocation for AutoPackage
	strcpy( tmpstr, prefix );
	strcat( tmpstr, "/share/warzone/mp.wz" );
	PHYSFS_addToSearchPath( tmpstr, PHYSFS_APPEND );
	strcpy( tmpstr, prefix );
	strcat( tmpstr, "/share/warzone/warzone.wz" );
	PHYSFS_addToSearchPath( tmpstr, PHYSFS_APPEND );

	// Hack for the hackers... Use data in SVN dir
	strcpy( tmpstr, prefix );
	strcat( tmpstr, "/data/mp" );
	PHYSFS_addToSearchPath( tmpstr, PHYSFS_APPEND );
	strcpy( tmpstr, prefix );
	strcat( tmpstr, "/data" );
	PHYSFS_addToSearchPath( tmpstr, PHYSFS_APPEND );
		

	/** Debugging and sanity checks **/

	printSearchPath();

	if( PHYSFS_exists("gamedesc.lev") )
	{
		debug( LOG_WZ, "gamedesc.lev found at %s", PHYSFS_getRealDir( "gamedesc.lev" ) );
	}
	else
	{
		debug( LOG_ERROR, "Could not find game data. Aborting." );
		exit(1);
	}
}

/***************************************************************************
	Make a directory in write path and set a variable to point to it.
***************************************************************************/
static void make_dir(char *dest, char *dirname, char *subdir)
{
	strcpy(dest, dirname);
	if (subdir != NULL) {
		strcat(dest, "/");
		strcat(dest, subdir);
	}
	{
		size_t l = strlen(dest);

		if (dest[l-1] != '/') {
			dest[l] = '/';
			dest[l+1] = '\0';
		}
	}
	PHYSFS_mkdir(dest);
	if (PHYSFS_isDirectory(dest) == 0) {
		debug(LOG_ERROR, "Unable to create directory \"%s\" in write dir \"%s\"!",
		      dest, PHYSFS_getWriteDir());
		exit(1);
	}
}

int main(int argc, char *argv[])
{
	FRAME_STATUS		frameRet;
	BOOL			quit = FALSE;
	BOOL			Restart = FALSE;
	BOOL			paused = FALSE;//, firstTime = TRUE;
	BOOL			bVidMem = FALSE;
	SDWORD			dispBitDepth = DISP_BITDEPTH;
	SDWORD			introVideoControl = 3;
	int			loopStatus = 0;
	iColour*		psPaletteBuffer;
	UDWORD			pSize;

	/*** Initialize the debug subsystem ***/
	/* Debug stuff for .net, don't delete :) */
#ifdef _MSC_VER
#ifdef _DEBUG
	{
		int tmpFlag; //debug stuff for VC -Q

		tmpFlag = _CrtSetDbgFlag( _CRTDBG_REPORT_FLAG );
		tmpFlag |= (_CRTDBG_CHECK_ALWAYS_DF | _CRTDBG_ALLOC_MEM_DF);
		_CrtSetDbgFlag( tmpFlag );			// just turning on VC debug stuff...
	}
#endif
	#ifndef MAX_PATH
	#define MAX_PATH 512
	#endif
#endif

	debug_init();

	// find early boot info
	if ( !ParseCommandLineEarly(argc, argv) ) {
		return -1;
	}

	/*** Initialize PhysicsFS ***/

	PHYSFS_init(argv[0]);
	initialize_PhysicsFS();

	make_dir(ScreenDumpPath, "screendumps", NULL);
	make_dir(SaveGamePath, "savegame", NULL);
	make_dir(MultiPlayersPath, "multiplay", NULL);
	make_dir(MultiPlayersPath, "multiplay", "players");
	make_dir(MultiForcesPath, "multiplay", "forces");
	make_dir(MultiCustomMapsPath, "multiplay", "custommaps");

	/* Put these files in the writedir root */
	strcpy(RegFilePath, "config");
	strcpy(KeyMapPath, "keymap.map");
	strcpy(UserMusicPath, "music");

	// initialise all the command line states
	clIntroVideo = FALSE;
	war_SetDefaultStates();

init://jump here from the end if re_initialising

	if (!blkInitialise())
	{
		return FALSE;
	}

	loadRenderMode(); //get the registry entry for clRendMode

	bDisableLobby = FALSE;

	// parse the command line
	if (!reInit) {
		if (!ParseCommandLine(argc, argv)) {
			return -1;
		}
	}

	scanDataDirs();

	debug(LOG_MAIN, "reinitializing");

	// find out if the lobby stuff has been disabled
//	bDisableLobby = checkDisableLobby();
	if (!bDisableLobby &&
		!lobbyInitialise())			// ajl. Init net stuff. Lobby can modify startup conditions like commandline.
	{
		return -1;
	}

	reInit = FALSE;//just so we dont restart again

	bVidMem = FALSE;
	dispBitDepth = DISP_BITDEPTH;

//	frameDDEnumerate();

	if (!frameInitialise(NULL, "Warzone 2100", DISP_WIDTH,DISP_HEIGHT,dispBitDepth, war_getFullscreen(), bVidMem))
	{
		return -1;
	}

	pie_SetFogStatus(FALSE);
	pie_ScreenFlip(CLEAR_BLACK);
	pie_ScreenFlip(CLEAR_BLACK);

	if(gameStatus == GS_VIDEO_MODE)
	{
		introVideoControl = 0;//play video
		gameStatus = GS_TITLE_SCREEN;
	}

	//load palette
	psPaletteBuffer = (iColour*)MALLOC(256 * sizeof(iColour)+1);
	if (psPaletteBuffer == NULL)
	{
		DBERROR(("Out of memory"));
		return -1;
	}
	if (!loadFileToBuffer("palette.bin", (char*)psPaletteBuffer, (256 * sizeof(iColour)+1),&pSize))
	{
		DBERROR(("Couldn't load palette data"));
		return -1;
	}
	pal_AddNewPalette(psPaletteBuffer);
	FREE(psPaletteBuffer);

	pie_LoadBackDrop(SCREEN_RANDOMBDROP,FALSE);
	pie_SetFogStatus(FALSE);
	pie_ScreenFlip(CLEAR_BLACK);

	quit = FALSE;

	if (!systemInitialise())
	{
		return -1;
	}

	//set all the pause states to false
	setAllPauseStates(FALSE);

	while (!quit)
	{
// Do the game mode specific initialisation.
		switch(gameStatus)
		{
			case GS_TITLE_SCREEN:
				screen_RestartBackDrop();

				//loadLevels(DIR_MULTIPLAYER);

				if (!frontendInitialise("wrf/frontend.wrf"))
				{
					goto exit;
				}

				frontendInitialised = TRUE;
				frontendInitVars();
				//if intro required set up the video
				if (introVideoControl <= 1)
				{
					seq_ClearSeqList();
					seq_AddSeqToList("eidos-logo.rpl",NULL, NULL, FALSE,0);
					seq_AddSeqToList("pumpkin.rpl",NULL, NULL, FALSE,0);
					seq_AddSeqToList("titles.rpl",NULL, NULL, FALSE,0);
					seq_AddSeqToList("devastation.rpl",NULL,"devastation.txa", FALSE,0);

					seq_StartNextFullScreenVideo();
					introVideoControl = 2;
				}
				break;

			case GS_SAVEGAMELOAD:
				screen_RestartBackDrop();
				gameStatus = GS_NORMAL;
				// load up a save game
				if (!loadGameInit(saveGameName,FALSE))
				{
					goto exit;
				}
				/*if (!levLoadData(pLevelName, saveGameName)) {
					return -1;
				}*/
				screen_StopBackDrop();
				break;
			case GS_NORMAL:
				if (!levLoadData(pLevelName, NULL, 0)) {
					goto exit;
				}
				//after data is loaded check the research stats are valid
				if (!checkResearchStats())
				{
					DBERROR(("Invalid Research Stats"));
					goto exit;
				}
				//and check the structure stats are valid
				if (!checkStructureStats())
				{
					DBERROR(("Invalid Structure Stats"));
					goto exit;
				}

				//set a flag for the trigger/event system to indicate initialisation is complete
				gameInitialised = TRUE;
				screen_StopBackDrop();
				break;
			case GS_VIDEO_MODE:
				DBERROR(("Video_mode no longer valid"));
				if (introVideoControl == 0)
				{
					videoInitialised = TRUE;
				}
				break;

			default:
				debug(LOG_ERROR, "Unknown game status on shutdown!");
		}


		debug(LOG_MAIN, "Entering main loop");

		Restart = FALSE;
		//firstTime = TRUE;

		while (!Restart)
		{
			frameRet = frameUpdate();

			if (pie_GetRenderEngine() == ENGINE_OPENGL)	//Was ENGINE_D3D -Q
			{
				if ( frameRet == FRAME_SETFOCUS )
				{
//					D3DTestCooperativeLevel( TRUE );
				}
				else
				{
//					D3DTestCooperativeLevel( FALSE );
				}
			}

			switch (frameRet)
			{
			case FRAME_KILLFOCUS:
				paused = TRUE;
				gameTimeStop();

				mixer_SaveIngameVols();
				mixer_RestoreWinVols();
				audio_StopAll();
				break;
			case FRAME_SETFOCUS:
				paused = FALSE;
				gameTimeStart();
				if (!dispModeChange())
				{
					quit = TRUE;
					Restart = TRUE;
				}

				else if (pie_GetRenderEngine() == ENGINE_OPENGL)	//Was ENGINE_D3D -Q
				{
//					dtm_RestoreTextures();
				}
				mixer_SaveWinVols();
				mixer_RestoreIngameVols();
				break;
			case FRAME_QUIT:
				debug(LOG_MAIN, "frame quit");
				quit = TRUE;
				Restart = TRUE;
				break;
			default:
				break;
			}

			lastStatus = gameStatus;

			if ((!paused) && (!quit))
			{
				switch(gameStatus)
				{
				case	GS_TITLE_SCREEN:
					pie_SetSwirlyBoxes(TRUE);
					if (loop_GetVideoStatus())
					{
						videoLoop();
					}
					else
					{
						switch(titleLoop()) {
							case TITLECODE_QUITGAME:
								debug(LOG_MAIN, "TITLECODE_QUITGAME");
								Restart = TRUE;
								quit = TRUE;
								break;

	//						case TITLECODE_ATTRACT:
	//							DBPRINTF(("TITLECODE_ATTRACT\n"));
	//							break;

							case TITLECODE_SAVEGAMELOAD:
								debug(LOG_MAIN, "TITLECODE_SAVEGAMELOAD");
								gameStatus = GS_SAVEGAMELOAD;
								Restart = TRUE;
								break;
							case TITLECODE_STARTGAME:
								debug(LOG_MAIN, "TITLECODE_STARTGAME");
								gameStatus = GS_NORMAL;
								Restart = TRUE;
								break;

							case TITLECODE_SHOWINTRO:
								debug(LOG_MAIN, "TITLECODE_SHOWINTRO");
								seq_ClearSeqList();
								seq_AddSeqToList("eidos-logo.rpl",NULL,NULL, FALSE,0);
								seq_AddSeqToList("pumpkin.rpl",NULL,NULL, FALSE,0);
								seq_AddSeqToList("titles.rpl",NULL,NULL, FALSE,0);
								seq_AddSeqToList("devastation.rpl",NULL,"devastation.txa", FALSE,0);
								seq_StartNextFullScreenVideo();
								introVideoControl = 2;//play the video but dont init the sound system
								break;

							case TITLECODE_CONTINUE:
								break;

							default:
								debug(LOG_ERROR, "Unknown code returned by titleLoop");
						}
					}
					pie_SetSwirlyBoxes(FALSE);
					break;

/*				case GS_SAVEGAMELOAD:
					if (loopNewLevel)
					{
						//the start of a campaign/expand mission
						DBPRINTF(("GAMECODE_NEWLEVEL\n"));
						loopNewLevel = FALSE;
						// gameStatus is unchanged, just loading additional data
						Restart = TRUE;
					}
					break;
*/
				case	GS_NORMAL:
					if (loop_GetVideoStatus())
					{
						videoLoop();
					}
					else
					{
						loopStatus = gameLoop();
						switch(loopStatus) {
							case GAMECODE_QUITGAME:
								debug(LOG_MAIN, "GAMECODE_QUITGAME");
								gameStatus = GS_TITLE_SCREEN;
								Restart = TRUE;
								if(NetPlay.bLobbyLaunched)
								{
//									changeTitleMode(QUIT);
									quit = TRUE;
								}
								break;
							case GAMECODE_FASTEXIT:
								debug(LOG_MAIN, "GAMECODE_FASTEXIT");
								Restart = TRUE;
								quit = TRUE;
								break;

							case GAMECODE_LOADGAME:
								debug(LOG_MAIN, "GAMECODE_LOADGAME");
								Restart = TRUE;
								gameStatus = GS_SAVEGAMELOAD;
								break;

							case GAMECODE_PLAYVIDEO:
								debug(LOG_MAIN, "GAMECODE_PLAYVIDEO");
//dont schange mode any more								gameStatus = GS_VIDEO_MODE;
								Restart = FALSE;
								break;

							case GAMECODE_NEWLEVEL:
								debug(LOG_MAIN, "GAMECODE_NEWLEVEL");
								// gameStatus is unchanged, just loading additional data
								Restart = TRUE;
								break;

							case GAMECODE_RESTARTGAME:
								debug(LOG_MAIN, "GAMECODE_RESTARTGAME");
								Restart = TRUE;
								break;

							case GAMECODE_CONTINUE:
								break;

							default:
								debug(LOG_ERROR, "Unknown code returned by gameLoop");
						}
					}
					break;

				case	GS_VIDEO_MODE:
					debug(LOG_ERROR, "Video_mode no longer valid");
					if (loop_GetVideoStatus())
					{
						videoLoop();
					}
					else
					{
						if (introVideoControl <= 1)
						{
								seq_ClearSeqList();

								seq_AddSeqToList("factory.rpl",NULL,NULL, FALSE,0);
								seq_StartNextFullScreenVideo();//"sequences\\factory.rpl","sequences\\factory.wav");
								introVideoControl = 2;
						}
						else
						{
								debug(LOG_MAIN, "VIDEO_QUIT");
								if (introVideoControl == 2)//finished playing intro video
								{
									gameStatus = GS_TITLE_SCREEN;
									if (videoInitialised)
									{
										Restart = TRUE;
									}
									introVideoControl = 3;
								}
								else
								{
									gameStatus = GS_NORMAL;
								}
						}
					}

					break;

				default:
					debug(LOG_ERROR, "Weirdy game status, I'm afraid!!");
					break;
				}

				gameTimeUpdate();
			}
		}	// End of !Restart loop.

// Do game mode specific shutdown.
		switch(lastStatus) {
			case GS_TITLE_SCREEN:
				if (!frontendShutdown())
				{
					goto exit;
				}
				frontendInitialised = FALSE;
				break;

/*			case GS_SAVEGAMELOAD:
				//get the next level to load up
				gameStatus = GS_NORMAL;
				break;*/
			case GS_NORMAL:
				if (loopStatus != GAMECODE_NEWLEVEL)
				{
					initLoadingScreen(TRUE,FALSE);	// returning to f.e. do a loader.render not active
					pie_EnableFog(FALSE);//dont let the normal loop code set status on
					fogStatus = 0;
					if (loopStatus != GAMECODE_LOADGAME)
					{
						levReleaseAll();
					}
				}
				gameInitialised = FALSE;
				break;

			case	GS_VIDEO_MODE:
				debug(LOG_ERROR, "Video_mode no longer valid");
				if (videoInitialised)
				{
					videoInitialised = FALSE;
				}
				break;

			default:
				debug(LOG_ERROR, "Unknown game status on shutdown!");
				break;
		}

	} // End of !quit loop.

  debug(LOG_MAIN, "Shuting down application");

	systemShutdown();
	pal_ShutDown();
	frameShutDown();

	if (reInit) goto init;

	return 0;

exit:

	debug(LOG_ERROR, "Shutting down after failure");
	systemShutdown();
	pal_ShutDown();
	frameShutDown();

	return 1;
}


UDWORD GetGameMode(void)
{
	return gameStatus;
}

void SetGameMode(UDWORD status)
{
	ASSERT((status == GS_TITLE_SCREEN ||
			status == GS_MISSION_SCREEN ||
			status == GS_NORMAL ||
			status == GS_VIDEO_MODE ||
			status == GS_SAVEGAMELOAD,
		"SetGameMode: invalid game mode"));

	gameStatus = status;
}

