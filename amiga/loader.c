#include <exec/types.h>
#include <exec/execbase.h>
#include <dos/dos.h>
#include <graphics/gfxbase.h>
#include <intuition/intuitionbase.h>
#include <intuition/screens.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/graphics.h>
#include <proto/intuition.h>
#include <string.h>

/* Separate title stage: its file-backed bitmap and Intuition screen are
 * released BEFORE DOS loads the game. This process exits completely. */
#define LOADER_WIDTH 320
#define LOADER_HEIGHT 200
#define LOADER_PLANES 5
#define LOADER_ROW_BYTES (LOADER_WIDTH / 8)
#define LOADER_BITMAP_BYTES (LOADER_HEIGHT * LOADER_PLANES * LOADER_ROW_BYTES)

struct ExecBase* SysBase;
struct DosLibrary* DOSBase;
struct GfxBase* GfxBase;
struct IntuitionBase* IntuitionBase;

static UBYTE loadTitle(struct Screen* screen) {
    UBYTE row[LOADER_PLANES * LOADER_ROW_BYTES];
    UBYTE paletteBytes[64];
    UWORD palette[32];
    BPTR file = Open((CONST_STRPTR)"loading_screen.pal", MODE_OLDFILE);
    if (!file) return 0;
    LONG count = Read(file, paletteBytes, sizeof(paletteBytes));
    Close(file);
    if (count != sizeof(paletteBytes)) return 0;
    for (UWORD i=0; i<32; i++) palette[i]=(paletteBytes[i*2]<<8)|paletteBytes[i*2+1];
    file = Open((CONST_STRPTR)"loading_screen.bpl", MODE_OLDFILE);
    if (!file) return 0;
    for (UWORD y=0; y<LOADER_HEIGHT; y++) {
        if (Read(file, row, sizeof(row)) != sizeof(row)) { Close(file); return 0; }
        for (UBYTE plane=0; plane<LOADER_PLANES; plane++)
            memcpy(screen->RastPort.BitMap->Planes[plane] + y*screen->RastPort.BitMap->BytesPerRow,
                   row + plane*LOADER_ROW_BYTES, LOADER_ROW_BYTES);
    }
    Close(file);
    LoadRGB4(&screen->ViewPort, palette, 32);
    return 1;
}

int main(void) {
	SysBase = *((struct ExecBase**)4UL);
	DOSBase = (struct DosLibrary*)OpenLibrary((CONST_STRPTR)"dos.library", 0);
	GfxBase = (struct GfxBase*)OpenLibrary(
		(CONST_STRPTR)"graphics.library", 0);
	IntuitionBase = (struct IntuitionBase*)OpenLibrary(
		(CONST_STRPTR)"intuition.library", 0);
	if (!DOSBase || !GfxBase || !IntuitionBase)
		goto cleanup;

	struct NewScreen newScreen = {
		0, 0, LOADER_WIDTH, LOADER_HEIGHT, LOADER_PLANES,
		0, 1, 0, CUSTOMSCREEN | SCREENQUIET,
		0, 0, 0, 0
	};
	struct Screen* screen = OpenScreen(&newScreen);
    if (screen) {
        UWORD black[32] = {0};
        LoadRGB4(&screen->ViewPort, black, 32);
        ShowTitle(screen, FALSE);
        if (loadTitle(screen)) {
            ScreenToFront(screen);
            ShowTitle(screen, FALSE);
            Delay(150);
        }
        CloseScreen(screen);
    }
    /* No title pixels, bitmap, palette allocation or title screen survives. */
    CloseLibrary((struct Library*)IntuitionBase); IntuitionBase=0;
    CloseLibrary((struct Library*)GfxBase); GfxBase=0;
    Write(Output(), (APTR)"\nLOADING...\n", 12);

cleanup:
	if (IntuitionBase)
		CloseLibrary((struct Library*)IntuitionBase);
	if (GfxBase)
		CloseLibrary((struct Library*)GfxBase);
	if (DOSBase)
		CloseLibrary((struct Library*)DOSBase);
	return 0;
}
