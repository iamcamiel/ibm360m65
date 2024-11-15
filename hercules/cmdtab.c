/* CMDTAB.C     (c) Copyright Roger Bowler, 1999-2009                */
/*              (c) Copyright "Fish" (David B. Trout), 2002-2009     */
/*              (c) Copyright Jan Jaeger, 2003-2009                  */
/*              Route all Hercules configuration statements          */
/*              and panel commands to the appropriate functions      */

// $Id: cmdtab.c 5457 2009-09-01 16:29:56Z bernard $

// $Log$
// Revision 1.3  2009/01/19 12:16:57  rbowler
// Fix dynamic linkage errors in cmdtab.c for MSVC
//
// Revision 1.2  2009/01/18 21:43:59  jj
// Always display short help info, detailed info only if available
//
// Revision 1.1  2009/01/18 20:49:25  jj
// Rework command table and move to separate source files
//


#include "hstdinc.h"

#define _CMDTAB_C_
#define _HENGINE_DLL_

#include "hercules.h"
#include "history.h"


// Handle externally defined commands...

// (for use in CMDTAB COMMAND entry further below)
#define      EXT_CMD(xxx_cmd)  call_ ## xxx_cmd

// (for defining routing function immediately below)
#define CALL_EXT_CMD(xxx_cmd)  \
int call_ ## xxx_cmd ( int argc, char *argv[], char *cmdline )  { \
    return   xxx_cmd (     argc,       argv,         cmdline ); }

// Externally defined commands routing functions...

CALL_EXT_CMD ( ptt_cmd    )
CALL_EXT_CMD ( cache_cmd  )
CALL_EXT_CMD ( shared_cmd )

/*-------------------------------------------------------------------*/
/* Create forward references for all commands in the command table   */
/*-------------------------------------------------------------------*/
#define _FW_REF
#define COMMAND(_stmt, _type, _func, _sdesc, _ldesc)        \
int (_func)(int argc, char *argv[], char *cmdline);
#define CMDABBR(_stmt, _abbr, _type, _func, _sdesc, _ldesc) \
int (_func)(int argc, char *argv[], char *cmdline);
#include "cmdtab.h"
#undef COMMAND
#undef CMDABBR
#undef _FW_REF


typedef int CMDFUNC(int argc, char *argv[], char *cmdline);

/* Layout of command routing table                        */
typedef struct _CMDTAB
{
    const char  *statement;        /* statement           */
    const size_t statminlen;       /* min abbreviation    */
          int   type;              /* statement type      */
#define DISABLED   0x00            /* disabled statement  */
#define CONFIG     0x01            /* config statement    */
#define PANEL      0x02            /* command statement   */
    CMDFUNC    *function;          /* handler function    */
    const char *shortdesc;         /* description         */
    const char *longdesc;          /* detaled description */
} CMDTAB;

#define COMMAND(_stmt, _type, _func, _sdesc, _ldesc) \
{ (_stmt),     (0), (_type), (_func), (_sdesc), (_ldesc) },
#define CMDABBR(_stmt, _abbr, _type, _func, _sdesc, _ldesc) \
{ (_stmt), (_abbr), (_type), (_func), (_sdesc), (_ldesc) },

static CMDTAB cmdtab[] =
{
#include "cmdtab.h"
COMMAND ( NULL, 0, NULL, NULL, NULL ) /* End of table */
};


/*-------------------------------------------------------------------*/
/* $zapcmd - internal debug - may cause havoc - use with caution     */
/*-------------------------------------------------------------------*/
int zapcmd_cmd(int argc, char *argv[], char *cmdline)
{
CMDTAB* cmdent;
int i;

    UNREFERENCED(cmdline);

    if (argc > 1)
    {
        for (cmdent = cmdtab; cmdent->statement; cmdent++)
        {
            if(!strcasecmp(argv[1], cmdent->statement))
            {
                if(argc > 2)
                    for(i = 2; i < argc; i++)
                    {
                        if(!strcasecmp(argv[i],"Cfg"))
                            cmdent->type |= CONFIG;
                        else
                        if(!strcasecmp(argv[i],"NoCfg"))
                            cmdent->type &= ~CONFIG;
                        else
                        if(!strcasecmp(argv[i],"Cmd"))
                            cmdent->type |= PANEL;
                        else
                        if(!strcasecmp(argv[i],"NoCmd"))
                            cmdent->type &= ~PANEL;
                        else
                        {
                            logmsg(_("Invalid arg: %s: %s %s [(No)Cfg|(No)Cmd]\n"),argv[i],argv[0],argv[1]);
                            return -1;
                        }
                    }
                else
                    logmsg(_("%s: %s(%sCfg,%sCmd)\n"),argv[0],cmdent->statement,
                      (cmdent->type&CONFIG)?"":"No",(cmdent->type&PANEL)?"":"No");
                return 0;
            }
        }
        logmsg(_("%s: %s not in command table\n"),argv[0],argv[1]);
        return -1;
    }
    else
        logmsg(_("Usage: %s <command> [(No)Cfg|(No)Cmd]\n"),argv[0]);
    return -1;
}

CMDT_DLL_IMPORT
int ProcessConfigCommand (int argc, char **argv, char *cmdline)
{
CMDTAB* cmdent;

    if (argc)
        for (cmdent = cmdtab; cmdent->statement; cmdent++)
            if(cmdent->function && (cmdent->type & CONFIG))
                if(!strcasecmp(argv[0], cmdent->statement))
                    return cmdent->function(argc, argv, cmdline);

    return -1;
}


/*-------------------------------------------------------------------*/
/* Main panel command processing function                            */
/*-------------------------------------------------------------------*/
int OnOffCommand(int argc, char *argv[], char *cmdline);
int ShadowFile_cmd(int argc, char *argv[], char *cmdline);

int    cmd_argc;
char*  cmd_argv[MAX_ARGS];

int ProcessPanelCommand (char* pszCmdLine)
{
    CMDTAB*  pCmdTab         = NULL;
    char*    pszSaveCmdLine  = NULL;
    char*    cl              = NULL;
    int      rc              = -1;
    int      cmdl;

    if (!pszCmdLine || !*pszCmdLine)
    {
        /* [enter key] by itself: start the CPU
           (ignore if not instruction stepping) */
        if (sysblk.inststep)
            rc = start_cmd(0,NULL,NULL);
        goto ProcessPanelCommandExit;
    }

#if defined(OPTION_CONFIG_SYMBOLS)
    /* Perform variable substitution */
    /* First, set some 'dynamic' symbols to their own values */
    set_symbol("CUU","$(CUU)");
    set_symbol("cuu","$(cuu)");
    set_symbol("CCUU","$(CCUU)");
    set_symbol("ccuu","$(ccuu)");
    cl=resolve_symbol_string(pszCmdLine);
#else
    cl=pszCmdLine;
#endif

    /* Save unmodified copy of the command line in case
       its format is unusual and needs customized parsing. */
    pszSaveCmdLine = strdup(cl);

    /* Parse the command line into its individual arguments...
       Note: original command line now sprinkled with nulls */
    parse_args (cl, MAX_ARGS, cmd_argv, &cmd_argc);

    /* If no command was entered (i.e. they entered just a comment
       (e.g. "# comment")) then ignore their input */
    if ( !cmd_argv[0] )
        goto ProcessPanelCommandExit;

#if defined(OPTION_DYNAMIC_LOAD)
    if(system_command)
        if((rc = system_command(cmd_argc, (char**)cmd_argv, pszSaveCmdLine)))
            goto ProcessPanelCommandExit;
#endif

    /* Route standard formatted commands from our routing table... */
    if (cmd_argc)
        for (pCmdTab = cmdtab; pCmdTab->function; pCmdTab++)
        {
            if(pCmdTab->function && (pCmdTab->type & PANEL))
            {
                if (!pCmdTab->statminlen)
                {
                    if(!strcasecmp(cmd_argv[0], pCmdTab->statement))
                    {
                        rc = pCmdTab->function(cmd_argc, (char**)cmd_argv, pszSaveCmdLine);
                        goto ProcessPanelCommandExit;
                    }
                }
                else
                {
                    cmdl=MAX(strlen(cmd_argv[0]),pCmdTab->statminlen);
                    if(!strncasecmp(cmd_argv[0],pCmdTab->statement,cmdl))
                    {
                        rc = pCmdTab->function(cmd_argc, (char**)cmd_argv, pszSaveCmdLine);
                        goto ProcessPanelCommandExit;
                    }
                }
            }
        }

    /* Route non-standard formatted commands... */

    /* sf commands - shadow file add/remove/set/compress/display */
    if (0
        || !strncasecmp(pszSaveCmdLine,"sf+",3)
        || !strncasecmp(pszSaveCmdLine,"sf-",3)
        || !strncasecmp(pszSaveCmdLine,"sfc",3)
        || !strncasecmp(pszSaveCmdLine,"sfd",3)
        || !strncasecmp(pszSaveCmdLine,"sfk",3)
    )
    {
        rc = ShadowFile_cmd(cmd_argc,(char**)cmd_argv,pszSaveCmdLine);
        goto ProcessPanelCommandExit;
    }

    /* x+ and x- commands - turn switches on or off */
    if ('+' == pszSaveCmdLine[1] || '-' == pszSaveCmdLine[1])
    {
        rc = OnOffCommand(cmd_argc,(char**)cmd_argv,pszSaveCmdLine);
        goto ProcessPanelCommandExit;
    }

    /* Error: unknown/unsupported command... */
    ASSERT( cmd_argv[0] );

    logmsg( _("HHCPN139E Command \"%s\" not found; enter '?' for list.\n"),
              cmd_argv[0] );

ProcessPanelCommandExit:

    /* Free our saved copy */
    free(pszSaveCmdLine);

#if defined(OPTION_CONFIG_SYMBOLS)
    if (cl != pszCmdLine)
        free(cl);
#endif

    return rc;
}


/*-------------------------------------------------------------------*/
/* help command - display additional help for a given command        */
/*-------------------------------------------------------------------*/
int HelpCommand(int argc, char *argv[], char *cmdline)
{
    CMDTAB* pCmdTab;

    UNREFERENCED(cmdline);

    if (argc < 2)
    {
        logmsg( _("HHCPN140I Valid panel commands are...\n\n") );
        logmsg( "  %-9.9s    %s \n", "Command", "Description..." );
        logmsg( "  %-9.9s    %s \n", "-------", "-----------------------------------------------" );

        /* List standard formatted commands from our routing table... */

        for (pCmdTab = cmdtab; pCmdTab->statement; pCmdTab++)
        {
            if ( (pCmdTab->type & PANEL) && (pCmdTab->shortdesc))
                logmsg( _("  %-9.9s    %s \n"), pCmdTab->statement, pCmdTab->shortdesc );
        }

    }
    else
    {
        for (pCmdTab = cmdtab; pCmdTab->statement; pCmdTab++)
        {
            if (!strcasecmp(pCmdTab->statement,argv[1]) && (pCmdTab->type & PANEL) )
            {
                logmsg( _("%s: %s\n"),pCmdTab->statement,pCmdTab->shortdesc);
                if(pCmdTab->longdesc)
                    logmsg( _("%s\n"),pCmdTab->longdesc );
                return 0;
            }
        }
    
        logmsg( _("HHCPN142I Command %s not found - no help available\n"),argv[1]);
        return -1;
    }
    return 0;
}
    

int scr_recursion_level();

#if defined(OPTION_DYNAMIC_LOAD)
DLL_EXPORT void *panel_command_r (void *cmdline)
#else
void *panel_command (void *cmdline)
#endif
{
#define MAX_CMD_LEN (32768)
    char  cmd[MAX_CMD_LEN];             /* Copy of panel command     */
    char *pCmdLine;
    unsigned i;
    int noredisp;

    pCmdLine = cmdline; ASSERT(pCmdLine);
    /* every command will be stored in history list */
    /* except null commands and script commands */
    if (*pCmdLine != 0 && scr_recursion_level()  == 0)
        history_add(cmdline);

    /* Copy panel command to work area, skipping leading blanks */

    /* If the command starts with a -, then strip it and indicate
     * we do not want command redisplay
     */

    noredisp=0;
    while (*pCmdLine && isspace(*pCmdLine)) pCmdLine++;
    i = 0;
    while (*pCmdLine && i < (MAX_CMD_LEN-1))
    {
        if(i==0 && *pCmdLine=='-')
        {
            noredisp=1;
            /* and remove blanks again.. */
            while (*pCmdLine && isspace(*pCmdLine)) pCmdLine++;
        }
        else
        {
            cmd[i] = *pCmdLine;
            i++;
        }
        pCmdLine++;
    }
    cmd[i] = 0;

    /* Ignore null commands (just pressing enter)
       unless instruction stepping is enabled or
       commands are being sent to the SCP by default. */
    if (!sysblk.inststep && (0 == cmd[0]))
        return NULL;

    /* Echo the command to the control panel */
    if(!noredisp)
    {
        logmsg( "%s\n", cmd);
    }

    ProcessPanelCommand(cmd);

    return NULL;
}
