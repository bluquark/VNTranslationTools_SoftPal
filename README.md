# Softpal-optimized fork notes

This is a fork of VNTranslationTools tailored for the SoftPal engine.  Primarily
tested on Flyable Heart (2009), and partially tested on Akatsuki Yureru Koi Akari
(2020).

The base VNTranslationTools repo has basic support for SoftPal, but
this adds the following improvements:
* Correct extraction/insertion for SoftPAL shake effects and "splits" (single textboxes split into 2 `TEXT.DAT` entries)
* Add hardcoded support for characters like `é` via the half-width katakana codespace (because SJIS tunnelling does not work in SoftPAL)
* Correct amount of spacing between letters.  (Notably, space characters are inserted as an ASCII pipe `|` to work around hardcoded space width in SoftPAL.)
* Kerning
* Italics (`<i></i>`)
* Switch between the default proportional and a monospace font (`<monospace></monospace>`)
* Fullscreen mode videos and aspect ratio fixed for older SoftPal games (i.e. 800x600 games appear correct on widescreens).  Enable/disable using `graphicsMode` feature in `VNTranslationToolsConstants.json` (this feature is not needed on newer games).

Basic instructions to use these tools to translate a SoftPal game:
1. Using [GARBro](https://github.com/morkt/GARbro), extract the game's `script.src`, `text.dat` and `point.dat` from `data.pac`.  I suggest putting them in a subdirectory called `source\`.
2. [Download](https://github.com/alexelias/VNTranslationTools_Softpal/releases) the latest release of this fork.
   * Extract the zip into the root directory of the game. Note that is especially required that `winmm.dll` (the VNTextProxy) and the TTF font files be present in the root directory of the game.
3. To extract the script, open a terminal, `cd` into the root directory of the game, and run `VNTextPatch\VNTextPatch.exe extractlocal source\script.src test.xlsx`
4. It is safe to import this `xlsx` into a Google Doc for collaborative translation. If you do that, then at insertion time, you can either use VNTranslationTools' native Google Docs insertion support, or simply export it back out to another `xlsx` file.
5. Write your translated names in the 3rd column of the sheet, and the translated messages in the 4th column of the sheet. In this fork, columns after the 4th will be ignored, so use them
for whatever you want.
6. To insert the translated script, run `mkdir data`, then `VNTextPatch\VNTextPatch.exe insertlocal source\script.src test.xlsx data\script.src`
   * 1st parameter: the extracted original SoftPal script assembly.
   * 2nd parameter: the translated script spreadsheet.
   * 3rd parameter: the output script assembly linked to the translation.
       * It is required that this be in a subdirectory called `data\`.  The SoftPal engine will look for script files in the `data\` directory first, and if it doesn't find them there, then it will fall back to `data.pac`.

See also [the original Readme](https://github.com/arcusmaximus/VNTranslationTools) for more details.
