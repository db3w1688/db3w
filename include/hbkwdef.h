/*
 *  DB3W -- A database application development and deployment platform
 *  HB*Engine -- A 64-bit dBASE/xBase compatible database server
 *  HB*Commander -- A command line HB*Engine client
 *  DB3W.CGI -- A Web application delivery client for HB*Engine
 *
 *  Module Name -- include/hbkwdef.h
 *
 *  Copyright (C) 1997-2019 Kimberlite Software <info@kimberlitesoftware.com>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License version 2
 *  as published by the Free Software Foundation.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along
 *  with this program; if not, write to the Free Software Foundation, Inc.,
 *  51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#define MAXKWLEN  64
#define KWTABSZ   0xd9

#define ABS                                0x01
#define ACCEPT                             0x02
#define ALIAS                              0x03
#define ALL                                0x04
#define APPEND                             0x05
#define ARRAY                              0x06
#define ASC                                0x07
#define AT                                 0x08
#define ATROOT                             0xc5
#define AUTH                               0xba
#define AUTO                               0xc7
#define AVAIL                              0xcb
#define _BACKWARD                          0x09
#define BETWEEN                            0x0a
#define BLANK                              0x0b
#define BOF_                               0x0c
#define BOTTOM                             0x0d
#define CANCEL                             0xd1
#define CASE                               0x0e
#define CD_                                0x0f
#define CDOW                               0x10
#define CENTURY                            0x11 //deprecated
#define K_CHARCTR                          0x12
#define CHDIR                              0x13
#define CHR                                0x14
#define CID                                0xcc
#define RESET_                             0x15
#define CMONTH                             0x16
#define COMMIT                             0xcf
#define CONTINUE                           0x17
#define COPY                               0x18
#define COUNT                              0x19
#define CREATE                             0x1a
#define CRYPT                              0x1b
#define CTOD                               0x1c
#define DATE_                              0x1d
#define DATEFMT                            0xd9
#define DAY                                0x1e
#define DBF_                               0x1f
#define DEBUG                              0xad
#define DECIMAL                            0x20
#define DEFAULT                            0x21 //deprecated
#define DEFINED                            0xbc
#define DELETE                             0x22
#define DELETED                            0x23
#define DELIMITED                          0x24
#define DENYWRITE_                         0xd2
#define DFPI                               0xd8
#define DIRLST                             0x25
#define DISKSIZE                           0xae
#define DISKSPACE                          0xaf
#define DISPLAY                            0x26
#define DO                                 0x27
#define DOW                                0x28
#define DTOC                               0x29
#define ELSE                               0x2a
#define ELSEIF                             0x2b
#define ENDCASE                            0x2c
#define ENDDO                              0x2d
#define ENDIF                              0x2e
#define EOF_                               0x2f
#define EPOCH                              0xaa
#define EXACT                              0x30
#define EXCEPT                             0x31
#define EXCLUSIVE                          0x32
#define EXECUTE                            0x33
#define EXIT                               0x34
#define EXP                                0x35
#define EXPORT                             0xc4
#define FAULT                              0xb9
#define FIELDCNT                           0x36
#define FIELDNAME                          0x37
#define FIELDNO                            0x38
#define FIELDS                             0x39
#define FIELDSIZE                          0x3a
#define FILE_                              0x3b
#define FILTER                             0x3c
#define FINAL                              0x3d
#define FIND                               0x3e
#define FLOCK                              0x3f
#define FOR                                0x40
#define FORM                               0x41
#define _FORWARD                           0x42
#define FOUND                              0x43
#define FROM                               0x44
#define FUNCTION                           0x45
#define GO                                 0x46
#define HASH                               0xb7
#define HBX                                0x47
#define HEX                                0xb4
#define HELP                               0xc3
#define IF                                 0x48
#define IIF                                0x49
#define INDEX                              0x4a
#define INLIST                             0x4b
#define INPUT                              0x4c
#define INT                                0x4d
#define INTO                               0x4e
#define ISALPHA                            0x4f
#define ISDIGIT                            0x50
#define ISEMPTY                            0xc6
#define ISLOWER                            0x51
#define ISNULL                             0xb6
#define ISUPPER                            0x52
#define KEY                                0x53
#define LEFT                               0x54
#define LEN                                0x55
#define LIKE                               0x56
#define LOCATE                             0x57
#define LOG                                0x58
#define LOG10                              0x59
#define K_LOGIC                            0x5a
#define LOOP                               0x5b
#define LOWER                              0x5c
#define LTRIM                              0x5d
#define MAX                                0x5e
#define MD_                                0xb2
#define MEMORY                             0x5f
#define MIN                                0x60
#define MKDIR                              0xb3
#define MKTMP                              0xbd
#define MOD                                0x61
#define MONTH                              0x62
#define NDX_                               0x63
#define NEXT                               0x64
#define NULL_                              0xd3
#define K_NUMBER                           0x65
#define OCCURS                             0x66
#define OFF                                0x67
#define OFFSET                             0xca
#define ON                                 0x68
#define ORDER                              0x69
#define OTHERWISE                          0x6a
#define OWNER                              0xc1
#define PACK                               0x6b
#define PADC                               0x6c
#define PADL                               0x6d
#define PADR                               0x6e
#define PARAMETER                          0x6f
#define PARENT                             0xc2
#define PATH                               0x70
#define PEER                               0xbe
#define PI                                 0x71
#define PREV                               0xd5
#define PRINT                              0x72
#define PRIVATE                            0x73
#define PROCEDURE                          0x74
#define PROPER                             0x75
#define PUBLIC                             0x76
#define QSORT                              0xac
#define QUIT                               0x77
#define RAND                               0x78
#define RANDOM                             0xb1
#define RANDSTR                            0xab
#define READ_                              0x79
#define READONLY                           0x7a
#define RECALL                             0x7b
#define RECCOUNT                           0x7c
#define RECNO                              0x7d
#define RECORD                             0x7e
#define RECSIZE                            0x7f
#define REINDEX                            0x80
#define RELATION                           0x81
#define RELEASE                            0x82
#define RENAME                             0xb5
#define REPLACE                            0x83
#define REPLICATE                          0x84
#define RETURN_                            0x85
#define RIGHT                              0x86
#define RLOCK                              0x87
#define ROLLBACK                           0xd0
#define ROUND                              0x88
#define RTRIM                              0x89
#define SDF                                0x8a
#define SEEK                               0x8b
#define SELECT                             0x8c
#define SERIAL                             0x8d
#define SET                                0x8e
#define SHARED                             0xd7
#define SKIP                               0x8f
#define SLEEP                              0xc9
#define SOUNDEX                            0x90
#define SPACE                              0x91
#define SPELLNUM                           0x92
#define SQRT                               0x93
#define STAMP                              0xb8
#define START                              0xce
#define STATUS_                            0xd4
#define STORE                              0x94
#define STR                                0x95
#define STRUCTURE                          0x96
#define STUFF                              0x97
#define SU                                 0xc0
#define SUBSTR                             0x98
#define SYSTEM_                            0x99
#define TALK                               0x9a
#define TIME_                              0x9b
#define TIMESTR                            0xbb
#define TO                                 0x9c
#define TOP                                0x9d
#define TRANSAC                            0xcd
#define TRIM                               0x9e
#define TTOC                               0x9f
#define TYPE_                              0xa0
#define UNIQUE                             0xa1
#define UNLOCK                             0xa2
#define UPDATE                             0xb0
#define UPPER                              0xa3
#define USE                                0xa4
#define USING                              0xd6
#define VAL                                0xa5
#define WHILE                              0xa6
#define WHOAMI                             0xbf
#define WIDTH                              0xa7
#define WITH                               0xa8
#define WOK                                0xc8
#define YEAR                               0xa9

/*********************** FUNCTIONS ******************************/
#define MAXFCN                             0x61

#define ABS_FCN                            0x00
#define ALIAS_FCN                          0x01
#define ASC_FCN                            0x02
#define AT_FCN                             0x03
#define ATROOT_FCN                         0x5d
#define AUTH_FCN                           0x56
#define BETWEEN_FCN                        0x04
#define BOF_FCN                            0x05
#define CDOW_FCN                           0x06
#define CHR_FCN                            0x07
#define CID_FCN                            0x60
#define CMONTH_FCN                         0x08
#define CRYPT_FCN                          0x09
#define CTOD_FCN                           0x0a
#define DATE_FCN                           0x0b
#define DAY_FCN                            0x0c
#define DBF_FCN                            0x0d
#define DEFINED_FCN                        0x58
#define DELETED_FCN                        0x0e
#define DISKSIZE_FCN                       0x51
#define DISKSPACE_FCN                      0x52
#define DOW_FCN                            0x0f
#define DTOC_FCN                           0x10
#define EOF_FCN                            0x11
#define EPOCH_FCN                          0x50
#define EXP_FCN                            0x12
#define FAULT_FCN                          0x55
#define FIELDCNT_FCN                       0x13
#define FIELDNAME_FCN                      0x14
#define FIELDNO_FCN                        0x15
#define FIELDSIZE_FCN                      0x16
#define FILE_FCN                           0x17
#define FILTER_FCN                         0x18
#define FLOCK_FCN                          0x19
#define FOUND_FCN                          0x1a
#define HASH_FCN                           0x54
#define IIF_FCN                            0x1b
#define INDEX_FCN                          0x1c
#define INLIST_FCN                         0x1d
#define INT_FCN                            0x1e
#define ISALPHA_FCN                        0x1f
#define ISDIGIT_FCN                        0x20
#define ISEMPTY_FCN                        0x5e
#define ISLOWER_FCN                        0x21
#define ISNULL_FCN                         0x53
#define ISUPPER_FCN                        0x22
#define KEY_FCN                            0x23
#define LEFT_FCN                           0x24
#define LEN_FCN                            0x25
#define LOCATE_FCN                         0x26
#define LOG_FCN                            0x27
#define LOG10_FCN                          0x28
#define LOWER_FCN                          0x29
#define LTRIM_FCN                          0x2a
#define MAX_FCN                            0x2b
#define MIN_FCN                            0x2c
#define MKTMP_FCN                          0x59
#define MOD_FCN                            0x2d
#define MONTH_FCN                          0x2e
#define NDX_FCN                            0x2f
#define OCCURS_FCN                         0x30
#define PADC_FCN                           0x31
#define PADL_FCN                           0x32
#define PADR_FCN                           0x33
#define PARENT_FCN                         0x5c
#define PATH_FCN                           0x34
#define PEER_FCN                           0x5a
#define PI_FCN                             0x35
#define PROPER_FCN                         0x36
#define RAND_FCN                           0x37
#define RANDSTR_FCN                        0x4f
#define RECCOUNT_FCN                       0x38
#define RECNO_FCN                          0x39
#define RECSIZE_FCN                        0x3a
#define REPLICATE_FCN                      0x3b
#define RIGHT_FCN                          0x3c
#define RLOCK_FCN                          0x3d
#define ROUND_FCN                          0x3e
#define RTRIM_FCN                          0x3f
#define SERIAL_FCN                         0x40
#define SOUNDEX_FCN                        0x41
#define SPACE_FCN                          0x42
#define SPELLNUM_FCN                       0x43
#define SQRT_FCN                           0x44
#define STR_FCN                            0x45
#define STUFF_FCN                          0x46
#define SUBSTR_FCN                         0x47
#define SYSTEM_FCN                         0x48
#define TIME_FCN                           0x49
#define TIMESTR_FCN                        0x57
#define TTOC_FCN                           0x4a
#define TYPE_FCN                           0x4b
#define UPPER_FCN                          0x4c
#define WHOAMI_FCN                         0x5b
#define WOK_FCN                            0x5f
#define VAL_FCN                            0x4d
#define YEAR_FCN                           0x4e

/*********************** COMMANDS *******************************/
#define NULL_CMD                           0x00
#define PRINT_CMD                          0x01
#define ACCEPT_CMD                         0x02
#define APPEND_CMD                         0x03
#define ARRAY_CMD                          0x04
#define RESET_CMD                          0x05
#define CHDIR_CMD                          0x06
#define CONTINUE_CMD                       0x07
#define COPY_CMD                           0x08
#define COUNT_CMD                          0x09
#define CREATE_CMD                         0x0a
#define DELETE_CMD                         0x0b
#define DIRLST_CMD                         0x0c
#define DISPLAY_CMD                        0x0d
#define EXECUTE_CMD                        0x0e
#define FIND_CMD                           0x0f
#define GO_CMD                             0x10
#define INDEX_CMD                          0x11
#define LOCATE_CMD                         0x12
#define MKDIR_CMD                          0x13
#define PACK_CMD                           0x14
#define PARAMETER_CMD                      0x15
#define PRIVATE_CMD                        0x16
#define PUBLIC_CMD                         0x17
#define QUIT_CMD                           0x18
#define READ_CMD                           0x19
#define RECALL_CMD                         0x1a
#define REINDEX_CMD                        0x1b
#define RELEASE_CMD                        0x1c
#define REPLACE_CMD                        0x1d
#define SEEK_CMD                           0x1e
#define SELECT_CMD                         0x1f
#define SET_CMD                            0x20
#define SKIP_CMD                           0x21
#define STORE_CMD                          0x22
#define UNLOCK_CMD                         0x23
#define USE_CMD                            0x24
#define UPDATE_CMD                         0x25
#define RENAME_CMD                         0x26
#define SU_CMD                             0x27
#define ASSIGN_CMD                         0x28
#define HELP_CMD                           0x29
#define EXPORT_CMD                         0x2a
#define SLEEP_CMD                          0x2b
#define TRANSAC_CMD                        0x2c
#define SERIAL_CMD                         0x2d

/***** Execution control keywords *********/
#define CASE_CTL                           0x80
#define DO_CTL                             0x81
#define ELSE_CTL                           0x82
#define ELSEIF_CTL                         0x83
#define ENDCASE_CTL                        0x84
#define ENDDO_CTL                          0x85
#define ENDIF_CTL                          0x86
#define EXIT_CTL                           0x87
#define FUNCTION_CTL                       0x88
#define IF_CTL                             0x89
#define LOOP_CTL                           0x8a
#define OTHERWISE_CTL                      0x8b
#define PROCEDURE_CTL                      0x8c
#define RETURN_CTL                         0x8d
#define WHILE_CTL                          0x8e

#define is_control(cmd)                    (0x80 & cmd)

#define KW_COMMENT                         0x1000
#define KW_UNKNOWN                         0x0fff
#define KW_UNDEF                           -1

#ifdef KW_MAIN
HBKW keywords[KWTABSZ] = {
{ "ABS",      ABS,           -1,              ABS_FCN,                -1               },
{ "ACCEPT",   ACCEPT,        ACCEPT_CMD,      -1,                     -1               },
{ "ALIAS",    ALIAS,         -1,              ALIAS_FCN,              -1               },
{ "ALL",      ALL,           -1,              -1,                     -1               },
{ "APPEND",   APPEND,        APPEND_CMD,      -1,                     -1               },
{ "ARRAY",    ARRAY,         ARRAY_CMD,       -1,                     -1               },
{ "ASC",      ASC,           -1,              ASC_FCN,                -1               },
{ "AT",       AT,            -1,              AT_FCN,                 -1               },
{ "ATROOT",   ATROOT,        -1,              ATROOT_FCN,             -1               },
{ "AUTH",     AUTH,          -1,              AUTH_FCN,               -1,              },
{ "AUTO",     AUTO,          -1,              -1,                     -1,              },
{ "AVAILABLE",AVAIL,         -1,              -1,                     -1,              },
{ "BACKWARD", _BACKWARD,     -1,              -1,                     -1               },
{ "BETWEEN",  BETWEEN,       -1,              BETWEEN_FCN,            -1               },
{ "BLANK",    BLANK,         -1,              -1,                     -1               },
{ "BOF",      BOF_,          -1,              BOF_FCN,                -1               },
{ "BOTTOM",   BOTTOM,        -1,              -1,                     -1               },
{ "CANCEL",   CANCEL,        -1,              -1,                     -1               },
{ "CASE",     CASE,          CASE_CTL,        -1,                     -1               },
{ "CD",       CD_,           CHDIR_CMD,       -1,                     -1               },
{ "CDOW",     CDOW,          -1,              CDOW_FCN,               -1               },
//{ "CENTURY",  CENTURY,       -1,              -1,                     CENTURY_ON       },
{ "CENTURY",  CENTURY,       -1,              -1,                     -1               }, //deprecated
{ "CHARACTER",K_CHARCTR,     -1,              -1,                     -1               },
{ "CHDIR",    CHDIR,         CHDIR_CMD,       -1,                     -1               },
{ "CHR",      CHR,           -1,              CHR_FCN,                -1               },
{ "CID",      CID,           -1,              CID_FCN,                -1               },
{ "CMONTH",   CMONTH,        -1,              CMONTH_FCN,             -1               },
{ "COMMIT",   COMMIT,        -1,              -1,                     -1               },
{ "CONTINUE", CONTINUE,      CONTINUE_CMD,    -1,                     -1               },
{ "COPY",     COPY,          COPY_CMD,        -1,                     -1               },
{ "COUNT",    COUNT,         COUNT_CMD,       -1,                     -1               },
{ "CREATE",   CREATE,        CREATE_CMD,      -1,                     -1               },
{ "CRYPT",    CRYPT,         -1,              CRYPT_FCN,              -1               },
{ "CTOD",     CTOD,          -1,              CTOD_FCN,               -1               },
{ "DATE",     DATE_,         -1,              DATE_FCN,               DATEFMT_TO       },
{ "DATEFMT",  DATEFMT,       -1,              -1,                     DATEFMT_TO       },
{ "DAY",      DAY,           -1,              DAY_FCN,                -1               },
{ "DBF",      DBF_,          -1,              DBF_FCN,                -1               },
{ "DEBUG",    DEBUG,         -1,              -1,                     DEBUG_ON         },
{ "DECIMAL",  DECIMAL,       -1,              -1,                     DECI_TO          },
{ "DEFAULT",  DEFAULT,       -1,              -1,                     DEFA_TO          },
{ "DEFINED",  DEFINED,       -1,              DEFINED_FCN,            -1               },
{ "DELETE",   DELETE,        DELETE_CMD,      -1,                     DELETE_ON        },
{ "DELETED",  DELETED,       -1,              DELETED_FCN,            -1               },
{ "DELIMITED",DELIMITED,     -1,              -1,                     -1               },
{ "DENYWRITE",DENYWRITE_,    -1,              -1,                     -1               },
{ "DFPI",     DFPI,          -1,              -1,                     DFPI_ON          },
{ "DIR",      DIRLST,        DIRLST_CMD,      -1,                     -1               },
{ "DISKSIZE", DISKSIZE,      -1,              DISKSIZE_FCN,           -1               },
{ "DISKSPACE",DISKSPACE,     -1,              DISKSPACE_FCN,          -1               },
{ "DISPLAY",  DISPLAY,       DISPLAY_CMD,     -1,                     -1               },
{ "DO",       DO,            DO_CTL,          -1,                     -1               },
{ "DOW",      DOW,           -1,              DOW_FCN,                -1               },
{ "DTOC",     DTOC,          -1,              DTOC_FCN,               -1               },
{ "ELSE",     ELSE,          ELSE_CTL,        -1,                     -1               },
{ "ELSEIF",   ELSEIF,        ELSEIF_CTL,      -1,                     -1               },
{ "ENDCASE",  ENDCASE,       ENDCASE_CTL,     -1,                     -1               },
{ "ENDDO",    ENDDO,         ENDDO_CTL,       -1,                     -1               },
{ "ENDIF",    ENDIF,         ENDIF_CTL,       -1,                     -1               },
{ "EOF",      EOF_,          -1,              EOF_FCN,                -1               },
{ "EPOCH",    EPOCH,         -1,              EPOCH_FCN,              EPOCH_TO         },
{ "EXACT",    EXACT,         -1,              -1,                     EXACT_ON         },
{ "EXCEPT",   EXCEPT,        -1,              -1,                     -1               },
{ "EXCLUSIVE",EXCLUSIVE,     -1,              -1,                     EXCL_ON          },
{ "EXECUTE",  EXECUTE,       EXECUTE_CMD,     -1,                     -1               },
{ "EXIT",     EXIT,          EXIT_CTL,        -1,                     -1               },
{ "EXP",      EXP,           -1,              EXP_FCN,                -1               },
{ "EXPORT",   EXPORT,        EXPORT_CMD,      -1,                     -1               },
{ "FAULT",    FAULT,         -1,              FAULT_FCN,              -1               },
{ "FIELDCNT", FIELDCNT,      -1,              FIELDCNT_FCN,           -1               },
{ "FIELDNAME",FIELDNAME,     -1,              FIELDNAME_FCN,          -1               },
{ "FIELDNO",  FIELDNO,       -1,              FIELDNO_FCN,            -1               },
{ "FIELDS",   FIELDS,        -1,              -1,                     -1               },
{ "FIELDSIZE",FIELDSIZE,     -1,              FIELDSIZE_FCN,          -1               },
{ "FILE",     FILE_,         -1,              FILE_FCN,               -1               },
{ "FILTER",   FILTER,        -1,              FILTER_FCN,             FILTER_TO        },
{ "FINAL",    FINAL,         -1,              -1,                     -1               },
{ "FIND",     FIND,          FIND_CMD,        -1,                     -1               },
{ "FLOCK",    FLOCK,         -1,              FLOCK_FCN,              -1               },
{ "FOR",      FOR,           -1,              -1,                     -1               },
{ "FORM",     FORM,          -1,              -1,                     FORM_TO          },
{ "FORWARD",  _FORWARD,      -1,              -1,                     -1               },
{ "FOUND",    FOUND,         -1,              FOUND_FCN,              -1               },
{ "FROM",     FROM,          -1,              -1,                     -1               },
{ "FUNCTION", FUNCTION,      FUNCTION_CTL,    -1,                     -1               },
{ "GO",       GO,            GO_CMD,          -1,                     -1               },
{ "HASH",     HASH,          -1,              HASH_FCN,               -1               },
{ "HBX",      HBX,           -1,              -1,                     -1               },
{ "HELP",     HELP,          HELP_CMD,        -1,                     -1               },
{ "HEX",      HEX,           -1,              -1,                     -1               },
{ "IF",       IF,            IF_CTL,          -1,                     -1               },
{ "IIF",      IIF,           -1,              IIF_FCN,                -1               },
{ "INDEX",    INDEX,         INDEX_CMD,       INDEX_FCN,              INDEX_TO         },
{ "INLIST",   INLIST,        -1,              INLIST_FCN,             -1               },
{ "INPUT",    INPUT,         ACCEPT_CMD,      -1,                     -1               },
{ "INT",      INT,           -1,              INT_FCN,                -1               },
{ "INTO",     INTO,          -1,              -1,                     -1               },
{ "ISALPHA",  ISALPHA,       -1,              ISALPHA_FCN,            -1               },
{ "ISDIGIT",  ISDIGIT,       -1,              ISDIGIT_FCN,            -1               },
{ "ISEMPTY",  ISEMPTY,       -1,              ISEMPTY_FCN,            -1               },
{ "ISLOWER",  ISLOWER,       -1,              ISLOWER_FCN,            -1               },
{ "ISNULL",   ISNULL,        -1,              ISNULL_FCN,             -1               },
{ "ISUPPER",  ISUPPER,       -1,              ISUPPER_FCN,            -1               },
{ "KEY",      KEY,           -1,              KEY_FCN,                -1               },
{ "LEFT",     LEFT,          -1,              LEFT_FCN,               -1               },
{ "LEN",      LEN,           -1,              LEN_FCN,                -1               },
{ "LIKE",     LIKE,          -1,              -1,                     -1               },
{ "LOCATE",   LOCATE,        LOCATE_CMD,      LOCATE_FCN,             -1               },
{ "LOG",      LOG,           -1,              LOG_FCN,                -1               },
{ "LOG10",    LOG10,         -1,              LOG10_FCN,              -1               },
{ "LOGIC",    K_LOGIC,       -1,              -1,                     -1               },
{ "LOOP",     LOOP,          LOOP_CTL,        -1,                     -1               },
{ "LOWER",    LOWER,         -1,              LOWER_FCN,              -1               },
{ "LTRIM",    LTRIM,         -1,              LTRIM_FCN,              -1               },
{ "MAX",      MAX,           -1,              MAX_FCN,                -1               },
{ "MD",       MD_,           MKDIR_CMD,       -1,                     -1               },
{ "MEMORY",   MEMORY,        -1,              -1,                     -1               },
{ "MIN",      MIN,           -1,              MIN_FCN,                -1               },
{ "MKDIR",    MKDIR,         MKDIR_CMD,       -1,                     -1               },
{ "MKTMP",    MKTMP,         -1,              MKTMP_FCN,              -1               },
{ "MOD",      MOD,           -1,              MOD_FCN,                -1               },
{ "MONTH",    MONTH,         -1,              MONTH_FCN,              -1               },
{ "NDX",      NDX_,          -1,              NDX_FCN,                -1               },
{ "NEXT",     NEXT,          -1,              -1,                     -1               },
{ "NULL",     NULL_,         -1,              -1,                     NULL_ON          },
{ "NUMBER",   K_NUMBER,      -1,              -1,                     -1               },
{ "OCCURS",   OCCURS,        -1,              OCCURS_FCN,             -1               },
{ "OFF",      OFF,           -1,              -1,                     -1               },
{ "OFFSET",   OFFSET,        -1,              -1,                     -1               },
{ "ON",       ON,            -1,              -1,                     -1               },
{ "ORDER",    ORDER,         -1,              -1,                     ORDER_TO         },
{ "OTHERWISE",OTHERWISE,     OTHERWISE_CTL,   -1,                     -1               },
{ "OWNER",    OWNER,         -1,              -1,                     -1               },
{ "PACK",     PACK,          PACK_CMD,        -1,                     -1               },
{ "PADC",     PADC,          -1,              PADC_FCN,               -1               },
{ "PADL",     PADL,          -1,              PADL_FCN,               -1               },
{ "PADR",     PADR,          -1,              PADR_FCN,               -1               },
{ "PARAMETERS",PARAMETER,    PARAMETER_CMD,   -1,                     -1               },
{ "PARENT",   PARENT,        -1,              PARENT_FCN,             -1,              },
{ "PATH",     PATH,          -1,              PATH_FCN,               PATH_TO          },
{ "PEER",     PEER,          -1,              PEER_FCN,               -1,              },
{ "PI",       PI,            -1,              PI_FCN,                 -1               },
{ "PREVIOUS", PREV,          -1,              -1,                     -1               },
{ "PRINT",    PRINT,         PRINT_CMD,       -1,                     -1               },
{ "PRIVATE",  PRIVATE,       PRIVATE_CMD,     -1,                     -1               },
{ "PROCEDURE",PROCEDURE,     PROCEDURE_CTL,   -1,                     PROC_TO          },
{ "PROPER",   PROPER,        -1,              PROPER_FCN,             -1               },
{ "PUBLIC",   PUBLIC,        PUBLIC_CMD,      -1,                     -1               },
{ "QSORT",    QSORT,         -1,              -1,                     -1               },
{ "QUIT",     QUIT,          QUIT_CMD,        -1,                     -1               },
{ "RAND",     RAND,          -1,              RAND_FCN,               -1               },
{ "RANDOM",   RANDOM,        -1,              -1,                     -1               },
{ "RANDSTR",  RANDSTR,       -1,              RANDSTR_FCN,            -1               },
{ "READ",     READ_,         READ_CMD,        -1,                     -1               },
{ "READONLY", READONLY,      -1,              -1,                     READONLY_ON,     },
{ "RECALL",   RECALL,        RECALL_CMD,      -1,                     -1               },
{ "RECCOUNT", RECCOUNT,      -1,              RECCOUNT_FCN,           -1               },
{ "RECNO",    RECNO,         -1,              RECNO_FCN,              -1               },
{ "RECORD",   RECORD,        -1,              -1,                     -1               },
{ "RECSIZE",  RECSIZE,       -1,              RECSIZE_FCN,            -1               },
{ "REINDEX",  REINDEX,       REINDEX_CMD,     -1,                     -1               },
{ "RELATION", RELATION,      -1,              -1,                     RELA_TO          },
{ "RELEASE",  RELEASE,       RELEASE_CMD,     -1,                     -1               },
{ "RENAME",   RENAME,        RENAME_CMD,      -1,                     -1               },
{ "REPLACE",  REPLACE,       REPLACE_CMD,     -1,                     -1               },
{ "REPLICATE",REPLICATE,     -1,              REPLICATE_FCN,          -1               },
{ "RESET",    RESET_,        RESET_CMD,       -1,                     -1               },
{ "RETURN",   RETURN_,       RETURN_CTL,      -1,                     -1               },
{ "RIGHT",    RIGHT,         -1,              RIGHT_FCN,              -1               },
{ "RLOCK",    RLOCK,         -1,              RLOCK_FCN,              -1               },
{ "ROLLBACK", ROLLBACK,      -1,              -1,                     -1               },
{ "ROUND",    ROUND,         -1,              ROUND_FCN,              -1               },
{ "RTRIM",    RTRIM,         -1,              RTRIM_FCN,              -1               },
{ "SDF",      SDF,           -1,              -1,                     -1               },
{ "SEEK",     SEEK,          SEEK_CMD,        -1,                     -1               },
{ "SELECT",   SELECT,        SELECT_CMD,      -1,                     -1               },
{ "SERIAL",   SERIAL,        SERIAL_CMD,      SERIAL_FCN,             -1               },
{ "SET",      SET,           SET_CMD,         -1,                     -1               },
{ "SHARED",   SHARED,        -1,              -1,                     -1               },
{ "SKIP",     SKIP,          SKIP_CMD,        -1,                     -1               },
{ "SLEEP",    SLEEP,         SLEEP_CMD,       -1,                     -1               },
{ "SOUNDEX",  SOUNDEX,       -1,              SOUNDEX_FCN,            -1               },
{ "SPACE",    SPACE,         -1,              SPACE_FCN,              -1               },
{ "SPELLNUM", SPELLNUM,      -1,              SPELLNUM_FCN,           -1               },
{ "SQRT",     SQRT,          -1,              SQRT_FCN,               -1               },
{ "STAMP",    STAMP,         -1,              -1,                     -1               },
{ "START",    START,         -1,              -1,                     -1               },
{ "STATUS",   STATUS_,       -1,              -1,                     -1               },
{ "STORE",    STORE,         STORE_CMD,       -1,                     -1               },
{ "STR",      STR,           -1,              STR_FCN,                -1               },
{ "STRUCTURE",STRUCTURE,     -1,              -1,                     -1               },
{ "STUFF",    STUFF,         -1,              STUFF_FCN,              -1               },
{ "SU",       SU,            SU_CMD,          -1,                     -1               },
{ "SUBSTR",   SUBSTR,        -1,              SUBSTR_FCN,             -1               },
{ "SYSTEM",   SYSTEM_,       -1,              SYSTEM_FCN,             -1               },
{ "TALK",     TALK,          -1,              -1,                     TALK_ON          },
{ "TIME",     TIME_,         -1,              TIME_FCN,               -1               },
{ "TIMESTR",  TIMESTR,       -1,              TIMESTR_FCN,            -1               },
{ "TO",       TO,            -1,              -1,                     -1               },
{ "TOP",      TOP,           -1,              -1,                     -1               },
{ "TRANSACTION",   TRANSAC,  TRANSAC_CMD,     -1,                     -1               },
{ "TRIM",     TRIM,          -1,              RTRIM_FCN,              -1               },
{ "TTOC",     TTOC,          -1,              TTOC_FCN,               -1               },
{ "TYPE",     TYPE_,         -1,              TYPE_FCN,               -1               },
{ "UNIQUE",   UNIQUE,        -1,              -1,                     UNIQUE_ON        },
{ "UNLOCK",   UNLOCK,        UNLOCK_CMD,      -1,                     -1               },
{ "UPDATE",   UPDATE,        UPDATE_CMD,      -1,                     -1               },
{ "UPPER",    UPPER,         -1,              UPPER_FCN,              -1               },
{ "USE",      USE,           USE_CMD,         -1,                     -1               },
{ "USING",    USING,         -1,              -1,                     -1               },
{ "VAL",      VAL,           -1,              VAL_FCN,                -1               },
{ "WHILE",    WHILE,         WHILE_CTL,       -1,                     -1               },
{ "WHOAMI",   WHOAMI,        -1,              WHOAMI_FCN,             -1               },
{ "WIDTH",    WIDTH,         -1,              -1,                     WIDTH_TO         },
{ "WITH",     WITH,          -1,              -1,                     -1               },
{ "WOK",      WOK,           -1,              WOK_FCN,                -1               },
{ "YEAR",     YEAR,          -1,              YEAR_FCN,               -1               }
};
#else
extern HBKW keywords[];
#endif
