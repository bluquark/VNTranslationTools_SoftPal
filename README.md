# Softpal-optimized fork notes

This is a small fork tuned for the Softpal engine.  The base VNTranslationTools repo supports Softpal but was missing a few engine features.
I also improved the behavior of VNTextProxy with SoftPal, particularly with respect to spacing.

Basic instructions to use these tools to translate a Softpal game:
1. Using [GARBro](https://github.com/morkt/GARbro), extract the game's `script.src`, `text.dat` and `point.dat` from `data.pac`.  I suggest putting them in a subdirectory called `source\`.
2. [Download](https://github.com/alexelias/VNTranslationTools_Softpal/releases) the latest release of this fork.
   * Copy `winmm.dll` (the VNTextProxy) and `sjis_ext.bin` into the root directory of the game.
   * Copy VNTextPatch.exe anywhere you want (in the following examples, it is assumed to also be in the root directory of the game).
   * If it isn't already present, create a subdirectory called `data`.
3. To extract the script, run `.\VNTextPatch.exe extractlocal source\script.src .\test.xlsx`
4. It is safe to import this `xlsx` into a Google Doc for collaborative translation. If you do that, you can either use VNTranslationTools' native Google Docs insertion support, or simply export
it back out to another `xlsx` file.
5. Write your translated names in the 3rd column of the sheet, and the translated messages in the 4th column of the sheet. In this fork, columns after the 4th will be ignored, so use them
for whatever you want.
6. To insert the translated script, run `.\VNTextPatch.exe insertlocal source\script.src .\test.xlsx .\data\script.src .\sjis_ext.bin`
   * 1st parameter: the extracted original Softpal script assembly.
   * 2nd parameter: the translated script spreadsheet.
   * 3rd parameter: the output script assembly linked to the translation.
       * It is required that this be in a subdirectory called `data\`.  The Softpal engine will look for script files in the `data\` directory first, and if it doesn't find them there, then it will fall back to `data.pac` to search for them.
   * 4th parameter: the SJIS tunneling mapping, for special characters like `é`.
       * It is required that this be in the root directory.

See also [the original Readme](https://github.com/arcusmaximus/VNTranslationTools) for more details.
