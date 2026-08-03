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

/* Tiny resident bootstrap.  AmigaDOS must load an executable completely
 * before its main() can run, so the full game cannot show its own bitmap
 * during that load.  This small program gets on screen first, then Execute()
 * loads/runs the large game while this two-plane page remains visible.
 * Two planes retain the title art while using 16 KiB rather than the normal
 * five-plane page's 40 KiB of precious A500 chip RAM. */

#define LOADER_WIDTH 320
#define LOADER_HEIGHT 200
#define LOADER_PLANES 2
#define LOADER_ROW_BYTES (LOADER_WIDTH / 8)
#define LOADER_BITMAP_BYTES \
	(LOADER_HEIGHT * LOADER_PLANES * LOADER_ROW_BYTES)

struct ExecBase* SysBase;
struct DosLibrary* DOSBase;
struct GfxBase* GfxBase;
struct IntuitionBase* IntuitionBase;

static const UBYTE loaderBitmap[LOADER_BITMAP_BYTES] = {
	#embed "assets/loading_screen_2plane.bpl"
};

static const UWORD loaderPalette[4] = {
	0x100, 0xf00, 0xfff, 0x57c
};

static void copyLoaderBitmap(struct Screen* screen) {
	struct BitMap* bitmap = screen->RastPort.BitMap;
	for (UWORD y = 0; y < LOADER_HEIGHT; y++) {
		const UBYTE* sourceRow = loaderBitmap +
			y * LOADER_PLANES * LOADER_ROW_BYTES;
		for (UBYTE plane = 0; plane < LOADER_PLANES; plane++) {
			UBYTE* destination = bitmap->Planes[plane] +
				y * bitmap->BytesPerRow;
			memcpy(destination,
				sourceRow + plane * LOADER_ROW_BYTES,
				LOADER_ROW_BYTES);
		}
	}
}

static LONG runGame(void) {
	static const char* const commands[] = {
		"harrier_amiga.exe",
		"DH1:harrier_amiga.exe",
		"DF0:harrier_amiga.exe"
	};
	for (UBYTE index = 0;
		index < (UBYTE)(sizeof(commands) / sizeof(commands[0])); index++) {
		if (Execute((CONST_STRPTR)commands[index], Input(), Output()))
			return 1;
	}
	return 0;
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
		copyLoaderBitmap(screen);
		LoadRGB4(&screen->ViewPort, loaderPalette, 4);
		ScreenToFront(screen);
		ShowTitle(screen, FALSE);
		runGame();
		CloseScreen(screen);
	} else {
		/* Low-memory fallback still launches the game. */
		runGame();
	}

cleanup:
	if (IntuitionBase)
		CloseLibrary((struct Library*)IntuitionBase);
	if (GfxBase)
		CloseLibrary((struct Library*)GfxBase);
	if (DOSBase)
		CloseLibrary((struct Library*)DOSBase);
	return 0;
}
