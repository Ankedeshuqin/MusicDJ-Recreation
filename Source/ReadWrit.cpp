#include <Windows.h>
#include "MusicDJ.h"
#include "resource.h"

WORD CRC16(BYTE *pb, DWORD cb) {
    WORD wCRC = 0x0000;
    for(UINT u = 0; u < cb; u++) {
        wCRC ^= pb[u];
        for(int i = 0; i < 8; i++) {
            if(wCRC & 0x0001) {
                wCRC >>= 1;
                wCRC ^= 0xA001;
            } else {
                wCRC >>= 1;
            }
        }
    }
    return wCRC;
}

BYTE ReadByte(BYTE *pb, DWORD *pdwLoc) {
    return pb[(*pdwLoc)++];
}

void WriteByte(BYTE *pb, DWORD *pdwLoc, BYTE b) {
    pb[(*pdwLoc)++] = b;
}

DWORD ReadBEInt(BYTE *pb, DWORD *pdwLoc, DWORD cb) {
    DWORD dwRet = 0;
    for(UINT u = 0; u < cb; u++)
        dwRet = dwRet << 8 | ReadByte(pb, pdwLoc);
    return dwRet;
}

void WriteBEInt(BYTE *pb, DWORD *pdwLoc, DWORD dw, DWORD cb) {
    for(UINT u = cb; u > 0; u--)
        WriteByte(pb, pdwLoc, (BYTE)(dw >> ((u - 1) * 8)));
}

DWORD ReadVarLenInt(BYTE *pb, DWORD *pdwLoc) {
    DWORD dwRet = 0;
    BYTE b;
    do {
        b = ReadByte(pb, pdwLoc);
        dwRet = dwRet << 7 | (b & 127);
    } while(b & 128);
    return dwRet;
}

void WriteVarLenInt(BYTE *pb, DWORD *pdwLoc, DWORD dw) {
    BYTE bBuf[5];
    DWORD cb = 0;
    do {
        bBuf[cb++] = dw & 127;
        dw = dw >> 7;
    } while(dw);
    for(UINT u = cb; u > 0; u--) {
        BYTE b = bBuf[u - 1];
        if(u - 1 > 0)
            b |= 128;
        WriteByte(pb, pdwLoc, b);
    }
}

void ReadStr(BYTE *pb, DWORD *pdwLoc, char *lpsz, DWORD cb) {
    for(UINT u = 0; u < cb; u++)
        lpsz[u] = (char)ReadByte(pb, pdwLoc);
    lpsz[cb] = '\0';
}

void WriteStr(BYTE *pb, DWORD *pdwLoc, const char *lpsz) {
    UINT u = 0;
    while(lpsz[u]) {
        WriteByte(pb, pdwLoc, (BYTE)lpsz[u]);
        u++;
    }
}

BOOL LoadMelody(LPCWSTR pszFilePath, int (*melody)[99], int *piTempo) {
    HANDLE hFile = CreateFile(pszFilePath, GENERIC_READ, 0, NULL,
        OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if(hFile == INVALID_HANDLE_VALUE)
        return FALSE;
    
    DWORD dwFileSize = GetFileSize(hFile, NULL);
    BYTE *pb = (BYTE *)malloc(dwFileSize);
    DWORD dwDummy;
    ReadFile(hFile, pb, dwFileSize, &dwDummy, NULL);
    CloseHandle(hFile);

    DWORD dwLoc = 0;
    DWORD dwChkSize;
    char szChkName[5];
    
    /* Read MThd */
    if(dwLoc + 8 > dwFileSize) goto err;
    ReadStr(pb, &dwLoc, szChkName, 4);
    if(strcmp(szChkName, "MThd") != 0)
        goto err;
    dwChkSize = ReadBEInt(pb, &dwLoc, 4);
    if(dwChkSize != 6)
        goto err;
    if(dwLoc + 6 > dwFileSize) goto err;
    dwLoc += 6; // These don't matter

    /* Read SEM1 */
    if(dwLoc + 8 > dwFileSize) goto err;
    ReadStr(pb, &dwLoc, szChkName, 4);
    if(strcmp(szChkName, "SEM1") != 0)
        goto err;
    dwChkSize = ReadBEInt(pb, &dwLoc, 4);
    if(dwLoc + dwChkSize > dwFileSize) goto err;

    int part = 0, bar = 0, i;
    ZeroMemory(melody, sizeof(int[4][99]));
    do {
        i = ReadByte(pb, &dwLoc);
        if(i != 0xFF) { // 0xFF means end of a part
            if(bar == 99)
                goto err;
            melody[part][bar] = i;
            if(ReadByte(pb, &dwLoc) != (i - 1) / 8)
                goto err;
            bar++;
        } else {
            part++;
            bar = 0;
        }
    } while(part < 4);

    WORD wCRC = ReadBEInt(pb, &dwLoc, 2);
    if(wCRC != CRC16(&pb[14], dwChkSize + 6))
        goto err;

    /* Read tempo */
    if(dwLoc + 8 > dwFileSize) goto err;
    ReadStr(pb, &dwLoc, szChkName, 4);
    if(strcmp(szChkName, "MTrk") != 0)
        goto err;
    dwChkSize = ReadBEInt(pb, &dwLoc, 4);
    if(dwLoc + dwChkSize > dwFileSize) goto err;
    dwLoc += 15; // These are constant in normal
    if(ReadBEInt(pb, &dwLoc, 3) != 0xFF5103)
        goto err;
    *piTempo = 60000000 / ReadBEInt(pb, &dwLoc, 3);

    free(pb);
    return TRUE;

err:
    free(pb);
    return FALSE;
}

/* Used when loading blocks data */
void ReadTrackData(BYTE *pb, DWORD *pdwLoc, TRACKDATA *ptrkdata) {
    *pdwLoc += 4; // No need to judge chunk name
    ptrkdata->cb = ReadBEInt(pb, pdwLoc, 4);
    ptrkdata->pbData = (BYTE *)malloc(ptrkdata->cb);
    memcpy(ptrkdata->pbData, pb + *pdwLoc, ptrkdata->cb);

    /* Traverse events to get total ticks */
    DWORD dwCurTk = 0;
    BYTE bCurEvtStatus;
    DWORD cbCurEvtData;

    BOOL fIsEnd = FALSE;
    do {
        dwCurTk += ReadVarLenInt(pb, pdwLoc);

        if(pb[*pdwLoc] & 0x80)
            bCurEvtStatus = ReadByte(pb, pdwLoc);
        
        if(bCurEvtStatus < 0xF0) {
            (*pdwLoc)++;
            switch(bCurEvtStatus >> 4) {
            case 0x8:
            case 0x9:
            case 0xA:
            case 0xB:
            case 0xE:
                (*pdwLoc)++;
                break;
            }
        } else { // Meta events
            fIsEnd = ReadByte(pb, pdwLoc) == 0x2F;
            cbCurEvtData = ReadVarLenInt(pb, pdwLoc);
            *pdwLoc += cbCurEvtData;
        }
    } while(!fIsEnd);

    ptrkdata->cTk = dwCurTk;
}

void FreeTrackData(TRACKDATA *ptrkdata) {
    free(ptrkdata->pbData);
}

/* Load blocks data from resource */
void LoadBlocksData(BLOCKDATA (*lpblkdata)[32]) {
    for(int part = 0; part < 4; part++) {
        for(int block = 0; block < 32; block++) {
            HRSRC hrsrc = FindResource(NULL, MAKEINTRESOURCE(IDR_BLK101 + part * 32 + block), RT_RCDATA);
            HGLOBAL hRes = LoadResource(NULL, hrsrc);
            DWORD cbRes = SizeofResource(NULL, hrsrc);
            BYTE *pbRes = (BYTE *)LockResource(hRes);

            ZeroMemory(lpblkdata[part][block], sizeof(BLOCKDATA));

            DWORD dwLoc = 63; // No need to judge MThd & SEM1 chunk and first (tempo) track
            int iTrk = 0;
            while(dwLoc < cbRes) {
                ReadTrackData(pbRes, &dwLoc, &lpblkdata[part][block][iTrk]);
                iTrk++;
            }
        }
    }
}

/* Free blocks data loaded from resource */
void FreeBlocksData(BLOCKDATA (*lpblkdata)[32]) {
    for(int part = 0; part < 4; part++) {
        for(int block = 0; block < 32; block++) {
            for(int iTrk = 0; iTrk < 3; iTrk++) {
                FreeTrackData(&lpblkdata[part][block][iTrk]);
            }
        }
    }
}

void GetMelodyData(int (*melody)[99], MELODYDATA melodydata) {
    BLOCKDATA blkdata[4][32];
    LoadBlocksData(blkdata);

    for(int part = 0; part < 4; part++) {
        for(int iTrk = 0; iTrk < 3; iTrk++) {
            melodydata[part * 3 + iTrk].cb = 0;
            melodydata[part * 3 + iTrk].pbData = (BYTE *)malloc(81920);
            melodydata[part * 3 + iTrk].cTk = 0;

            DWORD dwDTk = 0;
            for(int bar = 0; bar < 99; bar++) {
                int block = melody[part][bar];
                if(block != 0 && blkdata[part][block - 1][iTrk].pbData != NULL) {
                    WriteVarLenInt(melodydata[part * 3 + iTrk].pbData, &melodydata[part * 3 + iTrk].cb, dwDTk);
                    memcpy(melodydata[part * 3 + iTrk].pbData + melodydata[part * 3 + iTrk].cb,
                        blkdata[part][block - 1][iTrk].pbData + 1, blkdata[part][block - 1][iTrk].cb - 5); // Exclude delta tick at beginning (always 0) and 0x00FF2F00 at end
                    melodydata[part * 3 + iTrk].cb += blkdata[part][block - 1][iTrk].cb - 5;
                    dwDTk = 768 - blkdata[part][block - 1][iTrk].cTk;
                } else {
                    dwDTk += 768;
                }
            }
            if(melodydata[part * 3 + iTrk].cb != 0)
                WriteBEInt(melodydata[part * 3 + iTrk].pbData, &melodydata[part * 3 + iTrk].cb, 0x00FF2F00, 4); // Append 0x00FF2F00 (end of track)
        }
    }
    for(int i = 0; i < 12; i++) {
        if(melodydata[i].cb == 0) {
            free(melodydata[i].pbData);
            melodydata[i].pbData = NULL;
        } else {
            realloc(melodydata[i].pbData, melodydata[i].cb);
        }
    }

    FreeBlocksData(blkdata);
}

void FreeMelodyData(MELODYDATA melodydata) {
    for(int i = 0; i < 12; i++) {
        free(melodydata[i].pbData);
    }
}

void SaveMelody(LPCWSTR pszFilePath, int (*melody)[99], int iTempo, BOOL fSEM) {
    HANDLE hFile = CreateFile(pszFilePath, GENERIC_WRITE, 0, NULL,
        CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if(hFile == INVALID_HANDLE_VALUE)
        return;
    DWORD dwDummy;

    DWORD dwLoc = 0;

    MELODYDATA melodydata;
    GetMelodyData(melody, melodydata);

    /* Write MThd */
    BYTE abMThd[14];
    int cTrk = 1; // Including tempo track
    for(int i = 0; i < 12; i++) {
        if(melodydata[i].pbData != NULL)
            cTrk++;
    }
    WriteStr(abMThd, &dwLoc, "MThd");
    WriteBEInt(abMThd, &dwLoc, 6, 4);
    WriteBEInt(abMThd, &dwLoc, 1, 2);
    WriteBEInt(abMThd, &dwLoc, cTrk, 2);
    WriteBEInt(abMThd, &dwLoc, 192, 2);
    WriteFile(hFile, abMThd, 14, &dwDummy, NULL);

    /* Write SEM1 */
    if(fSEM) {
        BYTE abSEM1[806]; // Maximum possible size of SEM1 chunk
        dwLoc = 0;
        WriteStr(abSEM1, &dwLoc, "SEM1");
        dwLoc += 4; // Chunk size not determined yet
        int part, bar, block;
        for(part = 0; part < 4; part++) {
            int iPartLen = GetPartLen(melody, part);
            for(bar = 0; bar < iPartLen; bar++) {
                block = melody[part][bar];
                WriteByte(abSEM1, &dwLoc, (BYTE)block);
                WriteByte(abSEM1, &dwLoc, (BYTE)((block - 1) / 8));
            }
            WriteByte(abSEM1, &dwLoc, 0xFF); // End of part
        }
        DWORD dwSEM1Size = dwLoc - 6;
        dwLoc = 4;
        WriteBEInt(abSEM1, &dwLoc, dwSEM1Size, 4);
        WORD wCRC = CRC16(abSEM1, dwSEM1Size + 6);
        dwLoc = dwSEM1Size + 6;
        WriteBEInt(abSEM1, &dwLoc, wCRC, 2);
        WriteFile(hFile, abSEM1, dwLoc, &dwDummy, NULL);
    }

    /* Write tempo track */
    BYTE abTempoTrk[33] = {
        'M', 'T', 'r', 'k', 0, 0, 0, 25,
        0x00, 0xFF, 0x58, 0x04, 0x04, 0x02, 0x18, 0x08,
        0x00, 0xFF, 0x59, 0x02, 0x00, 0x00,
        0x00, 0xFF, 0x51, 0x03
    };
    dwLoc = 26;
    WriteBEInt(abTempoTrk, &dwLoc, 60000000 / iTempo, 3);
    WriteBEInt(abTempoTrk, &dwLoc, 0x00FF2F00, 4);
    WriteFile(hFile, abTempoTrk, 33, &dwDummy, NULL);

    /* Write melody data */
    for(int i = 0; i < 12; i++) {
        if(melodydata[i].pbData != NULL) {
            BYTE abTrkHdr[8];
            dwLoc = 0;
            WriteStr(abTrkHdr, &dwLoc, "MTrk");
            WriteBEInt(abTrkHdr, &dwLoc, melodydata[i].cb, 4);
            WriteFile(hFile, abTrkHdr, 8, &dwDummy, NULL);
            WriteFile(hFile, melodydata[i].pbData, melodydata[i].cb, &dwDummy, NULL);
        }
    }

    CloseHandle(hFile);
    FreeMelodyData(melodydata);
}

MIDIEVENTNODE *GetMelodyMEvtLink(int (*melody)[99]) {
    MELODYDATA melodydata;

    DWORD adwTrkLoc[12];
    BYTE abTrkCurStatus[12];
    DWORD adwTrkCurTk[12];
    BOOL afTrkIsEnd[12];

    DWORD dwCurTk = 0, dwPrevTk = 0;
    DWORD dwDTk;
    int iCurEvtTrk;
    BYTE bCurEvtStatus;
    BYTE bCurEvtData1;
    BYTE bCurEvtData2;
    DWORD cbCurEvtData;

    BOOL fAllIsEnd;

    MIDIEVENTNODE *pmenHead = NULL;
    MIDIEVENTNODE *pmenCur = NULL, *pmenPrev = NULL;

    int i;

    GetMelodyData(melody, melodydata);

    for(i = 0; i < 12; i++) {
        adwTrkLoc[i] = 0;
        abTrkCurStatus[i] = 0;
        if(melodydata[i].pbData != NULL)
            adwTrkCurTk[i] = ReadVarLenInt(melodydata[i].pbData, &adwTrkLoc[i]);
        afTrkIsEnd[i] = melodydata[i].pbData == NULL;
    }

    int iCurBar = 0;
    do {
        /* Find the track with the earliest event to be read */
        i = 0;
        while(afTrkIsEnd[i]) {
            i++;
        }
        dwCurTk = adwTrkCurTk[i];
        iCurEvtTrk = i;
        for(; i < 12; i++) {
            if(!afTrkIsEnd[i] && adwTrkCurTk[i] < dwCurTk) {
                dwCurTk = adwTrkCurTk[i];
                iCurEvtTrk = i;
            }
        }

        dwDTk = dwCurTk - dwPrevTk;
        while(dwCurTk / 768 != iCurBar) { // Enters a new bar
            iCurBar++;

            /* Add callback event for cursor moving */
            pmenCur = (MIDIEVENTNODE *)malloc(sizeof(MIDIEVENTNODE));
            pmenCur->mevt.dwDeltaTime = dwDTk % 768 != 0 ? dwDTk % 768 : 768;
            pmenCur->mevt.dwStreamID = 0;
            pmenCur->mevt.dwEvent = (MEVT_NOP << 24) | MEVT_F_CALLBACK;

            dwDTk -= pmenCur->mevt.dwDeltaTime;

            if(pmenHead == NULL)
                pmenHead = pmenCur;
            else
                pmenPrev->pmenNext = pmenCur;
            pmenPrev = pmenCur;
        }

        if(melodydata[iCurEvtTrk].pbData[adwTrkLoc[iCurEvtTrk]] & 0x80) {
            bCurEvtStatus = ReadByte(melodydata[iCurEvtTrk].pbData, &adwTrkLoc[iCurEvtTrk]);
            if(bCurEvtStatus < 0xF0)
                abTrkCurStatus[iCurEvtTrk] = bCurEvtStatus;
        } else {
            bCurEvtStatus = abTrkCurStatus[iCurEvtTrk];
        }

        pmenCur = (MIDIEVENTNODE *)malloc(sizeof(MIDIEVENTNODE));
        pmenCur->mevt.dwDeltaTime = dwDTk;
        pmenCur->mevt.dwStreamID = 0;

        if(bCurEvtStatus < 0xF0) {
            bCurEvtData1 = ReadByte(melodydata[iCurEvtTrk].pbData, &adwTrkLoc[iCurEvtTrk]);

            switch(bCurEvtStatus >> 4) {
            case 0x8:
            case 0x9:
            case 0xA:
            case 0xB:
            case 0xE:
                bCurEvtData2 = ReadByte(melodydata[iCurEvtTrk].pbData, &adwTrkLoc[iCurEvtTrk]);
                break;
            case 0xC:
            case 0xD:
                bCurEvtData2 = 0;
                break;
            }

            pmenCur->mevt.dwEvent = MAKEFOURCC(bCurEvtStatus, bCurEvtData1, bCurEvtData2, MEVT_SHORTMSG);
        } else { // Meta events
            adwTrkLoc[iCurEvtTrk]++; // Skip 0xFF
            cbCurEvtData = ReadVarLenInt(melodydata[iCurEvtTrk].pbData, &adwTrkLoc[iCurEvtTrk]);
            adwTrkLoc[iCurEvtTrk] += cbCurEvtData; // Skip meta event data

            pmenCur->mevt.dwEvent = MEVT_NOP << 24;
        }

        if(pmenHead == NULL)
            pmenHead = pmenCur;
        else
            pmenPrev->pmenNext = pmenCur;
        pmenPrev = pmenCur;

        afTrkIsEnd[iCurEvtTrk] = adwTrkLoc[iCurEvtTrk] == melodydata[iCurEvtTrk].cb;

        dwPrevTk = dwCurTk;

        /* Find whether all tracks have already ended */
        fAllIsEnd = TRUE;
        for(i = 0; i < 12; i++) 
            fAllIsEnd = fAllIsEnd && afTrkIsEnd[i];

        if(!afTrkIsEnd[iCurEvtTrk]) {
            adwTrkCurTk[iCurEvtTrk] += ReadVarLenInt(melodydata[iCurEvtTrk].pbData, &adwTrkLoc[iCurEvtTrk]);
        }
    } while(!fAllIsEnd);

    pmenPrev->pmenNext = NULL;

    FreeMelodyData(melodydata);
    return pmenHead;
}

void FreeMelodyMEvtLink(MIDIEVENTNODE *pmenHead) {
    MIDIEVENTNODE *pmenCur, *pmenNext;

    pmenCur = pmenHead;
    while(pmenCur) {
        pmenNext = pmenCur->pmenNext;
        free(pmenCur);
        pmenCur = pmenNext;
    }
}

MIDIEVENTNODE *FillMelodyStrmBuf(MIDIHDR *pmhdr, MIDIEVENTNODE *pmenCurBuf) {
    DWORD dwStrmDataOffset = 0;

    pmhdr->dwUser = FALSE; // Last buffer flag

    while(pmenCurBuf) {
        if(dwStrmDataOffset + 12 > STRMBUFLEN)
            break;
        
        memcpy(pmhdr->lpData + dwStrmDataOffset, &pmenCurBuf->mevt, 12);
        dwStrmDataOffset += 12;

        pmenCurBuf = pmenCurBuf->pmenNext;
    }

    if(!pmenCurBuf) // All events have been written
        pmhdr->dwUser = TRUE;

    pmhdr->dwBytesRecorded = dwStrmDataOffset;
    return pmenCurBuf;
}