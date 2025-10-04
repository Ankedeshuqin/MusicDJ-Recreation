typedef struct bpt {
    int part;
    int bar;
} BLOCKPOINT;

typedef struct cp {
    int (*melody)[99];
    BLOCKPOINT *pbptCursor;
    int *piBarOffset;
    int *piBlockSize;
    BLOCKPOINT *pbptCopied;
    int *piCurPlayingBar;
} CHILD_PARAMS;

typedef struct ip {
    BLOCKPOINT bptCursor;
    int *piPrevSelBlocks;
    int (*melody)[99];
} INSERT_PARAMS;

typedef struct sbp {
    int part;
    int *pblockSel;
} SELBLOCK_PARAMS;

typedef struct trkdata {
    DWORD cb;
    BYTE *pbData;
    DWORD cTk;
} TRACKDATA;

typedef TRACKDATA BLOCKDATA[3];
typedef TRACKDATA MELODYDATA[12]; // 3 tracks * 4 parts

typedef struct men {
    MIDIEVENT mevt;
    struct men *pmenNext;
} MIDIEVENTNODE;

enum eAppMsg {
    WM_APP_REPAINTPANEL = WM_APP,
    WM_APP_UPDATELEFTKEY,
    WM_APP_CURSORMOVED,
    WM_APP_PLAYBLOCK // wParam: part, lParam: block
};

enum eLeftBtnFn {
    LBTN_INSERT, LBTN_COPY, LBTN_PASTE, LBTN_STOP
};

/* Functions in MusicDJ.cpp */
int GetPartLen(int (*)[99], int);

/* Functions in ChildProc.cpp */
LRESULT CALLBACK MelodyProc(HWND, UINT, WPARAM, LPARAM);
LRESULT CALLBACK BarNumProc(HWND, UINT, WPARAM, LPARAM);
LRESULT CALLBACK PartIconProc(HWND, UINT, WPARAM, LPARAM);
LRESULT CALLBACK NumInputShowProc(HWND, UINT, WPARAM, LPARAM);

/* Functions in DlgProc.cpp */
BOOL CALLBACK SetTempoDlgProc(HWND, UINT, WPARAM, LPARAM);
BOOL CALLBACK InsertDlgProc(HWND, UINT, WPARAM, LPARAM);
LRESULT CALLBACK SelBlockProc(HWND, UINT, WPARAM, LPARAM);

/* Functions in ReadWrit.cpp */
BOOL LoadMelody(LPCWSTR, int (*)[99], int *);
void LoadBlocksData(BLOCKDATA (*)[32]);
void FreeBlocksData(BLOCKDATA (*)[32]);
void GetMelodyData(int (*)[99], MELODYDATA);
void FreeMelodyData(MELODYDATA);
void SaveMelody(LPCWSTR, int (*)[99], int, BOOL);
MIDIEVENTNODE *GetMelodyMEvtLink(int (*)[99]);
void FreeMelodyMEvtLink(MIDIEVENTNODE *);
MIDIEVENTNODE *FillMelodyStrmBuf(MIDIHDR *, MIDIEVENTNODE *);

#define STRMBUFLEN 8192